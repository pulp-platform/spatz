find_package(Python3 COMPONENTS Interpreter REQUIRED)
if(BUILD_TESTS AND "te16" IN_LIST VME_TILE_VARIANT)
  set(header ${CMAKE_CURRENT_BINARY_DIR}/data.h)
  add_custom_command(OUTPUT ${header}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/script/gen_data.py --output ${header}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/script/gen_data.py ${CMAKE_CURRENT_SOURCE_DIR}/../../script/vme_data.py VERBATIM)
  add_custom_target(generate-fp16-gathermlp-dlrm DEPENDS ${header})
  foreach(variant IN ITEMS non-fused fused)
    set(name fp16-gathermlp-dlrm-${variant}_R144_B127_D64_O128)
    add_snitch_test(${name} ${CMAKE_CURRENT_SOURCE_DIR}/main.c)
    set(target test-${SNITCH_TEST_PREFIX}${name})
    add_dependencies(${target} generate-fp16-gathermlp-dlrm)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_compile_definitions(${target} PRIVATE DATAHEADER="${header}")
    if(variant STREQUAL "non-fused")
      target_compile_definitions(${target} PRIVATE APP_FUSED=0)
    else()
      target_compile_definitions(${target} PRIVATE
        GP_FUSED_TAIL=0 GG_INTERLEAVE_GATHER=0 GP_USE_E32_LOAD=0
        GG_BULK_B_TILES=0 GP_PACK_E32_STORE=0)
    endif()
    target_compile_options(${target} PRIVATE -fno-vectorize -fno-slp-vectorize)
  endforeach()
endif()
