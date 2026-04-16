// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "fll.h"
#include "pb_addrmap.h"
#include <stdint.h>

// INFO(fischeti): This is not tested yet!!!

int main() {

  // Set output frequency of the FLL
  init_fll();
  set_fll_freq(0x1000, 2);

  return 0;
}
