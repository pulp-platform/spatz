# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
# FP16 embedding -> LayerNorm -> GeMM (O-POPE P10: C=64, T=64, V=256, COUT=64)

set(_src "${CMAKE_CURRENT_SOURCE_DIR}")
set(_bench_c "${_src}/../../../../spatzBenchmarks/benchmark/benchmark.c")
file(GLOB _gen_scripts "${_src}/script/*.py" "${_src}/../../script/tf/*.py")

set(_hdr_p10 "${CMAKE_CURRENT_BINARY_DIR}/data/data_p10.h")
add_custom_command(OUTPUT "${_hdr_p10}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/script/gen_embed_ln_gemm.py" --data "${_hdr_p10}" --kernel "${CMAKE_CURRENT_BINARY_DIR}/p10_kernel_regenerated.c"
  DEPENDS ${_gen_scripts} VERBATIM)
add_custom_target(generate-fp16-embed-ln-gemm-p10 DEPENDS "${_hdr_p10}")

set(_hdr_p10f "${CMAKE_CURRENT_BINARY_DIR}/data/data_p10f.h")
add_custom_command(OUTPUT "${_hdr_p10f}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/script/gen_embed_ln_gemm_fused.py" --data "${_hdr_p10f}" --kernel "${CMAKE_CURRENT_BINARY_DIR}/p10f_kernel_regenerated.c"
  DEPENDS ${_gen_scripts} VERBATIM)
add_custom_target(generate-fp16-embed-ln-gemm-p10f DEPENDS "${_hdr_p10f}")

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  add_snitch_test(fp16-embed-ln-gemm_C64_T64_V256_O64 "${_src}/main.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-embed-ln-gemm_C64_T64_V256_O64 generate-fp16-embed-ln-gemm-p10)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-embed-ln-gemm_C64_T64_V256_O64 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-embed-ln-gemm_C64_T64_V256_O64 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-embed-ln-gemm_C64_T64_V256_O64 PRIVATE DATAHEADER="data/data_p10.h" P6_KERNEL=1)

  add_snitch_test(fp16-embed-ln-gemm-fused_C64_T64_V256_O64 "${_src}/main_fused.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-embed-ln-gemm-fused_C64_T64_V256_O64 generate-fp16-embed-ln-gemm-p10f)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-embed-ln-gemm-fused_C64_T64_V256_O64 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-embed-ln-gemm-fused_C64_T64_V256_O64 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-embed-ln-gemm-fused_C64_T64_V256_O64 PRIVATE DATAHEADER="data/data_p10f.h" P6_KERNEL=1)

endif()
