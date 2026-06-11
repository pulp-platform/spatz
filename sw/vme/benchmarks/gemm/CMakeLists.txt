# Copyright 2024 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Author: Pei-Yu Lin <peilin@ethz.ch>

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kernel)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/data)

# FP GEMM functional check
# Uses DATAHEADER="data/data_64_64_64.h"
add_vme_fp_test(fp-gemm main_fp_functional_check.c 64 64 64)

# INT GEMM functional check
# Uses DATAHEADER="data/data_int_64_64_64.h"
add_vme_int_test(int-gemm main_int_functional_check.c 64 64 64)