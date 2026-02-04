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

#include_directories(include)
include_directories(${SNRUNTIME_INCLUDE_DIRS})

add_compile_options(-O3 -g -ffunction-sections)

# Macro to regenerate the golden values and compile a module
macro(add_hwpe_test_oneParam name file param1)
  set(target_name ${name}_M${param1})
  add_snitch_test(${target_name} ${file})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}.h")
endmacro()

macro(add_hwpe_test_twoParam name file param1 param2)
  set(target_name ${name}_M${param1}_N${param2})
  add_snitch_test(${target_name} ${file})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}.h")
endmacro()

macro(add_hwpe_test_threeParam name file param1 param2 param3)
  set(target_name ${name}_M${param1}_N${param2}_K${param3})
  add_snitch_test(${target_name} ${file})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${param3}.h")
endmacro()

# Tests
enable_testing()
set(SNITCH_TEST_PREFIX non-linear-)

#WUS
add_snitch_test(sin_cos /usr/scratch2/pisoc1/msc25h16/spatz/sw/non-linearities/FP32/sin_cos/sin_cos.c)
add_snitch_test(logf /usr/scratch2/pisoc1/msc25h16/spatz/sw/non-linearities/FP32/logf/logf.c)
add_snitch_test(rsqrt /usr/scratch2/pisoc1/msc25h16/spatz/sw/non-linearities/FP32/rsqrt/rsqrt.c)
add_snitch_test(tanh /usr/scratch2/pisoc1/msc25h16/spatz/sw/non-linearities/FP32/tanh/tanh.c)
add_snitch_test(cosh /usr/scratch2/pisoc1/msc25h16/spatz/sw/non-linearities/FP32/cosh/cosh.c)
add_snitch_test(exp /usr/scratch2/pisoc1/msc25h16/spatz/sw/non-linearities/FP32/exp/exp.c)
add_snitch_test(rec /usr/scratch2/pisoc1/msc25h16/spatz/sw/non-linearities/FP32/rec/rec.c)


