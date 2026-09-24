# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Author: Pei-Yu Lin <peilin@ethz.ch>

cmake_minimum_required(VERSION 3.13)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kernel)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/data)

enable_testing()

set(SNITCH_TEST_PREFIX vme-)

# Register the FP32 VTMV/VSE variants directly from the VME apps level.
# The variant CMake file assigns the requested test-fp32-* target names.
add_subdirectory(tew32/fp32-matmul-vtse)
