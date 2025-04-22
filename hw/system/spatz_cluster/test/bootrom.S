// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

#include <spatz_cluster_peripheral.h>
#include <encoding.h>

  .global _start
  .global BOOTDATA

_start:
  // Clear integer registers
  mv x1, x0
  mv x2, x0
  mv x3, x0
  mv x4, x0
  mv x5, x0
  mv x6, x0
  mv x7, x0
  mv x8, x0
  mv x9, x0
  mv x10, x0
  mv x11, x0
  mv x12, x0
  mv x13, x0
  mv x14, x0
  mv x15, x0
  mv x16, x0
  mv x17, x0
  mv x18, x0
  mv x19, x0
  mv x20, x0
  mv x21, x0
  mv x22, x0
  mv x23, x0
  mv x24, x0
  mv x25, x0
  mv x26, x0
  mv x27, x0
  mv x28, x0
  mv x29, x0
  mv x30, x0
  mv x31, x0

  la      t1, dummy_exception
  csrw    mtvec, t1
  la      a1, BOOTDATA

  // Activate MEIP
  li t1, MIP_MEIP
  csrw mie, t1
  // Activate MSIP
  csrr    a2, mie
  ori     a2, a2, 0x8
  csrw    mie, a2

  // Wait for the wakeup interrupt
  wfi

  // Load the start address of the TCDM
  lw      t2, 24(a1)
  // Load the size of the TCDM
  lw      t3, 32(a1)
  // Final address of the TCDM
  add     t2, t2, t3

  // Find the entry point address
  addi    t2, t2, SPATZ_CLUSTER_PERIPHERAL_CLUSTER_BOOT_CONTROL_REG_OFFSET
  addi    t2, t2, SPATZ_CLUSTER_PERIPHERAL_CLUSTER_BOOT_CONTROL_ENTRY_POINT_OFFSET

  // Load the entry point
  lw      t2, 0(t2)

  // Jump to the entry point
  jr      t2

// fallback isr with an additional `wfi` to park the core in case of issues
// during boot. In the bootrom, we should never jump to this; the `wfi` at line 58
// should resume the core and let it execute the next instruction when an
// interrupt or debug request are received.
dummy_exception:
  wfi
  j     dummy_exception

.pushsection .boot_section,"aw",@progbits;
entry_addr:
  .word dummy_exception
