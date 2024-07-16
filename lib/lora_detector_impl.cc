/* -*- c++ -*- */
/*
 * Copyright 2024 kazawai.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lora_detector_impl.h"

#include <gnuradio/gr_complex.h>
#include <gnuradio/io_signature.h>
#include <gnuradio/types.h>
#include <liquid/liquid.h>
#include <pmt/pmt.h>
#include <sys/types.h>
#include <volk/volk.h>
#include <volk/volk_complex.h>
#include <volk/volk_malloc.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <utility>

namespace gr {
namespace first_lora {

#define DEMOD_HISTORY (8 + 5)

int write_f_to_file(float *f, const char *filename, int n);

using input_type = gr_complex;
using output_type = gr_complex;
lora_detector::sptr lora_detector::make(float threshold, uint8_t sf,
                                        uint32_t bw, int method) {
  return gnuradio::make_block_sptr<lora_detector_impl>(threshold, sf, bw,
                                                       method);
}

/*
 * The private constructor
 */
lora_detector_impl::lora_detector_impl(float threshold, uint8_t sf, uint32_t bw,
                                       int method)
    : gr::block("lora_detector",
                gr::io_signature::make(1 /* min inputs */, 1 /* max inputs */,
                                       sizeof(input_type)),
                gr::io_signature::make(1 /* min outputs */, 1 /*max outputs */,
                                       sizeof(output_type))),
      d_threshold(threshold), d_sf(sf), d_bw(bw), d_method(method) {
  assert((d_sf > 5) && (d_sf < 13));

  // Number of symbols
  d_sps = 1 << d_sf;
  std::cout << "Symbols: " << d_sps << std::endl;
  d_sn = 2 * d_sps;
  std::cout << "Samples: " << d_sn << std::endl;

  d_fs = d_bw * 2;
  d_fft_size = 10 * d_sn;
  d_bin_size = 10 * d_sps;
  std::cout << "FFT size: " << d_fft_size << std::endl;
  std::cout << "Bin size: " << d_bin_size << std::endl;
  d_cfo = 0;
  d_max_val = 0;
  // FFT input vector
  d_mult_hf_fft = std::vector<gr_complex>(d_fft_size);
  d_fft_result = (lv_32fc_t *)malloc(d_fft_size * sizeof(lv_32fc_t));
  if (d_fft_result == NULL) {
    std::cerr << "Error: Failed to allocate memory for fft_result\n";
    return;
  }

  // Reference downchip signal
  d_ref_downchirp = g_downchirp(d_sf, d_bw, d_fs);
  // Reference upchip signal
  d_ref_upchirp = g_upchirp(d_sf, d_bw, d_fs);

  d_dechirped.reserve(d_sn);

  d_state = 0;

  message_port_register_out(pmt::mp("detected"));

  set_history(DEMOD_HISTORY * d_sn);

  // Set output buffer size to 8 + 5 * d_sn
  set_output_multiple((8 + 5) * d_sn);
}

/*
 * Our virtual destructor.
 */
lora_detector_impl::~lora_detector_impl() {
  // Free memory
  buffer.clear();
  free(d_fft_result);

  // Print the number of detected LoRa symbols
  std::cout << "Detected LoRa symbols: " << detected_count << std::endl;
  detected_count = 0;
}

void lora_detector_impl::forecast(int noutput_items,
                                  gr_vector_int &ninput_items_required) {
  /* <+forecast+> e.g. ninput_items_required[0] = noutput_items */
  ninput_items_required[0] = noutput_items;
}

uint32_t lora_detector_impl::argmax_32f(const float *x, float *max,
                                        uint16_t n) {
  float mag = abs(x[0]);
  float m = mag;
  uint32_t index = 0;

  for (int i = 0; i < n; i++) {
    mag = abs(x[i]);
    if (mag > m) {
      m = mag;
      index = i;
    }
  }

  *max = m;
  return index;
}

uint32_t lora_detector_impl::get_fft_peak_abs(const lv_32fc_t *fft_r, float *b1,
                                              float *b2, float *max) {
  uint32_t peak = 0;
  *max = 0;
  // Compute the magnitude of the FFT in b1
  volk_32fc_magnitude_32f(b1, fft_r, d_fft_size);
  // Add the last part of the FFT to the first part.
  // This is the CPA proposed in the paper to determine the phase misalignment
  volk_32f_x2_add_32f(b2, b1, &b1[d_fft_size - d_bin_size], d_bin_size);
  peak = argmax_32f(b2, max, d_bin_size);
  return peak;
}

uint32_t lora_detector_impl::get_fft_peak_phase(const lv_32fc_t *fft_r,
                                                float *b1, float *b2,
                                                gr_complex *buffer_c,
                                                float *max) {
  uint32_t tmp_max_idx = 0;
  float tmp_max_val;
  uint32_t peak = 0;
  // This is the FPA proposed in the paper to determine the phase misalignment
  for (int i = 0; i < 4; i++) {
    float phase_offset = 2 * M_PI / 4 * i;
    tmp_max_idx = fft_add(fft_r, b2, buffer_c, &tmp_max_val, phase_offset);
    if (tmp_max_val > *max) {
      *max = tmp_max_val;
      peak = tmp_max_idx;
    }
  }
  return peak;
}

uint32_t lora_detector_impl::fft_add(const lv_32fc_t *fft_result, float *buffer,
                                     gr_complex *buffer_c, float *max_val_p,
                                     float phase_offset) {
  lv_32fc_t s =
      lv_cmake((float)std::cos(phase_offset), (float)std::sin(phase_offset));
  volk_32fc_s32fc_multiply_32fc(buffer_c, fft_result, s, d_bin_size);
  volk_32fc_x2_add_32fc(buffer_c, buffer_c,
                        &fft_result[d_fft_size - d_bin_size], d_bin_size);
  volk_32fc_magnitude_32f(buffer, buffer_c, d_bin_size);
  return argmax_32f(buffer, max_val_p, d_bin_size);
}

int lora_detector_impl::compare_peak(const gr_complex *in, gr_complex *out) {
  float max_amplitude = 0.0;
  for (ulong i = 0; i < d_sn; i++) {
    // Compute the amplitude of the received sample
    float amplitude = std::abs(in[i]);

    if (amplitude > max_amplitude) {
      max_amplitude = amplitude;
    }
  }

  if (max_amplitude < d_threshold) {
    return 0;
  } else {
    return 1;
  }
}

int lora_detector_impl::write_chirp_to_file(
    const std::vector<gr_complex> &chirp, const char *filename) {
  std::cout << "Writing chirp to file\n";
  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    std::cerr << "Error: Failed to open file\n";
    return -1;
  }

  for (int j = 0; j < 5; j++) {
    for (ulong i = 0; i < chirp.size(); i++) {
      fprintf(file, "%f %f\n", chirp[i].real(), chirp[i].imag());
    }
    for (ulong i = 0; i < chirp.size(); i++) {
      fprintf(file, "%f %f\n", 0.0, 0.0);
    }
  }

  fclose(file);
  std::cout << "Chirp written to file\n";
  return 0;
}

int write_symbol_to_file(const std::vector<gr_complex> &symbol,
                         const char *filename) {
  std::cout << "Writing symbol to file\n";
  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    std::cerr << "Error: Failed to open file\n";
    return -1;
  }

  for (ulong i = 0; i < symbol.size(); i++) {
    fprintf(file, "%f %f\n", symbol[i].real(), symbol[i].imag());
  }

  fclose(file);
  std::cout << "Symbol written to file\n";
  return 0;
}

int write_fft_result_to_file(const lv_32fc_t *fft_result, const char *filename,
                             int n) {
  std::cout << "Writing fft result to file\n";
  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    std::cerr << "Error: Failed to open file\n";
    return -1;
  }

  for (int i = 0; i < n; i++) {
    fprintf(file, "%f %f\n", fft_result[i].real(), fft_result[i].imag());
  }

  fclose(file);
  std::cout << "FFT result written to file\n";
  return 0;
}

int write_f_to_file(float *f, const char *filename, int n) {
  std::cout << "Writing f to file\n";
  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    std::cerr << "Error: Failed to open file\n";
    return -1;
  }

  for (int i = 0; i < n; i++) {
    fprintf(file, "%f\n", f[i]);
  }

  fclose(file);
  std::cout << "f written to file\n";
  return 0;
}

std::pair<float, uint32_t> lora_detector_impl::dechirp(const gr_complex *in,
                                                       bool is_up) {
  gr_complex *blocks = (gr_complex *)volk_malloc(
      d_fft_size * sizeof(gr_complex), volk_get_alignment());
  if (blocks == NULL) {
    std::cerr << "Error: Failed to allocate memory for up_blocks\n";
    return std::make_pair(0, 0);
  }

  // Dechirp https://dl.acm.org/doi/10.1145/3546869#d1e1181
  volk_32fc_x2_multiply_32fc(
      blocks, in, is_up ? &d_ref_downchirp[0] : &d_ref_upchirp[0], d_sn);

  // Buffer for FFT
  // lv_32fc_t *fft_r = (lv_32fc_t *)malloc(d_fft_size * sizeof(lv_32fc_t));
  // if (fft_r == NULL) {
  //   std::cerr << "Error: Failed to allocate memory for fft_r\n";
  //   return std::make_pair(0, 0);
  // }

  // Copy dechirped signal to d_mult_hf_fft (zero padding)
  memset(&d_mult_hf_fft[0], 0, d_fft_size * sizeof(gr_complex));
  memcpy(&d_mult_hf_fft[0], blocks, d_sn * sizeof(gr_complex));

  fft = fft_create_plan(d_fft_size, &d_mult_hf_fft[0], d_fft_result,
                        LIQUID_FFT_FORWARD, 0);

  // FFT
  fft_execute(fft);

  // Destroy FFT plan
  fft_destroy_plan(fft);

  // Free memory
  volk_free(blocks);

  // Get peak of FFT
  float *b1 =
      (float *)volk_malloc(d_fft_size * sizeof(float), volk_get_alignment());
  float *b2 =
      (float *)volk_malloc(d_bin_size * sizeof(float), volk_get_alignment());

  if (b1 == NULL || b2 == NULL) {
    std::cerr << "Error: Failed to allocate memory for b1 or b2\n";
    return std::make_pair(0, 0);
  }
  float max;
  uint32_t peak = get_fft_peak_abs(d_fft_result, b1, b2, &max);

  // Free memory
  // free(fft_r);
  volk_free(b1);
  volk_free(b2);

  return std::make_pair(max, peak);
}

int lora_detector_impl::detect_preamble(const gr_complex *in, gr_complex *out) {
  int num_consumed = d_sn;
  // Check if peak is above threshold
  bool preamble_detected = false;
  if (buffer.size() < MIN_PREAMBLE_CHIRPS) {
    return num_consumed;
  } else {
    preamble_detected = true;
  }

  if (preamble_detected) {
    std::cout << "Detected preamble\n";
    d_state = 2;
    // Move preamble peak to bin zero
    num_consumed = d_sn - 2 * buffer[0] / 10;
    // detected = true;
    std::cout << "Buffer size: " << buffer.size() << std::endl;
    for (ulong i = 0; i < buffer.size(); i++) {
      std::cout << buffer[i] << std::endl;
    }
  }

  return num_consumed;
}

int lora_detector_impl::detect_sfd(const gr_complex *in, gr_complex *out,
                                   const gr_complex *in0) {
  int num_consumed = d_sn;
  detected = false;

  if (d_sfd_recovery++ > 5) {
    d_state = 0;
    std::cout << "SFD recovery failed\n";
    return 0;
  }

  auto [up_val, up_idx] = dechirp(in, true);
  auto [down_val, down_idx] = dechirp(in, false);
  // If absolute value of down_val is greater then we are in the sfd
  if (abs(up_val) >= abs(down_val)) {
    return num_consumed;
  }

  std::cout << "SFD detected\n";
  std::cout << "Up: " << up_val << " Down: " << down_val << std::endl;

  num_consumed = round(1.25 * d_sn);

  // detected = true;
  d_state = 3;
  return num_consumed;
}

int lora_detector_impl::instantaneous_frequency(const gr_complex *in, int n) {
  float sum = 0;
  for (int i = 0; i < n; i++) {
    sum += std::arg(in[i]);
  }
  return sum / n;
}

float realmod(float x, float y) {
  float result = fmod(x, y);
  return result >= 0 ? result : result + y;
}

int lora_detector_impl::general_work(int noutput_items,
                                     gr_vector_int &ninput_items,
                                     gr_vector_const_void_star &input_items,
                                     gr_vector_void_star &output_items) {
  if (ninput_items[0] < (int)(DEMOD_HISTORY * d_sn))
    return 0; // Not enough input

  auto in0 = static_cast<const input_type *>(input_items[0]);
  auto in = &in0[d_sn * (DEMOD_HISTORY - 1)]; // Get the last lora symbol
  auto out = static_cast<output_type *>(output_items[0]);
  uint32_t num_consumed = d_sn;

  // Set the output to be the reference downchirp
  // memcpy(out, &d_ref_downchirp[0], d_sn * sizeof(gr_complex));
  // consume_each(d_sn);
  // return noutput_items;

  switch (d_method) {
  case 1: {
    // Dechirp
    auto [up_val, up_idx] = dechirp(in, true);
    d_max_val = up_val;
    if (!buffer.empty()) {
      float num = (float)up_idx - (float)buffer[0];
      float distance = realmod(num, d_bin_size);
      if (distance > (float)d_bin_size / 2) {
        distance = d_bin_size - distance;
      }
      if (distance <= MAX_DISTANCE) {
        buffer.insert(buffer.begin(), up_idx);
      } else {
        buffer.clear();
        buffer.insert(buffer.begin(), up_idx);
      }
    } else {
      buffer.insert(buffer.begin(), up_idx);
    }
    // std::cout << "\n--------------\nPeak: " << peak << " Max: " << max
    //           << "\n--------------\n"
    //           << std::endl;

    switch (d_state) {
    case 0: // Reset state
      detected = false;
      buffer.clear();
      d_sfd_recovery = 0;
      d_state = 1;
      // std::cout << "State 0\n";
      break;
    case 1: // Preamble
      // std::cout << "State 2\n";
      num_consumed = detect_preamble(in, out);
      if (num_consumed != d_sn) {
        // Write the new buffer to a file
        // write_f_to_file(b2,
        //                 "/home/kazawai/dev/python/LoRa/LoRa/files/b2.txt",
        //                 d_bin_size);
      }
      d_max_val = up_val;
      break;
    case 2: // SFD
      // std::cout << "State 3\n";
      num_consumed = detect_sfd(in, out, in0);
      break;
    case 3: // Output signal
      // std::cout << "State 4\n";
      // num_consumed = noutput_items;
      detected = true;
      d_state = 0;
      break;
    }
    break;
  }
  case 0: {
    detected = compare_peak(in, out);
    num_consumed = noutput_items;
    break;
  }
  case 2: { // DEBUG
    // Dechirp
    gr_complex *blocks = (gr_complex *)volk_malloc(
        d_fft_size * sizeof(gr_complex), volk_get_alignment());
    if (blocks == NULL) {
      std::cerr << "Error: Failed to allocate memory for up_blocks\n";
      return -1;
    }

    // Dechirp https://dl.acm.org/doi/10.1145/3546869#d1e1181
    volk_32fc_x2_multiply_32fc(blocks, in, &d_ref_downchirp[0], d_sn);

    // Return the dechirped signal
    memcpy(out, blocks, d_sn * sizeof(gr_complex));
    num_consumed = d_sn;
    consume_each(num_consumed);
    return num_consumed;

    break;
  }
  default:
    std::cerr << "Error: Invalid method\n";
    return -1;
  }

  // Reset output buffer
  memset(out, 0, noutput_items * sizeof(gr_complex));

  if (detected) {
    std::cout << "Detected\n";
    detected_count++;
    // Signal should be centered around the peak of the preamble
    // Copy the preamble to the output
    memcpy(out, in0, (8 + 5) * d_sn * sizeof(gr_complex));
    std::cout << "Copied to output" << std::endl;

    // Send "detected" message
    message_port_pub(pmt::mp("detected"), pmt::from_bool(true));

    std::cout << "Sended message if connected" << std::endl;
    consume_each(noutput_items);
    std::cout << "Consumed noutput_items" << std::endl;
    return (8 + 5) * d_sn;
  } else {
    // If no peak is detected, we do not want to output anything
    consume_each(num_consumed);
    return 0;
  }

  // Set the output to be the reference downchirp
  // memcpy(out, &d_ref_downchirp[0], noutput_items * sizeof(gr_complex));

  // Tell runtime system how many output items we produced.
  // return noutput_items;
}

} /* namespace first_lora */
} /* namespace gr */
