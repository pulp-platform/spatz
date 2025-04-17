# Copyright 2020 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13)

# Defines
set(SNRT_NFPU_PER_CORE "0" CACHE STRING "Number of FPUs per Spatz")

# Allow QuadrilateroBenchmarks to be built as a standalone library.
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
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="matrixMul32i.h" SNRT_NFPU_PER_CORE=16)
endmacro()

macro(add_quadrilatero_test_threeParam name file param1 param2 param3)
  set(target_name ${name}_M${param1}_N${param2}_K${param3})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${param3}.h" SNRT_NFPU_PER_CORE=16)
endmacro()

# # Benchmark library
add_library(benchmark benchmark/benchmark.c)

# # Kernels
add_library(sp-matmul sp-matmul/kernel/sp-matmul.c)
add_library(widening2x-hp-matmul widening2x-hp-matmul/kernel/widening2x-hp-matmul.c)
add_library(widening4x-bp-matmul widening4x-bp-matmul/kernel/widening4x-bp-matmul.c)
add_library(sp-fmatmul sp-fmatmul/kernel/sp-fmatmul.c)

# # Tests
enable_testing()
set(SNITCH_TEST_PREFIX quadrilateroBenchmarks-)
# add_quadrilatero_test_noParam(quadrilatero-matmul quadrilatero_matmul/main.c)

add_quadrilatero_test_threeParam(sp-matmul sp-matmul/main.c 64 64 64)
add_quadrilatero_test_threeParam(widening2x-hp-matmul widening2x-hp-matmul/main.c 64 64 64)
add_quadrilatero_test_threeParam(widening4x-bp-matmul widening4x-bp-matmul/main.c 64 64 64)
add_quadrilatero_test_threeParam(sp-fmatmul sp-fmatmul/main.c 64 64 64)