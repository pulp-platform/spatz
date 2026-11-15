# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
# FP16 causal attention head (O-POPE P8: D=64, T=96)

set(_src "${CMAKE_CURRENT_SOURCE_DIR}")
set(_bench_c "${_src}/../../../../spatzBenchmarks/benchmark/benchmark.c")
file(GLOB _gen_scripts "${_src}/script/*.py" "${_src}/../../script/tf/*.py")

set(_hdr_p8 "${CMAKE_CURRENT_BINARY_DIR}/data/data_p8.h")
add_custom_command(OUTPUT "${_hdr_p8}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/script/gen_attention.py" --data "${_hdr_p8}" --kernel "${CMAKE_CURRENT_BINARY_DIR}/p8_kernel_regenerated.c"
  DEPENDS ${_gen_scripts} VERBATIM)
add_custom_target(generate-fp16-attention-head-p8 DEPENDS "${_hdr_p8}")

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  add_snitch_test(fp16-attention_D64_T96 "${_src}/main.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-attention_D64_T96 generate-fp16-attention-head-p8)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-attention_D64_T96 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-attention_D64_T96 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-attention_D64_T96 PRIVATE DATAHEADER="data/data_p8.h" P6_KERNEL=1)

endif()
