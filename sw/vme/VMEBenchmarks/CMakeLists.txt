# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(NOT DEFINED SNRT_TCDM_SIZE OR NOT SNRT_TCDM_SIZE GREATER 0)
    message(STATUS "Skipping VME benchmarks: SNRT_TCDM_SIZE is not configured")
    return()
endif()

set(SNITCH_TEST_PREFIX vme-)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/vme/VMEBenchmarks")

add_subdirectory(widening-hp-fmatmul)
