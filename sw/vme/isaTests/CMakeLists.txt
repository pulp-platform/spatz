# Copyright 2024 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Author: Pei-Yu Lin <peilin@ethz.ch>

cmake_minimum_required(VERSION 3.13)

enable_testing()
set(SNITCH_TEST_PREFIX vme-)

add_snitch_test(msetmtype msetmtype.c)
add_snitch_test(msetmtypei msetmtypei.c)
add_snitch_test(msettn msettn.c)
add_snitch_test(msettm msettm.c)
add_snitch_test(msettk msettk.c)
add_snitch_test(vtmm vtmm.c)
add_snitch_test(vtfmm vtfmm.c)
add_snitch_test(vtle vtle.c)
add_snitch_test(vtse vtse.c)
add_snitch_test(vtmv vtmv.c)
add_snitch_test(vtzero vtzero.c)