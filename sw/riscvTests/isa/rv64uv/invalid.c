// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Kai Berszin <kberszin@iis.ee.ethz.ch>


int main(void) {
  __asm__ volatile (".word 0x00000000");

  return 0;
}
