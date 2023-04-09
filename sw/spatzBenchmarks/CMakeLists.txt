# Copyright 2020 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13)

# Allow spatzBenchmarks to be built as a standalone library.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
    set(CMAKE_TOOLCHAIN_FILE toolchain-gcc CACHE STRING "Toolchain to use")

    project(Benchmarks LANGUAGES C ASM)
    include(SnitchUtilities)

    add_compile_options(-O3 -g -ffunction-sections)

    # Build the runtime.
    add_subdirectory(../snRuntime snRuntime)
endif()

include_directories(include)
include_directories(${SNRUNTIME_INCLUDE_DIRS})

# Benchmark library
add_library(benchmark benchmark/benchmark.c)

# DP-FMATMUL
add_library(dp-fmatmul dp-fmatmul/kernel/dp-fmatmul.c)
add_snitch_executable(benchmark-dp-fmatmul dp-fmatmul/main.c)

target_link_libraries(dp-fmatmul benchmark ${SNITCH_RUNTIME})
target_link_libraries(benchmark-dp-fmatmul dp-fmatmul)

# Tests
enable_testing()
add_snitch_raw_test_rtl(benchmark-dp-fmatmul benchmark-dp-fmatmul)
