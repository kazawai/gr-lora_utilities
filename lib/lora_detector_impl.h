/* -*- c++ -*- */
/*
 * Copyright 2024 kazawai.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_FIRST_LORA_LORA_DETECTOR_IMPL_H
#define INCLUDED_FIRST_LORA_LORA_DETECTOR_IMPL_H

#include <gnuradio/first_lora/lora_detector.h>
#include <gnuradio/gr_complex.h>
#include <liquid/liquid.h>
#include <volk/volk_complex.h>

#include <complex>
#include <cstdint>
#include <vector>

#define MIN_PREAMBLE_CHIRPS 4
#define MAX_DRIFT 0.1

namespace gr {
namespace first_lora {

class lora_detector_impl : public lora_detector {
private:
  float d_threshold;                      // Threshold for detecting LoRa signal
  uint8_t d_sf;                           // Spreading factor
  uint32_t d_bw;                          // Bandwidth
  uint32_t d_fs;                          // Sampling rate
  uint32_t d_sr;                          // Symbol rate
  uint32_t d_sps;                         // Samples per symbol (2^sf)
  std::vector<uint32_t> buffer;           // Buffer for LoRa symbol
  std::vector<gr_complex> d_dechirped;    // Dechirped samples
  std::vector<gr_complex> d_ref_downchip; // Downchip reference signal
  int32_t index;                          // Index for buffer
  uint8_t detected_count = 0;             // Number of detected LoRa symbols
  uint16_t d_fft_size;                    // FFT size
  uint16_t d_bin_size;                    // Bin size (d_fft_size / 2)
  fftplan fft;                            // FFT plan
  std::vector<gr_complex> d_mult_hf_fft;  // FFT result
  bool detected = false;                  // Detected LoRa signal
  /**
   * @brief Dechirp LoRa symbol
   * https://dl.acm.org/doi/10.1145/3546869#d1e1181
   * @param in LoRa symbol
   * @param block Dechirped LoRa symbol
   * @return Value of LoRa symbol (dechirped)
   */
  int32_t dechirp(const gr_complex *in,
                  gr_complex *block); // Dechirp LoRa symbol
  /**
   * @brief Generate chirp signal
   * @param sf Spreading factor
   * @param bw Bandwidth
   * @param fs Sampling rate
   * @param upchirp Upchirp or downchirp
   * @return Chirp signal
   */
  std::vector<gr_complex> g_chirp(uint8_t sf, uint32_t bw, uint32_t fs,
                                  bool upchirp) {
    std::vector<gr_complex> chirp;
    uint32_t n = 1 << sf;
    double fsr = (double)fs / bw;
    for (int i = 0; i < n; i++) {
      double phase = M_PI / (int)fsr * (i - i * i / (double)n);
      chirp.push_back(gr_complex(std::polar(1.0, upchirp ? phase : -phase)));
    }
    return chirp;
  }
  /**
   * @brief Generate downchirp signal
   * @param sf Spreading factor
   * @param bw Bandwidth
   * @param fs Sampling rate
   * @return Downchirp signal
   */
  std::vector<gr_complex> g_downchirp(uint8_t sf, uint32_t bw, uint32_t fs) {
    return g_chirp(sf, bw, fs, false);
  }
  /**
   * @brief Generate upchirp signal
   * @param sf Spreading factor
   * @param bw Bandwidth
   * @param fs Sampling rate
   * @return Upchirp signal
   */
  std::vector<gr_complex> g_upchirp(uint8_t sf, uint32_t bw, uint32_t fs) {
    return g_chirp(sf, bw, fs, true);
  }

  /**
   * @brief Get peak of FFT
   * @param fft_r FFT result
   * @param b1 Buffer 1 (real)
   * @param b2 Buffer 2 (imag)
   * @param buffer_c Buffer complex
   * @return Peak of FFT
   */
  uint32_t get_fft_peak(const lv_32fc_t *fft_r, float *b1, float *b2,
                        float *max);
  /**
   * @brief Get maximum value of array
   * @param x Array
   * @param max Maximum value
   * @param n Length of array
   * @return Maximum value
   */
  uint32_t argmax_32f(const float *x, float *max, uint16_t n);

public:
  lora_detector_impl(float threshold, uint8_t sf, uint32_t bw, uint32_t sr);
  ~lora_detector_impl();

  // Where all the action really happens
  void forecast(int noutput_items, gr_vector_int &ninput_items_required);

  int general_work(int noutput_items, gr_vector_int &ninput_items,
                   gr_vector_const_void_star &input_items,
                   gr_vector_void_star &output_items);
};

} // namespace first_lora
} // namespace gr

#endif /* INCLUDED_FIRST_LORA_LORA_DETECTOR_IMPL_H */
