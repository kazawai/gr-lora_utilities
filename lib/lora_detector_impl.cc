/* -*- c++ -*- */
/*
 * Copyright 2024 kazawai.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lora_detector_impl.h"

#include <gnuradio/gr_complex.h>
#include <gnuradio/io_signature.h>
#include <liquid/liquid.h>
#include <volk/volk.h>
#include <volk/volk_complex.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace gr {
namespace first_lora {

using input_type = gr_complex;
using output_type = gr_complex;
lora_detector::sptr lora_detector::make(float threshold, uint8_t sf,
                                        uint32_t bw, uint32_t sr) {
  return gnuradio::make_block_sptr<lora_detector_impl>(threshold, sf, bw, sr);
}

/*
 * The private constructor
 */
lora_detector_impl::lora_detector_impl(float threshold, uint8_t sf, uint32_t bw,
                                       uint32_t sr)
    : gr::block("lora_detector",
                gr::io_signature::make(1 /* min inputs */, 1 /* max inputs */,
                                       sizeof(input_type)),
                gr::io_signature::make(1 /* min outputs */, 1 /*max outputs */,
                                       sizeof(output_type))) {
  d_threshold = threshold;
  d_sf = sf;

  // Number of symbols
  d_sps = 1 << d_sf;
  std::cout << "Symbols: " << d_sps << std::endl;
  d_sn = 2 * d_sps;

  d_bw = bw;
  d_fs = bw * 2;
  d_sr = sr;
  index = 0;
  d_fft_size = 10 * (2 * d_sps);
  d_bin_size = 10 * d_sps;
  // FFT input vector
  d_mult_hf_fft = std::vector<gr_complex>(d_fft_size);

  // Reference downchip signal
  d_ref_downchip = g_downchirp(d_sf, d_bw, d_fs);

  d_dechirped.reserve(d_sps);
}

/*
 * Our virtual destructor.
 */
lora_detector_impl::~lora_detector_impl() { /* <+destructor+> */ }

void lora_detector_impl::forecast(int noutput_items,
                                  gr_vector_int &ninput_items_required) {
  /* <+forecast+> e.g. ninput_items_required[0] = noutput_items */
  ninput_items_required[0] = noutput_items;
}

int32_t lora_detector_impl::dechirp(const gr_complex *in, gr_complex *block) {
  volk_32fc_x2_multiply_32fc(block, in, &d_ref_downchip[0], d_sn);
  return 0;
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

  return index;
}

uint32_t lora_detector_impl::get_fft_peak_abs(const lv_32fc_t *fft_r, float *b1,
                                              float *b2, float *max) {
  // TODO : Peak always returns 0.. Fix this
  uint32_t peak = 0;
  max = 0;
  volk_32fc_magnitude_32f(b1, fft_r, d_fft_size);
  volk_32f_x2_add_32f(b2, b1, &b1[d_fft_size - d_bin_size],
                      d_fft_size - d_bin_size);
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

int lora_detector_impl::general_work(int noutput_items,
                                     gr_vector_int &ninput_items,
                                     gr_vector_const_void_star &input_items,
                                     gr_vector_void_star &output_items) {
  auto in = static_cast<const input_type *>(input_items[0]);
  auto out = static_cast<output_type *>(output_items[0]);

  uint32_t num_consumed = d_sn;

  // Since we only want to detect the preamble, we don't need to process the
  // downchirps
  gr_complex *up_blocks = (gr_complex *)volk_malloc(d_sn * sizeof(gr_complex),
                                                    volk_get_alignment());
  if (up_blocks == NULL) {
    std::cerr << "Error: Failed to allocate memory for up_blocks\n";
    return -1;
  }

  // Dechirp
  dechirp(in, up_blocks);

  // Buffer for FFT
  lv_32fc_t *fft_r = (lv_32fc_t *)malloc(d_fft_size * sizeof(lv_32fc_t));
  if (fft_r == NULL) {
    std::cerr << "Error: Failed to allocate memory for fft_r\n";
    return -1;
  }

  // Copy dechirped signal to d_mult_hf_fft
  memset(&d_mult_hf_fft[0], 0, d_fft_size * sizeof(gr_complex));
  memcpy(&d_mult_hf_fft[0], up_blocks, d_sps * sizeof(gr_complex));

  fft = fft_create_plan(d_fft_size, &d_mult_hf_fft[0], fft_r,
                        LIQUID_FFT_FORWARD, 0);

  // FFT
  fft_execute(fft);

  // Destroy FFT plan
  fft_destroy_plan(fft);

  // Get peak of FFT
  float *b1 =
      (float *)volk_malloc(d_fft_size * sizeof(float), volk_get_alignment());
  float *b2 =
      (float *)volk_malloc(d_bin_size * sizeof(float), volk_get_alignment());
  gr_complex *buffer_c = (gr_complex *)volk_malloc(
      d_bin_size * sizeof(gr_complex), volk_get_alignment());
  if (b1 == NULL || b2 == NULL) {
    std::cerr << "Error: Failed to allocate memory for b1 or b2\n";
    return -1;
  }
  float max;
  uint32_t peak = get_fft_peak_abs(fft_r, b1, b2, &max);
  buffer.push_back(peak);

  // Free memory
  volk_free(up_blocks);
  free(fft_r);
  volk_free(b1);
  volk_free(b2);

  if (buffer.size() > MIN_PREAMBLE_CHIRPS) {
    buffer.erase(buffer.begin());
  }

  // Check if peak is above threshold
  if (buffer.size() >= MIN_PREAMBLE_CHIRPS) {
    detected = true;
    // Preamble detected
    // Check if preamble is valid (iterate over buffer)
    // to check for maximum distance (drift)
    for (int i = 1; i < buffer.size(); i++) {
      if (buffer[i] - buffer[i - 1] > MAX_DRIFT) {
        // Preamble is invalid
        detected = false;
      }
    }

    if (detected) {
      // Preamble is valid
      std::cout << "Preamble detected\n";
      memcpy(out, in, noutput_items * sizeof(gr_complex));
      num_consumed = noutput_items;
      detected = true;
    }
  }

  if (!detected) {
    memset(out, 0, noutput_items * sizeof(gr_complex));
  }

  // Do <+signal processing+>
  // Tell runtime system how many input items we consumed on
  // each input stream.
  consume_each(num_consumed);

  // Tell runtime system how many output items we produced.
  return noutput_items;
}

} /* namespace first_lora */
} /* namespace gr */
