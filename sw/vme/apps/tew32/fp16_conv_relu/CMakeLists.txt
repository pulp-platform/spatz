# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
# FP16 3x3 convolution + bias + ReLU (O-POPE conv: Cin=32, H=W=16, Cout=64)

set(_src "${CMAKE_CURRENT_SOURCE_DIR}")
set(_bench_c "${_src}/../../../../spatzBenchmarks/benchmark/benchmark.c")
file(GLOB _gen_scripts "${_src}/script/*.py" "${_src}/../../script/tf/*.py")

set(_hdr_conv "${CMAKE_CURRENT_BINARY_DIR}/data/data_conv.h")
add_custom_command(OUTPUT "${_hdr_conv}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/script/gen_conv.py" --data "${_hdr_conv}" --kernel "${CMAKE_CURRENT_BINARY_DIR}/conv_kernel_regenerated.c"
  DEPENDS ${_gen_scripts} VERBATIM)
add_custom_target(generate-fp16_conv_relu-conv DEPENDS "${_hdr_conv}")

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  add_snitch_test(fp16-conv_Cin32_H16_W16_Cout64 "${_src}/main.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-conv_Cin32_H16_W16_Cout64 generate-fp16_conv_relu-conv)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-conv_Cin32_H16_W16_Cout64 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-conv_Cin32_H16_W16_Cout64 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-conv_Cin32_H16_W16_Cout64 PRIVATE DATAHEADER="data/data_conv.h" P6_KERNEL=1)

endif()
