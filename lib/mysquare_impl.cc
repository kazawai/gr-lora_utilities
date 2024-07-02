/* -*- c++ -*- */
/*
 * Copyright 2024 kazawai.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mysquare_impl.h"

#include <gnuradio/gr_complex.h>
#include <gnuradio/io_signature.h>

namespace gr {
namespace first_lora {

// Define input and output types as complex
using input_type = gr_complex;
using output_type = gr_complex;
mysquare::sptr mysquare::make() {
  return gnuradio::make_block_sptr<mysquare_impl>();
}

/*
 * The private constructor
 */
mysquare_impl::mysquare_impl()
    : gr::block("mysquare",
                gr::io_signature::make(1 /* min inputs */, 1 /* max inputs */,
                                       sizeof(input_type)),
                gr::io_signature::make(1 /* min outputs */, 1 /*max outputs */,
                                       sizeof(output_type))) {}

/*
 * Our virtual destructor.
 */
mysquare_impl::~mysquare_impl() {}

void mysquare_impl::forecast(int noutput_items,
                             gr_vector_int& ninput_items_required) {
  /* <+forecast+> e.g. ninput_items_required[0] = noutput_items */
  ninput_items_required[0] = noutput_items;
}

int mysquare_impl::general_work(int noutput_items, gr_vector_int& ninput_items,
                                gr_vector_const_void_star& input_items,
                                gr_vector_void_star& output_items) {
  auto in = static_cast<const input_type*>(input_items[0]);
  auto out = static_cast<output_type*>(output_items[0]);

  for (int i = 0; i < noutput_items; i++) {
    std::cout << in[i] << std::endl;
  }
  // Do <+signal processing+>
  // Tell runtime system how many input items we consumed on
  // each input stream.
  consume_each(noutput_items);

  // Tell runtime system how many output items we produced.
  return noutput_items;
}

} /* namespace first_lora */
} /* namespace gr */
