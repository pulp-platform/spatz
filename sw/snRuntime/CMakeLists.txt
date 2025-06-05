# Copyright 2020 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13)

# Allow snRuntime to be built as a standalone library.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Read SnitchUtilities
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
    include(SnitchUtilities)
    # Create snRuntime project
    project(snRuntime LANGUAGES C ASM)
else()
    # Export package information to includer.
    set(SNRUNTIME_DIR ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
    set(SNRUNTIME_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
    set(SNRUNTIME_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
        ${CMAKE_CURRENT_SOURCE_DIR}/../toolchain/riscv-opcodes
        PARENT_SCOPE)
endif()

add_compile_options(-O3 -g -ffunction-sections)

# Platform sources
if("${SPATZ_CLUSTER_CFG}" STREQUAL "spatz_cluster.default.dram.hjson" OR ${SPATZ_CLUSTER_CFG} STREQUAL "spatz_cluster.mempool.dram.hjson")
  set(_plat_folder "standalone")
elseif("${SPATZ_CLUSTER_CFG}" MATCHES "^spatz_cluster.carfield\\.(l2|dram)\\.hjson$")
  set(_plat_folder "cheshire")
else()
  message(FATAL_ERROR
    "Unknown configuration SPATZ_CLUSTER_CFG: ${SPATZ_CLUSTER_CFG} for platform assignment")
endif()

set(PLATFORM_SOURCE_FOLDER "src/platforms/${_plat_folder}" CACHE STRING "Path to the platform-specific sources")

# Default memory regions
set(MEM_SPATZ_CLUSTER_DEFAULT_DRAM_HJSON_ORIGIN  0x80000000)
set(MEM_SPATZ_CLUSTER_DEFAULT_DRAM_HJSON_SIZE    0x80000000)
set(MEM_SPATZ_CLUSTER_CARFIELD_L2_HJSON_ORIGIN   0x78000000)
set(MEM_SPATZ_CLUSTER_CARFIELD_L2_HJSON_SIZE     0x00400000)
set(MEM_SPATZ_CLUSTER_CARFIELD_DRAM_HJSON_ORIGIN 0x80000000)
set(MEM_SPATZ_CLUSTER_CARFIELD_DRAM_HJSON_SIZE   0x80000000)
set(MEM_SPATZ_CLUSTER_MEMPOOL_DRAM_HJSON_ORIGIN  0x80000000)
set(MEM_SPATZ_CLUSTER_MEMPOOL_DRAM_HJSON_SIZE    0x80000000)

# sanitize lookup key (replace dots with underscores, upper-case)
string(REPLACE "." "_" _key "${SPATZ_CLUSTER_CFG}")
string(TOUPPER       "${_key}"   _key)

# build the variable names we defined above
set(_orig_var "MEM_${_key}_ORIGIN")
set(_size_var "MEM_${_key}_SIZE")

if(DEFINED ${_orig_var} AND DEFINED ${_size_var})
    # perform the lookup
    set(MEM_DRAM_ORIGIN "${${_orig_var}}" CACHE STRING "Base address of external memory")
    set(MEM_DRAM_SIZE   "${${_size_var}}"   CACHE STRING "Size of external memory")
else()
    message(FATAL_ERROR "Unknown configuration SPATZ_CLUSTER_CFG: ${SPATZ_CLUSTER_CFG} for memory origin assignment")
endif()

set(SNRT_BASE_HARTID "0" CACHE STRING "Base hart id of this cluster")
set(SNRT_CLUSTER_CORE_NUM "0" CACHE STRING "Number of cores in this cluster")
set(SNRT_TCDM_START_ADDR "0" CACHE STRING "Start address of the TCDM region")
set(SNRT_TCDM_SIZE "0" CACHE STRING "Length of the TCDM region")
set(SNRT_CLUSTER_OFFSET "0" CACHE STRING "Address offset of this cluster's TCDM region")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/link/common.ld.in common.ld @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/start.S.in start.S @ONLY)
set(LINKER_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/common.ld CACHE PATH "")

# provide linker script
# set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/link/common.ld" CACHE PATH "")
message(STATUS "Using common linker script: ${LINKER_SCRIPT}")

# OpenMP
set(OMPSTATIC_NUMTHREADS "0" CACHE STRING "If set to a non-zero value the OpenMP runtime is optimized to the number of cores")


if(RUNTIME_TRACE)
    # Enable runtime tracing
    add_compile_definitions(__SNRT_USE_TRACE)
endif()

if(RUNTIME_PRINT)
    # Enable runtime debugging with printfs
    add_compile_definitions(__SNRT_USE_PRINT)
endif()

include_directories(
    include
    vendor
    ../toolchain/riscv-opcodes
)

# Common sources
set(sources
    src/barrier.c
    src/dma.c
    src/memcpy.c
    src/printf.c
    src/team.c
    src/alloc.c
    src/interrupt.c
    src/perf_cnt.c
)

# platform specific sources
set(standalone_snitch_sources
    ${PLATFORM_SOURCE_FOLDER}/start_snitch.S
    ${PLATFORM_SOURCE_FOLDER}/putchar.c
)

# Sources only compatible with the LLVM toolchain
if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(sources
        ${sources}
        # OpenMP support
        src/omp/omp.c
        src/omp/kmp.c
        src/omp/eu.c
        src/dm.c
    )
    # Check if static OpenMP runtime is requested
    if(OMPSTATIC_NUMTHREADS GREATER 0)
        message(STATUS "Using ${OMPSTATIC_NUMTHREADS} threads for optimized OpenMP runtime")
        add_compile_definitions(OMPSTATIC_NUMTHREADS=${OMPSTATIC_NUMTHREADS})
    else()
        message(STATUS "Generic OpenMP runtime")
    endif()
endif()

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Build all runtimes

    # Generic Runtime (requiring bootloader/OS)
    add_snitch_library(snRuntime ${CMAKE_CURRENT_BINARY_DIR}/start.S ${sources})

    # Bare Runtimes (with startup code)
    add_snitch_library(snRuntime-cluster ${CMAKE_CURRENT_BINARY_DIR}/start.S src/platforms/shared/start.c ${standalone_snitch_sources} ${sources})


else()
    # snRuntime is added externally, only build required runtime
    if(SNITCH_RUNTIME STREQUAL "snRuntime")
        # Generic Runtime (requiring bootloader/OS)
        add_snitch_library(snRuntime ${CMAKE_CURRENT_BINARY_DIR}/start.S src/platforms/shared/start.c ${sources})
    # Bare Runtimes (with startup code)
    elseif(SNITCH_RUNTIME STREQUAL "snRuntime-cluster")
        add_snitch_library(snRuntime-cluster ${CMAKE_CURRENT_BINARY_DIR}/start.S src/platforms/shared/start.c ${standalone_snitch_sources} ${sources})
    else()
        message(FATAL_ERROR "Requested runtime not implemented: ${SNITCH_RUNTIME}")
    endif()
endif()

# Tests
enable_testing()
set(SNITCH_TEST_PREFIX snRuntime-)

# General snRuntime tests
add_snitch_test(tls tests/tls.c)
add_snitch_test(simple tests/simple.c)
add_snitch_test(varargs_1 tests/varargs_1.c)
add_snitch_test(varargs_2 tests/varargs_2.c)
add_snitch_test(barrier tests/barrier.c)
add_snitch_test(fence_i tests/fence_i.c)
add_snitch_test(interrupt-local tests/interrupt-local.c)
add_snitch_test(printf_simple tests/printf_simple.c)

# RTL only tests
if(SNITCH_RUNTIME STREQUAL "snRuntime-cluster")
    add_snitch_test(dma_simple tests/dma_simple.c)
    add_snitch_test(atomics tests/atomics.c)
endif()
