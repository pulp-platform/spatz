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
void l1d_commit();
void l1d_init(uint32_t size);
void l1d_flush();
void l1d_wait();
void l1d_spm_config (uint32_t size);
