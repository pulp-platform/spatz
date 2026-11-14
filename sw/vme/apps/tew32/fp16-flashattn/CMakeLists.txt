find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Keep one validated schedule for each requested workload. The profile data
# is kept in profile/; production targets have no cycle-counter instrumentation.
set(_flash_header "${CMAKE_CURRENT_BINARY_DIR}/data/data_tq64_t256_d64_bc64.h")
set(_key64_header "${CMAKE_CURRENT_BINARY_DIR}/data/data_tq64_t64_d64_bc64.h")
set(_direct_header "${CMAKE_CURRENT_BINARY_DIR}/data/data_tq16_t64_d64_dv16.h")

add_custom_command(
    OUTPUT "${_flash_header}"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/script/gen_data_tq64.py"
            --output "${_flash_header}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/script/gen_data_tq64.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../script/vme_data.py"
    VERBATIM)
add_custom_command(
    OUTPUT "${_key64_header}"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/script/gen_data_tq64.py"
            --output "${_key64_header}" --T 64
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/script/gen_data_tq64.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../script/vme_data.py"
    VERBATIM)
add_custom_command(
    OUTPUT "${_direct_header}"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/script/gen_data_tq16.py"
            --output "${_direct_header}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/script/gen_data_tq16.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../script/vme_data.py"
    VERBATIM)

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
    add_custom_target(generate-fp16-flashattn-vrf-data
        DEPENDS "${_direct_header}")
    add_snitch_test(fp16-flashattn-vrf_TQ16_T64_D64_DV16
        "${CMAKE_CURRENT_SOURCE_DIR}/main_tq16.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/kernel/flashattn_tq16.c")
    set(_direct_target
        "test-${SNITCH_TEST_PREFIX}fp16-flashattn-vrf_TQ16_T64_D64_DV16")
    add_dependencies(${_direct_target} generate-fp16-flashattn-vrf-data)
    target_include_directories(${_direct_target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_definitions(${_direct_target} PRIVATE
        DATAHEADER="data/data_tq16_t64_d64_dv16.h"
        FLASH_TQ=16 FLASH_T=64 FLASH_D=64 FLASH_DV=16 FLASH_QK_M2=1)
    target_compile_options(${_direct_target} PRIVATE
        -fno-vectorize -fno-slp-vectorize)

    add_custom_target(generate-fp16-flashattn-key64-data
        DEPENDS "${_key64_header}")
    add_snitch_test(fp16-flashattn-key64-single_TQ64_T64_D64_DV64
        "${CMAKE_CURRENT_SOURCE_DIR}/main_tq64.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/kernel/flashattn_tq64.c")
    set(_key64_target
        "test-${SNITCH_TEST_PREFIX}fp16-flashattn-key64-single_TQ64_T64_D64_DV64")
    add_dependencies(${_key64_target} generate-fp16-flashattn-key64-data)
    target_include_directories(${_key64_target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/kernel"
        "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_definitions(${_key64_target} PRIVATE
        DATAHEADER="data/data_tq64_t64_d64_bc64.h"
        TQ=64 T_SIZE=64 D_SIZE=64 DV_SIZE=64 BC=64 NT=1
        FLASH_PV_ZERO_SEED=1 FLASH_SINGLE_KEY_BLOCK=1
        FLASH_SINGLE_QUAD=1 FLASH_RECIP2=1)
    target_compile_options(${_key64_target} PRIVATE
        -fno-vectorize -fno-slp-vectorize)

    add_custom_target(generate-fp16-flashattn-global-max4-data
        DEPENDS "${_flash_header}")
    add_snitch_test(fp16-flashattn-global-max4_TQ64_T256_D64_DV64
        "${CMAKE_CURRENT_SOURCE_DIR}/main_tq64.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/kernel/flashattn_tq64.c")
    set(_flash_target
        "test-${SNITCH_TEST_PREFIX}fp16-flashattn-global-max4_TQ64_T256_D64_DV64")
    add_dependencies(${_flash_target}
        generate-fp16-flashattn-global-max4-data)
    target_include_directories(${_flash_target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/kernel"
        "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_definitions(${_flash_target} PRIVATE
        DATAHEADER="data/data_tq64_t256_d64_bc64.h"
        TQ=64 T_SIZE=256 D_SIZE=64 DV_SIZE=64 BC=64 NT=4
        FLASH_PV_ZERO_SEED=1 FLASH_RECIP2=1)
    target_compile_options(${_flash_target} PRIVATE
        -fno-vectorize -fno-slp-vectorize)
endif()
