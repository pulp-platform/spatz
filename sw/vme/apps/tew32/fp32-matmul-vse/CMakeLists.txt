# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(_fp32_vse_te16_shapes
    "64x64x64"
    "64x64x128"
    "64x64x256")

set(_fp32_vse_te32_shapes
    "128x128x64"
    "128x128x128"
    "128x128x256")

function(add_fp32_vse_variant precision variant source_dir)
    if(NOT EXISTS "${source_dir}/main.c" OR
       NOT EXISTS "${source_dir}/kernel/matmul.c" OR
       NOT EXISTS "${source_dir}/kernel/matmul.h" OR
       NOT EXISTS "${source_dir}/script/gen_data.py" OR
       NOT EXISTS "${source_dir}/data/layer.h")
        message(STATUS "Skipping FP32 VSE ${precision}/${variant}: source tree is incomplete")
        return()
    endif()

    if(ARGC GREATER 3)
        set(_variant_shapes ${ARGN})
    elseif(precision STREQUAL "te32")
        set(_variant_shapes ${_fp32_vse_te32_shapes})
    else()
        set(_variant_shapes ${_fp32_vse_te16_shapes})
    endif()

    if(precision STREQUAL "te32")
        set(_tile_edge 32)
    else()
        set(_tile_edge 16)
    endif()

    foreach(_shape IN LISTS _variant_shapes)
        string(REPLACE "x" ";" _dims "${_shape}")
        list(GET _dims 0 _m)
        list(GET _dims 1 _n)
        list(GET _dims 2 _k)

        # main.c reserves two input panels when they fit in the 512-KiB TCDM.
        # This is computed per shape so te32/K256 can fall back to one panel.
        math(EXPR _panel_bytes
             "32 * ${_tile_edge} * ${_k} + 64 * ${_tile_edge} * ${_tile_edge}")
        math(EXPR _double_buffer_bytes "2 * ${_panel_bytes}")
        if(_double_buffer_bytes GREATER 524288)
            set(_buffer_count 1)
        else()
            set(_buffer_count 2)
        endif()

        set(_variant_build_dir
            "${CMAKE_CURRENT_BINARY_DIR}/generated/${precision}/${variant}/K${_k}")
        set(_variant_script_dir "${_variant_build_dir}/script")
        set(_variant_data_dir "${source_dir}/data")
        set(_variant_script "${source_dir}/script/gen_data.py")

        file(MAKE_DIRECTORY "${_variant_script_dir}")

        set(_config "${_variant_script_dir}/matmul_${_m}_${_n}_${_k}.json")
        file(WRITE "${_config}"
             "{ M: ${_m}, N: ${_n}, K: ${_k}, TM: ${_tile_edge}, TN: ${_tile_edge}, seed: 42 }\n")

        set(_header "${source_dir}/data/data_${_m}_${_n}_${_k}.h")
        set(_data_header "data/data_${_m}_${_n}_${_k}.h")
        set(_target
            "fp32-matmul-vse-${precision}-${variant}_M${_m}_N${_n}_K${_k}")
        set(_test_target "test-${SNITCH_TEST_PREFIX}${_target}")

        add_custom_command(
            OUTPUT "${_header}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_variant_data_dir}"
            COMMAND ${Python3_EXECUTABLE} "${_variant_script}" -c "${_config}"
            DEPENDS "${_variant_script}" "${_config}"
                    "${source_dir}/data/layer.h"
            VERBATIM)

        if(BUILD_TESTS)
            add_custom_target(generate-${_target} DEPENDS "${_header}")
            add_snitch_test(${_target} "${source_dir}/main.c")
            add_dependencies(${_test_target} generate-${_target})
            set_target_properties(${_test_target} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/vme/apps")
            target_include_directories(${_test_target} PRIVATE
                "${source_dir}"
                "${source_dir}/kernel"
                "${source_dir}/data")
            target_compile_definitions(${_test_target} PRIVATE
                MATMUL_K_CHUNK=${_k}
                MATMUL_BUFFER_COUNT=${_buffer_count}
                DATAHEADER="${_data_header}")
        endif()
    endforeach()
endfunction()

foreach(_precision IN ITEMS te16 te32)
    if(_precision STREQUAL "te16")
        # te16 is the active validation target. Keep all rolled scheduling
        # variants aligned with the passing VTSE target matrix.
        add_fp32_vse_variant(${_precision} rolled-store-afterK
            "${CMAKE_CURRENT_SOURCE_DIR}/${_precision}/rolled/store-afterK"
            ${_fp32_vse_te16_shapes})
        add_fp32_vse_variant(${_precision} rolled-store-interleave
            "${CMAKE_CURRENT_SOURCE_DIR}/${_precision}/rolled/store-interleave"
            ${_fp32_vse_te16_shapes})
        add_fp32_vse_variant(${_precision} rolled-vsetvl
            "${CMAKE_CURRENT_SOURCE_DIR}/${_precision}/rolled/vsetvl"
            ${_fp32_vse_te16_shapes})
    else()
        # Build all te32 LMUL=2 scheduling variants.
        add_fp32_vse_variant(${_precision} rolled-store-afterK
            "${CMAKE_CURRENT_SOURCE_DIR}/${_precision}/rolled/store-afterK"
            ${_fp32_vse_te32_shapes})
        add_fp32_vse_variant(${_precision} rolled-store-interleave
            "${CMAKE_CURRENT_SOURCE_DIR}/${_precision}/rolled/store-interleave"
            ${_fp32_vse_te32_shapes})
        add_fp32_vse_variant(${_precision} rolled-vsetvl
            "${CMAKE_CURRENT_SOURCE_DIR}/${_precision}/rolled/vsetvl"
            ${_fp32_vse_te32_shapes})
    endif()
endforeach()
