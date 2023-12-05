# Copyright 2020 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Function to determine the initial cluster layout to setup the initial stack.
# See `hw/system/snitch_cluster/test/tb_lib.hh` for details of the bootdata
# layout.

# Must leave a0 untouched.
_snrt_cluster_barrier:
    .globl _snrt_cluster_barrier
    addi      sp, sp, -4
    sw        ra, 0(sp)
    call      _snrt_barrier_reg_ptr
    lw        a0, 0(a0)
    mv        zero, a0
    lw        ra, 0(sp)
    addi      sp, sp, 4
    ret

# Mustn't use any non-temporary registers
# Expecting:
# - a0: device tree ptr ("bootdata" in `tb_lib.hh`)
# a0 = bootdata->hartid_base
_snrt_get_base_hartid:
    .globl _snrt_get_base_hartid
    lw        a0, 16(a0)  # load cluster's hartid offset
    ret
