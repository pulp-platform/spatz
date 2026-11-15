# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
# FP16 DLRM embedding-bag + MLP (O-POPE gathermlp dlrm: R=144, D=64, B=127, L=8, Dout=128)

set(_src "${CMAKE_CURRENT_SOURCE_DIR}")
set(_bench_c "${_src}/../../../../spatzBenchmarks/benchmark/benchmark.c")
file(GLOB _gen_scripts "${_src}/script/*.py" "${_src}/../../script/tf/*.py")

set(_hdr_dlrm "${CMAKE_CURRENT_BINARY_DIR}/data/data_gmdlrm.h")
add_custom_command(OUTPUT "${_hdr_dlrm}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/script/gen_gathermlp_dlrm.py" --data "${_hdr_dlrm}" --kernel "${CMAKE_CURRENT_BINARY_DIR}/gmdlrm_kernel_regenerated.c"
  DEPENDS ${_gen_scripts} VERBATIM)
add_custom_target(generate-fp16-gathermlp-dlrm-dlrm DEPENDS "${_hdr_dlrm}")

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  add_snitch_test(fp16-gathermlp-dlrm_R144_D64_B127_O128 "${_src}/main.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-gathermlp-dlrm_R144_D64_B127_O128 generate-fp16-gathermlp-dlrm-dlrm)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-gathermlp-dlrm_R144_D64_B127_O128 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-gathermlp-dlrm_R144_D64_B127_O128 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-gathermlp-dlrm_R144_D64_B127_O128 PRIVATE DATAHEADER="data/data_gmdlrm.h" P6_KERNEL=1)

endif()
