# Copyright 2020 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13)

# Defines
set(SNRT_NFPU_PER_CORE "0" CACHE STRING "Number of FPUs per Spatz")
set(QUAD_RLEN "0" CACHE STRING "Quadrilatero RLEN parameter")

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
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark-quadrilatero ${SNITCH_RUNTIME})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} matmul ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${param3}.h" SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE} QUAD_RLEN=${QUAD_RLEN})
endmacro()

# Benchmark library
add_library(benchmark-quadrilatero kernels/benchmark.c)

# Kernels
add_library(matmul kernels/matmul.c)
# add_library(verify-quadrilatero verify-quadrilatero/kernel/verify-quadrilatero.c)

# # Tests
enable_testing()
set(SNITCH_TEST_PREFIX quadrilateroBenchmarks-)
# add_quadrilatero_test_noParam(quadrilatero-matmul quadrilatero_matmul/main.c)

add_quadrilatero_test_threeParam(sp-matmul-quadrilatero matmul/sp-matmul-quadrilatero/main.c 64 64 64)
add_quadrilatero_test_threeParam(widening2x-hp-matmul-quadrilatero matmul/widening2x-hp-matmul-quadrilatero/main.c 64 64 128)
add_quadrilatero_test_threeParam(widening4x-bp-matmul-quadrilatero matmul/widening4x-bp-matmul-quadrilatero/main.c 64 64 256)

add_quadrilatero_test_threeParam(sp-umatmul-quadrilatero matmul/sp-umatmul-quadrilatero/main.c 64 64 64)
add_quadrilatero_test_threeParam(widening2x-hp-umatmul-quadrilatero matmul/widening2x-hp-umatmul-quadrilatero/main.c 64 64 128)
add_quadrilatero_test_threeParam(widening4x-bp-umatmul-quadrilatero matmul/widening4x-bp-umatmul-quadrilatero/main.c 64 64 256)

add_quadrilatero_test_threeParam(sp-fmatmul-quadrilatero matmul/sp-fmatmul-quadrilatero/main.c 64 64 64)
add_quadrilatero_test_threeParam(widening2x-hp-fmatmul-quadrilatero matmul/widening2x-hp-fmatmul-quadrilatero/main.c 64 64 128)
add_quadrilatero_test_threeParam(widening4x-bp-fmatmul-quadrilatero matmul/widening4x-bp-fmatmul-quadrilatero/main.c 64 64 256)

add_quadrilatero_test_threeParam(widening2x-hp-altfmatmul-quadrilatero matmul/widening2x-hp-altfmatmul-quadrilatero/main.c 64 64 128)
add_quadrilatero_test_threeParam(widening4x-bp-altfmatmul-quadrilatero matmul/widening4x-bp-altfmatmul-quadrilatero/main.c 64 64 256)

# add_quadrilatero_test_threeParam(sp-fmatmul-quadrilatero sp-fmatmul-quadrilatero/main.c 64 64 8)
# add_quadrilatero_test_threeParam(sp-fmatmul-quadrilatero sp-fmatmul-quadrilatero/main.c 8 8 8)
# add_quadrilatero_test_threeParam(sp-fmatmul-quadrilatero sp-fmatmul-quadrilatero/main.c 8 16 72)
# add_quadrilatero_test_threeParam(verify-quadrilatero verify-quadrilatero/main.c 64 64 64)
