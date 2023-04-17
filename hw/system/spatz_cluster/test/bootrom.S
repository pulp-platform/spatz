# Copyright 2020 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

#include <spatz_cluster_peripheral.h>

  .global _start
  .global BOOTDATA

_start:
  la      t1, exception
  csrw    mtvec, t1
  csrr    a0, mhartid
  la      a1, BOOTDATA
  // Wait for the wakeup interrupt
  wfi

  // Load the start address of the TCDM
  lw      t2, 12(a1)
  // Load the size of the TCDM
  lw      t3, 16(a1)
  // Final address of the TCDM
  add     t2, t2, t3

  // Find the entry point address
  addi    t2, t2, SPATZ_CLUSTER_PERIPHERAL_CLUSTER_BOOT_CONTROL_REG_OFFSET
  addi    t2, t2, SPATZ_CLUSTER_PERIPHERAL_CLUSTER_BOOT_CONTROL_ENTRY_POINT_OFFSET

  // Load the entry point
  lw      t2, 0(t2)

  // Jump to the entry point
  jr      t2

exception:
  wfi
  j     exception

.pushsection .boot_section,"aw",@progbits;
entry_addr:
  .word exception
