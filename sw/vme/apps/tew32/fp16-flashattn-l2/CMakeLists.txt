# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
# FP16 FlashAttention Tq64/T256/d64/dv64/Bc64 for the 128 KiB TCDM: the global-max kernel of fp16-flashattn
# (bobo, commit bf7400a) with V kept in L2 and the dead buffers reused (see README.md).

set(_src "${CMAKE_CURRENT_SOURCE_DIR}")
set(_hdr "${CMAKE_CURRENT_BINARY_DIR}/data/data_tq64_t256_d64_bc64.h")
add_custom_command(OUTPUT "${_hdr}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
  COMMAND ${PYTHON} "${_src}/script/gen_data_tq64.py" --output "${_hdr}"
  DEPENDS "${_src}/script/gen_data_tq64.py" "${_src}/../../script/vme_data.py" VERBATIM)
add_custom_target(generate-fp16-flashattn-l2-data DEPENDS "${_hdr}")

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  set(_name fp16-flashattn-l2_TQ64_T256_D64_DV64)
  add_snitch_test(${_name} "${_src}/main_tq64.c" "${_src}/kernel/flashattn_tq64.c")
  set(_t "test-${SNITCH_TEST_PREFIX}${_name}")
  add_dependencies(${_t} generate-fp16-flashattn-l2-data)
  target_include_directories(${_t} PRIVATE "${_src}" "${_src}/kernel" "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_definitions(${_t} PRIVATE
    DATAHEADER="data/data_tq64_t256_d64_bc64.h"
    TQ=64 T_SIZE=256 D_SIZE=64 DV_SIZE=64 BC=64 NT=4
    FLASH_PV_ZERO_SEED=1 FLASH_RECIP2=1 FLASH_V_L2=1)
  target_compile_options(${_t} PRIVATE -fno-vectorize -fno-slp-vectorize -falign-loops=1)
endif()
