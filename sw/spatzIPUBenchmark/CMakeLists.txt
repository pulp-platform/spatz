# Copyright 2020 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13)

# Defines
set(SNRT_NFPU_PER_CORE "0" CACHE STRING "Number of FPUs per Spatz")
set(ELEN "64" CACHE STRING "Element length (32 or 64)")

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

# These sources contain hand-written RVV inline assembly: opt into the
# vector march (the toolchain default deliberately omits 'v'; the last
# -march on the command line wins). Compiler-side vectorization stays
# disabled through the toolchain-wide kill switches.
if(DEFINED SPATZ_MARCH_VECTOR)
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-march=${SPATZ_MARCH_VECTOR}>")
endif()

include_directories(include)
include_directories(${SNRUNTIME_INCLUDE_DIRS})
include_directories(isa/macros/vector)

# Tests
enable_testing()
set(SNITCH_TEST_PREFIX IPUBenchmarks-)


add_snitch_test(resize_bilinear       resize_bilinear/main.c)
add_snitch_test(transpose_block       transpose_block/main.c)
