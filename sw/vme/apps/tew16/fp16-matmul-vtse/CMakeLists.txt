# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0

set(_target fp16-matmul-vtse_M64_N64_K64)
set(_test_target test-${SNITCH_TEST_PREFIX}${_target})

if(NOT TARGET ${_test_target})
    add_snitch_test(${_target} fp16-matmul-vtse/main.c)
    target_include_directories(${_test_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/fp16-matmul-vtse
        ${CMAKE_CURRENT_SOURCE_DIR}/fp16-matmul-vtse/kernel
        ${CMAKE_CURRENT_SOURCE_DIR}/fp16-matmul-vtse/data)
    target_compile_definitions(${_test_target} PUBLIC
        DATAHEADER="data/data_64_64_64.h")
    configure_vme_matmul_target(${_test_target})
endif()