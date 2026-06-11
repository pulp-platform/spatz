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

    # Build the runtime.
    add_subdirectory(../snRuntime snRuntime)
endif()

add_compile_options(-O3 -g0 -ffunction-sections)
add_compile_options(-DELEN=${ELEN})

include_directories(include)
include_directories(${SNRUNTIME_INCLUDE_DIRS})
include_directories(../riscvTests/isa/macros/vector)

# Tests
enable_testing()
set(SNITCH_TEST_PREFIX vme-)

add_snitch_test(msetmtype isaTests/msetmtype.c)
add_snitch_test(msetmtypei isaTests/msetmtypei.c)
add_snitch_test(msettn isaTests/msettn.c)
add_snitch_test(msettm isaTests/msettm.c)
add_snitch_test(msettk isaTests/msettk.c)
add_snitch_test(vtmm isaTests/vtmm.c)
add_snitch_test(vtfmm isaTests/vtfmm.c)
add_snitch_test(vtle isaTests/vtle.c)
add_snitch_test(vtse isaTests/vtse.c)
add_snitch_test(vtmv isaTests/vtmv.c)
add_snitch_test(vtzero isaTests/vtzero.c)

add_subdirectory(benchmarks)