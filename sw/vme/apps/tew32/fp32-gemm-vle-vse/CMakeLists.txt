find_package(Python3 COMPONENTS Interpreter REQUIRED)

option(VME_GEMM_SINGLE_CORE
       "Run general VME GEMM kernels on one compute core" OFF)

set(VME_GEMM_SHAPES
    "64x64x64;64x64x128;64x128x64;64x128x128;128x64x64;128x64x128;128x128x64;128x128x128" CACHE STRING
    "Semicolon-separated MxNxK shapes for the general FP32 GEMM tests")

set(_vme_gemm_build_shapes ${VME_GEMM_SHAPES})
list(APPEND _vme_gemm_build_shapes
     "13x13x13" "13x23x13" "23x13x13" "23x23x13"
     "13x45x7" "13x55x7" "23x45x7" "23x55x7"
     "45x13x8" "45x23x8" "55x13x8" "55x23x8"
     "45x45x9" "45x55x9" "55x45x9" "55x55x9"
     "77x13x13" "77x23x13" "87x13x13" "87x23x13"
     "77x45x9" "77x55x9" "87x45x9" "87x55x9"
     "77x77x77" "77x87x77" "87x77x77" "87x87x77"
     "77x109x71" "77x119x71" "87x109x71" "87x119x71"
     "109x77x72" "109x87x72" "119x77x72" "119x87x72"
     "109x109x73" "109x119x73" "119x109x73" "119x119x73"
     "141x77x77" "141x87x77" "151x77x77" "151x87x77"
     "141x109x73" "141x119x73" "151x109x73" "151x119x73"
     "17x53x95" "64x64x256" "64x96x128" "64x128x192"
     "724x256x192")
set(_vme_gemm_normalized_shapes)
foreach(_shape IN LISTS _vme_gemm_build_shapes)
    string(STRIP "${_shape}" _shape)
    list(APPEND _vme_gemm_normalized_shapes "${_shape}")
endforeach()
set(_vme_gemm_build_shapes ${_vme_gemm_normalized_shapes})
list(REMOVE_DUPLICATES _vme_gemm_build_shapes)

foreach(_shape IN LISTS _vme_gemm_build_shapes)
    if(NOT _shape MATCHES "^([0-9]+)x([0-9]+)x([0-9]+)$")
        message(FATAL_ERROR "Invalid GEMM shape '${_shape}', expected MxNxK")
    endif()
    set(_m ${CMAKE_MATCH_1})
    set(_n ${CMAKE_MATCH_2})
    set(_k ${CMAKE_MATCH_3})
    set(_target fp32-gemm-vle-vse_M${_m}_N${_n}_K${_k})
    set(_generated_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
    set(_header ${_generated_dir}/data/data_${_m}_${_n}_${_k}.h)

    add_custom_command(
        OUTPUT ${_header}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_generated_dir}/data
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/script/gen_data.py
                --m ${_m} --n ${_n} --k ${_k} --output ${_header}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/script/gen_data.py
                ${CMAKE_CURRENT_SOURCE_DIR}/data/layer.h
        VERBATIM)

    if(BUILD_TESTS)
        add_custom_target(generate-${_target} DEPENDS ${_header})
        add_snitch_test(${_target} main.c)
        add_dependencies(test-${SNITCH_TEST_PREFIX}${_target} generate-${_target})
        target_include_directories(test-${SNITCH_TEST_PREFIX}${_target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/data
            ${_generated_dir})
        target_compile_definitions(test-${SNITCH_TEST_PREFIX}${_target} PRIVATE
            DATAHEADER="data/data_${_m}_${_n}_${_k}.h"
            KERNELHEADER="kernel/gemm.c")
        if(VME_GEMM_SINGLE_CORE)
            target_compile_definitions(test-${SNITCH_TEST_PREFIX}${_target} PRIVATE
                SPATZ_GEMM_SINGLE_CORE=1)
        endif()
    endif()
endforeach()
