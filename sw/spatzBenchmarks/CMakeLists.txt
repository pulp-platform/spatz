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

macro(add_spatz_test_threeParam_vq_header name file data_header param1 param2 param3)
  set(target_name ${name}_M${param1}_N${param2}_K${param3})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name}
    PUBLIC DATAHEADER="${data_header}"
           SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

macro(add_spatz_test_vqgemm_materialized_variant name use_vlxblk block_len data_header param1 param2 param3)
  add_spatz_test_threeParam_vq_header(${name}
    hp-vqmatmul/main_materialized.c
    ${data_header}
    ${param1} ${param2} ${param3})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${name}_M${param1}_N${param2}_K${param3}
    PUBLIC VQ_MATERIALIZED_USE_VLXBLK=${use_vlxblk} VQ_BLOCK_LEN=${block_len})
endmacro()

macro(add_spatz_test_vqgemv_materialized_variant name use_vlxblk block_len data_header param1 param2 param3)
  add_spatz_test_threeParam_vq_header(${name}
    hp-vqmatmul/main_materialized.c
    ${data_header}
    ${param1} ${param2} ${param3})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${name}_M${param1}_N${param2}_K${param3}
    PUBLIC VQ_MATERIALIZED_USE_VLXBLK=${use_vlxblk} VQ_MATERIALIZED_IS_GEMV=1 VQ_BLOCK_LEN=${block_len})
endmacro()

# 3-way dequantization variant selection for the materialized GEMM:
# variant 0 = rvv, 1 = vlxblk, 2 = vle scalar-loop (see main_materialized.c).
macro(add_spatz_test_vqgemm_dequant_variant name variant block_len data_header param1 param2 param3)
  add_spatz_test_threeParam_vq_header(${name}
    hp-vqmatmul/main_materialized.c
    ${data_header}
    ${param1} ${param2} ${param3})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${name}_M${param1}_N${param2}_K${param3}
    PUBLIC VQ_DEQUANT_VARIANT=${variant} VQ_BLOCK_LEN=${block_len})
endmacro()

macro(add_spatz_test_vqgemm_fused_variant name use_vlxblk block_len data_header param1 param2 param3)
  add_spatz_test_threeParam_vq_header(${name}
    hp-vqmatmul/main.c
    ${data_header}
    ${param1} ${param2} ${param3})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${name}_M${param1}_N${param2}_K${param3}
    PUBLIC VQGEMM_USE_VLXBLK_FUSED=${use_vlxblk} VQ_BLOCK_LEN=${block_len})
endmacro()

macro(add_spatz_test_vq_shape_targets op_name block_len param1 param2 param3)
  if("${block_len}" STREQUAL "8")
    set(vq_target_suffix "")
    set(vq_data_header "../hp-vqmatmul/data/data_vq_${param1}_${param2}_${param3}.h")
  else()
    set(vq_target_suffix "-d${block_len}")
    set(vq_data_header "../hp-vqmatmul/data/data_vq_D${block_len}_${param1}_${param2}_${param3}.h")
  endif()

  if("${op_name}" STREQUAL "gemm")
    add_spatz_test_vqgemm_materialized_variant(hp-vqgemm-rvv-materialized${vq_target_suffix} 0 ${block_len}
      ${vq_data_header}
      ${param1} ${param2} ${param3})
    add_spatz_test_vqgemm_materialized_variant(hp-vqgemm-vlxblk-materialized${vq_target_suffix} 1 ${block_len}
      ${vq_data_header}
      ${param1} ${param2} ${param3})
    add_spatz_test_vqgemm_fused_variant(hp-vqgemm-rvv-fused${vq_target_suffix} 0 ${block_len}
      ${vq_data_header}
      ${param1} ${param2} ${param3})
    add_spatz_test_vqgemm_fused_variant(hp-vqgemm-vlxblk-fused${vq_target_suffix} 1 ${block_len}
      ${vq_data_header}
      ${param1} ${param2} ${param3})
  else()
    add_spatz_test_vqgemv_materialized_variant(hp-vqgemv-rvv-materialized${vq_target_suffix} 0 ${block_len}
      ${vq_data_header}
      ${param1} ${param2} ${param3})
    add_spatz_test_vqgemv_materialized_variant(hp-vqgemv-vlxblk-materialized${vq_target_suffix} 1 ${block_len}
      ${vq_data_header}
      ${param1} ${param2} ${param3})
    add_spatz_test_vqgemv_variant(hp-vqgemv-rvv-fused${vq_target_suffix} 0 ${block_len}
      ${vq_data_header}
      ${param1} ${param2} ${param3})
    add_spatz_test_vqgemv_variant(hp-vqgemv-vlxblk-fused${vq_target_suffix} 1 ${block_len}
      ${vq_data_header}
      ${param1} ${param2} ${param3})
  endif()
endmacro()

macro(add_spatz_test_vqgemv_variant name use_vlxblk block_len data_header param1 param2 param3)
  add_spatz_test_threeParam_vq_header(${name}
    hp-vqgemv/main.c
    ${data_header}
    ${param1} ${param2} ${param3})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${name}_M${param1}_N${param2}_K${param3}
    PUBLIC VQGEMV_USE_VLXBLK=${use_vlxblk} VQ_BLOCK_LEN=${block_len})
endmacro()

macro(add_spatz_test_vqdotp_variant name block_len data_header param1 param2 param3)
  add_spatz_test_threeParam_vq_header(${name}
    hp-vqdotp/main.c
    ${data_header}
    ${param1} ${param2} ${param3})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${name}_M${param1}_N${param2}_K${param3}
    PUBLIC VQ_BLOCK_LEN=${block_len})
endmacro()

macro(add_spatz_test_vqdotp_fused_variant name use_vlxblk block_len data_header param1 param2 param3)
  add_spatz_test_vqdotp_variant(${name}
    ${block_len}
    ${data_header}
    ${param1} ${param2} ${param3})
  if("${use_vlxblk}" STREQUAL "1")
    target_compile_definitions(test-${SNITCH_TEST_PREFIX}${name}_M${param1}_N${param2}_K${param3}
      PUBLIC VQDOTP_USE_VLXBLK=1 VQDOTP_PRINT_MISMATCHES=0
             VQDOTP_NAME="hp vqdotp vlxblk-fused")
  else()
    target_compile_definitions(test-${SNITCH_TEST_PREFIX}${name}_M${param1}_N${param2}_K${param3}
      PUBLIC VQDOTP_USE_VLXBLK=0 VQDOTP_PRINT_MISMATCHES=0
             VQDOTP_NAME="hp vqdotp rvv-fused")
  endif()
endmacro()

macro(add_spatz_test_vqdotp_vlxblk_fused_profile_variant name block_len data_header param1 param2 param3)
  add_spatz_test_vqdotp_variant(${name}
    ${block_len}
    ${data_header}
    ${param1} ${param2} ${param3})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${name}_M${param1}_N${param2}_K${param3}
    PUBLIC VQDOTP_USE_VLXBLK=1
           VQDOTP_PRINT_MISMATCHES=0 VQDOTP_PROFILE_REDUCE=1 VQDOTP_SINGLE_CORE=1
           VQDOTP_NAME="hp vqdotp vlxblk-fused-profile")

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
add_library(hp-vqmatmul-materialized hp-vqmatmul/kernel/hp-vqmatmul-materialized.c)
add_library(hp-vqgemv hp-vqgemv/kernel/hp-vqgemv.c)
add_library(hp-vqdotp hp-vqdotp/kernel/hp-vqdotp.c)


add_library(widening-hp-fmatmul widening-hp-fmatmul/kernel/widening-fmatmul.c)
add_library(widening-bp-fmatmul widening-bp-fmatmul/kernel/widening-fmatmul.c)

add_library(sdotp-hp-fmatmul sdotp-hp-fmatmul/kernel/sdotp-fmatmul.c)
add_library(sdotp-bp-fmatmul sdotp-bp-fmatmul/kernel/sdotp-fmatmul.c)

add_library(sp-fft sp-fft/kernel/fft.c)

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
# VQ GEMM/GEMV targets are generated from the addressing strategy
# (rvv or VLXBLK) and the algorithm shape (fused or materialized).

set(VQ_GEMM_D8_SHAPES
  32_32_32
  32_128_32
  128_128_128
  128_256_128)
set(VQ_GEMM_BLOCK_SHAPES
  32_128_32
  128_256_128)
set(VQ_GEMV_D8_SHAPES
  1_32_32
  1_128_32
  1_128_128
  1_256_128
  1_512_128)
set(VQ_GEMV_BLOCK_SHAPES
  1_128_32
  1_256_128)
set(VQ_EXTRA_BLOCK_LENGTHS 4 16)
set(VQ_DOTP_D8_SHAPES
  1_1_32
  1_32_32
  1_128_128
  1_256_128)
set(VQ_DOTP_BLOCK_SHAPES
  1_128_128
  1_256_128)

foreach(shape IN LISTS VQ_GEMM_D8_SHAPES)
  string(REPLACE "_" ";" dims "${shape}")
  list(GET dims 0 vq_m)
  list(GET dims 1 vq_n)
  list(GET dims 2 vq_k)
  add_spatz_test_vq_shape_targets(gemm 8 ${vq_m} ${vq_n} ${vq_k})
endforeach()

foreach(block_len IN LISTS VQ_EXTRA_BLOCK_LENGTHS)
  foreach(shape IN LISTS VQ_GEMM_BLOCK_SHAPES)
    string(REPLACE "_" ";" dims "${shape}")
    list(GET dims 0 vq_m)
    list(GET dims 1 vq_n)
    list(GET dims 2 vq_k)
    add_spatz_test_vq_shape_targets(gemm ${block_len} ${vq_m} ${vq_n} ${vq_k})
  endforeach()
endforeach()

foreach(shape IN LISTS VQ_GEMV_D8_SHAPES)
  string(REPLACE "_" ";" dims "${shape}")
  list(GET dims 0 vq_m)
  list(GET dims 1 vq_n)
  list(GET dims 2 vq_k)
  add_spatz_test_vq_shape_targets(gemv 8 ${vq_m} ${vq_n} ${vq_k})
endforeach()

foreach(block_len IN LISTS VQ_EXTRA_BLOCK_LENGTHS)
  foreach(shape IN LISTS VQ_GEMV_BLOCK_SHAPES)
    string(REPLACE "_" ";" dims "${shape}")
    list(GET dims 0 vq_m)
    list(GET dims 1 vq_n)
    list(GET dims 2 vq_k)
    add_spatz_test_vq_shape_targets(gemv ${block_len} ${vq_m} ${vq_n} ${vq_k})
  endforeach()
endforeach()

# large-block ablation: scope-boundary study for VLXBLK at the one-register
# block size (d*SEW == VLEN). At d=32 (64-B e16 blocks == one m1 register) a
# scalar loop of plain unit-stride vle16 loads into consecutive destination
# registers should reach parity with vlxblk (the register number is the
# packing); at d=8 (16-B quarter-register blocks) the same vle loop needs a
# vslideup packing chain per block and is expected to collapse.
# d=32 x {vlxblk, rvv, vle} and d=8 x {vle}, all materialized at 32x128x32.
add_spatz_test_vqgemm_dequant_variant(hp-vqgemm-vlxblk-materialized-d32 1 32
  "../hp-vqmatmul/data/data_vq_D32_32_128_32.h"
  32 128 32)
add_spatz_test_vqgemm_dequant_variant(hp-vqgemm-rvv-materialized-d32 0 32
  "../hp-vqmatmul/data/data_vq_D32_32_128_32.h"
  32 128 32)
add_spatz_test_vqgemm_dequant_variant(hp-vqgemm-vle-materialized-d32 2 32
  "../hp-vqmatmul/data/data_vq_D32_32_128_32.h"
  32 128 32)
add_spatz_test_vqgemm_dequant_variant(hp-vqgemm-vle-materialized 2 8
  "../hp-vqmatmul/data/data_vq_32_128_32.h"
  32 128 32)

foreach(shape IN LISTS VQ_DOTP_D8_SHAPES)
  string(REPLACE "_" ";" dims "${shape}")
  list(GET dims 0 vq_m)
  list(GET dims 1 vq_n)
  list(GET dims 2 vq_k)
  add_spatz_test_vqdotp_fused_variant(hp-vqdotp-rvv-fused 0 8
    "../hp-vqmatmul/data/data_vq_KBLK_${vq_m}_${vq_n}_${vq_k}.h"
    ${vq_m} ${vq_n} ${vq_k})
  add_spatz_test_vqdotp_fused_variant(hp-vqdotp-vlxblk-fused 1 8
    "../hp-vqmatmul/data/data_vq_KBLK_${vq_m}_${vq_n}_${vq_k}.h"
    ${vq_m} ${vq_n} ${vq_k})
  add_spatz_test_vqdotp_vlxblk_fused_profile_variant(hp-vqdotp-vlxblk-fused-profile 8
    "../hp-vqmatmul/data/data_vq_KBLK_${vq_m}_${vq_n}_${vq_k}.h"
    ${vq_m} ${vq_n} ${vq_k})
endforeach()

foreach(block_len IN LISTS VQ_EXTRA_BLOCK_LENGTHS)
  foreach(shape IN LISTS VQ_DOTP_BLOCK_SHAPES)
    string(REPLACE "_" ";" dims "${shape}")
    list(GET dims 0 vq_m)
    list(GET dims 1 vq_n)
    list(GET dims 2 vq_k)
    add_spatz_test_vqdotp_fused_variant(hp-vqdotp-rvv-fused-d${block_len} 0 ${block_len}
      "../hp-vqmatmul/data/data_vq_KBLK_D${block_len}_${vq_m}_${vq_n}_${vq_k}.h"
      ${vq_m} ${vq_n} ${vq_k})
    add_spatz_test_vqdotp_fused_variant(hp-vqdotp-vlxblk-fused-d${block_len} 1 ${block_len}
      "../hp-vqmatmul/data/data_vq_KBLK_D${block_len}_${vq_m}_${vq_n}_${vq_k}.h"
      ${vq_m} ${vq_n} ${vq_k})
  endforeach()
endforeach()

# sp-dictdecode (VLXBLK benchmark)
# Database dictionary decompression: a column of N codes indexes a
# dictionary of K fixed-width entries (D=4 x fp32 = 16 B records); decoding
# materializes the values column. Pure indexed-block-copy engine test:
# VLXBLK indexed block loads vs. best-effort plain-RVV baseline
# (vluxei16 lane gathers + strided stores).
macro(add_spatz_test_dictdecode name use_vlxblk n_codes k)
  set(target_name ${name}_N${n_codes}_K${k})
  add_snitch_test(${target_name} sp-dictdecode/main.c)
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC
    DATAHEADER="data/data_dict_${n_codes}_${k}.h"
    DICTDECODE_USE_VLXBLK=${use_vlxblk}
    SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

add_spatz_test_dictdecode(sp-dictdecode-vlxblk 1 8192 1024)
add_spatz_test_dictdecode(sp-dictdecode-rvv    0 8192 1024)
add_spatz_test_dictdecode(sp-dictdecode-vlxblk 1 8192 256)
add_spatz_test_dictdecode(sp-dictdecode-rvv    0 8192 256)

