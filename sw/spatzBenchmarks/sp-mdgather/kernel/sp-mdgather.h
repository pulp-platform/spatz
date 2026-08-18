// Copyright 2026 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

void mdgather_vlxblk(float *forces, const float *coords,
                     const uint16_t *pairlist, const unsigned int NC,
                     const unsigned int LIST, const float cut2);

void mdgather_rvv(float *forces, const float *coords,
                  const uint16_t *pairlist_exp, const unsigned int NC,
                  const unsigned int LIST, const float cut2);
