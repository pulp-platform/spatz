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
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/vme/apps")

option(VME_TE32 "Build VME kernels for a TE=32 hardware configuration" OFF)
if(VME_TE32)
    set(VME_TILE_VARIANT te32)
else()
    set(VME_TILE_VARIANT te16)
endif()
message(STATUS "Building VME ${VME_TILE_VARIANT} kernels")

add_subdirectory(tew32/fp32-matmul-vtse fp32-matmul-vtse)
add_subdirectory(tew32/fp32-matmul-vse fp32-matmul-vse)
add_subdirectory(tew32/fp16-matmul-vtse fp16-matmul-vtse)
add_subdirectory(tew32/fp16-gemm-unit fp16-gemm-unit)
add_subdirectory(tew32/fp16-gemm-alpha fp16-gemm-alpha)
add_subdirectory(tew32/fp16-matmul-vse fp16-matmul-vse)
