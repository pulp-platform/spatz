# Copyright 2020 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

.global _start

_start:
    la      t1, exception
    csrw    mtvec, t1
    csrr    a0, mhartid
    la      a1, boot_data
    lw      t2, entry_addr
    jalr    t2

exception:
    wfi
    j     exception

.pushsection .boot_section,"aw",@progbits;
entry_addr:
    .word exception
boot_data:
