# Copyright 2020 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13)

# Defines
set(SNRT_NFPU_PER_CORE "0" CACHE STRING "Number of FPUs per Spatz")

# Allow spatzBenchmarks to be built as a standalone library.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
    set(CMAKE_TOOLCHAIN_FILE toolchain-gcc CACHE STRING "Toolchain to use")

    project(Benchmarks LANGUAGES C ASM)
    include(SnitchUtilities)

    # Build the runtime.
    add_subdirectory(../snRuntime snRuntime)
endif()

include_directories(include)
include_directories(${SNRUNTIME_INCLUDE_DIRS})

add_compile_options(-O3 -g -ffunction-sections)


# Macro to regenerate the golden values and compile a module
macro(add_quadrilatero_test_noParam name file)
  set(target_name ${name})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="matrixMul32i.h" SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

# # Benchmark library
# add_library(benchmark benchmark/benchmark.c)

# # Kernels
# add_library(dp-fmatmul dp-fmatmul/kernel/dp-fmatmul.c)
# add_library(sp-fmatmul sp-fmatmul/kernel/sp-fmatmul.c)
# add_library(hp-fmatmul hp-fmatmul/kernel/hp-fmatmul.c)

# add_library(widening-hp-fmatmul widening-hp-fmatmul/kernel/widening-fmatmul.c)
# add_library(widening-bp-fmatmul widening-bp-fmatmul/kernel/widening-fmatmul.c)

# add_library(sdotp-hp-fmatmul sdotp-hp-fmatmul/kernel/sdotp-fmatmul.c)
# add_library(sdotp-bp-fmatmul sdotp-bp-fmatmul/kernel/sdotp-fmatmul.c)

# add_library(dp-faxpy dp-faxpy/kernel/faxpy.c)

# add_library(dp-fdotp dp-fdotp/kernel/fdotp.c)

# add_library(dp-fconv2d dp-fconv2d/kernel/fconv2d.c)

# add_library(dp-fft dp-fft/kernel/fft.c)
# add_library(sp-fft sp-fft/kernel/fft.c)

# # Tests
enable_testing()
set(SNITCH_TEST_PREFIX quadrilateroBenchmarks-)
add_quadrilatero_test_noParam(quadrilatero-matmul quadrilatero_matmul/main.c)
# target_link_libraries(test-${SNITCH_TEST_PREFIX}quadrilatero-matmul)
# target_link_libraries(test-${SNITCH_TEST_PREFIX}quadrilatero-matmul benchmark ${SNITCH_RUNTIME})

