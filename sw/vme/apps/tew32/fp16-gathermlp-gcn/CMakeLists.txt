# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
# FP16 GCN layer (O-POPE gathermlp gcn: N=127, F=64, L=8, Dout=128)

set(_src "${CMAKE_CURRENT_SOURCE_DIR}")
set(_bench_c "${_src}/../../../../spatzBenchmarks/benchmark/benchmark.c")
file(GLOB _gen_scripts "${_src}/script/*.py" "${_src}/../../script/tf/*.py")

set(_hdr_gcn "${CMAKE_CURRENT_BINARY_DIR}/data/data_gmgcn.h")
add_custom_command(OUTPUT "${_hdr_gcn}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/script/gen_gathermlp_gcn.py" --data "${_hdr_gcn}" --kernel "${CMAKE_CURRENT_BINARY_DIR}/gmgcn_kernel_regenerated.c"
  DEPENDS ${_gen_scripts} VERBATIM)
add_custom_target(generate-fp16-gathermlp-gcn-gcn DEPENDS "${_hdr_gcn}")

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  add_snitch_test(fp16-gathermlp-gcn_N127_F64_O128 "${_src}/main.c" "${_bench_c}")
  add_dependencies(test-${SNITCH_TEST_PREFIX}fp16-gathermlp-gcn_N127_F64_O128 generate-fp16-gathermlp-gcn-gcn)
  target_include_directories(test-${SNITCH_TEST_PREFIX}fp16-gathermlp-gcn_N127_F64_O128 PRIVATE "${_src}" "${_src}/kernel"
    "${CMAKE_CURRENT_BINARY_DIR}" "${_src}/../../../../spatzBenchmarks/include")
  target_compile_options(test-${SNITCH_TEST_PREFIX}fp16-gathermlp-gcn_N127_F64_O128 PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}fp16-gathermlp-gcn_N127_F64_O128 PRIVATE DATAHEADER="data/data_gmgcn.h" P6_KERNEL=1)

endif()
