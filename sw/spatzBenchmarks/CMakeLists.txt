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

# Macro to regenerate the golden values and compile a module
macro(add_spatz_test_threeParam name file param1 param2 param3 json)
  set(target_name ${name}_M${param1}_N${param2}_K${param3})
  add_custom_target(gen_${target_name})
  add_custom_command(
    TARGET gen_${target_name}
    POST_BUILD
    COMMAND sed s/PARAM1/${param1}/g ${CMAKE_CURRENT_SOURCE_DIR}/${name}/script/${json}.json | sed s/PARAM2/${param2}/g | sed s/PARAM3/${param3}/g > ${json}_${param1}_${param2}_${param3}.json
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/${name}/script/gen_data.py -c ${json}_${param1}_${param2}_${param3}.json)

  add_snitch_executable(${target_name} ${file})
  target_link_libraries(${target_name} benchmark ${SNITCH_RUNTIME})

  add_dependencies(${target_name} gen_${target_name})
endmacro()

# Benchmark library
add_library(benchmark benchmark/benchmark.c)

# Kernels
add_library(dp-fmatmul dp-fmatmul/kernel/dp-fmatmul.c)
add_library(sp-fmatmul sp-fmatmul/kernel/sp-fmatmul.c)
add_library(hp-fmatmul hp-fmatmul/kernel/hp-fmatmul.c)

add_library(widening-hp-fmatmul widening-hp-fmatmul/kernel/widening-fmatmul.c)
add_library(widening-bp-fmatmul widening-bp-fmatmul/kernel/widening-fmatmul.c)

add_library(sdotp-hp-fmatmul sdotp-hp-fmatmul/kernel/sdotp-fmatmul.c)

# Tests
enable_testing()
add_spatz_test_threeParam(dp-fmatmul dp-fmatmul/main.c 64  64  64  matmul)

add_spatz_test_threeParam(sp-fmatmul sp-fmatmul/main.c 64  64  64  matmul)
add_spatz_test_threeParam(sp-fmatmul sp-fmatmul/main.c 64  128 64  matmul)

add_spatz_test_threeParam(hp-fmatmul hp-fmatmul/main.c 64  64  64  matmul)
add_spatz_test_threeParam(hp-fmatmul hp-fmatmul/main.c 64  128 64  matmul)
add_spatz_test_threeParam(hp-fmatmul hp-fmatmul/main.c 128 128 128 matmul)

add_spatz_test_threeParam(widening-hp-fmatmul widening-hp-fmatmul/main.c 64  64  64  matmul)
add_spatz_test_threeParam(widening-hp-fmatmul widening-hp-fmatmul/main.c 64  128 64  matmul)
add_spatz_test_threeParam(widening-hp-fmatmul widening-hp-fmatmul/main.c 128 128 128 matmul)

add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 64  64  64  matmul)
add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 64  128 64  matmul)
add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 128 128 128 matmul)
add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 128 256 128 matmul)

add_spatz_test_threeParam(sdotp-hp-fmatmul sdotp-hp-fmatmul/main.c 64  64  64  matmul)
add_spatz_test_threeParam(sdotp-hp-fmatmul sdotp-hp-fmatmul/main.c 64  128 64  matmul)
add_spatz_test_threeParam(sdotp-hp-fmatmul sdotp-hp-fmatmul/main.c 128 128 128 matmul)
