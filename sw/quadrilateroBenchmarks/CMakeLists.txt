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

macro(add_quadrilatero_ldst name file mm mn)
  set(target_name ${name}_${mm}x${mn})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC
      SHAPE_M=${mm} SHAPE_N=${mn}
      QUAD_RLEN=${QUAD_RLEN} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
  # Keep printf as printf: clang -O3 turns printf("text\n") into puts(), which is not in snRuntime
  target_compile_options(test-${SNITCH_TEST_PREFIX}${target_name} PRIVATE -fno-builtin-printf)
endmacro()

macro(add_quadrilatero_isa name file)
  add_snitch_test(${name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${name} ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${name} PUBLIC QUAD_RLEN=${QUAD_RLEN})
  target_compile_options(test-${SNITCH_TEST_PREFIX}${name} PRIVATE -fno-builtin-printf)
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

foreach(shape "1;1" "1;2" "1;4" "2;1" "4;1" "2;2")
  list(GET shape 0 mm)
  list(GET shape 1 mn)
  add_quadrilatero_ldst(ldst-quadrilatero ldst-quadrilatero/main.c ${mm} ${mn})
endforeach()

foreach(t mcfg mzero mld mst mmov mmacc)
  add_quadrilatero_isa(quad-${t} quadrilatero_rv64uv/${t}.c)
endforeach()

# Transposed loads/store (mldt.lhs, mldt.rhs, mstt)
foreach(shape "1;1" "1;2" "1;4" "2;1" "4;1" "2;2")
  list(GET shape 0 mm)
  list(GET shape 1 mn)
  add_quadrilatero_ldst(trans-quadrilatero trans-quadrilatero/main.c ${mm} ${mn})
  add_quadrilatero_ldst(trans-stride trans-stride/main.c ${mm} ${mn})
endforeach()

# Simple (non-pipelined) INT32 matmul: normal and transposed
add_quadrilatero_test_threeParam(sp-matmul-simple-quadrilatero       matmul/sp-matmul-simple-quadrilatero/main.c       64 64 64)
add_quadrilatero_test_threeParam(sp-matmul-simple-trans-quadrilatero matmul/sp-matmul-simple-trans-quadrilatero/main.c 64 64 64)