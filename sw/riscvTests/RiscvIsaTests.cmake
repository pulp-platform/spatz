# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Upstream RISC-V ISA tests (github.com/riscv-software-src/riscv-tests).
#
# The unmodified upstream .S tests build against the stock riscv-tests `env/p`
# environment (its own _start, trap handler, hart!=0 parking and tohost pass/fail)
# and run as RTL ctests via ${SNITCH_SIMULATOR} -- hence no custom env and no
# snRuntime linkage, unlike the sibling vector .c tests.
#
# Modelled on snitch_cluster's sw/riscv-tests/riscv-tests.mk.

if (BUILD_TESTS)
  include(FetchContent)
  # Patch `move` to mask spatz's non-standard fmode out of fcsr WPRI bits.
  set(RVTESTS_PATCH ${CMAKE_CURRENT_LIST_DIR}/patches/move-fcsr-wpri.patch)
  FetchContent_Declare(riscv_tests
    GIT_REPOSITORY https://github.com/riscv-software-src/riscv-tests.git
    GIT_TAG        34e6b6d1e7936b526075432fb730d89148623484
    GIT_SUBMODULES_RECURSE TRUE
    PATCH_COMMAND  sh -c "git apply --reverse --check '${RVTESTS_PATCH}' 2>/dev/null || git apply '${RVTESTS_PATCH}'")
  FetchContent_MakeAvailable(riscv_tests)
  set(RVTESTS_ROOT ${riscv_tests_SOURCE_DIR})
  set(RVTESTS_ISA  ${RVTESTS_ROOT}/isa)
  set(RVTESTS_ENV  ${RVTESTS_ROOT}/env/p)
endif()

if (BUILD_TESTS AND EXISTS ${RVTESTS_ENV}/riscv_test.h)
  set(RVTESTS_EXTS rv32ui rv32um rv32uf rv32ud)

  # Target capabilities. When OFF, the matching tests still build and run but are
  # marked WILL_FAIL. Used for unimplemented instructions (e.g. fdiv).
  option(RVTESTS_HAS_DIVSQRT    "Target implements hardware FP divide/sqrt"    OFF)
  option(RVTESTS_HAS_MISALIGNED "Target handles misaligned data access in HW"  OFF)

  function(add_riscv_isa_test ext name)
    set(tgt test-${SNITCH_TEST_PREFIX}${ext}-p-${name})
    add_executable(${tgt} ${RVTESTS_ISA}/${ext}/${name}.S)
    set_target_properties(${tgt} PROPERTIES LINKER_LANGUAGE C)
    target_include_directories(${tgt} PRIVATE
      ${RVTESTS_ENV} ${RVTESTS_ISA}/macros/scalar)
    # env/p provides its own crt/_start and linker script; do NOT link snRuntime.
    target_link_options(${tgt} PRIVATE "SHELL:-T ${RVTESTS_ENV}/link.ld")
    add_custom_command(TARGET ${tgt} POST_BUILD
      COMMAND ${CMAKE_OBJDUMP} -dhS $<TARGET_FILE:${tgt}> > $<TARGET_FILE:${tgt}>.s)
    add_snitch_raw_test_rtl(${SNITCH_TEST_PREFIX}rtl-${ext}-p-${name} ${tgt})
    if ((NOT RVTESTS_HAS_DIVSQRT AND name STREQUAL "fdiv") OR
        (NOT RVTESTS_HAS_MISALIGNED AND name STREQUAL "ma_data"))
      set_tests_properties(${SNITCH_TEST_PREFIX}rtl-${ext}-p-${name}
        PROPERTIES WILL_FAIL TRUE)
    endif()
  endfunction()

  foreach(ext ${RVTESTS_EXTS})
    set(frag ${RVTESTS_ISA}/${ext}/Makefrag)
    if (NOT EXISTS ${frag})
      message(WARNING "riscv-tests: missing ${frag}; skipping ${ext}")
      continue()
    endif()
    # Parse the upstream "<ext>_sc_tests = ..." list rather than globbing, to
    # honour its selection (e.g. rv32ud omits the unfinished move/structural tests).
    file(STRINGS ${frag} fraglines)
    set(collect OFF)
    set(names "")
    foreach(line ${fraglines})
      if ("${line}" MATCHES "^${ext}_sc_tests[ \t]*=(.*)$")
        set(rest "${CMAKE_MATCH_1}")
        set(collect ON)
      elseif (collect)
        set(rest "${line}")
      else()
        set(rest "")
      endif()
      if (collect)
        set(names "${names} ${rest}")
        if (NOT "${line}" MATCHES "\\\\[ \t]*$")  # no trailing backslash -> last line
          set(collect OFF)
        endif()
      endif()
    endforeach()
    # Extract bare test identifiers (ignores whitespace, comments, backslashes).
    string(REGEX MATCHALL "[A-Za-z0-9_]+" namelist "${names}")
    foreach(name ${namelist})
      add_riscv_isa_test(${ext} ${name})
    endforeach()
    list(LENGTH namelist n)
    message(STATUS "riscv-tests: ${ext} -> ${n} test(s)")
  endforeach()
endif()
