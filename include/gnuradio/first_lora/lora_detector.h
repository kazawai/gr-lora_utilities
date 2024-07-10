/* -*- c++ -*- */
/*
 * Copyright 2024 kazawai.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_FIRST_LORA_LORA_DETECTOR_H
#define INCLUDED_FIRST_LORA_LORA_DETECTOR_H

#include <gnuradio/block.h>
#include <gnuradio/first_lora/api.h>

namespace gr {
namespace first_lora {

/*!
 * \brief <+description of block+>
 * \ingroup first_lora
 *
 * \details Implementation based on the LoRa PHY layer.
 *  View
 * https://wirelesspi.com/understanding-lora-phy-long-range-physical-layer/
 */
class FIRST_LORA_API lora_detector : virtual public gr::block {
 public:
  typedef std::shared_ptr<lora_detector> sptr;

  /*!
   * \brief Return a shared_ptr to a new instance of first_lora::lora_detector.
   *
   * To avoid accidental use of raw pointers, first_lora::lora_detector's
   * constructor is in a private implementation
   * class. first_lora::lora_detector::make is the public interface for
   * creating new instances.
   */
  static sptr make(float threshold = 0.1, uint8_t sf = 7, uint32_t bw = 125000,
                   int method = 0);
};

}  // namespace first_lora
}  // namespace gr

#endif /* INCLUDED_FIRST_LORA_LORA_DETECTOR_H */
