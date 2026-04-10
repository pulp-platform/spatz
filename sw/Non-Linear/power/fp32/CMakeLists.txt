# Copyright 2020 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# FP32 Power Evaluation Framework
# Generates 64 test binaries: 16 primitives × 4 LMUL modes (m1, m2, m4, m8)
# 8 SW approximation kernels + 8 HW custom-instruction kernels

cmake_minimum_required(VERSION 3.13)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
    set(CMAKE_TOOLCHAIN_FILE toolchain-gcc CACHE STRING "Toolchain to use")
    project(PowerFP32 LANGUAGES C ASM)
    include(SnitchUtilities)
    add_subdirectory(../../snRuntime snRuntime)
endif()

include_directories(${SNRUNTIME_INCLUDE_DIRS})
add_compile_options(-O3 -g -ffunction-sections)

enable_testing()
set(SNITCH_TEST_PREFIX power-fp32-)

# ---------- source list (name → source file) ----------
set(POWER_KERNELS
    exp_sw
    exp_hw
    cosh_sw
    cosh_hw
    tanh_sw
    tanh_hw
    log_sw
    log_hw
    rec_sw
    rec_hw
    rsqrt_sw
    rsqrt_hw
    sin_sw
    sin_hw
    cos_sw
    cos_hw
)

# ---------- generate 4 LMUL variants per kernel ----------
foreach(KERNEL ${POWER_KERNELS})
    foreach(LMUL 1 2 4 8)
        set(TARGET_NAME ${KERNEL}_m${LMUL})
        add_snitch_test(${TARGET_NAME}
            ${CMAKE_CURRENT_SOURCE_DIR}/${KERNEL}.c)
        target_compile_definitions(
            test-${SNITCH_TEST_PREFIX}${TARGET_NAME}
            PUBLIC LMUL_MODE=${LMUL})
    endforeach()
endforeach()

# ---------- standalone sanity test (all 16 kernels, LMUL=8 only) ----------
add_snitch_test(sanity_test ${CMAKE_CURRENT_SOURCE_DIR}/sanity_test.c)