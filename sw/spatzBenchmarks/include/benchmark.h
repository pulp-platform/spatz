// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <snrt.h>
#include <stddef.h>

#include "printf.h"

size_t benchmark_get_cycle();

void start_kernel();
void stop_kernel();

// Write a free-form benchmark phase marker (observed/timestamped by the TB).
void benchmark_mark(uint32_t value);
