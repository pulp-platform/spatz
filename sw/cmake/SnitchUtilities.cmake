# Copyright 2020 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

set(SNITCH_RUNTIME "snRuntime-cluster" CACHE STRING "Target name of the snRuntime flavor to link against")
set(SNITCH_SIMULATOR "" CACHE PATH "Command to run a binary in an RTL simulation")
set(SIMULATOR_TIMEOUT "1800" CACHE STRING "Timeout when running tests on RTL simulation")
set(SPIKE_DASM "spike-dasm" CACHE PATH "Path to the spike-dasm for generating traces")
set(LLVM_PATH "/home/spatz" CACH PATH "Path to the LLVM RISCV installation")
set(GCC_PATH "/home/spatz" CACHE PATH "Path to the GCC RISCV installation")
set(RUNTIME_TRACE OFF CACHE BOOL "Enable runtime trace output")
set(RUNTIME_PRINT OFF CACHE BOOL "Enable runtime debug output with printfs")
set(SNITCH_TEST_PREFIX "")
if (SNITCH_SIMULATOR)
    message(STATUS "Using RTL simulator: ${SNITCH_SIMULATOR}")
endif()
message(STATUS "Using runtime: ${SNITCH_RUNTIME}")

# Toolchain to use
set(CMAKE_TOOLCHAIN_FILE toolchain-llvm CACHE STRING "Toolchain to use")

# Select to build the tests
set(BUILD_TESTS OFF CACHE BOOL "Build test executables")

macro(add_snitch_library name)
    add_library(${ARGV})
    add_custom_command(
        TARGET ${name}
        POST_BUILD
        COMMAND ${CMAKE_OBJDUMP} -dhS $<TARGET_FILE:${name}> > $<TARGET_FILE:${name}>.s)
endmacro()

macro(add_snitch_executable name)
    add_executable(${ARGV})
    target_link_libraries(${name} ${SNITCH_RUNTIME})
    target_link_options(${name} PRIVATE "SHELL:-T ${LINKER_SCRIPT}")
    add_custom_command(
        TARGET ${name}
        POST_BUILD
        COMMAND ${CMAKE_OBJDUMP} -dhS $<TARGET_FILE:${name}> > $<TARGET_FILE:${name}>.s)
    # Run target for RTL simulator
    if (SNITCH_SIMULATOR AND SNITCH_RUNTIME STREQUAL "snRuntime-cluster")
        add_custom_target(run-rtl-${name}
            COMMAND ${SNITCH_SIMULATOR} $<TARGET_FILE:${name}>
            COMMAND for f in logs/trace_hart_*.dasm\; do ${SPIKE_DASM} < $$f | ${PYTHON} ${SNRUNTIME_SRC_DIR}/../../util/gen_trace.py > $$\(echo $$f | sed 's/\\.dasm/\\.txt/'\)\; done
            DEPENDS $<TARGET_FILE:${name}>)
    endif()
endmacro()

macro(add_snitch_test_executable name)
    if (BUILD_TESTS)
        add_snitch_executable(test-${SNITCH_TEST_PREFIX}${name} ${ARGN})
    endif()
endmacro()

macro(add_snitch_raw_test_rtl test_name target_name)
  add_test(NAME ${SNITCH_TEST_PREFIX}rtl-${test_name} COMMAND ${SNITCH_SIMULATOR} $<TARGET_FILE:${target_name}>)
  set_property(TEST ${SNITCH_TEST_PREFIX}rtl-${test_name}
    PROPERTY LABELS ${SNITCH_TEST_PREFIX})
  set_tests_properties(${SNITCH_TEST_PREFIX}rtl-${test_name} PROPERTIES TIMEOUT ${SIMULATOR_TIMEOUT})
  set_tests_properties(${SNITCH_TEST_PREFIX}rtl-${test_name} PROPERTIES PASS_REGULAR_EXPRESSION "SUCCESS;PASS")
  set_tests_properties(${SNITCH_TEST_PREFIX}rtl-${test_name} PROPERTIES FAIL_REGULAR_EXPRESSION "FAILURE")
endmacro()

macro(add_snitch_test_rtl name)
  add_snitch_raw_test_rtl(${SNITCH_TEST_PREFIX}rtl-${name} test-${SNITCH_TEST_PREFIX}${name})
endmacro()

macro(add_snitch_test name)
    if (BUILD_TESTS)
        message(STATUS "Adding test: ${name}")
        add_snitch_test_executable(${ARGV})
        add_snitch_test_rtl(${name})
    endif()
endmacro()
