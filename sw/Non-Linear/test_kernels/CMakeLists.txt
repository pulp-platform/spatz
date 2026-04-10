# Copyright 2020 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# ML Kernel Test Framework
# Generates test binaries for 6 kernels × 2 variants × 3 precisions × 4 sizes
#
# Precisions:  fp32, fp16, bf16
# Sizes:       64, 128, 256, 512
#
# Kernels:
#   01_RoPE          – Rotary Positional Embedding (sin, cos)
#   02_Mish          – Mish activation (exp, ln, tanh)
#   03_Stable_Softmax – Numerically stable softmax (exp, rec)
#   04_RMSNorm       – Root Mean Square Normalization (rsqrt)
#   05_GeLU_Tanh     – GeLU Tanh approximation (tanh)
#   06_LogCosh_Loss  – Log-Cosh loss function (cosh, ln)

cmake_minimum_required(VERSION 3.13)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
    set(CMAKE_TOOLCHAIN_FILE toolchain-gcc CACHE STRING "Toolchain to use")
    project(TestKernels LANGUAGES C ASM)
    include(SnitchUtilities)
    add_subdirectory(../snRuntime snRuntime)
endif()

include_directories(${SNRUNTIME_INCLUDE_DIRS})
add_compile_options(-O3 -g -ffunction-sections)

enable_testing()
set(SNITCH_TEST_PREFIX test-kernel-)

# ---------- kernel list ----------
set(KERNEL_DIRS
    01_RoPE
    02_Mish
    03_Stable_Softmax
    04_RMSNorm
    05_GeLU_Tanh
    06_LogCosh_Loss
)

set(KERNEL_VARIANTS
    baseline_function
    custom_function
)

set(ELEMENT_COUNTS 64 128 256 512)
set(PRECISIONS fp32 fp16 bf16)

# ---------- generate targets: kernel × variant × precision × size ----------
foreach(PREC ${PRECISIONS})
    foreach(NELEM ${ELEMENT_COUNTS})
        foreach(KDIR ${KERNEL_DIRS})
            # Source directory: fp32 uses original dir, fp16/bf16 use suffixed dirs
            if (PREC STREQUAL "fp32")
                set(SRC_DIR ${KDIR})
            else()
                set(SRC_DIR ${KDIR}_${PREC})
            endif()

            foreach(VARIANT ${KERNEL_VARIANTS})
                set(TARGET_NAME ${KDIR}_${VARIANT}_${PREC}_n${NELEM})
                set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}/${VARIANT}.c)

                if (EXISTS ${SRC_FILE})
                    add_snitch_test(${TARGET_NAME} ${SRC_FILE})
                    # Tell the kernel which data header to include
                    target_compile_definitions(test-${SNITCH_TEST_PREFIX}${TARGET_NAME} PRIVATE
                        DATA_HEADER_PATH=../data/data_${PREC}_${NELEM}.h)
                endif()
            endforeach()
        endforeach()
    endforeach()
endforeach()
