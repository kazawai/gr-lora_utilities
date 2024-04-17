/* -*- c++ -*- */
/*
 * Copyright 2024 kazawai.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_FIRST_LORA_MYSQUARE_IMPL_H
#define INCLUDED_FIRST_LORA_MYSQUARE_IMPL_H

#include <gnuradio/first_lora/mysquare.h>

namespace gr {
namespace first_lora {

class mysquare_impl : public mysquare
{
private:
    // Nothing to declare in this block.

public:
    mysquare_impl();
    ~mysquare_impl();

    // Where all the action really happens
    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);
};

} // namespace first_lora
} // namespace gr

#endif /* INCLUDED_FIRST_LORA_MYSQUARE_IMPL_H */
