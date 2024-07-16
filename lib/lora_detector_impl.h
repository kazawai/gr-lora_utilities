/* -*- c++ -*- */
/*
 * Copyright 2024 kazawai.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_FIRST_LORA_LORA_DETECTOR_IMPL_H
#define INCLUDED_FIRST_LORA_LORA_DETECTOR_IMPL_H

#include <gnuradio/expj.h>
#include <gnuradio/first_lora/lora_detector.h>
#include <gnuradio/gr_complex.h>
#include <liquid/liquid.h>
#include <pmt/pmt.h>
#include <volk/volk_complex.h>

#include <complex>
#include <cstdint>
#include <vector>

#define MIN_PREAMBLE_CHIRPS 6
#define MAX_DISTANCE 10

namespace gr {
namespace first_lora {

static int detected_count = 0; // Number of detected LoRa symbols

static const pmt::pmt_t d_pmt_detected = pmt::intern("detected");

class lora_detector_impl : public lora_detector {
private:
  float d_threshold;                   // Threshold for detecting LoRa signal
  uint8_t d_sf;                        // Spreading factor
  uint32_t d_bw;                       // Bandwidth
  uint32_t d_fs;                       // Sampling rate
  int d_method;                        // Method used
  int d_prev_detected = 0;             // Previous detected LoRa symbols
  uint32_t d_sps;                      // Samples per symbol (2^sf)
  uint32_t d_sn;                       // Number of samples
  float d_cfo;                         // Carrier frequency offset
  float d_max_val;                     // Maximum value of the FFT
  std::vector<uint32_t> buffer;        // Buffer for LoRa symbol
  std::vector<gr_complex> d_dechirped; // Dechirped samples
  std::vector<gr_complex> d_ref_downchirp; // Downchirp reference signal
  std::vector<gr_complex> d_ref_upchirp;   // Upchirp reference signal
  uint32_t d_fft_size;                     // FFT size
  uint32_t d_bin_size;                     // Bin size (d_fft_size / 2)
  fftplan fft;                             // FFT plan
  std::vector<gr_complex> d_mult_hf_fft;   // FFT result
  int d_sfd_recovery = 0;                  // SFD recovery count
  bool detected = false;                   // Detected LoRa signal
  int d_state = 0;                         // State of the detector
  lv_32fc_t *d_fft_result;                 // FFT result
  /**
   * @brief Generate chirp signal
   * chirp(t;f_0) = A(t)exp(j2π(f_0 + (B/2T)t)t) (where A(t) is the amplitude
   * envelope, f_0 is the initial frequency, B is the bandwidth, and T is the
   * chirp period)
   * @param sf Spreading factor
   * @param bw Bandwidth
   * @param fs Sampling rate
   * @param upchirp Upchirp or downchirp
   * @return Chirp signal
   */
  std::vector<gr_complex> g_chirp(uint8_t sf, uint32_t bw, uint32_t fs,
                                  bool upchirp) {
    std::vector<gr_complex> chirp;
    uint32_t n = (1 << sf) * 2;
    double T = n / (double)fs;
    for (ulong i = 0; i < n; i++) {
      double t = i / (double)fs;
      double phase = 2 * M_PI * (bw / (2 * T) * t * t);
      if (!upchirp) {
        phase = -phase;
      }
      chirp.push_back(gr_complex(std::cos(phase), std::sin(phase)));
    }
    return chirp;
  }

  /**
   * @brief Generate chirp signal (equivalent method to the traditional one)
   * @see g_chirp
   */
  std::vector<gr_complex> g_chirp2(uint8_t sf, uint32_t bw, uint32_t fs,
                                   bool upchirp) {
    std::vector<gr_complex> chirp;
    uint32_t n = (1 << sf) * 2;
    int fsr = (int)fs / bw;
    for (ulong i = 0; i < n; i++) {
      double phase = M_PI / fsr * (i - i * i / (float)n);
      chirp.push_back(gr_complex(std::polar(1.0, upchirp ? -phase : phase)));
    }
    return chirp;
  }

  std::vector<gr_complex> g_chirp3(uint8_t sf, uint32_t bw, uint32_t fs,
                                   bool upchirp) {
    std::vector<gr_complex> chirp;
    uint32_t n = (1 << sf) * 2;
    for (ulong i = 0; i < n; i++) {
      chirp.push_back(gr_complex(1.0, 1.0) *
                      gr_expj(2.0 * M_PI * 1 / fs * i *
                              (bw / 2.0 * (-0.5 * bw * n / 2) * 1 / fs * i) *
                              (upchirp ? 1 : -1.0f)));
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
    return g_chirp2(sf, bw, fs, false);
  }
  /**
   * @brief Generate upchirp signal
   * @param sf Spreading factor
   * @param bw Bandwidth
   * @param fs Sampling rate
   * @return Upchirp signal
   */
  std::vector<gr_complex> g_upchirp(uint8_t sf, uint32_t bw, uint32_t fs) {
    return g_chirp2(sf, bw, fs, true);
  }

  int write_chirp_to_file(const std::vector<gr_complex> &chirp,
                          const char *filename);

  /**
   * @brief Get peak of FFT using ABS comparaison
   * @param fft_r FFT result
   * @param b1 Buffer 1 (real)
   * @param b2 Buffer 2 (imag)
   * @param buffer_c Buffer complex
   * @return Peak of FFT
   */
  uint32_t get_fft_peak_abs(const lv_32fc_t *fft_r, float *b1, float *b2,
                            float *max);

  /**
   * @brief Get peak of FFT using its phase
   * @param fft_r FFT result
   * @param b1 Buffer 1 (real)
   * @param b2 Buffer 2 (imag)
   * @param buffer_c Buffer complex
   * @return Peak of FFT
   */
  uint32_t get_fft_peak_phase(const lv_32fc_t *fft_r, float *b1, float *b2,
                              gr_complex *buffer_c, float *max);

  /**
   * @brief Add FFT to phase offset
   * @param fft_result FFT result
   * @param buffer Buffer 1
   * @param buffer_c Buffer complex
   * @param max_val_p the maximum value of the new FFT
   * @param phase_offset The phase offset
   */
  uint32_t fft_add(const lv_32fc_t *fft_result, float *buffer,
                   gr_complex *buffer_c, float *max_val_p, float phase_offset);

  /**
   * @brief Get maximum value of array
   * @param x Array
   * @param max Maximum value
   * @param n Length of array
   * @return Maximum value
   */
  uint32_t argmax_32f(const float *x, float *max, uint16_t n);

  int compare_peak(const gr_complex *in, gr_complex *out);

  std::pair<float, uint32_t> dechirp(const gr_complex *in, bool is_up);

  int instantaneous_frequency(const gr_complex *in, int n);

  int detect_preamble(const gr_complex *in, gr_complex *out);

  int detect_sfd(const gr_complex *in, gr_complex *out, const gr_complex *in0);

  void on_detected_message(pmt::pmt_t msg);

public:
  lora_detector_impl(float threshold, uint8_t sf, uint32_t bw, int method);
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
