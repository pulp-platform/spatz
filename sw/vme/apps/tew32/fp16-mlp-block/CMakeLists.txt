# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
# FP16 MLP block: LN -> GeMM -> GELU -> GeMM + residual (O-POPE P9: C=64, T=64, H=144)

set(_src "${CMAKE_CURRENT_SOURCE_DIR}")
set(_bench_c "${_src}/../../../../spatzBenchmarks/benchmark/benchmark.c")
file(GLOB _gen_scripts "${_src}/script/*.py" "${_src}/../../script/tf/*.py")

set(_hdr_p9 "${CMAKE_CURRENT_BINARY_DIR}/data/data_p9.h")
add_custom_command(OUTPUT "${_hdr_p9}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/script/gen_mlp_block.py" --data "${_hdr_p9}" --kernel "${CMAKE_CURRENT_BINARY_DIR}/p9_kernel_regenerated.c"
  DEPENDS ${_gen_scripts} VERBATIM)
add_custom_target(generate-fp16-mlp-block-p9 DEPENDS "${_hdr_p9}")

set(_hdr_p9f "${CMAKE_CURRENT_BINARY_DIR}/data/data_p9f.h")
add_custom_command(OUTPUT "${_hdr_p9f}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/script/gen_mlp_block_fused.py" --data "${_hdr_p9f}" --kernel "${CMAKE_CURRENT_BINARY_DIR}/p9f_kernel_regenerated.c"
  DEPENDS ${_gen_scripts} VERBATIM)
add_custom_target(generate-fp16-mlp-block-p9f DEPENDS "${_hdr_p9f}")

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  add_snitch_test(fp16-mlp-block_C64_T64_H144 "${_src}/main.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-mlp-block_C64_T64_H144 generate-fp16-mlp-block-p9)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-mlp-block_C64_T64_H144 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-mlp-block_C64_T64_H144 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-mlp-block_C64_T64_H144 PRIVATE DATAHEADER="data/data_p9.h" P6_KERNEL=1)

  add_snitch_test(fp16-mlp-block-fused_C64_T64_H144 "${_src}/main_fused.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-mlp-block-fused_C64_T64_H144 generate-fp16-mlp-block-p9f)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-mlp-block-fused_C64_T64_H144 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-mlp-block-fused_C64_T64_H144 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-mlp-block-fused_C64_T64_H144 PRIVATE DATAHEADER="data/data_p9f.h" P6_KERNEL=1)

endif()
