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

#include <cstdint>
#include <cstdlib>

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

  // Samples per symbol (2^sf)
  d_sps = 1 << d_sf;
  std::cout << "Samples per symbol: " << d_sps << std::endl;

  d_bw = bw;
  d_fs = bw * 2;
  d_sr = sr;
  index = 0;
  d_fft_size = 10 * (2 * d_sps);
  d_bin_size = 10 * d_sps;
  // FFT decimation result
  std::vector<gr_complex> d_mult_hf = std::vector<gr_complex>(d_sps);
  // FFT result
  d_mult_hf_fft = std::vector<gr_complex>(d_sps);
  fft = fft_create_plan(d_sps, &d_mult_hf[0], &d_mult_hf_fft[0],
                        LIQUID_FFT_FORWARD, 0);

  // Reference downchip signal
  d_ref_downchip = g_downchirp(d_sf, d_bw, d_fs);

  d_dechirped.reserve(d_sps);
}

/*
 * Our virtual destructor.
 */
lora_detector_impl::~lora_detector_impl() {
  /* <+destructor+> */
  fft_destroy_plan(fft);
}

void lora_detector_impl::forecast(int noutput_items,
                                  gr_vector_int &ninput_items_required) {
  /* <+forecast+> e.g. ninput_items_required[0] = noutput_items */
  ninput_items_required[0] = noutput_items;
}

int32_t lora_detector_impl::dechirp(const gr_complex *in, gr_complex *block) {
  volk_32fc_x2_multiply_32fc(block, in, &d_ref_downchip[0], d_sps);
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

uint32_t lora_detector_impl::get_fft_peak(const lv_32fc_t *fft_r, float *b1,
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

int lora_detector_impl::general_work(int noutput_items,
                                     gr_vector_int &ninput_items,
                                     gr_vector_const_void_star &input_items,
                                     gr_vector_void_star &output_items) {
  auto in = static_cast<const input_type *>(input_items[0]);
  auto out = static_cast<output_type *>(output_items[0]);

  // Since we only want to detect the preamble, we don't need to process the
  // downchirps
  gr_complex *up_blocks = (gr_complex *)volk_malloc(d_sps * sizeof(gr_complex),
                                                    volk_get_alignment());
  if (up_blocks == NULL) {
    std::cerr << "Error: Failed to allocate memory for up_blocks\n";
    return -1;
  }

  // Symbols per second
  double sps_f = ((double)d_sr) / d_sps;

  // Dechirp
  dechirp(in, up_blocks);

  // Buffer for FFT
  lv_32fc_t *fft_r = (lv_32fc_t *)malloc(d_fft_size * sizeof(lv_32fc_t));
  if (fft_r == NULL) {
    std::cerr << "Error: Failed to allocate memory for fft_r\n";
    return -1;
  }

  // FFT
  fft_execute(fft);
  // Copy FFT result
  memcpy(fft_r, &d_mult_hf_fft[0], d_fft_size * sizeof(lv_32fc_t));

  // Get peak of FFT
  float *b1 =
      (float *)volk_malloc(d_fft_size * sizeof(float), volk_get_alignment());
  float *b2 =
      (float *)volk_malloc(d_bin_size * sizeof(float), volk_get_alignment());
  if (b1 == NULL || b2 == NULL) {
    std::cerr << "Error: Failed to allocate memory for b1 or b2\n";
    return -1;
  }
  float max = 0;
  uint32_t peak = get_fft_peak(fft_r, b1, b2, &max);
  buffer.push_back(peak);
  std::cout << "Peak of FFT: " << peak << std::endl;

  // Free memory
  volk_free(up_blocks);
  free(fft_r);
  volk_free(b1);
  volk_free(b2);

  // Check if peak is above threshold
  if (buffer.size() >= MIN_PREAMBLE_CHIRPS) {
    // Preamble detected
    // Check if preamble is valid (iterate over buffer)
    // to check for maximum distance (drift)
    for (int i = 0; i < buffer.size(); i++) {
      if (buffer[i] - buffer[i - 1] > MAX_DRIFT) {
        // Preamble is invalid
        buffer.clear();
        detected = false;
        return 0;
      }
    }

    // Preamble is valid
    memcpy(out, in, noutput_items * sizeof(gr_complex));
    std::cout << "Preamble detected\n";
    detected = true;
  }

  if (!detected) {
    memset(out, 0, noutput_items * sizeof(gr_complex));
  }

  // Do <+signal processing+>
  // Tell runtime system how many input items we consumed on
  // each input stream.
  consume_each(noutput_items);
  detected = false;

  // Tell runtime system how many output items we produced.
  return noutput_items;
}

} /* namespace first_lora */
} /* namespace gr */
