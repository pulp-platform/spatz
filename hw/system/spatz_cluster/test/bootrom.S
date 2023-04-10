# Copyright 2020 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

  .global _start
  .global BOOTDATA

_start:
  la      t1, exception
  csrw    mtvec, t1
  csrr    a0, mhartid
  la      a1, BOOTDATA
  li      t1, 0x80002500
  jr t1

exception:
    wfi
    j     exception

.pushsection .boot_section,"aw",@progbits;
entry_addr:
    .word exception
