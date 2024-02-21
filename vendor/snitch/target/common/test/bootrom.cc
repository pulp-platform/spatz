// Copyright 2023 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

asm(".global tb_bootrom_start \n"
    ".global tb_bootrom_end \n"
    "tb_bootrom_start: .incbin \"bootrom.bin\" \n"
    "tb_bootrom_end: \n");
