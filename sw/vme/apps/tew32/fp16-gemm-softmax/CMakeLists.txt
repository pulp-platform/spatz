# SPDX-License-Identifier: Apache-2.0

set(_dataset "${CMAKE_CURRENT_SOURCE_DIR}/data/data_c128_t64_v192.h")
if(NOT EXISTS "${_dataset}")
    message(FATAL_ERROR
        "Missing repository-local baseline P6 data: ${_dataset}")
endif()

if(BUILD_TESTS)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/logs")
    foreach(_variant IN ITEMS non-fused fused)
        set(_target fp16-gemm-softmax-${_variant}_M192_N64_K128)
        add_snitch_test(${_target} "${CMAKE_CURRENT_SOURCE_DIR}/main.c")
        set(_test_target "test-${SNITCH_TEST_PREFIX}${_target}")
        target_include_directories(${_test_target} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/kernel")
        target_compile_options(${_test_target} PRIVATE
            -fno-vectorize -fno-slp-vectorize)
        target_compile_definitions(${_test_target} PRIVATE
            DATAHEADER="data/data_c128_t64_v192.h")
        if(_variant STREQUAL "non-fused")
            target_compile_definitions(${_test_target} PRIVATE APP_FUSED=0)
        endif()
    endforeach()
endif()
