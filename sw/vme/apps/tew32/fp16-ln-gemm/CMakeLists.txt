# SPDX-License-Identifier: Apache-2.0
set(_dataset "${CMAKE_CURRENT_SOURCE_DIR}/data/data_c96_t64_o176.h")
if(NOT EXISTS "${_dataset}")
  message(FATAL_ERROR "Missing repository-local baseline P1 data: ${_dataset}")
endif()

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/logs")
  foreach(_variant IN ITEMS non-fused fused mt4x1-cache)
    set(_target fp16-ln-gemm-${_variant}_M176_N64_K96)
    add_snitch_test(${_target} "${CMAKE_CURRENT_SOURCE_DIR}/main.c")
    set(_test_target "test-${SNITCH_TEST_PREFIX}${_target}")
    target_sources(${_test_target} PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/kernel/gemm_unit_backend.c")
    target_include_directories(${_test_target} PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/kernel")
    target_compile_definitions(${_test_target} PRIVATE
      DATAHEADER="data/data_c96_t64_o176.h" K_CHUNK=96)
    if(_variant STREQUAL "mt4x1-cache")
      target_compile_definitions(${_test_target} PRIVATE APP_MT4X1_CACHE=1)
    elseif(_variant STREQUAL "non-fused")
      target_compile_definitions(${_test_target} PRIVATE APP_FUSED=0)
    endif()
    target_compile_options(${_test_target} PRIVATE -fno-vectorize -fno-slp-vectorize)
  endforeach()
endif()
