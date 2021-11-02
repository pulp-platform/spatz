/* Copyright 2021 ETH Zurich and University of Bologna. */
/* Licensed under the Apache License, Version 2.0, see LICENSE for details. */
/* SPDX-License-Identifier: Apache-2.0 */

/* Include the file processed by the preprocessor called arch.ld.c */
INCLUDE arch.ld

OUTPUT_ARCH("riscv")
ENTRY(_start)


SECTIONS {
  /* Instructions on L2 */
  .text : {
    *(.text.init)
    *(.text)
    . = ALIGN(0x40);
  } > l2

  /* Data on L2 */
  .data           : {
    . = ALIGN(0x10);
    *(.data)
  } > l2
  .rodata : {
    *(.rodata .rodata.* .gnu.linkonce.r.*)
  } > l2
  .rodata1 : {
    *(.rodata1)
  } > l2
  .sdata2 : {
    *(.sdata2 .sdata2.* .gnu.linkonce.s2.*)
  } > l2
  .sdata : {
    __global_pointer$ = . + 0x800;
    *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2) *(.srodata .srodata.*)
    *(.sdata .sdata.* .gnu.linkonce.s.*)
  } > l2

  .bss : {
    __bss_start = .;
    *(.bss)
    *(.sbss2 .sbss2.* .gnu.linkonce.sb2.*);
    __bss_end = .;
  } > l2

  .l2 : {
    . = ALIGN(0x10);
    *(.l2)
    __l2_alloc_base = ALIGN(0x10);
  } > l2

  .comment : {
    *(.comment)
  } > l2
}
