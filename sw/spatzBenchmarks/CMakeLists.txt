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

# default value to select the HPDcache L1 variant
if(NOT DEFINED HPDCACHE)
  set(HPDCACHE 0)
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
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}.h" PRINT_RESULT USE_CACHE=${USE_CACHE} HPDCACHE=${HPDCACHE} MEAS_1ITER=${MEAS_1ITER} PRINT_UART=${PRINT_UART})
endmacro()

macro(add_spatz_test_twoParam name file param1 param2)
  set(target_name ${name}_M${param1}_N${param2})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}.h" PRINT_RESULT USE_CACHE=${USE_CACHE} HPDCACHE=${HPDCACHE} MEAS_1ITER=${MEAS_1ITER} PRINT_UART=${PRINT_UART})
endmacro()

macro(add_spatz_test_threeParam name file param1 param2 param3)
  set(target_name ${name}_M${param1}_N${param2}_K${param3})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${param3}.h" PRINT_RESULT USE_CACHE=${USE_CACHE} HPDCACHE=${HPDCACHE} MEAS_1ITER=${MEAS_1ITER} PRINT_UART=${PRINT_UART})
endmacro()

# Use data type as another parameter
macro(add_spatz_test_oneParam_type name file param1 type)
  set(target_name ${name}_M${param1})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${type}.h" USE_CACHE=${USE_CACHE} HPDCACHE=${HPDCACHE} PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

add_definitions(-DPRINT_RESULT)
macro(add_spatz_test_twoParam_type name file param1 param2 type)
  set(target_name ${name}_M${param1}_N${param2})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${type}.h" USE_CACHE=${USE_CACHE} HPDCACHE=${HPDCACHE} PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

add_definitions(-DPRINT_RESULT)
macro(add_spatz_test_threeParam_type name file param1 param2 param3 type)
  set(target_name ${name}_M${param1}_N${param2}_K${param3})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${param3}_${type}.h" USE_CACHE=${USE_CACHE} HPDCACHE=${HPDCACHE} PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

# Benchmark library
add_library(benchmark benchmark/benchmark.c)

# Kernels
add_library(dp-fmatmul dp-fmatmul/kernel/dp-fmatmul.c)
add_library(sp-fmatmul sp-fmatmul/kernel/sp-fmatmul.c)
add_library(hp-fmatmul hp-fmatmul/kernel/hp-fmatmul.c)

add_spatz_test_twoParam_type(dp-gemv gemv/main.c 64  128 64)
add_spatz_test_twoParam_type(dp-gemv gemv/main.c 128  128 64)
add_spatz_test_twoParam_type(dp-gemv gemv/main.c 512  128 64)
add_spatz_test_twoParam_type(dp-gemv gemv/main.c 64  512 64)
add_spatz_test_twoParam_type(hp-gemv gemv/main.c 4096  128 16)
add_spatz_test_twoParam_type(db-gemv db-gemv/main.c 64  128 64)
add_spatz_test_twoParam_type(db-gemv db-gemv/main.c 512  128 64)

add_spatz_test_threeParam_type(dp-sa-gemv sa_gemv/main.c 256 128 16 64)
add_spatz_test_threeParam_type(hp-sa-gemv sa_gemv/main.c 128 4096 512 16)

add_spatz_test_threeParam_type(dp-spmv spmv/main.c 64 64 512 64)
add_spatz_test_threeParam_type(dp-spmv spmv/main.c 128 512 16384 64)
add_spatz_test_threeParam_type(dp-spmv spmv/main.c 512 512 65536 64)
add_spatz_test_threeParam_type(dp-spmv spmv/main.c 2048 2048 262144 64)
macro(add_spmv_kernel_source target_name)
  target_sources(test-${SNITCH_TEST_PREFIX}${target_name} PRIVATE spmv/kernel/spmv.c)
  target_compile_options(test-${SNITCH_TEST_PREFIX}${target_name} PRIVATE -fno-unroll-loops)
endmacro()

add_spmv_kernel_source(dp-spmv_M64_N64_K512)
add_spmv_kernel_source(dp-spmv_M128_N512_K16384)
add_spmv_kernel_source(dp-spmv_M512_N512_K65536)
add_spmv_kernel_source(dp-spmv_M2048_N2048_K262144)

# --- md: non-bonded Lennard-Jones, non-Newton (gather-only, no
# scatter-accumulate hazard) ---
macro(add_md_test name file M K type)
  set(target_name ${name}_M${M}_K${K})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${M}_${K}_${type}.h" USE_CACHE=${USE_CACHE} HPDCACHE=${HPDCACHE} PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
  target_sources(test-${SNITCH_TEST_PREFIX}${target_name} PRIVATE md/kernel/md.c)
  target_compile_options(test-${SNITCH_TEST_PREFIX}${target_name} PRIVATE -fno-unroll-loops)
endmacro()

add_md_test(dp-md md/main.c 64 1024 64)
add_md_test(dp-md md/main.c 1024 56320 64)
add_md_test(dp-md md/main.c 256 16384 64)
add_md_test(dp-md md/main.c 1024 65536 64)

# --- db-md: double-buffered SPM-streaming variant of md, same kernel math
# (reuses md/kernel/md.c unmodified), only the streaming orchestration
# differs -- see db-md/main.c ---
macro(add_db_md_test name file M K type)
  set(target_name ${name}_M${M}_K${K})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${M}_${K}_${type}.h" USE_CACHE=${USE_CACHE} HPDCACHE=${HPDCACHE} PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
  target_sources(test-${SNITCH_TEST_PREFIX}${target_name} PRIVATE md/kernel/md.c)
  target_compile_options(test-${SNITCH_TEST_PREFIX}${target_name} PRIVATE -fno-unroll-loops)
endmacro()

add_db_md_test(dp-db-md db-md/main.c 64 1024 64)
add_db_md_test(dp-db-md db-md/main.c 1024 65536 64)

# --- rs_spmm: row-sparse SpMM (sparse attention pattern) ---
# Paper sizes: S=64x512xN16, L=128x512xN16, Sq=128x128xN16
macro(add_rs_spmm_test name file M K N nzr type)
  set(target_name ${name}_M${M}_K${K}_N${N}_NZR${nzr})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${M}_${K}_${N}_${nzr}_${type}.h" USE_CACHE=${USE_CACHE} HPDCACHE=${HPDCACHE} PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

add_rs_spmm_test(dp-rs-spmm rs_spmm/main.c 64 128 64 16 64)
add_rs_spmm_test(dp-rs-spmm rs_spmm/main.c 64 256 64 16 64)
# add_rs_spmm_test(dp-rs-spmm rs_spmm/main.c 128 512 16 16 64)
# add_rs_spmm_test(dp-rs-spmm rs_spmm/main.c 128 128 16 16 64)

macro(add_rs_spmm_kernel_source target_name)
  target_sources(test-${SNITCH_TEST_PREFIX}${target_name} PRIVATE rs_spmm/kernel/rs_spmm.c)
  target_compile_options(test-${SNITCH_TEST_PREFIX}${target_name} PRIVATE -fno-unroll-loops)
endmacro()

add_rs_spmm_kernel_source(dp-rs-spmm_M64_K128_N64_NZR16)
add_rs_spmm_kernel_source(dp-rs-spmm_M64_K256_N64_NZR16)


# --- rs_spmv: row-sparse SpMV (sparse attention pattern) ---
# Paper sizes: S=64x512, L=128x512, Sq=128x128
add_spatz_test_threeParam_type(dp-rs-spmv rs_spmv/main.c 64 512 16 64)
add_spatz_test_threeParam_type(dp-rs-spmv rs_spmv/main.c 128 512 16 64)
# add_spatz_test_threeParam_type(dp-rs-spmv rs_spmv/main.c 128 128 16 64)

macro(add_rs_spmv_kernel_source target_name)
  target_sources(test-${SNITCH_TEST_PREFIX}${target_name} PRIVATE rs_spmv/kernel/rs_spmv.c)
  target_compile_options(test-${SNITCH_TEST_PREFIX}${target_name} PRIVATE -fno-unroll-loops)
endmacro()

add_rs_spmv_kernel_source(dp-rs-spmv_M64_N512_K16)
add_rs_spmv_kernel_source(dp-rs-spmv_M128_N512_K16)


add_library(dp-faxpy dp-faxpy/kernel/faxpy.c)

add_library(dp-fdotp dp-fdotp/kernel/fdotp.c)
add_library(dp-dbfdotp dp-db-fdotp/kernel/db-fdotp.c)

add_library(dp-fconv2d dp-fconv2d/kernel/fconv2d.c)

add_library(dp-fft dp-fft/kernel/fft.c)
add_library(sp-fft sp-fft/kernel/fft.c)
add_library(dp-db-fft dp-db-fft/kernel/db-fft.c)

add_library(linked-list linked-list/kernel/llist.c)

# Tests
enable_testing()
set(SNITCH_TEST_PREFIX )

add_spatz_test_oneParam(dp-faxpy dp-faxpy/main.c 256)
add_spatz_test_oneParam(dp-faxpy dp-faxpy/main.c 1024)

add_spatz_test_oneParam(dp-fdotp dp-fdotp/main.c 128)
add_spatz_test_oneParam(dp-fdotp dp-fdotp/main.c 4096)
add_spatz_test_oneParam(dp-fdotp dp-fdotp/main.c 32768)
add_spatz_test_oneParam(dp-db-fdotp dp-db-fdotp/main.c 8192)
add_spatz_test_oneParam(dp-db-fdotp dp-db-fdotp/main.c 32768)

add_spatz_test_threeParam(dp-fconv2d dp-fconv2d/main.c 32 32 7)
add_spatz_test_threeParam(dp-fconv2d dp-fconv2d/main.c 64 64 7)

add_spatz_test_twoParam(dp-fft dp-fft/main.c 128 2)
add_spatz_test_twoParam(dp-fft dp-fft/main.c 1024 2)
add_spatz_test_twoParam(dp-fft dp-fft/main.c 2048 2)

add_spatz_test_threeParam(dp-db-fft dp-db-fft/main.c 256 4 2)
add_spatz_test_threeParam(dp-db-fft dp-db-fft/main.c 1024 4 2)
add_spatz_test_threeParam(dp-db-fft dp-db-fft/main.c 2048 8 2)

add_spatz_test_threeParam(dp-fmatmul-4x4vl dp-fmatmul-4x4vl/main.c 64 64 64)
add_spatz_test_threeParam(dp-fmatmul-4x4vl dp-fmatmul-4x4vl/main.c 128 128 128)
add_spatz_test_threeParam(dp-fmatmul-4x4vl dp-fmatmul-4x4vl/main.c 128 256 128)
add_spatz_test_threeParam(dp-fmatmul-4x4vl dp-fmatmul-4x4vl/main.c 256 256 256)

# Double buffering
add_spatz_test_threeParam(dp-db-fmatmul-4x4vl dp-db-fmatmul-4x4vl/main.c 128 128 128)
add_spatz_test_threeParam(dp-db-fmatmul-4x4vl dp-db-fmatmul-4x4vl/main.c 128 256 128)
add_spatz_test_threeParam(dp-db-fmatmul-4x4vl dp-db-fmatmul-4x4vl/main.c 256 256 256)
