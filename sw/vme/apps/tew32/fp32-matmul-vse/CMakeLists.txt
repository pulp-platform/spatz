# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(_fp32_vse_shapes
    "64x64x64"
    "64x64x128"
    "64x64x256")

function(add_fp32_vse_variant variant source_dir)
    if(NOT EXISTS "${source_dir}/main.c" OR
       NOT EXISTS "${source_dir}/kernel/matmul.c" OR
       NOT EXISTS "${source_dir}/kernel/matmul.h" OR
       NOT EXISTS "${source_dir}/script/gen_data.py" OR
       NOT EXISTS "${source_dir}/data/layer.h")
        message(STATUS "Skipping FP32 VSE ${variant}: source tree is incomplete")
        return()
    endif()

    foreach(_shape IN LISTS _fp32_vse_shapes)
        string(REPLACE "x" ";" _dims "${_shape}")
        list(GET _dims 0 _m)
        list(GET _dims 1 _n)
        list(GET _dims 2 _k)

        set(_variant_build_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/${variant}/K${_k}")
        set(_variant_script_dir "${_variant_build_dir}/script")
        set(_variant_data_dir "${_variant_build_dir}/data")
        set(_variant_script "${_variant_script_dir}/gen_data.py")

        file(MAKE_DIRECTORY "${_variant_script_dir}")
        configure_file("${source_dir}/script/gen_data.py" "${_variant_script}" COPYONLY)

        set(_config "${_variant_script_dir}/matmul_${_m}_${_n}_${_k}.json")
        file(WRITE "${_config}" "{ M: ${_m}, N: ${_n}, K: ${_k}, TM: 16, TN: 16, seed: 42 }\n")

        set(_header "${_variant_data_dir}/data_${_m}_${_n}_${_k}.h")
        set(_target "fp32-matmul-vse-${variant}_M${_m}_N${_n}_K${_k}")
        set(_test_target "test-${SNITCH_TEST_PREFIX}${_target}")

        add_custom_command(
            OUTPUT "${_header}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_variant_data_dir}"
            COMMAND ${Python3_EXECUTABLE} "${_variant_script}" -c "${_config}"
            DEPENDS "${_variant_script}" "${_config}" "${source_dir}/data/layer.h"
            VERBATIM)

        if(BUILD_TESTS)
            add_custom_target(generate-${_target} DEPENDS "${_header}")
            add_snitch_test(${_target} "${source_dir}/main.c")
            add_dependencies(${_test_target} generate-${_target})
            set_target_properties(${_test_target} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/vme/apps")
            target_include_directories(${_test_target} PRIVATE
                "${_variant_build_dir}"
                "${source_dir}"
                "${source_dir}/kernel"
                "${source_dir}/data")
            target_compile_definitions(${_test_target} PRIVATE
                MATMUL_K_CHUNK=${_k}
                MATMUL_BUFFER_COUNT=2
                DATAHEADER="${_header}")
        endif()
    endforeach()
endfunction()

add_fp32_vse_variant(rolled-vsetvl "${CMAKE_CURRENT_SOURCE_DIR}/rolled/vsetvl")
