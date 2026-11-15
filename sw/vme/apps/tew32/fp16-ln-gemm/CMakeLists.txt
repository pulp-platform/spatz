# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
# FP16 LayerNorm -> GeMM (O-POPE P1: C=96, T=64, COUT=176)

set(_src "${CMAKE_CURRENT_SOURCE_DIR}")
set(_bench_c "${_src}/../../../../spatzBenchmarks/benchmark/benchmark.c")
file(GLOB _gen_scripts "${_src}/script/*.py" "${_src}/../../script/tf/*.py")

set(_hdr_p1 "${CMAKE_CURRENT_BINARY_DIR}/data/data_p1f.h")
add_custom_command(OUTPUT "${_hdr_p1}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/script/gen_ln_gemm.py" --data "${_hdr_p1}" --kernel "${CMAKE_CURRENT_BINARY_DIR}/p1_kernel_regenerated.c"
  DEPENDS ${_gen_scripts} VERBATIM)
add_custom_target(generate-fp16-ln-gemm-p1 DEPENDS "${_hdr_p1}")

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  add_snitch_test(fp16-ln-gemm_C96_T64_O176 "${_src}/main.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-ln-gemm_C96_T64_O176 generate-fp16-ln-gemm-p1)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-ln-gemm_C96_T64_O176 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-ln-gemm_C96_T64_O176 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-ln-gemm_C96_T64_O176 PRIVATE DATAHEADER="data/data_p1f.h" P6_KERNEL=1)

endif()
