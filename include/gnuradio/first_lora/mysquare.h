/* -*- c++ -*- */
/*
 * Copyright 2024 kazawai.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_FIRST_LORA_MYSQUARE_H
#define INCLUDED_FIRST_LORA_MYSQUARE_H

#include <gnuradio/block.h>
#include <gnuradio/first_lora/api.h>

namespace gr {
namespace first_lora {

/*!
 * \brief <+description of block+>
 * \ingroup first_lora
 *
 */
class FIRST_LORA_API mysquare : virtual public gr::block
{
public:
    typedef std::shared_ptr<mysquare> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of first_lora::mysquare.
     *
     * To avoid accidental use of raw pointers, first_lora::mysquare's
     * constructor is in a private implementation
     * class. first_lora::mysquare::make is the public interface for
     * creating new instances.
     */
    static sptr make();
};

} // namespace first_lora
} // namespace gr

#endif /* INCLUDED_FIRST_LORA_MYSQUARE_H */
