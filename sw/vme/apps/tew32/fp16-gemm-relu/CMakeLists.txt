# SPDX-License-Identifier: Apache-2.0
set(_dataset "${CMAKE_CURRENT_SOURCE_DIR}/data/data_c128_t64_o192.h")
if(NOT EXISTS "${_dataset}")
    message(FATAL_ERROR "Missing repository-local baseline P5 data: ${_dataset}")
endif()

if(BUILD_TESTS)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/logs")
    set(_target fp16-gemm-relu-fused_M192_N64_K128)
    add_snitch_test(${_target} "${CMAKE_CURRENT_SOURCE_DIR}/fused/main.c")
    set(_test_target "test-${SNITCH_TEST_PREFIX}${_target}")
    target_include_directories(${_test_target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/kernel")
    target_compile_definitions(${_test_target} PRIVATE
        DATAHEADER="data/data_c128_t64_o192.h")
    target_compile_options(${_test_target} PRIVATE
        -fno-vectorize -fno-slp-vectorize)

    set(_small_target fp16-gemm-relu-fused_M64_N64_K64)
    add_snitch_test(${_small_target} "${CMAKE_CURRENT_SOURCE_DIR}/main_m64_k64.c")
    set(_small_test_target "test-${SNITCH_TEST_PREFIX}${_small_target}")
    target_include_directories(${_small_test_target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/kernel")
    target_compile_definitions(${_small_test_target} PRIVATE
        GEMM_RELU_M64_K64=1 GG_OPE_BIAS=1)
    target_compile_options(${_small_test_target} PRIVATE
        -fno-vectorize -fno-slp-vectorize)
endif()
