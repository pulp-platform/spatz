# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
# FP16 GeMM -> column softmax (+ cross-entropy) on VME (O-POPE P6 / P7: C=128, T=64, V=192)

set(_src "${CMAKE_CURRENT_SOURCE_DIR}")
set(_bench_c "${_src}/../../../../spatzBenchmarks/benchmark/benchmark.c")
file(GLOB _gen_scripts "${_src}/script/*.py" "${_src}/../../script/tf/*.py")

set(_hdr_p6 "${CMAKE_CURRENT_BINARY_DIR}/data/data_p6.h")
add_custom_command(OUTPUT "${_hdr_p6}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/../../script/tf/gen_data.py" --output "${_hdr_p6}" --kernel p6
  DEPENDS ${_gen_scripts} VERBATIM)
add_custom_target(generate-fp16-gemm-softmax-ce-p6 DEPENDS "${_hdr_p6}")

set(_hdr_p7 "${CMAKE_CURRENT_BINARY_DIR}/data/data_p7.h")
add_custom_command(OUTPUT "${_hdr_p7}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/../../script/tf/gen_data.py" --output "${_hdr_p7}" --kernel p7
  DEPENDS ${_gen_scripts} VERBATIM)
add_custom_target(generate-fp16-gemm-softmax-ce-p7 DEPENDS "${_hdr_p7}")

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  add_snitch_test(fp16-gemm-softmax_C128_T64_V192 "${_src}/main_softmax.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-gemm-softmax_C128_T64_V192 generate-fp16-gemm-softmax-ce-p6)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-gemm-softmax_C128_T64_V192 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-gemm-softmax_C128_T64_V192 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-gemm-softmax_C128_T64_V192 PRIVATE DATAHEADER="data/data_p6.h" P6_KERNEL=1)

  add_snitch_test(fp16-gemm-softmax-ce_C128_T64_V192 "${_src}/main_softmax_ce.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-gemm-softmax-ce_C128_T64_V192 generate-fp16-gemm-softmax-ce-p7)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-gemm-softmax-ce_C128_T64_V192 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-gemm-softmax-ce_C128_T64_V192 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-gemm-softmax-ce_C128_T64_V192 PRIVATE DATAHEADER="data/data_p7.h" P6_KERNEL=1)

endif()
