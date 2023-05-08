// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>

#ifndef __DATASET_H__
#define __DATASET_H__

#include <stdint.h>

uint64_t *Au64;
uint32_t *Au32;
uint16_t *Au16;
uint8_t *Au8;
int64_t *Ai64;
int32_t *Ai32;
int16_t *Ai16;
int8_t *Ai8;
uint64_t *Af64;
uint32_t *Af32;
uint16_t *Af16;

uint64_t *Ru64;
uint32_t *Ru32;
uint16_t *Ru16;
uint8_t *Ru8;
int64_t *Ri64;
int32_t *Ri32;
int16_t *Ri16;
int8_t *Ri8;
uint64_t *Rf64;
uint32_t *Rf32;
uint16_t *Rf16;

uint64_t Xf64;
uint32_t Xf32;
uint16_t Xf16;

#endif // __DATASET__H__
