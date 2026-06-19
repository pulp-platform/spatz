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

macro(add_vme_fp_test name file param_m param_n param_k)
    set(_target ${name}_M${param_m}_N${param_n}_K${param_k})
    add_snitch_test(${_target} ${file})
    target_compile_definitions(test-${SNITCH_TEST_PREFIX}${_target} PUBLIC
        DATAHEADER="data/data_${param_m}_${param_n}_${param_k}.h")
endmacro()

set(SNITCH_TEST_PREFIX vme-)

add_vme_fp_test(fp-gemm fp-gemm/main.c 64 64 64)