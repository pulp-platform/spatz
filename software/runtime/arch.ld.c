// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

/* This file will get processed by the precompiler to expand all macros. */

MEMORY {
  l2 : ORIGIN = 0x80000000, LENGTH = 0x02000000
}

SECTIONS {
  __l2_start = ORIGIN(l2);
  __l2_end = ORIGIN(l2) + LENGTH(l2);

  eoc_reg                = 0x40000000;
  fake_uart              = 0xC0000000;
}
