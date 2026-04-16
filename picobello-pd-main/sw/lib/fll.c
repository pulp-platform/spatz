// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Lorenzo Leone <lleone@iis.ee.ethz.ch>
// Tim Fischer <fischeti@iis.ee.ethz.ch>

#include <stdint.h>
#include "fll.h"
#include "pb_addrmap.h"

// INFO(fischeti): This is not tested yet!!!

volatile fll_t *fll = &picobello_addrmap.cheshire_internal.fll;

// Set the FLL CFG REG 1 to the desired reset value: 0xC958
void init_fll() {
  fll->config1.f.mult = 0xC958; // Set the multiplication factor to 0xC958
}

// Set the Multiplication and Divider values
void set_fll_freq(uint32_t mult, uint32_t div) {
  fll->config1.f.mult = mult;
  fll->config1.f.div = div;
}
