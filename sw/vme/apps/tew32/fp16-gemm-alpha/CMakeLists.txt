# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(_source "${CMAKE_CURRENT_SOURCE_DIR}")

foreach(_k IN ITEMS 128 256 512)
    math(EXPR _a_bytes "64 * ${_k} * 2")
    math(EXPR _b_bytes "${_k} * 64 * 2")
    math(EXPR _c_bytes "64 * 64 * 4")
    math(EXPR _required_tcdm_bytes
         "${_a_bytes} + ${_b_bytes} + ${_c_bytes}")
    if(_required_tcdm_bytes GREATER SNRT_TCDM_SIZE)
        message(STATUS
            "Skipping fp16-gemm-alpha M64 N64 K${_k}: requires "
            "${_required_tcdm_bytes} TCDM bytes, but only "
            "${SNRT_TCDM_SIZE} are configured")
        continue()
    endif()

    set(_target "fp16-gemm-alpha_M64_N64_K${_k}")
    set(_config "${_source}/script/gemm_64_64_${_k}.json")
    set(_header "${_source}/data/data_64_64_${_k}.h")

    add_custom_command(
        OUTPUT "${_header}"
        COMMAND ${Python3_EXECUTABLE} "${_source}/script/gen_data.py"
                -c "${_config}"
        DEPENDS "${_source}/script/gen_data.py"
                "${_config}"
                "${_source}/data/layer.h"
        VERBATIM)

    if(BUILD_TESTS)
        add_custom_target(generate-${_target} DEPENDS "${_header}")
        add_snitch_test(${_target} "${_source}/main.c")
        set(_test_target "test-${SNITCH_TEST_PREFIX}${_target}")
        add_dependencies(${_test_target} generate-${_target})
        target_include_directories(${_test_target} PRIVATE
            "${_source}" "${_source}/kernel" "${_source}/data")
        target_compile_definitions(${_test_target} PRIVATE
            K_CHUNK=${_k}
            DATAHEADER="data/data_64_64_${_k}.h")
    endif()
endforeach()
