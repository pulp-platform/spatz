# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Pei-Yu Lin (peilin@ethz.ch)

cmake_minimum_required(VERSION 3.13)

# Allow spatzBenchmarks to be built as a standalone library.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
    set(CMAKE_TOOLCHAIN_FILE toolchain-gcc CACHE STRING "Toolchain to use")

    project(Benchmarks LANGUAGES C ASM)
    include(SnitchUtilities)

    # Build the runtime.
    add_subdirectory(../snRuntime snRuntime)
endif()

add_compile_options(-O3 -g0 -ffunction-sections)
add_compile_options(-DELEN=${ELEN})

include_directories(include)
include_directories(${SNRUNTIME_INCLUDE_DIRS})
include_directories(../riscvTests/isa/macros/vector)

# Tests
add_subdirectory(isaTests)
add_subdirectory(apps)