# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Author: Pei-Yu Lin <peilin@ethz.ch>

cmake_minimum_required(VERSION 3.13)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# VME binaries require a configured TCDM capacity for buffer sizing.
if(NOT DEFINED SNRT_TCDM_SIZE OR NOT SNRT_TCDM_SIZE GREATER 0)
    message(STATUS "Skipping VME apps: SNRT_TCDM_SIZE is not configured")
    return()
endif()
add_compile_definitions(SNRT_TCDM_SIZE=${SNRT_TCDM_SIZE})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kernel)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/data)

enable_testing()

set(SNITCH_TEST_PREFIX vme-)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/vme/apps")

option(VME_TE32 "Build VME kernels for a TE=32 hardware configuration" OFF)
if(VME_TE32)
    set(VME_TILE_VARIANT te32)
else()
    set(VME_TILE_VARIANT te16)
endif()
message(STATUS "Building VME ${VME_TILE_VARIANT} kernels")

### Microbenchmarks
if(VME_ENABLE_FMA32)
    add_subdirectory(tew32/fp32-matmul-vtse fp32-matmul-vtse)
    add_subdirectory(tew32/fp32-matmul-vse fp32-matmul-vse)
else()
    message(STATUS "Skipping VME FP32 applications: OPE FP32 FMA is disabled")
endif()
add_subdirectory(tew32/fp16-matmul-vtse fp16-matmul-vtse)
add_subdirectory(tew32/fp16-matmul-vse fp16-matmul-vse)
add_subdirectory(tew32/fp16-gemm-unit fp16-gemm-unit)
add_subdirectory(tew32/fp16-gemm-alpha fp16-gemm-alpha)
add_subdirectory(tew32/fp16-gemm-beta fp16-gemm-beta)

### Benchmarks
# add_subdirectory(tew32/fp16-vq-matmul fp16-vq-matmul)

# add_subdirectory(tew32/fp16-vq-gemm fp16-vq-gemm)
# add_subdirectory(tew32/fp16-dq-matmul fp16-dq-matmul)

# add_subdirectory(tew32/fp16-matmul-softmax fp16-matmul-softmax)
# add_subdirectory(tew32/fp16-gemm-softmax fp16-gemm-softmax)
# add_subdirectory(tew32/fp16-gemm-softmax-ce fp16-gemm-softmax-ce)
# add_subdirectory(tew32/fp16-ln-gemm fp16-ln-gemm)
# add_subdirectory(tew32/fp16-embed-ln-gemm fp16-embed-ln-gemm)
# add_subdirectory(tew32/fp16_conv_relu fp16_conv_relu)

# add_subdirectory(tew32/fp16-gemm-relu fp16-gemm-relu)
# add_subdirectory(tew32/fp16-gemm-gelu fp16-gemm-gelu)
# add_subdirectory(tew32/fp16-gemm-residual-separate fp16-gemm-residual-separate)
# add_subdirectory(tew32/fp16-gemm-fused-residual fp16-gemm-fused-residual)
# add_subdirectory(tew32/fp16-flashattn fp16-flashattn)
# add_subdirectory(tew32/fp16-gathermlp-dlrm fp16-gathermlp-dlrm)
# add_subdirectory(tew32/fp16-gathermlp-gcn fp16-gathermlp-gcn)

# add_subdirectory(tew32/fp16-attention-head fp16-attention-head)
# add_subdirectory(tew32/fp16-mlp-block fp16-mlp-block)
