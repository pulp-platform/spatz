# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_subdirectory(rolled rolled)

set(_source "${CMAKE_CURRENT_SOURCE_DIR}")

function(add_fp16_simple_gemm_unrolled variant)
    foreach(_k IN ITEMS 128 256 512)
        set(_dir "${_source}/unrolled/${variant}/K${_k}")
        set(_target "fp16-gemm-unit-unrolled-${variant}_M64_N64_K${_k}")
        set(_config "${_dir}/script/matmul_64_64_${_k}.json")
        set(_header "${_dir}/data/data_64_64_${_k}.h")

        add_custom_command(
            OUTPUT "${_header}"
            COMMAND ${Python3_EXECUTABLE} "${_dir}/script/gen_data.py"
                    -c "${_config}"
            DEPENDS "${_dir}/script/gen_data.py"
                    "${_config}"
                    "${_dir}/data/layer.h"
            VERBATIM)

        if(BUILD_TESTS)
            add_custom_target(generate-${_target} DEPENDS "${_header}")
            add_snitch_test(${_target} "${_dir}/main.c")
            set(_test_target "test-${SNITCH_TEST_PREFIX}${_target}")
            add_dependencies(${_test_target} generate-${_target})
            target_include_directories(${_test_target} PRIVATE
                "${_dir}" "${_dir}/kernel" "${_dir}/data")
            target_compile_definitions(${_test_target} PRIVATE
                MATMUL_K_CHUNK=${_k}
                MATMUL_BUFFER_COUNT=2
                DATAHEADER="data/data_64_64_${_k}.h")
        endif()
    endforeach()
endfunction()

# add_fp16_simple_gemm_unrolled(store_afterK)
# add_fp16_simple_gemm_unrolled(store_interleave)
# add_fp16_simple_gemm_unrolled(vsetvl)
