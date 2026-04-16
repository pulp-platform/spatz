// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Lorenzo Leone <lleone@iis.ee.ethz.ch>

#ifndef FLL_H
#define FLL_H
#include <stdint.h>

void init_fll();
void set_fll_freq(uint32_t mult, uint32_t div);

#endif // FLL_H
