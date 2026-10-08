# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(_fp16_vtse_te16_shapes
    "64x64x128"
    "64x64x256"
    "64x64x512")

set(_fp16_vtse_te32_shapes
    "128x128x128"
    "128x128x256")

function(add_fp16_vtse_variant precision variant source_dir)
    if(NOT EXISTS "${source_dir}/main.c" OR
       NOT EXISTS "${source_dir}/kernel/matmul.c" OR
       NOT EXISTS "${source_dir}/script/gen_data.py" OR
       NOT EXISTS "${source_dir}/data/layer.h")
        message(STATUS "Skipping ${variant}: source tree is incomplete")
        return()
    endif()

    if(ARGC GREATER 3)
        set(_variant_shapes ${ARGN})
    else()
        set(_variant_shapes ${_fp16_vtse_te16_shapes})
    endif()

    foreach(_shape IN LISTS _variant_shapes)
        string(REPLACE "x" ";" _dims "${_shape}")
        list(GET _dims 0 _m)
        list(GET _dims 1 _n)
        list(GET _dims 2 _k)

        set(_k_chunk ${_k})

        if(precision STREQUAL "te32")
            set(_tile_edge 32)
        else()
            set(_tile_edge 16)
        endif()

        # Use double buffering only when both input panels fit in the
        # 512-KiB TCDM.  Larger panels use a single buffer and are refilled
        # synchronously by main.c.
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

        # Keep generated configuration files in the build tree.
        file(MAKE_DIRECTORY "${_variant_script_dir}")

        set(_config "${_variant_script_dir}/matmul_${_m}_${_n}_${_k}.json")
        file(WRITE "${_config}"
             "{ M: ${_m}, N: ${_n}, K: ${_k}, TM: ${_tile_edge}, TN: ${_tile_edge}, seed: 42 }\n")

        set(_header "${source_dir}/data/data_${_m}_${_n}_${_k}.h")
        set(_data_header "data/data_${_m}_${_n}_${_k}.h")
        set(_target
            "fp16-matmul-vtse-${precision}-${variant}_M${_m}_N${_n}_K${_k}")
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
                MATMUL_K_CHUNK=${_k_chunk}
                MATMUL_BUFFER_COUNT=${_buffer_count}
                DATAHEADER="${_data_header}")
        endif()
    endforeach()
endfunction()

foreach(_precision IN ITEMS te16 te32)
    if(_precision STREQUAL "te32")
        set(_precision_shapes ${_fp16_vtse_te32_shapes})
        set(_unrolled_ks 128 256)
    else()
        set(_precision_shapes ${_fp16_vtse_te16_shapes})
        set(_unrolled_ks 128 256 512)
    endif()

    # Rolled kernels continue to build all three K shapes from one source.
    add_fp16_vtse_variant(${_precision} rolled-store-afterK
        "${CMAKE_CURRENT_SOURCE_DIR}/${_precision}/rolled/store-afterK"
        ${_precision_shapes})
    add_fp16_vtse_variant(${_precision} rolled-store-interleave
        "${CMAKE_CURRENT_SOURCE_DIR}/${_precision}/rolled/store-interleave"
        ${_precision_shapes})
    add_fp16_vtse_variant(${_precision} rolled-vsetvl
        "${CMAKE_CURRENT_SOURCE_DIR}/${_precision}/rolled/vsetvl"
        ${_precision_shapes})

    # Unrolled kernels are specialized by K within each scheduling variant.
    foreach(_k IN LISTS _unrolled_ks)
        if(_precision STREQUAL "te32")
            set(_unrolled_shape "128x128x${_k}")
        else()
            set(_unrolled_shape "64x64x${_k}")
        endif()

        add_fp16_vtse_variant(${_precision} unrolled-store-afterK
            "${CMAKE_CURRENT_SOURCE_DIR}/${_precision}/unrolled/store_afterK/K${_k}"
            "${_unrolled_shape}")
        add_fp16_vtse_variant(${_precision} unrolled-store-interleave
            "${CMAKE_CURRENT_SOURCE_DIR}/${_precision}/unrolled/store_interleave/K${_k}"
            "${_unrolled_shape}")
        add_fp16_vtse_variant(${_precision} unrolled-vsetvl
            "${CMAKE_CURRENT_SOURCE_DIR}/${_precision}/unrolled/vsetvl/K${_k}"
            "${_unrolled_shape}")
    endforeach()
endforeach()
