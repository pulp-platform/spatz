# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Author: Pei-Yu Lin, ETH Zürich

cmake_minimum_required(VERSION 3.13)

enable_testing()
set(SNITCH_TEST_PREFIX vme-)

# Match the enabled TEW modes in the OPE configuration.
set(VME_TEST_ENABLE_TEW8 0 CACHE STRING "Whether OPE supports TEW8")
set(VME_TEST_ENABLE_TEW16 0 CACHE STRING "Whether OPE supports TEW16")
set(VME_TEST_ENABLE_TEW32 1 CACHE STRING "Whether OPE supports TEW32")
set(VME_TEST_ENABLE_FMA32 0 CACHE STRING "Whether the OPE instantiates FP32 FMA")

add_snitch_test(msetmtype msetmtype.c)
add_snitch_test(msetmtypei msetmtypei.c)
add_snitch_test(msettn msettn.c)
add_snitch_test(msettm msettm.c)
add_snitch_test(msettk msettk.c)
# add_snitch_test(vtmm vtmm.c) # Integer matrix MAC is not implemented.
add_snitch_test(vtfmm vtfmm.c)
add_snitch_test(vtle vtle.c)
add_snitch_test(vtse vtse.c)
add_snitch_test(vtmv vtmv.c)
add_snitch_test(vtzero vtzero.c)

if(BUILD_TESTS)
    foreach(name msetmtype msetmtypei msettn msettm msettk vtfmm vtle vtse vtmv vtzero)
        target_compile_definitions(test-vme-${name} PRIVATE
            VME_TEST_ENABLE_TEW8=${VME_TEST_ENABLE_TEW8}
            VME_TEST_ENABLE_TEW16=${VME_TEST_ENABLE_TEW16}
            VME_TEST_ENABLE_TEW32=${VME_TEST_ENABLE_TEW32}
            VME_TEST_ENABLE_FMA32=${VME_TEST_ENABLE_FMA32})
    endforeach()
endif()
