find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(dq_header "${CMAKE_CURRENT_BINARY_DIR}/data_mt4_m128_n64_k128_cbw16.h")
add_custom_command(OUTPUT "${dq_header}"
  COMMAND ${PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/script/gen_mt4x1_data.py"
          --m 128 --output "${dq_header}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/script/gen_mt4x1_data.py"
  VERBATIM)
add_custom_target(generate-fp16-dq-matmul-data DEPENDS "${dq_header}")

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  set(dq_name fp16-dq-matmul-MT8x2_M128_N64_K128)
  add_snitch_test(${dq_name} main.c kernel/dq_matmul_mt8x2.c)
  set(dq_target "test-${SNITCH_TEST_PREFIX}${dq_name}")
  add_dependencies(${dq_target} generate-fp16-dq-matmul-data)
  target_include_directories(${dq_target} PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_options(${dq_target} PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(${dq_target} PRIVATE DATAHEADER="${dq_header}")
endif()
