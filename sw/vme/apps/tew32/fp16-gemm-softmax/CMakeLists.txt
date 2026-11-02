# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0

find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
    foreach(_k IN ITEMS 128 256 512)
        set(_target "fp16-gemm-softmax-te16-rolled_M64_N64_K${_k}")
        set(_header "${CMAKE_CURRENT_SOURCE_DIR}/data/data_64_64_${_k}.h")
        set(_config "${CMAKE_CURRENT_BINARY_DIR}/matmul_64_64_${_k}.json")
        file(WRITE "${_config}" "{ M: 64, N: 64, K: ${_k}, TM: 16, TN: 16, seed: 42 }\n")
        add_custom_command(
            OUTPUT "${_header}"
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/script/gen_data.py" -c "${_config}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/script/gen_data.py" "${_config}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/data/layer.h"
            VERBATIM)
        add_custom_target(generate-${_target} DEPENDS "${_header}")
        add_snitch_test(${_target} "${CMAKE_CURRENT_SOURCE_DIR}/main.c")
        set(_test_target "test-${SNITCH_TEST_PREFIX}${_target}")
        add_dependencies(${_test_target} generate-${_target})
        target_include_directories(${_test_target} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/kernel")
        target_compile_options(${_test_target} PRIVATE -fno-vectorize -fno-slp-vectorize)
        target_compile_definitions(${_test_target} PRIVATE
            MATMUL_BUFFER_COUNT=1 DATAHEADER="data/data_64_64_${_k}.h")
    endforeach()
endif()
