# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
# FP16 GeMM -> GELU (O-POPE P2: C=128, T=64, COUT=192)

set(_src "${CMAKE_CURRENT_SOURCE_DIR}")
set(_bench_c "${_src}/../../../../spatzBenchmarks/benchmark/benchmark.c")
file(GLOB _gen_scripts "${_src}/script/*.py" "${_src}/../../script/tf/*.py")

set(_hdr_p2f "${CMAKE_CURRENT_BINARY_DIR}/data/data_p2f.h")
add_custom_command(OUTPUT "${_hdr_p2f}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/script/gen_gemm_gelu.py" --data "${_hdr_p2f}" --kernel "${CMAKE_CURRENT_BINARY_DIR}/p2f_kernel_regenerated.c"
  DEPENDS ${_gen_scripts} VERBATIM)
add_custom_target(generate-fp16-gemm-gelu-p2f DEPENDS "${_hdr_p2f}")

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  add_snitch_test(fp16-gemm-gelu_C128_T64_O192 "${_src}/main.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-gemm-gelu_C128_T64_O192 generate-fp16-gemm-gelu-p2f)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-gemm-gelu_C128_T64_O192 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-gemm-gelu_C128_T64_O192 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-gemm-gelu_C128_T64_O192 PRIVATE DATAHEADER="data/data_p2f.h" P6_KERNEL=1)

endif()
