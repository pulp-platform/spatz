find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(VME_GEMM_SHAPES
    "64x64x64;64x64x32" CACHE STRING
    "Semicolon-separated MxNxK shapes for the general FP32 GEMM tests")

set(_vme_gemm_build_shapes ${VME_GEMM_SHAPES})
list(APPEND _vme_gemm_build_shapes
     "64x64x57" "64x64x37"
     "64x53x64" "53x64x64" "53x53x57" "64x53x57" "53x64x57"
     "16x16x64" "15x15x64" "16x32x64" "32x16x64" "15x31x64"
     "31x16x64" "15x31x57" "16x48x64" "15x48x64" 
     "15x47x64" "47x15x64" "47x15x57")
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
    set(_target fp32-gemm-vse_M${_m}_N${_n}_K${_k})
    set(_generated_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
    set(_header ${_generated_dir}/data/data_${_m}_${_n}_${_k}.h)

    add_custom_command(
        OUTPUT ${_header}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_generated_dir}/data
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/script/gen_data.py
                --m ${_m} --n ${_n} --k ${_k} --output ${_header}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/script/gen_data.py
                ${CMAKE_CURRENT_SOURCE_DIR}/../fp32-gemm-vtse/script/gen_data.py
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
    endif()
endforeach()
