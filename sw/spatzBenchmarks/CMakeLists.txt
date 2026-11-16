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

# Use unrolled spatzBenchmarks
if (UNROLL)
add_definitions(-DUNROLL)
endif()

# Macro to regenerate the golden values and compile a module
macro(add_spatz_test_oneParam name file param1)
  set(target_name ${name}_M${param1})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}.h" SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

macro(add_spatz_test_twoParam name file param1 param2)
  set(target_name ${name}_M${param1}_N${param2})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}.h" SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

macro(add_spatz_test_threeParam name file param1 param2 param3)
  set(target_name ${name}_M${param1}_N${param2}_K${param3})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${param3}.h" SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

# Use data type as another parameter
macro(add_spatz_test_oneParam_type name file param1 type)
  set(target_name ${name}_M${param1})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${type}.h" PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

macro(add_spatz_test_twoParam_type name file param1 param2 type)
  set(target_name ${name}_M${param1}_N${param2})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${type}.h" PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

macro(add_spatz_test_threeParam_type name file param1 param2 param3 type)
  set(target_name ${name}_M${param1}_N${param2}_K${param3})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${param3}_${type}.h" PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

# Parametrized SpMV variant: (format, N, P_W). Selects data header
# `data/data_spmv_<fmt>_N<N>_PW<PW>.h`. Target name suffixed with the shape.
macro(add_spatz_test_spmv name file fmt N P_W)
  set(target_name ${name}_${fmt}_N${N}_PW${P_W})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC
    DATAHEADER="data/data_spmv_${fmt}_N${N}_PW${P_W}.h"
    SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

# Parametrized SpMM variant: (format, M, N, P_W). Selects data header
# `data/data_spmm_<fmt>_M<M>_N<N>_PW<PW>.h`. Target name suffixed with the shape.
macro(add_spatz_test_spmm name file fmt M N P_W)
  set(target_name ${name}_${fmt}_M${M}_N${N}_PW${P_W})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC
    DATAHEADER="data/data_spmm_${fmt}_M${M}_N${N}_PW${P_W}.h"
    SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

# Benchmark library
add_library(benchmark benchmark/benchmark.c)

# Kernels

if (ELEN EQUAL 64)
  add_library(dp-fmatmul dp-fmatmul/kernel/dp-fmatmul.c)

  add_library(dp-faxpy dp-faxpy/kernel/faxpy.c)

  add_library(dp-fdotp dp-fdotp/kernel/fdotp.c)

  add_library(dp-fconv2d dp-fconv2d/kernel/fconv2d.c)

  add_library(dp-fft dp-fft/kernel/fft.c)
endif()

add_library(sp-fmatmul sp-fmatmul/kernel/sp-fmatmul.c)
add_library(hp-fmatmul hp-fmatmul/kernel/hp-fmatmul.c)

add_library(widening-hp-fmatmul widening-hp-fmatmul/kernel/widening-fmatmul.c)
add_library(widening-bp-fmatmul widening-bp-fmatmul/kernel/widening-fmatmul.c)

add_library(sdotp-hp-fmatmul sdotp-hp-fmatmul/kernel/sdotp-fmatmul.c)
add_library(sdotp-bp-fmatmul sdotp-bp-fmatmul/kernel/sdotp-fmatmul.c)

add_library(sp-fft sp-fft/kernel/fft.c)
add_library(srs-fft srs-fft/kernel/srs-fft.c)
add_library(srs-hard-decision srs-hard-decision/kernel/hard_decision.c)
add_library(srs-compute-soft-bits srs-compute-soft-bits/kernel/compute_soft_bits.c)
add_library(srs-compute-var-to-check-msgs srs-compute-var-to-check-msgs/kernel/compute_var_to_check_msgs.c)
add_library(srs-compute-var-to-check-msgs-indexed srs-compute-var-to-check-msgs-indexed/kernel/compute_var_to_check_msgs.c)
add_library(srs-compute-check-to-var-msgs srs-compute-check-to-var-msgs/kernel/compute_check_to_var_msgs.c)
add_library(srs-analyze-var-to-check-msgs srs-analyze-var-to-check-msgs/kernel/analyze_var_to_check_msgs.c)
add_library(srs-scale-llr srs-scale-llr/kernel/scale_llr.c)
add_library(srs-load-soft-bits srs-load-soft-bits/kernel/load_soft_bits.c)
add_library(srs-crc srs-crc/kernel/crc.c)
add_library(srs-decoder srs-decoder/kernel/ldpc_decoder.c)
add_library(srs-equalize_zf_1xn srs-equalize_zf_1xn/kernel/equalize_zf_1xn.c)
add_library(srs-equalize_zf_2xn srs-equalize_zf_2xn/kernel/equalize_zf_2xn.c)
add_library(srs-equalize_zf_srs srs-equalize_zf_srs/kernel/equalize_zf_2xn.c)

# Tests
enable_testing()
set(SNITCH_TEST_PREFIX spatzBenchmarks-)

if (ELEN EQUAL 64)
  add_spatz_test_threeParam(dp-fmatmul dp-fmatmul/main.c 64  64  64 )

  add_spatz_test_twoParam_type(dp-gemv gemv/main.c 64  128 64)
  add_spatz_test_threeParam_type(dp-sa-gemv sa-gemv/main.c 256 128 16 64)

  add_spatz_test_oneParam(dp-faxpy dp-faxpy/main.c 256)
  add_spatz_test_oneParam(dp-faxpy dp-faxpy/main.c 1024)

  add_spatz_test_oneParam(dp-fdotp dp-fdotp/main.c 128)
  add_spatz_test_oneParam(dp-fdotp dp-fdotp/main.c 4096)

  add_spatz_test_threeParam(dp-fconv2d dp-fconv2d/main.c 32 32 7)
  add_spatz_test_threeParam(dp-fconv2d dp-fconv2d/main.c 64 64 7)

  add_spatz_test_twoParam(dp-fft dp-fft/main.c 128 2)
  add_spatz_test_oneParam(precoding precoding/main.c 128)
  add_spatz_test_oneParam(precoding precoding/main.c 4096)

endif()

add_spatz_test_threeParam(sp-fmatmul sp-fmatmul/main.c 64  64  64 )
add_spatz_test_threeParam(sp-fmatmul sp-fmatmul/main.c 64  128 64 )

add_spatz_test_threeParam(hp-fmatmul hp-fmatmul/main.c 64  64  64 )
add_spatz_test_threeParam(hp-fmatmul hp-fmatmul/main.c 64  128 64 )
#add_spatz_test_threeParam(hp-fmatmul hp-fmatmul/main.c 128 128 128)

add_spatz_test_threeParam(widening-hp-fmatmul widening-hp-fmatmul/main.c 64  64  64 )
add_spatz_test_threeParam(widening-hp-fmatmul widening-hp-fmatmul/main.c 64  128 64 )
#add_spatz_test_threeParam(widening-hp-fmatmul widening-hp-fmatmul/main.c 128 128 128)

add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 64  64  64 )
add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 64  128 64 )
add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 128 128 128)
#add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 128 256 128)

add_spatz_test_threeParam(sdotp-hp-fmatmul sdotp-hp-fmatmul/main.c 64  64  64 )
add_spatz_test_threeParam(sdotp-hp-fmatmul sdotp-hp-fmatmul/main.c 64  128 64 )
#add_spatz_test_threeParam(sdotp-hp-fmatmul sdotp-hp-fmatmul/main.c 128 128 128)

add_spatz_test_threeParam(sdotp-bp-fmatmul sdotp-bp-fmatmul/main.c 64  64  64 )
add_spatz_test_threeParam(sdotp-bp-fmatmul sdotp-bp-fmatmul/main.c 64  128 64 )
add_spatz_test_threeParam(sdotp-bp-fmatmul sdotp-bp-fmatmul/main.c 128 128 128)
#add_spatz_test_threeParam(sdotp-bp-fmatmul sdotp-bp-fmatmul/main.c 128 256 128)

add_spatz_test_twoParam_type(sp-gemv gemv/main.c 128 128 32)
add_spatz_test_twoParam_type(hp-gemv gemv/main.c 256 128 16)
add_spatz_test_threeParam_type(hp-sa-gemv sa-gemv/main.c 128 4096 512 16)

add_spatz_test_twoParam(sp-fft sp-fft/main.c 256 2)
add_spatz_test_twoParam(sp-fft sp-fft/main.c 512 2)

add_spatz_test_twoParam(srs-fft srs-fft/main.c 32 2)
add_spatz_test_twoParam(srs-fft srs-fft/main.c 256 2)
add_spatz_test_twoParam(srs-fft srs-fft/main.c 512 2)

add_spatz_test_twoParam(srs-hard-decision srs-hard-decision/main.c 8 1)
add_spatz_test_twoParam(srs-hard-decision srs-hard-decision/main.c 200 1)
add_spatz_test_twoParam(srs-hard-decision srs-hard-decision/main.c 384 1)
add_spatz_test_twoParam(srs-hard-decision srs-hard-decision/main.c 512 1)

add_spatz_test_twoParam(srs-compute-soft-bits srs-compute-soft-bits/main.c 2 2)
add_spatz_test_twoParam(srs-compute-soft-bits srs-compute-soft-bits/main.c 384 2)
add_spatz_test_twoParam(srs-compute-var-to-check-msgs srs-compute-var-to-check-msgs/main.c 2 2)
add_spatz_test_twoParam(srs-compute-var-to-check-msgs srs-compute-var-to-check-msgs/main.c 14 2)
add_spatz_test_twoParam(srs-compute-var-to-check-msgs srs-compute-var-to-check-msgs/main.c 384 2)
add_spatz_test_twoParam(srs-compute-var-to-check-msgs-indexed srs-compute-var-to-check-msgs-indexed/main.c 10 2)
add_spatz_test_twoParam(srs-compute-var-to-check-msgs-indexed srs-compute-var-to-check-msgs-indexed/main.c 14 2)
add_spatz_test_twoParam(srs-compute-var-to-check-msgs-indexed srs-compute-var-to-check-msgs-indexed/main.c 384 2)
add_spatz_test_twoParam(srs-compute-check-to-var-msgs srs-compute-check-to-var-msgs/main.c 2 2)
add_spatz_test_twoParam(srs-compute-check-to-var-msgs srs-compute-check-to-var-msgs/main.c 384 2)
add_spatz_test_twoParam(srs-analyze-var-to-check-msgs srs-analyze-var-to-check-msgs/main.c 2 2)
add_spatz_test_twoParam(srs-analyze-var-to-check-msgs srs-analyze-var-to-check-msgs/main.c 4 2)
add_spatz_test_twoParam(srs-analyze-var-to-check-msgs srs-analyze-var-to-check-msgs/main.c 100 2)
add_spatz_test_twoParam(srs-analyze-var-to-check-msgs srs-analyze-var-to-check-msgs/main.c 384 2)
add_spatz_test_twoParam(srs-scale-llr srs-scale-llr/main.c 384 1)
add_spatz_test_twoParam(srs-load-soft-bits srs-load-soft-bits/main.c 1024 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 2 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 3 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 4 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 5 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 6 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 7 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 8 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 9 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 10 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 11 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 12 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 13 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 14 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 15 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 16 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 18 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 20 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 22 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 24 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 26 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 28 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 30 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 32 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 36 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 40 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 44 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 48 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 52 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 56 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 60 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 64 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 72 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 80 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 88 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 96 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 104 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 112 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 120 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 128 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 144 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 160 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 176 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 192 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 208 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 224 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 240 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 256 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 288 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 320 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 352 1)
add_spatz_test_twoParam(srs-decoder srs-decoder/main.c 384 1)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 25165923 8)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 25165923 16)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 25165923 32)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 25165923 257)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 25165923 997)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 25165923 6012)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 25578747 8)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 25578747 16)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 25578747 32)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 25578747 257)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 25578747 997)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 25578747 6012)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 28487959 8)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 28487959 16)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 28487959 32)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 28487959 257)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 28487959 997)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 28487959 6012)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 3617 8)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 3617 16)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 3617 32)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 3617 257)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 3617 997)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 3617 6012)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 69665 8)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 69665 16)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 69665 32)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 69665 257)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 69665 997)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 69665 6012)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 97 8)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 97 16)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 97 32)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 97 257)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 97 997)
add_spatz_test_twoParam(srs-crc srs-crc/main.c 97 6012)
add_spatz_test_twoParam(srs-equalize_zf_1xn srs-equalize_zf_1xn/main.c 13 1)
add_spatz_test_twoParam(srs-equalize_zf_1xn srs-equalize_zf_1xn/main.c 10 1)
add_spatz_test_twoParam(srs-equalize_zf_1xn srs-equalize_zf_1xn/main.c 227 3)
add_spatz_test_twoParam(srs-equalize_zf_1xn srs-equalize_zf_1xn/main.c 227 1)
add_spatz_test_twoParam(srs-equalize_zf_1xn srs-equalize_zf_1xn/main.c 1024 1)
add_spatz_test_twoParam(srs-equalize_zf_1xn srs-equalize_zf_1xn/main.c 1024 4)
add_spatz_test_twoParam(srs-equalize_zf_2xn srs-equalize_zf_2xn/main.c 13 2)
add_spatz_test_twoParam(srs-equalize_zf_2xn srs-equalize_zf_2xn/main.c 10 2)
add_spatz_test_twoParam(srs-equalize_zf_2xn srs-equalize_zf_2xn/main.c 227 3)
add_spatz_test_twoParam(srs-equalize_zf_2xn srs-equalize_zf_2xn/main.c 227 2)
add_spatz_test_twoParam(srs-equalize_zf_2xn srs-equalize_zf_2xn/main.c 1024 2)
add_spatz_test_twoParam(srs-equalize_zf_2xn srs-equalize_zf_2xn/main.c 1024 4)
add_spatz_test_twoParam(srs-equalize_zf_srs srs-equalize_zf_srs/main.c 612 2)
add_spatz_test_twoParam(srs-equalize_zf_srs srs-equalize_zf_srs/main.c 612 4)
add_spatz_test_twoParam(srs-equalize_zf_srs srs-equalize_zf_srs/main.c 4096 4)

# Ventaglio sparse benchmarks. The kernels carry BOTH a Ventaglio (vfx)
# implementation and a baseline RVV reference compiled in via the
# USE_BASELINE macro; only the vfx variants are registered as tests here.
# To run the baseline locally for comparison, add a target that compiles
# main.c with -DUSE_BASELINE. Data headers live under data/ and are
# regenerated by the benchmark data-generation flow.
#
# These tests require -DVENTAGLIO on the RTL side (see hjson `ventaglio: true`).
if(SPATZ_CLUSTER_VENTAGLIO)
  add_spatz_test_spmv(sp-SpMV sp-SpMV/main.c 1to4 32 128)
  add_spatz_test_spmv(sp-SpMV sp-SpMV/main.c 2to4 32 128)
  # sp-SpMM disabled: the tighter epilogue schedule emitted by LLVM 22
  # (back-to-back vfxmacc.vrf -> vse32.v -> lw drain barrier) exposes a
  # latent race in the Ventaglio bank store-drain path. The unmodified
  # kernel deadlocks on the scalar drain-barrier load; padding the window
  # with NOPs instead makes the next iteration's vventclr trap with an
  # illegal-instruction exception (mcause=2). Reproduces on current main
  # RTL as well, so this is not introduced by this PR. Reported to the
  # Ventaglio designer; re-enable once the RTL fix lands.
  # add_spatz_test_spmm(sp-SpMM sp-SpMM/main.c 1to4 4 8 64)
  # add_spatz_test_spmm(sp-SpMM sp-SpMM/main.c 2to4 4 8 64)
endif()
