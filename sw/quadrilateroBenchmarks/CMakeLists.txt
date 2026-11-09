# Copyright 2020 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13)

# Defines
set(SNRT_NFPU_PER_CORE "0" CACHE STRING "Number of FPUs per Spatz")
set(QUAD_RLEN "0" CACHE STRING "Quadrilatero RLEN parameter")

# Allow QuadrilateroBenchmarks to be built as a standalone library.
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


# Macro to regenerate the golden values and compile a module
macro(add_quadrilatero_test_noParam name file)
  set(target_name ${name})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="matrixMul32i.h" SNRT_NFPU_PER_CORE=16)
endmacro()

macro(add_quadrilatero_test_threeParam name file param1 param2 param3)
  set(target_name ${name}_M${param1}_N${param2}_K${param3})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark-quadrilatero ${SNITCH_RUNTIME})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} matmul ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${param3}.h" SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE} QUAD_RLEN=${QUAD_RLEN})
endmacro()

# Macro to compile a load/store test for a register group of mm x mn
macro(add_quadrilatero_ldst name file mm mn)
  set(target_name ${name}_${mm}x${mn})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC SHAPE_M=${mm} SHAPE_N=${mn} QUAD_RLEN=${QUAD_RLEN} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
  # Keep printf as printf: clang -O3 turns printf("text\n") into puts(), which is not in snRuntime
  target_compile_options(test-${SNITCH_TEST_PREFIX}${target_name} PRIVATE -fno-builtin-printf)
endmacro()

# Macro to compile an ISA test
macro(add_quadrilatero_isa name file)
  add_snitch_test(${name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${name} ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${name} PUBLIC QUAD_RLEN=${QUAD_RLEN})
  target_compile_options(test-${SNITCH_TEST_PREFIX}${name} PRIVATE -fno-builtin-printf)
endmacro()

# Macro to set the datatypes of A/B and C, the widening and the float compare of a gemm test
macro(gemm_dt_set dta dtc wid flt)
  set(g_dta ${dta})
  set(g_dtc ${dtc})
  set(g_wid ${wid})
  set(g_flt ${flt})
endmacro()

macro(gemm_dt_info dt)
  if(${dt} STREQUAL "INT8")
    gemm_dt_set(__INT8__     __INT32__  2 0)
  elseif(${dt} STREQUAL "INT16")
    gemm_dt_set(__INT16__    __INT32__  1 0)
  elseif(${dt} STREQUAL "INT32")
    gemm_dt_set(__INT32__    __INT32__  0 0)
  elseif(${dt} STREQUAL "UINT8")
    gemm_dt_set(__UINT8__    __UINT32__ 2 0)
  elseif(${dt} STREQUAL "UINT16")
    gemm_dt_set(__UINT16__   __UINT32__ 1 0)
  elseif(${dt} STREQUAL "UINT32")
    gemm_dt_set(__UINT32__   __UINT32__ 0 0)
  elseif(${dt} STREQUAL "FP8")
    gemm_dt_set(__FP8__      __FP32__   2 1)
  elseif(${dt} STREQUAL "FP8ALT")
    gemm_dt_set(__FP8ALT__   __FP32__   2 1)
  elseif(${dt} STREQUAL "FP16")
    gemm_dt_set(__FP16__     __FP32__   1 1)
  elseif(${dt} STREQUAL "BF16")
    gemm_dt_set(__FP16ALT__  __FP32__   1 1)
  elseif(${dt} STREQUAL "FP32")
    gemm_dt_set(__FP32__     __FP32__   0 1)
  else()
    message(FATAL_ERROR "gemm: unknown datatype ${dt}")
  endif()
endmacro()

# Macro to compile a gemm test (data from scripts/gen_gemm_data.sh, skipped if missing)
macro(add_quadrilatero_gemm shape trans dt m n k)
  if(${trans})
    set(lay MxK_NxK)
    set(dsub data_t/${dt})
  else()
    set(lay KxM_KxN)
    set(dsub data/${dt})
  endif()
  set(tname gemm-${shape}-${lay}-${dt}_M${m}_N${n}_K${k})
  gemm_dt_info(${dt})
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/matmul/gemm-quadrilatero/${dsub}/data_${m}_${n}_${k}.h)
    add_snitch_test(${tname} matmul/gemm-quadrilatero/main.c)
    target_link_libraries(test-${SNITCH_TEST_PREFIX}${tname} benchmark-quadrilatero ${SNITCH_RUNTIME})
    target_compile_definitions(test-${SNITCH_TEST_PREFIX}${tname} PUBLIC DATAHEADER="${dsub}/data_${m}_${n}_${k}.h" KERNEL_H="matmul_${shape}_${lay}.h" TRANS=${trans} DTA=${g_dta} DTB=${g_dta} DTC=${g_dtc} WIDENING=${g_wid} GEMM_FLOAT=${g_flt} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE} QUAD_RLEN=${QUAD_RLEN})
    target_compile_options(test-${SNITCH_TEST_PREFIX}${tname} PRIVATE -fno-builtin-printf)
  else()
    message(STATUS "${tname}: missing ${dsub}/data_${m}_${n}_${k}.h, run scripts/gen_gemm_data.sh")
  endif()
endmacro()

# Benchmark library
add_library(benchmark-quadrilatero kernels/benchmark.c)

# Kernels
add_library(matmul kernels/matmul.c kernels/matmul_simple.c)

# # Tests
enable_testing()
set(SNITCH_TEST_PREFIX quadrilateroBenchmarks-)

# add_quadrilatero_test_threeParam(sp-matmul-quadrilatero matmul/sp-matmul-quadrilatero/main.c 64 64 64)
add_quadrilatero_test_threeParam(widening2x-hp-matmul-quadrilatero matmul/widening2x-hp-matmul-quadrilatero/main.c 64 64 128)
add_quadrilatero_test_threeParam(widening4x-bp-matmul-quadrilatero matmul/widening4x-bp-matmul-quadrilatero/main.c 64 64 256)

# add_quadrilatero_test_threeParam(sp-umatmul-quadrilatero matmul/sp-umatmul-quadrilatero/main.c 64 64 64)
add_quadrilatero_test_threeParam(widening2x-hp-umatmul-quadrilatero matmul/widening2x-hp-umatmul-quadrilatero/main.c 64 64 128)
add_quadrilatero_test_threeParam(widening4x-bp-umatmul-quadrilatero matmul/widening4x-bp-umatmul-quadrilatero/main.c 64 64 256)

# add_quadrilatero_test_threeParam(sp-fmatmul-quadrilatero matmul/sp-fmatmul-quadrilatero/main.c 64 64 64)
add_quadrilatero_test_threeParam(widening2x-hp-fmatmul-quadrilatero matmul/widening2x-hp-fmatmul-quadrilatero/main.c 64 64 128)
add_quadrilatero_test_threeParam(widening4x-bp-fmatmul-quadrilatero matmul/widening4x-bp-fmatmul-quadrilatero/main.c 64 64 256)

add_quadrilatero_test_threeParam(widening2x-hp-altfmatmul-quadrilatero matmul/widening2x-hp-altfmatmul-quadrilatero/main.c 64 64 128)
add_quadrilatero_test_threeParam(widening4x-bp-altfmatmul-quadrilatero matmul/widening4x-bp-altfmatmul-quadrilatero/main.c 64 64 256)

add_quadrilatero_test_threeParam(sp-matmul-simple-KxM_KxN-quadrilatero matmul/sp-matmul-simple-KxM_KxN-quadrilatero/main.c 64 64 64)
add_quadrilatero_test_threeParam(sp-matmul-simple-MxK_NxK-quadrilatero matmul/sp-matmul-simple-MxK_NxK-quadrilatero/main.c 64 64 64)

# ISA tests
foreach(t mcfg mzero mld_KxM_KxN mst_rw mmov mmacc mld_MxK_NxK mst_cw)
  add_quadrilatero_isa(quad-${t} quadrilatero_riscVtests/${t}.c)
endforeach()

# Load/store tests for each register group
foreach(shape "1;1" "1;2" "1;4" "2;1" "4;1" "2;2")
  list(GET shape 0 mm)
  list(GET shape 1 mn)
  add_quadrilatero_ldst(ldst-quadrilatero ldst-quadrilatero/main.c ${mm} ${mn})
  add_quadrilatero_ldst(ldst-MxK_NxK-quadrilatero ldst-MxK_NxK-quadrilatero/main.c ${mm} ${mn})
  add_quadrilatero_ldst(ldst-MxK_NxK-stride ldst-MxK_NxK-stride/main.c ${mm} ${mn})
endforeach()

# Gemm tests: GEMM_SIZES (MxNxK) x GEMM_DTYPES x kernels, both lists also read by scripts/gen_gemm_data.sh
set(GEMM_SIZES
  64x64x64 1x1x1 3x5x7 17x9x33 31x33x29 63x65x61 65x63x67
  100x36x52 128x32x48 32x128x80 7x120x64 120x7x64 64x64x3 40x24x200)
set(GEMM_DTYPES
  FP8 FP8ALT FP16 BF16 FP32)
foreach(dt ${GEMM_DTYPES})
  foreach(sz ${GEMM_SIZES})
    string(REPLACE "x" ";" sz_list ${sz})
    list(GET sz_list 0 gm)
    list(GET sz_list 1 gn)
    list(GET sz_list 2 gk)
    foreach(shape 2x2 1x4 4x1)
      add_quadrilatero_gemm(${shape} 0 ${dt} ${gm} ${gn} ${gk})
      add_quadrilatero_gemm(${shape} 1 ${dt} ${gm} ${gn} ${gk})
    endforeach()
  endforeach()
endforeach()