# Copyright 2024 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Author: Pei-Yu Lin <peilin@ethz.ch>

cmake_minimum_required(VERSION 3.13)

add_compile_options(-O3 -g0 -ffunction-sections)
add_compile_options(-DELEN=${ELEN})

include_directories(${SNRUNTIME_INCLUDE_DIRS})
include_directories(../../riscvTests/isa/macros/vector)

enable_testing()
set(SNITCH_TEST_PREFIX vme-)

macro(add_vme_fp_test name file param_m param_n param_k)
    set(_target ${name}_M${param_m}_N${param_n}_K${param_k})
    add_snitch_test(${_target} ${file})
    target_compile_definitions(test-${SNITCH_TEST_PREFIX}${_target} PUBLIC
        DATAHEADER="data/data_${param_m}_${param_n}_${param_k}.h")
endmacro()

macro(add_vme_int_test name file param_m param_n param_k)
    set(_target ${name}_M${param_m}_N${param_n}_K${param_k})
    add_snitch_test(${_target} ${file})
    target_compile_definitions(test-${SNITCH_TEST_PREFIX}${_target} PUBLIC
        DATAHEADER="data/data_int_${param_m}_${param_n}_${param_k}.h")
endmacro()

macro(add_vme_test name file param_m param_n param_k)
    add_vme_int_test(${name} ${file} ${param_m} ${param_n} ${param_k})
endmacro()

add_subdirectory(gemm)