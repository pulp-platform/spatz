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

# default value to use SPM
if(NOT DEFINED USE_CACHE)
  set(USE_CACHE 0)
endif()

# Validate that USE_CACHE is either 0 or 1
if(NOT USE_CACHE MATCHES "^[01]$")
  message(FATAL_ERROR "USE_CACHE must be either 0 or 1. Current value: ${USE_CACHE}")
endif()

# default value to set iteration
if(NOT DEFINED MEAS_1ITER)
  set(MEAS_1ITER 1)
endif()

# Validate that MEAS_1ITER is either 0 or 1
if(NOT MEAS_1ITER MATCHES "^[01]$")
  message(FATAL_ERROR "MEAS_1ITER must be either 0 or 1. Current value: ${MEAS_1ITER}")
endif()

# default value to print with real UART
if(NOT DEFINED PRINT_UART)
  set(PRINT_UART 1)
endif()

# Validate that PRINT_UART is either 0 or 1
if(NOT PRINT_UART MATCHES "^[01]$")
  message(FATAL_ERROR "PRINT_UART must be either 0 or 1. Current value: ${PRINT_UART}")
endif()

include_directories(include)
include_directories(${SNRUNTIME_INCLUDE_DIRS})

add_compile_options(-O3 -g -ffunction-sections)

option(PRINT_CHECK "Enable Printing" OFF)

# Macro to regenerate the golden values and compile a module
# if(PRINT_CHECK)
add_definitions(-DPRINT_RESULT)
macro(add_spatz_test_oneParam name file param1)
  set(target_name ${name}_M${param1})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}.h" PRINT_RESULT USE_CACHE=${USE_CACHE} MEAS_1ITER=${MEAS_1ITER} PRINT_UART=${PRINT_UART})
endmacro()

macro(add_spatz_test_twoParam name file param1 param2)
  set(target_name ${name}_M${param1}_N${param2})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}.h" PRINT_RESULT USE_CACHE=${USE_CACHE} MEAS_1ITER=${MEAS_1ITER} PRINT_UART=${PRINT_UART})
endmacro()

macro(add_spatz_test_threeParam name file param1 param2 param3)
  set(target_name ${name}_M${param1}_N${param2}_K${param3})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${param3}.h" PRINT_RESULT USE_CACHE=${USE_CACHE} MEAS_1ITER=${MEAS_1ITER} PRINT_UART=${PRINT_UART})
endmacro()

# Use data type as another parameter
macro(add_spatz_test_oneParam_type name file param1 type)
  set(target_name ${name}_M${param1})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${type}.h" USE_CACHE=${USE_CACHE} PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

add_definitions(-DPRINT_RESULT)
macro(add_spatz_test_twoParam_type name file param1 param2 type)
  set(target_name ${name}_M${param1}_N${param2})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${type}.h" USE_CACHE=${USE_CACHE} PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

add_definitions(-DPRINT_RESULT)
macro(add_spatz_test_threeParam_type name file param1 param2 param3 type)
  set(target_name ${name}_M${param1}_N${param2}_K${param3})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${param3}_${type}.h" USE_CACHE=${USE_CACHE} PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

# Benchmark library
add_library(benchmark benchmark/benchmark.c)

# Kernels
add_library(dp-fmatmul dp-fmatmul/kernel/dp-fmatmul.c)
add_library(sp-fmatmul sp-fmatmul/kernel/sp-fmatmul.c)
add_library(hp-fmatmul hp-fmatmul/kernel/hp-fmatmul.c)

add_spatz_test_twoParam_type(dp-gemv gemv/main.c 64  128 64)
add_spatz_test_twoParam_type(sp-gemv gemv/main.c 128 128 32)

# add_library(widening-hp-fmatmul widening-hp-fmatmul/kernel/widening-fmatmul.c)
# add_library(widening-bp-fmatmul widening-bp-fmatmul/kernel/widening-fmatmul.c)

# add_library(sdotp-hp-fmatmul sdotp-hp-fmatmul/kernel/sdotp-fmatmul.c)
# add_library(sdotp-bp-fmatmul sdotp-bp-fmatmul/kernel/sdotp-fmatmul.c)

add_library(dp-faxpy dp-faxpy/kernel/faxpy.c)

add_library(dp-fdotp dp-fdotp/kernel/fdotp.c)

add_library(dp-fconv2d dp-fconv2d/kernel/fconv2d.c)

add_library(dp-fft dp-fft/kernel/fft.c)
add_library(sp-fft sp-fft/kernel/fft.c)

add_library(linked-list linked-list/kernel/llist.c)

# Tests
enable_testing()
set(SNITCH_TEST_PREFIX spatzBenchmarks-)

# add_spatz_test_threeParam(dp-fmatmul dp-fmatmul/main.c 64  64  64 )

# add_spatz_test_threeParam(sp-fmatmul sp-fmatmul/main.c 64  64  64 )
# add_spatz_test_threeParam(sp-fmatmul sp-fmatmul/main.c 64  128 64 )

# add_spatz_test_threeParam(hp-fmatmul hp-fmatmul/main.c 64  64  64 )
# add_spatz_test_threeParam(hp-fmatmul hp-fmatmul/main.c 64  128 64 )
# add_spatz_test_threeParam(hp-fmatmul hp-fmatmul/main.c 128 128 128)

# add_spatz_test_threeParam(widening-hp-fmatmul widening-hp-fmatmul/main.c 64  64  64 )
# add_spatz_test_threeParam(widening-hp-fmatmul widening-hp-fmatmul/main.c 64  128 64 )
# add_spatz_test_threeParam(widening-hp-fmatmul widening-hp-fmatmul/main.c 128 128 128)

# add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 64  64  64 )
# add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 64  128 64 )
# add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 128 128 128)
# add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 128 256 128)

# add_spatz_test_threeParam(sdotp-hp-fmatmul sdotp-hp-fmatmul/main.c 64  64  64 )
# add_spatz_test_threeParam(sdotp-hp-fmatmul sdotp-hp-fmatmul/main.c 64  128 64 )
# add_spatz_test_threeParam(sdotp-hp-fmatmul sdotp-hp-fmatmul/main.c 128 128 128)

# add_spatz_test_threeParam(sdotp-bp-fmatmul sdotp-bp-fmatmul/main.c 64  64  64 )
# add_spatz_test_threeParam(sdotp-bp-fmatmul sdotp-bp-fmatmul/main.c 64  128 64 )
# add_spatz_test_threeParam(sdotp-bp-fmatmul sdotp-bp-fmatmul/main.c 128 128 128)
# add_spatz_test_threeParam(sdotp-bp-fmatmul sdotp-bp-fmatmul/main.c 128 256 128)

add_spatz_test_oneParam(dp-faxpy dp-faxpy/main.c 256)
add_spatz_test_oneParam(dp-faxpy dp-faxpy/main.c 1024)

add_spatz_test_oneParam(dp-fdotp dp-fdotp/main.c 128)
add_spatz_test_oneParam(dp-fdotp dp-fdotp/main.c 4096)

add_spatz_test_threeParam(dp-fconv2d dp-fconv2d/main.c 32 32 7)
add_spatz_test_threeParam(dp-fconv2d dp-fconv2d/main.c 64 64 7)

add_spatz_test_twoParam(dp-fft dp-fft/main.c 128 2)

add_spatz_test_twoParam(sp-fft sp-fft/main.c 256 2)
add_spatz_test_twoParam(sp-fft sp-fft/main.c 512 2)

# add_spatz_test_threeParam(dp-mxfmatmul dp-mxfmatmul/main.c 64 64 64)

add_spatz_test_twoParam(linked-list linked-list/main.c 64 8)
add_spatz_test_twoParam(linked-list linked-list/main.c 256 16)

# Tests for DATE
# Vanilla fmatmul
add_spatz_test_threeParam(dp-fmatmul-8x2vl dp-fmatmul-8x2vl/main.c 64 64 64)
add_spatz_test_threeParam(dp-fmatmul-8x2vl dp-fmatmul-8x2vl/main.c 32 32 32)
add_spatz_test_threeParam(dp-fmatmul-8x2vl dp-fmatmul-8x2vl/main.c 16 16 16)
add_spatz_test_threeParam(dp-fmatmul-4x4vl dp-fmatmul-4x4vl/main.c 64 64 64)
add_spatz_test_threeParam(dp-fmatmul-4x4vl dp-fmatmul-4x4vl/main.c 32 32 32)
add_spatz_test_threeParam(dp-fmatmul-4x4vl dp-fmatmul-4x4vl/main.c 16 16 16)
add_spatz_test_threeParam(dp-fmatmul-4x4vl dp-fmatmul-4x4vl/main.c 128 128 128)
# # MXU matmul
# add_spatz_test_threeParam(dp-mxfmatmul-m8n4k4-b4 dp-mxfmatmul-m8n4k4-b4/main.c 64 64 64)
# add_spatz_test_threeParam(dp-mxfmatmul-m8n4k4-b4 dp-mxfmatmul-m8n4k4-b4/main.c 32 32 32)
# add_spatz_test_threeParam(dp-mxfmatmul-m8n4k4-b4 dp-mxfmatmul-m8n4k4-b4/main.c 16 16 16)
# add_spatz_test_threeParam(dp-mxfmatmul-m4n4k4-b4 dp-mxfmatmul-m4n4k4-b4/main.c 64 64 64)
# add_spatz_test_threeParam(dp-mxfmatmul-m4n4k4-b4 dp-mxfmatmul-m4n4k4-b4/main.c 32 32 32)
# add_spatz_test_threeParam(dp-mxfmatmul-m4n4k4-b4 dp-mxfmatmul-m4n4k4-b4/main.c 16 16 16)
# add_spatz_test_threeParam(dp-mxfmatmul-m4n4k8-b4 dp-mxfmatmul-m4n4k8-b4/main.c 64 64 64)
# add_spatz_test_threeParam(dp-mxfmatmul-m4n4k8-b4 dp-mxfmatmul-m4n4k8-b4/main.c 32 32 32)
# add_spatz_test_threeParam(dp-mxfmatmul-m4n4k8-b4 dp-mxfmatmul-m4n4k8-b4/main.c 16 16 16)
# add_spatz_test_threeParam(dp-mxfmatmul-m8n4k4-b2 dp-mxfmatmul-m8n4k4-b2/main.c 64 64 64)
# add_spatz_test_threeParam(dp-mxfmatmul-m8n4k4-b2 dp-mxfmatmul-m8n4k4-b2/main.c 32 32 32)
# add_spatz_test_threeParam(dp-mxfmatmul-m8n4k4-b2 dp-mxfmatmul-m8n4k4-b2/main.c 16 16 16)
# add_spatz_test_threeParam(dp-mxfmatmul-m4n4k4-b2 dp-mxfmatmul-m4n4k4-b2/main.c 64 64 64)
# add_spatz_test_threeParam(dp-mxfmatmul-m4n4k4-b2 dp-mxfmatmul-m4n4k4-b2/main.c 32 32 32)
# add_spatz_test_threeParam(dp-mxfmatmul-m4n4k4-b2 dp-mxfmatmul-m4n4k4-b2/main.c 16 16 16)
# add_spatz_test_threeParam(dp-mxfmatmul-m4n4k8-b2 dp-mxfmatmul-m4n4k8-b2/main.c 64 64 64)
# add_spatz_test_threeParam(dp-mxfmatmul-m4n4k8-b2 dp-mxfmatmul-m4n4k8-b2/main.c 32 32 32)
# add_spatz_test_threeParam(dp-mxfmatmul-m4n4k8-b2 dp-mxfmatmul-m4n4k8-b2/main.c 16 16 16)
# Double buffering
add_spatz_test_threeParam(dp-db-fmatmul-4x4vl dp-db-fmatmul-4x4vl/main.c 128 128 128)
add_spatz_test_threeParam(dp-db-fmatmul-4x4vl dp-db-fmatmul-4x4vl/main.c 64 64 64)
add_spatz_test_threeParam(dp-db-fmatmul-4x4vl dp-db-fmatmul-4x4vl/main.c 32 32 32)
