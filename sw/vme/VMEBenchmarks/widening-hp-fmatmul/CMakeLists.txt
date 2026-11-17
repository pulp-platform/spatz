# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

set(_source "${CMAKE_CURRENT_SOURCE_DIR}")

# The partial case also exercises the final tk=1 reduction mask.
foreach(_shape IN ITEMS "13;11;19" "16;16;64")
    list(GET _shape 0 _m)
    list(GET _shape 1 _n)
    list(GET _shape 2 _k)

    math(EXPR _kpairs "(${_k} + 1) / 2")
    math(EXPR _a_bytes "${_kpairs} * ${_m} * 4")
    math(EXPR _b_bytes "${_kpairs} * ${_n} * 4")
    math(EXPR _c_bytes "${_m} * ${_n} * 4")
    math(EXPR _required_tcdm_bytes
         "${_a_bytes} + ${_b_bytes} + ${_c_bytes}")
    if(_required_tcdm_bytes GREATER SNRT_TCDM_SIZE)
        message(STATUS "Skipping VME widening one-tile M${_m} N${_n} K${_k}: "
                       "requires ${_required_tcdm_bytes} TCDM bytes")
        continue()
    endif()

    set(_target "widening-hp-fmatmul-1tile_M${_m}_N${_n}_K${_k}")
    set(_config "${_source}/script/matmul_${_m}_${_n}_${_k}.json")
    set(_header "${_source}/data/data_${_m}_${_n}_${_k}.h")

    add_custom_command(
        OUTPUT "${_header}"
        COMMAND ${Python3_EXECUTABLE} "${_source}/script/gen_data.py"
                --config "${_config}" --output "${_header}"
        DEPENDS "${_source}/script/gen_data.py" "${_config}"
                "${_source}/data/layer.h"
        VERBATIM)

    if(BUILD_TESTS)
        add_custom_target(generate-${_target} DEPENDS "${_header}")
        add_snitch_test(${_target} "${_source}/main.c")
        set(_test_target "test-${SNITCH_TEST_PREFIX}${_target}")
        add_dependencies(${_test_target} generate-${_target})
        target_sources(${_test_target} PRIVATE
            "${_source}/kernel/widening-fmatmul.c")
        target_include_directories(${_test_target} PRIVATE
            "${_source}" "${_source}/kernel" "${_source}/data")
        target_compile_definitions(${_test_target} PRIVATE
            DATAHEADER="data/data_${_m}_${_n}_${_k}.h"
            SNRT_TCDM_SIZE=${SNRT_TCDM_SIZE})
    endif()
endforeach()

# A single public call tiles arbitrary M/N shapes into <=32x32 panels. The
# irregular case exercises 4-, both orientations of 2-, and 1-tile tails.
foreach(_shape IN ITEMS "45;37;19" "64;64;64")
    list(GET _shape 0 _m)
    list(GET _shape 1 _n)
    list(GET _shape 2 _k)

    math(EXPR _kpairs "(${_k} + 1) / 2")
    math(EXPR _a_bytes "${_kpairs} * ${_m} * 4")
    math(EXPR _b_bytes "${_kpairs} * ${_n} * 4")
    math(EXPR _c_bytes "${_m} * ${_n} * 4")
    math(EXPR _required_tcdm_bytes
         "${_a_bytes} + ${_b_bytes} + ${_c_bytes}")
    if(_required_tcdm_bytes GREATER SNRT_TCDM_SIZE)
        message(STATUS "Skipping VME widening auto M${_m} N${_n} K${_k}: "
                       "requires ${_required_tcdm_bytes} TCDM bytes")
        continue()
    endif()

    set(_target "widening-hp-fmatmul-auto_M${_m}_N${_n}_K${_k}")
    set(_config "${_source}/script/matmul_${_m}_${_n}_${_k}.json")
    set(_header "${_source}/data/data_${_m}_${_n}_${_k}.h")
    add_custom_command(
        OUTPUT "${_header}"
        COMMAND ${Python3_EXECUTABLE} "${_source}/script/gen_data.py"
                --config "${_config}" --output "${_header}"
        DEPENDS "${_source}/script/gen_data.py" "${_config}"
                "${_source}/data/layer.h"
        VERBATIM)

    if(BUILD_TESTS)
        add_custom_target(generate-${_target} DEPENDS "${_header}")
        add_snitch_test(${_target} "${_source}/main_auto.c")
        set(_test_target "test-${SNITCH_TEST_PREFIX}${_target}")
        add_dependencies(${_test_target} generate-${_target})
        target_sources(${_test_target} PRIVATE
            "${_source}/kernel/widening-fmatmul.c")
        target_include_directories(${_test_target} PRIVATE
            "${_source}" "${_source}/kernel" "${_source}/data")
        target_compile_definitions(${_test_target} PRIVATE
            DATAHEADER="data/data_${_m}_${_n}_${_k}.h"
            SNRT_TCDM_SIZE=${SNRT_TCDM_SIZE})
    endif()
endforeach()

# Four tiles form a 2x2 output panel. The two schedules use the same data.
foreach(_shape IN ITEMS "29;27;19" "32;32;64")
    list(GET _shape 0 _m)
    list(GET _shape 1 _n)
    list(GET _shape 2 _k)

    math(EXPR _kpairs "(${_k} + 1) / 2")
    math(EXPR _a_bytes "${_kpairs} * ${_m} * 4")
    math(EXPR _b_bytes "${_kpairs} * ${_n} * 4")
    math(EXPR _c_bytes "${_m} * ${_n} * 4")
    math(EXPR _required_tcdm_bytes
         "${_a_bytes} + ${_b_bytes} + ${_c_bytes}")
    if(_required_tcdm_bytes GREATER SNRT_TCDM_SIZE)
        message(STATUS "Skipping VME widening four-tile M${_m} N${_n} K${_k}: "
                       "requires ${_required_tcdm_bytes} TCDM bytes")
        continue()
    endif()

    set(_data_target "widening-hp-fmatmul-4tile-data_M${_m}_N${_n}_K${_k}")
    set(_config "${_source}/script/matmul_${_m}_${_n}_${_k}.json")
    set(_header "${_source}/data/data_${_m}_${_n}_${_k}.h")
    add_custom_command(
        OUTPUT "${_header}"
        COMMAND ${Python3_EXECUTABLE} "${_source}/script/gen_data.py"
                --config "${_config}" --output "${_header}"
        DEPENDS "${_source}/script/gen_data.py" "${_config}"
                "${_source}/data/layer.h"
        VERBATIM)

    if(BUILD_TESTS)
        add_custom_target(generate-${_data_target} DEPENDS "${_header}")
        foreach(_variant IN ITEMS pingpong reuse)
            set(_target
                "widening-hp-fmatmul-4tile-${_variant}_M${_m}_N${_n}_K${_k}")
            add_snitch_test(${_target} "${_source}/main_4tile.c")
            set(_test_target "test-${SNITCH_TEST_PREFIX}${_target}")
            add_dependencies(${_test_target} generate-${_data_target})
            target_sources(${_test_target} PRIVATE
                "${_source}/kernel/widening-fmatmul.c")
            target_include_directories(${_test_target} PRIVATE
                "${_source}" "${_source}/kernel" "${_source}/data")
            if(_variant STREQUAL "reuse")
                set(_variant_id 1)
            else()
                set(_variant_id 0)
            endif()
            target_compile_definitions(${_test_target} PRIVATE
                DATAHEADER="data/data_${_m}_${_n}_${_k}.h"
                SNRT_TCDM_SIZE=${SNRT_TCDM_SIZE}
                WIDENING_4TILE_REUSE=${_variant_id})
        endforeach()
    endif()
endforeach()

# Two tiles form a 1x2 output panel. Both implementations consume identical
# packed data so their schedules can be compared directly.
foreach(_shape IN ITEMS "13;29;19" "16;32;64")
    list(GET _shape 0 _m)
    list(GET _shape 1 _n)
    list(GET _shape 2 _k)

    math(EXPR _kpairs "(${_k} + 1) / 2")
    math(EXPR _a_bytes "${_kpairs} * ${_m} * 4")
    math(EXPR _b_bytes "${_kpairs} * ${_n} * 4")
    math(EXPR _c_bytes "${_m} * ${_n} * 4")
    math(EXPR _required_tcdm_bytes
         "${_a_bytes} + ${_b_bytes} + ${_c_bytes}")
    if(_required_tcdm_bytes GREATER SNRT_TCDM_SIZE)
        message(STATUS "Skipping VME widening two-tile M${_m} N${_n} K${_k}: "
                       "requires ${_required_tcdm_bytes} TCDM bytes")
        continue()
    endif()

    set(_data_target "widening-hp-fmatmul-2tile-data_M${_m}_N${_n}_K${_k}")
    set(_config "${_source}/script/matmul_${_m}_${_n}_${_k}.json")
    set(_header "${_source}/data/data_${_m}_${_n}_${_k}.h")
    add_custom_command(
        OUTPUT "${_header}"
        COMMAND ${Python3_EXECUTABLE} "${_source}/script/gen_data.py"
                --config "${_config}" --output "${_header}"
        DEPENDS "${_source}/script/gen_data.py" "${_config}"
                "${_source}/data/layer.h"
        VERBATIM)

    if(BUILD_TESTS)
        add_custom_target(generate-${_data_target} DEPENDS "${_header}")
        foreach(_variant IN ITEMS pingpong reuse)
            set(_target
                "widening-hp-fmatmul-2tile-${_variant}_M${_m}_N${_n}_K${_k}")
            add_snitch_test(${_target} "${_source}/main_2tile.c")
            set(_test_target "test-${SNITCH_TEST_PREFIX}${_target}")
            add_dependencies(${_test_target} generate-${_data_target})
            target_sources(${_test_target} PRIVATE
                "${_source}/kernel/widening-fmatmul.c")
            target_include_directories(${_test_target} PRIVATE
                "${_source}" "${_source}/kernel" "${_source}/data")
            if(_variant STREQUAL "reuse")
                set(_variant_id 1)
            else()
                set(_variant_id 0)
            endif()
            target_compile_definitions(${_test_target} PRIVATE
                DATAHEADER="data/data_${_m}_${_n}_${_k}.h"
                SNRT_TCDM_SIZE=${SNRT_TCDM_SIZE}
                WIDENING_2TILE_REUSE=${_variant_id})
        endforeach()
    endif()
endforeach()
