cmake_minimum_required(VERSION 3.13)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
    set(CMAKE_TOOLCHAIN_FILE toolchain-gcc CACHE STRING "Toolchain to use")
    project(PowerFP16 LANGUAGES C ASM)
    include(SnitchUtilities)
    add_subdirectory(../../snRuntime snRuntime)
endif()

include_directories(${SNRUNTIME_INCLUDE_DIRS})

add_compile_options(-O3 -g -ffunction-sections)

enable_testing()

set(SNITCH_TEST_PREFIX power-fp16-)

set(POWER_KERNELS
    exp_sw  exp_hw
    cosh_sw cosh_hw
    tanh_sw tanh_hw
    log_sw  log_hw
    rec_sw  rec_hw
    rsqrt_sw rsqrt_hw
    sin_sw  sin_hw
    cos_sw  cos_hw
)

foreach(KERNEL ${POWER_KERNELS})
    foreach(LMUL 1 2 4 8)
        set(TARGET_NAME ${KERNEL}_m${LMUL})
        add_snitch_test(${TARGET_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/${KERNEL}.c)
        target_compile_definitions(test-${SNITCH_TEST_PREFIX}${TARGET_NAME} PUBLIC LMUL_MODE=${LMUL})
    endforeach()
endforeach()
