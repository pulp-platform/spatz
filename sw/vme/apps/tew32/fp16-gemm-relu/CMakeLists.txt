# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
# FP16 GeMM -> ReLU on Spatz VFU + OPE (O-POPE P5 shape C=128, T=64, COUT=192)

set(_src "${CMAKE_CURRENT_SOURCE_DIR}")
set(_bench_c "${_src}/../../../../spatzBenchmarks/benchmark/benchmark.c")
file(GLOB _gen_scripts "${_src}/script/*.py" "${_src}/../../script/tf/*.py")

set(_hdr_b0 "${CMAKE_CURRENT_BINARY_DIR}/data/data_c128_t64_o192_b0.h")
add_custom_command(OUTPUT "${_hdr_b0}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/../../script/tf/gen_data.py" --output "${_hdr_b0}" --bias 0
  DEPENDS ${_gen_scripts} VERBATIM)
add_custom_target(generate-fp16-gemm-relu-b0 DEPENDS "${_hdr_b0}")

set(_hdr_b1 "${CMAKE_CURRENT_BINARY_DIR}/data/data_c128_t64_o192_b1.h")
add_custom_command(OUTPUT "${_hdr_b1}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/../../script/tf/gen_data.py" --output "${_hdr_b1}" --bias 1
  DEPENDS ${_gen_scripts} VERBATIM)
add_custom_target(generate-fp16-gemm-relu-b1 DEPENDS "${_hdr_b1}")

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  add_snitch_test(fp16-matmul-relu_C128_T64_O192 "${_src}/main.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-matmul-relu_C128_T64_O192 generate-fp16-gemm-relu-b0)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-matmul-relu_C128_T64_O192 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-matmul-relu_C128_T64_O192 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-matmul-relu_C128_T64_O192 PRIVATE DATAHEADER="data/data_c128_t64_o192_b0.h" P6_KERNEL=1)

  add_snitch_test(fp16-gemm-relu_C128_T64_O192 "${_src}/main.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-gemm-relu_C128_T64_O192 generate-fp16-gemm-relu-b1)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-gemm-relu_C128_T64_O192 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-gemm-relu_C128_T64_O192 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-gemm-relu_C128_T64_O192 PRIVATE DATAHEADER="data/data_c128_t64_o192_b1.h" P6_KERNEL=1)

  add_snitch_test(fp16-matmul-relu-ubench "${_src}/ubench.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-matmul-relu-ubench generate-fp16-gemm-relu-b0)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-matmul-relu-ubench PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-matmul-relu-ubench PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-matmul-relu-ubench PRIVATE DATAHEADER="data/data_c128_t64_o192_b0.h" P6_KERNEL=1)

  add_snitch_test(fp16-matmul-relu-ubench2 "${_src}/ubench2.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-matmul-relu-ubench2 generate-fp16-gemm-relu-b0)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-matmul-relu-ubench2 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-matmul-relu-ubench2 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-matmul-relu-ubench2 PRIVATE DATAHEADER="data/data_c128_t64_o192_b0.h" P6_KERNEL=1)

endif()
