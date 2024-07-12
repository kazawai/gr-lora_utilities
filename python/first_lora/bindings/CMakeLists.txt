# Copyright 2020 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# Check if there is C++ code at all
########################################################################
if(NOT first_lora_sources)
    message(STATUS "No C++ sources... skipping python bindings")
    return()
endif(NOT first_lora_sources)

########################################################################
# Check for pygccxml
########################################################################
gr_python_check_module_raw("pygccxml" "import pygccxml" PYGCCXML_FOUND)

include(GrPybind)

########################################################################
# Python Bindings
########################################################################
list(APPEND first_lora_python_files
    mysquare_python.cc
    lora_detector_python.cc
    python_bindings.cc)

gr_pybind_make_oot(first_lora ../../.. gr::first_lora "${first_lora_python_files}")

# copy bindings extension for use in QA test module
add_custom_command(
    TARGET first_lora_python
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:first_lora_python>
            ${PROJECT_BINARY_DIR}/test_modules/gnuradio/first_lora/)

install(
    TARGETS first_lora_python
    DESTINATION ${GR_PYTHON_DIR}/gnuradio/first_lora
    COMPONENT pythonapi)
