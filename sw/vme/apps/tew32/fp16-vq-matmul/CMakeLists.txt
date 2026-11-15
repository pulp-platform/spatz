find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(vq_header "${CMAKE_CURRENT_BINARY_DIR}/data/data_m96_n128_k128.h")
add_custom_command(OUTPUT "${vq_header}"
  COMMAND ${PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/script/gen_opope_data.py"
          --config "${CMAKE_CURRENT_SOURCE_DIR}/script/m96_n128_k128.json"
          --output "${vq_header}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/script/gen_opope_data.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/script/m96_n128_k128.json"
  VERBATIM)
add_custom_target(generate-fp16-vq-matmul-data DEPENDS "${vq_header}")

if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  set(vq_name fp16-vq-matmul-MT6x2-serial_M96_N128_K128)
  add_snitch_test(${vq_name} main.c kernel/matmul.c kernel/matmul_mt6x2.c)
  set(vq_target "test-${SNITCH_TEST_PREFIX}${vq_name}")
  add_dependencies(${vq_target} generate-fp16-vq-matmul-data)
  target_include_directories(${vq_target} PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_options(${vq_target} PRIVATE -fno-vectorize -fno-slp-vectorize)
  target_compile_definitions(${vq_target} PRIVATE
      DATAHEADER="data/data_m96_n128_k128.h")
endif()
