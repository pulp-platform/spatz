// Copyright 2021 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

// Author: Diyou Shen, ETH Zurich

#pragma once

#include <snrt.h>
#include <stdint.h>

// Phase 1 kernel: one butterfly stage on one section, fully in L1
// l1_src layout: [Re_upper: nfft_sec][Re_lower: nfft_sec][Im_upper: nfft_sec][Im_lower: nfft_sec]
// l1_dst layout: same as l1_src
// l1_twi layout: [Re_twi: nfft_sec][Im_twi: nfft_sec]
void fft_p1_kernel(const double *l1_src, double *l1_dst,
                   const double *l1_twi, const unsigned int nfft_sec);

// Phase 2 kernel: full sub-FFT (log2_sec stages) on one section in L1
// Final stage uses vsse64 strided store into global output array
void fft_p2_kernel(double *s, double *buf, const double *twi, double *out,
                   const uint16_t *seq_idx,
                   const unsigned int nfft_sec, const unsigned int nfft,
                   const unsigned int log2_sec,  const unsigned int stride,
                   const unsigned int log2_S,    const unsigned int ntwi);
