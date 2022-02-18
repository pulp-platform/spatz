// Nopyright 2020 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LINENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WAMMANTIES OM NONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Matteo Perotti

/*
  Optimized convolution for Ara
  The code is long only because of:
  1) Special cases related to the first/last 7 rows
  2) Unrolling of the loops to hide the latency of the moves, slides, mem ops
  At the end of the file, you can find the not-unrolled main loop in a comment,
  without the edge-code.
  Algorithm:
  a) Load the next input row
  b) Calculate its contributions to the F = 7 output rows using one column of
  the filter
  c) Slide down the input row by 1, injecting the next input scalar
  element in the tail
  d) Repeat from b), taking the next colum of the filter,
  until the last column is fetched
  d2) Repeat from a) for all the channels
  e) Store the first output row, the one that is complete
  f) Move all the output rows up by one register, to restore the initial
  condition g) Repeat from a)
  Every time a new input row is loaded, a new output row is created.
  The first 6 rows and the last 6 rows do not follow this pattern, thus we wrote
  dedicated code. Because of the unrolling, we counted for this the first and
  last 7 rows, instead of 6
  This algorithm helps in minimizing the data dependencies, as every input rows
  is used To calculate 7 different output rows.
*/

#include "conv3d.h"

#ifndef MATRIX_DIM
#define MATRIX_DIM 32
#endif
#ifndef KERNEL_M
#define KERNEL_M 1
#endif
#define M MATRIX_DIM
#define N MATRIX_DIM
#define C KERNEL_M
#define F 7

void conv3d_CHx7x7(int32_t *o, int32_t *i, int32_t *f) {

  uint32_t block_size_n;

  // Set the vector configuration
  asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(block_size_n) : "r"(N));

  // Slice the matrix into a manageable number of columns n_
  for (uint32_t n = 0; n < N; n += block_size_n) {
    // Set the vector length
    uint32_t n_ = MIN(N - n, block_size_n);

    // Find pointers to the submatrices
    int32_t *i_ = i + n;
    int32_t *o_ = o + n;

    asm volatile("vsetvli zero, %0, e32, m2, ta, ma" ::"r"(n_));

    conv3d_CHx7x7_block(o_, i_, M, f, n_);
  }
}

void conv3d_CHx7x7_block(int32_t *o, int32_t *i, uint32_t num_rows, int32_t *f, uint32_t n_) {

  // Helper variables
  int32_t lwo = N << 2;
  int32_t lwi_pad = (N + F - 1) << 2;

  // Number of elements that separates two adjacent channels
  int32_t ich_len = (M + F - 1) * (N + F - 1);
  int32_t fch_len = F * F;

  int32_t *i_ = i;
  int32_t *i__ = i;

  // Very last column of coefficients
  int32_t fl0, fl1, fl2, fl3, fl4, fl5, fl6;
  // Buffers for coefficients preloading (solve 16-lane starvation problem)
  int32_t f0_buf, f1_buf, f2_buf, f3_buf, f4_buf, f5_buf, f6_buf;

  int32_t *i_slide_ptr_0;
  int32_t *i_slide_ptr_1;
  int32_t *i_slide_ptr_2;
  int32_t *i_slide_ptr_3;

  // Buffer some of the filter coefficients not to lose efficiency after a
  // vector store (CVA6 cannot issue memory operations if there is a pending
  // store!)
  int32_t last_f_column = (C - 1) * fch_len + F - 1;

  fl0 = f[last_f_column + 0 * F];
  fl1 = f[last_f_column + 1 * F];
  fl2 = f[last_f_column + 2 * F];
  fl3 = f[last_f_column + 3 * F];
  fl4 = f[last_f_column + 4 * F];
  fl5 = f[last_f_column + 5 * F];
  fl6 = f[last_f_column + 6 * F];

  ////////////////
  // Row 0 -> 3 //
  ////////////////

  // Loop on the channels
  for (int ch = 0; ch < C; ++ch) {

    // Point to the first element of the channel ch
    i__ = i_ + ch * ich_len;

    // Point to the scalar elements to insert during a slide
    i_slide_ptr_0 = i__ + n_ + 0 * (N + F - 1);
    i_slide_ptr_1 = i__ + n_ + 1 * (N + F - 1);
    i_slide_ptr_2 = i__ + n_ + 2 * (N + F - 1);
    i_slide_ptr_3 = i__ + n_ + 3 * (N + F - 1);

    // Load four input rows belonging to channel ch
    asm volatile("vle32.v v0, (%0); add %0, %0, %1"
                 : "+&r"(i__)
                 : "r"(lwi_pad));
    asm volatile("vle32.v v4, (%0); add %0, %0, %1"
                 : "+&r"(i__)
                 : "r"(lwi_pad));
    asm volatile("vle32.v v8, (%0); add %0, %0, %1"
                 : "+&r"(i__)
                 : "r"(lwi_pad));
    asm volatile("vle32.v v12, (%0); add %0, %0, %1"
                 : "+&r"(i__)
                 : "r"(lwi_pad));

    // Main kernel, unrolled by 2
    // Unrolled because of int32_t buffering
    // With HW renaming, this unroll is not needed
    for (int32_t k = 0; k < F / 2; ++k) {
      // Two base indexes because of the unrolling
      // Point to the first element of the current column (k) of the current
      // channel (ch) of the filter (f)
      int32_t base_idx_0 = (2 * k) + (ch * fch_len);
      // Point to the first element of the current column (k+1) of the current
      // channel (ch) of the filter (f)
      int32_t base_idx_1 = (2 * k + 1) + (ch * fch_len);

      int32_t sld_elem = *i_slide_ptr_0++;

      if ((k | ch) == 0)
        asm volatile("vmul.vx v16, v0, %0" ::"r"(f[0 + base_idx_0]));
      else
        asm volatile("vmacc.vx v16, %0, v0" ::"r"(f[0 + base_idx_0]));
      if ((k | ch) == 0)
        asm volatile("vmul.vx v18, v4, %0" ::"r"(f[0 + base_idx_0]));
      else
        asm volatile("vmacc.vx v18, %0, v4" ::"r"(f[0 + base_idx_0]));
      asm volatile("vslide1down.vx v2, v0, %0" ::"r"(sld_elem));
      sld_elem = *i_slide_ptr_1++;
      asm volatile("vmacc.vx v16, %0, v4" ::"r"(f[7 + base_idx_0]));
      if ((k | ch) == 0)
        asm volatile("vmul.vx v22, v12, %0" ::"r"(f[0 + base_idx_0]));
      else
        asm volatile("vmacc.vx v22, %0, v12" ::"r"(f[0 + base_idx_0]));
      asm volatile("vslide1down.vx v6, v4, %0" ::"r"(sld_elem));
      sld_elem = *i_slide_ptr_2++;
      asm volatile("vmacc.vx v18, %0, v8" ::"r"(f[7 + base_idx_0]));
      asm volatile("vmacc.vx v16, %0, v8" ::"r"(f[14 + base_idx_0]));
      asm volatile("vslide1down.vx v10, v8, %0" ::"r"(sld_elem));
      sld_elem = *i_slide_ptr_3++;
      if ((k | ch) == 0)
        asm volatile("vmul.vx v20, v8, %0" ::"r"(f[0 + base_idx_0]));
      else
        asm volatile("vmacc.vx v20, %0, v8" ::"r"(f[0 + base_idx_0]));
      asm volatile("vmacc.vx v18, %0, v12" ::"r"(f[14 + base_idx_0]));
      asm volatile("vmacc.vx v16, %0, v12" ::"r"(f[21 + base_idx_0]));
      asm volatile("vslide1down.vx v14, v12, %0" ::"r"(sld_elem));
      sld_elem = *i_slide_ptr_0++;
      asm volatile("vmacc.vx v20, %0, v12" ::"r"(f[7 + base_idx_0]));

      asm volatile("vmacc.vx v16, %0, v2" ::"r"(f[0 + base_idx_1]));
      asm volatile("vmacc.vx v18, %0, v6" ::"r"(f[0 + base_idx_1]));
      asm volatile("vslide1down.vx v0, v2, %0" ::"r"(sld_elem));
      sld_elem = *i_slide_ptr_1++;
      asm volatile("vmacc.vx v16, %0, v6" ::"r"(f[7 + base_idx_1]));
      asm volatile("vmacc.vx v18, %0, v10" ::"r"(f[7 + base_idx_1]));
      asm volatile("vmacc.vx v20, %0, v10" ::"r"(f[0 + base_idx_1]));
      asm volatile("vslide1down.vx v4, v6, %0" ::"r"(sld_elem));
      sld_elem = *i_slide_ptr_2++;
      asm volatile("vmacc.vx v16, %0, v10" ::"r"(f[14 + base_idx_1]));
      asm volatile("vmacc.vx v18, %0, v14" ::"r"(f[14 + base_idx_1]));
      asm volatile("vslide1down.vx v8, v10, %0" ::"r"(sld_elem));
      sld_elem = *i_slide_ptr_3++;
      asm volatile("vmacc.vx v22, %0, v14" ::"r"(f[0 + base_idx_1]));
      asm volatile("vmacc.vx v16, %0, v14" ::"r"(f[21 + base_idx_1]));
      asm volatile("vslide1down.vx v12, v14, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v20, %0, v14" ::"r"(f[7 + base_idx_1]));
    }

    int32_t base_idx_0 = (F - 1) + (ch * fch_len);

    // Don't slide during the last iteration
    asm volatile("vmacc.vx v16, %0, v0" ::"r"(f[0 + base_idx_0]));
    asm volatile("vmacc.vx v18, %0, v4" ::"r"(f[0 + base_idx_0]));
    asm volatile("vmacc.vx v22, %0, v12" ::"r"(f[0 + base_idx_0]));
    asm volatile("vmacc.vx v16, %0, v4" ::"r"(f[7 + base_idx_0]));
    asm volatile("vmacc.vx v18, %0, v8" ::"r"(f[7 + base_idx_0]));
    asm volatile("vmacc.vx v20, %0, v8" ::"r"(f[0 + base_idx_0]));
    asm volatile("vmacc.vx v16, %0, v8" ::"r"(f[14 + base_idx_0]));
    asm volatile("vmacc.vx v18, %0, v12" ::"r"(f[14 + base_idx_0]));
    asm volatile("vmacc.vx v20, %0, v12" ::"r"(f[7 + base_idx_0]));
    asm volatile("vmacc.vx v16, %0, v12" ::"r"(f[21 + base_idx_0]));
  }

  // Bump the input ptr
  i_ += 4 * (N + F - 1);

  ////////////////
  // Row 4 -> 6 //
  ////////////////

  // Loop on the channels
  for (int ch = 0; ch < C; ++ch) {

    // Point to the first element of the channel ch
    i__ = i_ + ch * ich_len;

    // Start calculating the next pointers to the elements to be slided in
    i_slide_ptr_0 = i__ + n_ + 0 * (N + F - 1);
    i_slide_ptr_1 = i__ + n_ + 1 * (N + F - 1);
    i_slide_ptr_2 = i__ + n_ + 2 * (N + F - 1);

    asm volatile("vle32.v v2, (%0); add %0, %0, %1"
                 : "+&r"(i__)
                 : "r"(lwi_pad));
    asm volatile("vle32.v v6, (%0); add %0, %0, %1"
                 : "+&r"(i__)
                 : "r"(lwi_pad));
    asm volatile("vle32.v v10, (%0); add %0, %0, %1"
                 : "+&r"(i__)
                 : "r"(lwi_pad));

    // Main kernel, unrolled by 2
    for (int k = 0; k < F / 2; ++k) {
      // Two base indexes because of the unrolling
      // Point to the first element of the current column (k) of the current
      // channel (ch) of the filter (f)
      int32_t base_idx_0 = (2 * k) + (ch * fch_len);
      // Point to the first element of the current column (k+1) of the current
      // channel (ch) of the filter (f)
      int32_t base_idx_1 = (2 * k + 1) + (ch * fch_len);

      int32_t sld_elem = *i_slide_ptr_0++;

      // Unroll 0
      asm volatile("vmacc.vx v16, %0, v2" ::"r"(f[28 + base_idx_0]));
      asm volatile("vmacc.vx v18, %0, v2" ::"r"(f[21 + base_idx_0]));
      asm volatile("vmacc.vx v16, %0, v6" ::"r"(f[35 + base_idx_0]));
      asm volatile("vmacc.vx v18, %0, v6" ::"r"(f[28 + base_idx_0]));
      asm volatile("vmacc.vx v16, %0, v10" ::"r"(f[42 + base_idx_0]));
      asm volatile("vslide1down.vx v0, v2, %0" ::"r"(sld_elem));
      sld_elem = *i_slide_ptr_1++;

      asm volatile("vmacc.vx v18, %0, v10" ::"r"(f[35 + base_idx_0]));
      asm volatile("vslide1down.vx v4, v6, %0" ::"r"(sld_elem));
      sld_elem = *i_slide_ptr_2++;

      asm volatile("vmacc.vx v20, %0, v2" ::"r"(f[14 + base_idx_0]));
      asm volatile("vmacc.vx v20, %0, v6" ::"r"(f[21 + base_idx_0]));
      asm volatile("vmacc.vx v20, %0, v10" ::"r"(f[28 + base_idx_0]));
      asm volatile("vslide1down.vx v8, v10, %0" ::"r"(sld_elem));
      sld_elem = *i_slide_ptr_0++;

      asm volatile("vmacc.vx v22, %0, v2" ::"r"(f[7 + base_idx_0]));
      asm volatile("vmacc.vx v22, %0, v6" ::"r"(f[14 + base_idx_0]));
      asm volatile("vmacc.vx v22, %0, v10" ::"r"(f[21 + base_idx_0]));

      if ((k | ch) == 0)
        asm volatile("vmul.vx v24, v2, %0" ::"r"(f[0 + base_idx_0]));
      else
        asm volatile("vmacc.vx v24, %0, v2" ::"r"(f[0 + base_idx_0]));
      asm volatile("vmacc.vx v24, %0, v6" ::"r"(f[7 + base_idx_0]));
      asm volatile("vmacc.vx v24, %0, v10" ::"r"(f[14 + base_idx_0]));

      if ((k | ch) == 0)
        asm volatile("vmul.vx v26, v6, %0" ::"r"(f[0 + base_idx_0]));
      else
        asm volatile("vmacc.vx v26, %0, v6" ::"r"(f[0 + base_idx_0]));
      asm volatile("vmacc.vx v26, %0, v10" ::"r"(f[7 + base_idx_0]));

      if ((k | ch) == 0)
        asm volatile("vmul.vx v28, v10, %0" ::"r"(f[0 + base_idx_0]));
      else
        asm volatile("vmacc.vx v28, %0, v10" ::"r"(f[0 + base_idx_0]));

      // Unroll 1
      asm volatile("vmacc.vx v16, %0, v0" ::"r"(f[28 + base_idx_1]));
      asm volatile("vmacc.vx v16, %0, v4" ::"r"(f[35 + base_idx_1]));
      asm volatile("vmacc.vx v16, %0, v8" ::"r"(f[42 + base_idx_1]));
      asm volatile("vslide1down.vx v2, v0, %0" ::"r"(sld_elem));
      sld_elem = *i_slide_ptr_1++;

      asm volatile("vmacc.vx v18, %0, v0" ::"r"(f[21 + base_idx_1]));
      asm volatile("vmacc.vx v18, %0, v4" ::"r"(f[28 + base_idx_1]));
      asm volatile("vmacc.vx v18, %0, v8" ::"r"(f[35 + base_idx_1]));
      asm volatile("vslide1down.vx v6, v4, %0" ::"r"(sld_elem));
      sld_elem = *i_slide_ptr_2++;

      asm volatile("vmacc.vx v20, %0, v0" ::"r"(f[14 + base_idx_1]));
      asm volatile("vmacc.vx v20, %0, v4" ::"r"(f[21 + base_idx_1]));
      asm volatile("vmacc.vx v20, %0, v8" ::"r"(f[28 + base_idx_1]));
      asm volatile("vslide1down.vx v10, v8, %0" ::"r"(sld_elem));

      asm volatile("vmacc.vx v22, %0, v0" ::"r"(f[7 + base_idx_1]));
      asm volatile("vmacc.vx v22, %0, v4" ::"r"(f[14 + base_idx_1]));
      asm volatile("vmacc.vx v22, %0, v8" ::"r"(f[21 + base_idx_1]));

      asm volatile("vmacc.vx v24, %0, v0" ::"r"(f[0 + base_idx_1]));
      asm volatile("vmacc.vx v24, %0, v4" ::"r"(f[7 + base_idx_1]));
      asm volatile("vmacc.vx v24, %0, v8" ::"r"(f[14 + base_idx_1]));

      asm volatile("vmacc.vx v26, %0, v4" ::"r"(f[0 + base_idx_1]));
      asm volatile("vmacc.vx v26, %0, v8" ::"r"(f[7 + base_idx_1]));

      asm volatile("vmacc.vx v28, %0, v8" ::"r"(f[0 + base_idx_1]));
    }

    // The very last iterations require mixing the instructions with the store
    // and the moves
    if (ch != C - 1) {
      // Point to the first element of the current column (k) of the current
      // channel (ch) of the filter (f)
      int32_t base_idx_0 = (F - 1) + (ch * fch_len);

      // Don't slide the elements here
      asm volatile("vmacc.vx v16, %0, v2" ::"r"(f[28 + base_idx_0]));
      asm volatile("vmacc.vx v16, %0, v6" ::"r"(f[35 + base_idx_0]));
      asm volatile("vmacc.vx v16, %0, v10" ::"r"(f[42 + base_idx_0]));

      asm volatile("vmacc.vx v18, %0, v2" ::"r"(f[21 + base_idx_0]));
      asm volatile("vmacc.vx v18, %0, v6" ::"r"(f[28 + base_idx_0]));
      asm volatile("vmacc.vx v18, %0, v10" ::"r"(f[35 + base_idx_0]));

      asm volatile("vmacc.vx v20, %0, v2" ::"r"(f[14 + base_idx_0]));
      asm volatile("vmacc.vx v20, %0, v6" ::"r"(f[21 + base_idx_0]));
      asm volatile("vmacc.vx v20, %0, v10" ::"r"(f[28 + base_idx_0]));

      asm volatile("vmacc.vx v22, %0, v2" ::"r"(f[7 + base_idx_0]));
      asm volatile("vmacc.vx v22, %0, v6" ::"r"(f[14 + base_idx_0]));
      asm volatile("vmacc.vx v22, %0, v10" ::"r"(f[21 + base_idx_0]));

      asm volatile("vmacc.vx v24, %0, v2" ::"r"(f[0 + base_idx_0]));
      asm volatile("vmacc.vx v24, %0, v6" ::"r"(f[7 + base_idx_0]));
      asm volatile("vmacc.vx v24, %0, v10" ::"r"(f[14 + base_idx_0]));

      asm volatile("vmacc.vx v26, %0, v6" ::"r"(f[0 + base_idx_0]));
      asm volatile("vmacc.vx v26, %0, v10" ::"r"(f[7 + base_idx_0]));

      asm volatile("vmacc.vx v28, %0, v10" ::"r"(f[0 + base_idx_0]));
    }
  }

  // Reuse preloaded coefficients
  // Buffer the next coefficients for faster use
  asm volatile("vmacc.vx v16, %0, v2" ::"r"(fl4));
  f6_buf = f[42];
  asm volatile("vmacc.vx v16, %0, v6" ::"r"(fl5));
  f5_buf = f[35];
  asm volatile("vmacc.vx v16, %0, v10" ::"r"(fl6));
  asm volatile("vse32.v v16, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));

  asm volatile("vmacc.vx v18, %0, v2" ::"r"(fl3));
  asm volatile("vmacc.vx v18, %0, v6" ::"r"(fl4));
  asm volatile("vmacc.vx v18, %0, v10" ::"r"(fl5));
  asm volatile("vmv.v.v v16, v18");

  asm volatile("vmacc.vx v20, %0, v2" ::"r"(fl2));
  f4_buf = f[28];
  asm volatile("vmacc.vx v20, %0, v6" ::"r"(fl3));
  f3_buf = f[21];
  asm volatile("vmacc.vx v20, %0, v10" ::"r"(fl4));
  asm volatile("vmv.v.v v18, v20");

  asm volatile("vmacc.vx v22, %0, v2" ::"r"(fl1));
  f2_buf = f[14];
  asm volatile("vmacc.vx v22, %0, v6" ::"r"(fl2));
  f1_buf = f[7];
  asm volatile("vmacc.vx v22, %0, v10" ::"r"(fl3));
  asm volatile("vmv.v.v v20, v22");

  asm volatile("vmacc.vx v24, %0, v2" ::"r"(fl0));
  f0_buf = f[0];
  asm volatile("vmacc.vx v24, %0, v6" ::"r"(fl1));
  asm volatile("vmacc.vx v24, %0, v10" ::"r"(fl2));
  asm volatile("vmv.v.v v22, v24");

  asm volatile("vmacc.vx v26, %0, v6" ::"r"(fl0));
  asm volatile("vmacc.vx v26, %0, v10" ::"r"(fl1));
  asm volatile("vmv.v.v v24, v26");

  asm volatile("vmacc.vx v28, %0, v10" ::"r"(fl0));
  asm volatile("vmv.v.v v26, v28");

  // Bump the input ptr
  i_ += 3 * (N + F - 1);

  ////////////
  // REGIME //
  ////////////

  // The following loop is unrolled by 2
  // The input matrix has M + F - 1 rows
  // We have computed F input rows already
  // Nompute now until only F input rows are left
  // (The last F-1 rows do not contribute to F output rows each, so keep them
  // outside of this loop) (We keep F rows outside because of the unrolling by
  // 2, just for easeness)
  // limit: ((M + F - 1) - 2 * F) / 2
  for (uint32_t j = 0; j < (num_rows - F - 1) / 2; ++j) {
    // Work on F output rows

    // Loop on the channels
    for (int ch = 0; ch < C; ++ch) {
      // Point to the first element of the channel ch
      i__ = i_ + ch * ich_len;

      // Start calculating the next pointers to the elements to be slided in
      i_slide_ptr_0 = i__ + n_;

      asm volatile("vle32.v v0, (%0); add %0, %0, %1"
                   : "+&r"(i__)
                   : "r"(lwi_pad));

      //////////////
      // UNROLL 0 //
      //////////////

      // Main loop
      // Use int32_t buffering on the filter coefficients for 16-lanes config
      // The computation is too fast, and every coefficient belongs to a
      // different $line At every fld, CVA6 misses, and until it does not get
      // the new coefficient, it cannot dispatch the next V instruction
      for (int k = 0; k < F / 2; ++k) {
        // Two base indexes because of the unrolling
        // Look ahead to the first element of the current column (k+2) of the
        // current channel (ch) of the filter (f)
        int32_t base_idx_0 = (2 * k + 2) + (ch * fch_len);
        // Point to the first element of the current column (k+1) of the current
        // channel (ch) of the filter (f)
        int32_t base_idx_1 = (2 * k + 1) + (ch * fch_len);

        int32_t sld_elem = *i_slide_ptr_0++;

        // Calculate F contributions of the input rows, on F different output
        // rows
        asm volatile("vmacc.vx v16, %0, v0" ::"r"(f6_buf));
        f6_buf = f[42 + base_idx_1];
        asm volatile("vmacc.vx v18, %0, v0" ::"r"(f5_buf));
        f5_buf = f[35 + base_idx_1];
        asm volatile("vmacc.vx v20, %0, v0" ::"r"(f4_buf));
        f4_buf = f[28 + base_idx_1];
        asm volatile("vslide1down.vx v2, v0, %0" ::"r"(sld_elem));
        sld_elem = *i_slide_ptr_0++;
        asm volatile("vmacc.vx v22, %0, v0" ::"r"(f3_buf));
        f3_buf = f[21 + base_idx_1];
        asm volatile("vmacc.vx v24, %0, v0" ::"r"(f2_buf));
        f2_buf = f[14 + base_idx_1];
        asm volatile("vmacc.vx v26, %0, v0" ::"r"(f1_buf));
        f1_buf = f[7 + base_idx_1];
        if ((k | ch) == 0)
          asm volatile("vmul.vx v28, v0, %0" ::"r"(f0_buf));
        else
          asm volatile("vmacc.vx v28, %0, v0" ::"r"(f0_buf));
        f0_buf = f[0 + base_idx_1];

        // Nalculate F contributions of the input rows, on F different output
        // rows
        asm volatile("vmacc.vx v16, %0, v2" ::"r"(f6_buf));
        f6_buf = f[42 + base_idx_0];
        asm volatile("vmacc.vx v18, %0, v2" ::"r"(f5_buf));
        f5_buf = f[35 + base_idx_0];
        asm volatile("vmacc.vx v20, %0, v2" ::"r"(f4_buf));
        f4_buf = f[28 + base_idx_0];
        asm volatile("vslide1down.vx v0, v2, %0" ::"r"(sld_elem));
        asm volatile("vmacc.vx v22, %0, v2" ::"r"(f3_buf));
        f3_buf = f[21 + base_idx_0];
        asm volatile("vmacc.vx v24, %0, v2" ::"r"(f2_buf));
        f2_buf = f[14 + base_idx_0];
        asm volatile("vmacc.vx v26, %0, v2" ::"r"(f1_buf));
        f1_buf = f[7 + base_idx_0];
        asm volatile("vmacc.vx v28, %0, v2" ::"r"(f0_buf));
        f0_buf = f[0 + base_idx_0];
      }

      if (ch != C - 1) {
        int32_t base_idx_0 = (ch + 1) * fch_len;

        asm volatile("vmacc.vx v16, %0, v0" ::"r"(f6_buf));
        f6_buf = f[42 + base_idx_0];
        asm volatile("vmacc.vx v18, %0, v0" ::"r"(f5_buf));
        f5_buf = f[35 + base_idx_0];
        asm volatile("vmacc.vx v20, %0, v0" ::"r"(f4_buf));
        f4_buf = f[28 + base_idx_0];
        asm volatile("vmacc.vx v22, %0, v0" ::"r"(f3_buf));
        f3_buf = f[21 + base_idx_0];
        asm volatile("vmacc.vx v24, %0, v0" ::"r"(f2_buf));
        f2_buf = f[14 + base_idx_0];
        asm volatile("vmacc.vx v26, %0, v0" ::"r"(f1_buf));
        f1_buf = f[7 + base_idx_0];
        asm volatile("vmacc.vx v28, %0, v0" ::"r"(f0_buf));
        f0_buf = f[0 + base_idx_0];
      }
    }

    // The last iteration is used to mask the latency of the store and the moves
    // Use buffered coefficients not to stall NVA6 for coherency
    f6_buf = f[42];
    asm volatile("vmacc.vx v16, %0, v0" ::"r"(fl6));
    f5_buf = f[35];
    asm volatile("vse32.v  v16, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
    asm volatile("vmacc.vx v18, %0, v0" ::"r"(fl5));
    asm volatile("vmv.v.v v16, v18");
    asm volatile("vmacc.vx v20, %0, v0" ::"r"(fl4));
    asm volatile("vmv.v.v v18, v20");
    f4_buf = f[28];
    asm volatile("vmacc.vx v22, %0, v0" ::"r"(fl3));
    asm volatile("vmv.v.v v20, v22");
    f3_buf = f[21];
    asm volatile("vmacc.vx v24, %0, v0" ::"r"(fl2));
    asm volatile("vmv.v.v v22, v24");
    f2_buf = f[14];
    asm volatile("vmacc.vx v26, %0, v0" ::"r"(fl1));
    asm volatile("vmv.v.v v24, v26");
    f1_buf = f[7];
    asm volatile("vmacc.vx v28, %0, v0" ::"r"(fl0));
    asm volatile("vmv.v.v v26, v28");
    f0_buf = f[0];

    // Bump the input ptr
    i_ += N + F - 1;

    //////////////
    // UNROLL 1 //
    //////////////

    // Loop on the channels
    for (int ch = 0; ch < C; ++ch) {

      // Point to the first element of the channel ch
      i__ = i_ + ch * ich_len;

      // Start calculating the next pointers to the elements to be slided in
      i_slide_ptr_1 = i__ + n_;

      asm volatile("vle32.v v2, (%0); add %0, %0, %1"
                   : "+&r"(i__)
                   : "r"(lwi_pad));

      for (int k = 0; k < F / 2; ++k) {
        // Two base indexes because of the unrolling
        // Point to the first element of the current column (k) of the current
        // channel (ch) of the filter (f)
        int32_t base_idx_0 = (2 * k + 2) + (ch * fch_len);
        // Point to the first element of the current column (k+1) of the current
        // channel (ch) of the filter (f)
        int32_t base_idx_1 = (2 * k + 1) + (ch * fch_len);

        int32_t sld_elem = *i_slide_ptr_1++;

        asm volatile("vmacc.vx v16, %0, v2" ::"r"(f6_buf));
        f6_buf = f[42 + base_idx_1];
        asm volatile("vmacc.vx v18, %0, v2" ::"r"(f5_buf));
        f5_buf = f[35 + base_idx_1];
        asm volatile("vmacc.vx v20, %0, v2" ::"r"(f4_buf));
        f4_buf = f[28 + base_idx_1];
        asm volatile("vslide1down.vx v0, v2, %0" ::"r"(sld_elem));
        sld_elem = *i_slide_ptr_1++;
        asm volatile("vmacc.vx v22, %0, v2" ::"r"(f3_buf));
        f3_buf = f[21 + base_idx_1];
        asm volatile("vmacc.vx v24, %0, v2" ::"r"(f2_buf));
        f2_buf = f[14 + base_idx_1];
        asm volatile("vmacc.vx v26, %0, v2" ::"r"(f1_buf));
        f1_buf = f[7 + base_idx_1];
        if ((k | ch) == 0)
          asm volatile("vmul.vx v28, v2, %0" ::"r"(f0_buf));
        else
          asm volatile("vmacc.vx v28, %0, v2" ::"r"(f0_buf));
        f0_buf = f[0 + base_idx_1];

        asm volatile("vmacc.vx v16, %0, v0" ::"r"(f6_buf));
        f6_buf = f[42 + base_idx_0];
        asm volatile("vmacc.vx v18, %0, v0" ::"r"(f5_buf));
        f5_buf = f[35 + base_idx_0];
        asm volatile("vmacc.vx v20, %0, v0" ::"r"(f4_buf));
        f4_buf = f[28 + base_idx_0];
        asm volatile("vslide1down.vx v2, v0, %0" ::"r"(sld_elem));
        asm volatile("vmacc.vx v22, %0, v0" ::"r"(f3_buf));
        f3_buf = f[21 + base_idx_0];
        asm volatile("vmacc.vx v24, %0, v0" ::"r"(f2_buf));
        f2_buf = f[14 + base_idx_0];
        asm volatile("vmacc.vx v26, %0, v0" ::"r"(f1_buf));
        f1_buf = f[7 + base_idx_0];
        asm volatile("vmacc.vx v28, %0, v0" ::"r"(f0_buf));
        f0_buf = f[0 + base_idx_0];
      }

      if (ch != C - 1) {
        int32_t base_idx_0 = (ch + 1) * fch_len;

        asm volatile("vmacc.vx v16, %0, v2" ::"r"(f6_buf));
        f6_buf = f[42 + base_idx_0];
        asm volatile("vmacc.vx v18, %0, v2" ::"r"(f5_buf));
        f5_buf = f[35 + base_idx_0];
        asm volatile("vmacc.vx v20, %0, v2" ::"r"(f4_buf));
        f4_buf = f[28 + base_idx_0];
        asm volatile("vmacc.vx v22, %0, v2" ::"r"(f3_buf));
        f3_buf = f[21 + base_idx_0];
        asm volatile("vmacc.vx v24, %0, v2" ::"r"(f2_buf));
        f2_buf = f[14 + base_idx_0];
        asm volatile("vmacc.vx v26, %0, v2" ::"r"(f1_buf));
        f1_buf = f[7 + base_idx_0];
        asm volatile("vmacc.vx v28, %0, v2" ::"r"(f0_buf));
        f0_buf = f[0 + base_idx_0];
      }
    }

    // The last iteration is used to mask the latency of the store and the moves
    // Use buffered coefficients not to stall CVA6 for coherency
    f6_buf = f[42];
    asm volatile("vmacc.vx v16, %0, v2" ::"r"(fl6));
    f5_buf = f[35];
    asm volatile("vse32.v  v16, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
    asm volatile("vmacc.vx v18, %0, v2" ::"r"(fl5));
    asm volatile("vmv.v.v v16, v18");
    asm volatile("vmacc.vx v20, %0, v2" ::"r"(fl4));
    asm volatile("vmv.v.v v18, v20");
    f4_buf = f[28];
    asm volatile("vmacc.vx v22, %0, v2" ::"r"(fl3));
    asm volatile("vmv.v.v v20, v22");
    f3_buf = f[21];
    asm volatile("vmacc.vx v24, %0, v2" ::"r"(fl2));
    asm volatile("vmv.v.v v22, v24");
    f2_buf = f[14];
    asm volatile("vmacc.vx v26, %0, v2" ::"r"(fl1));
    asm volatile("vmv.v.v v24, v26");
    f1_buf = f[7];
    asm volatile("vmacc.vx v28, %0, v2" ::"r"(fl0));
    asm volatile("vmv.v.v v26, v28");
    f0_buf = f[0];

    // Bump the input ptr
    i_ += N + F - 1;
  }

  ////////////////////////
  // Row I-F -> (I-1)-3 //
  ////////////////////////

  for (int32_t ch = 0; ch < C; ++ch) {

    // Point to the first element of the channel ch
    i__ = i_ + ch * ich_len;

    // Point to the scalar elements to insert during a slide
    // i_slide_ptr_0 has already been computed
    i_slide_ptr_0 = i__ + n_ + 0 * (N + F - 1);
    i_slide_ptr_1 = i__ + n_ + 1 * (N + F - 1);
    i_slide_ptr_2 = i__ + n_ + 2 * (N + F - 1);
    i_slide_ptr_3 = i__ + n_ + 3 * (N + F - 1);

    // Load other three input rows (one was already loaded)
    asm volatile("vle32.v v0, (%0); add %0, %0, %1"
                 : "+&r"(i__)
                 : "r"(lwi_pad));
    asm volatile("vle32.v v4, (%0); add %0, %0, %1"
                 : "+&r"(i__)
                 : "r"(lwi_pad));
    asm volatile("vle32.v v8, (%0); add %0, %0, %1"
                 : "+&r"(i__)
                 : "r"(lwi_pad));
    asm volatile("vle32.v v12, (%0); add %0, %0, %1"
                 : "+&r"(i__)
                 : "r"(lwi_pad));

    // Main kernel, unrolled by 2
    // Process 4 input rows
    for (int k = 0; k < F / 2; ++k) {
      int32_t sld_elem = *i_slide_ptr_0++;
      // Two base indexes because of the unrolling
      // Point to the first element of the current column (k) of the current
      // channel (ch) of the filter (f)
      int32_t base_idx_0 = (2 * k) + (ch * fch_len);
      // Point to the first element of the current column (k+1) of the current
      // channel (ch) of the filter (f)
      int32_t base_idx_1 = (2 * k + 1) + (ch * fch_len);

      asm volatile("vslide1down.vx v2, v0, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v16, %0, v0" ::"r"(f[42 + base_idx_0]));
      sld_elem = *i_slide_ptr_1++;
      asm volatile("vmacc.vx v18, %0, v0" ::"r"(f[35 + base_idx_0]));
      asm volatile("vmacc.vx v20, %0, v0" ::"r"(f[28 + base_idx_0]));
      asm volatile("vmacc.vx v22, %0, v0" ::"r"(f[21 + base_idx_0]));
      asm volatile("vmacc.vx v24, %0, v0" ::"r"(f[14 + base_idx_0]));
      asm volatile("vmacc.vx v26, %0, v0" ::"r"(f[7 + base_idx_0]));
      if ((k | ch) == 0)
        asm volatile("vmul.vx v28, v0, %0" ::"r"(f[0 + base_idx_0]));
      else
        asm volatile("vmacc.vx v28, %0, v0" ::"r"(f[0 + base_idx_0]));
      asm volatile("vslide1down.vx v6, v4, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v18, %0, v4" ::"r"(f[42 + base_idx_0]));
      sld_elem = *i_slide_ptr_2++;
      asm volatile("vmacc.vx v20, %0, v4" ::"r"(f[35 + base_idx_0]));
      asm volatile("vmacc.vx v22, %0, v4" ::"r"(f[28 + base_idx_0]));
      asm volatile("vmacc.vx v24, %0, v4" ::"r"(f[21 + base_idx_0]));
      asm volatile("vmacc.vx v26, %0, v4" ::"r"(f[14 + base_idx_0]));
      asm volatile("vmacc.vx v28, %0, v4" ::"r"(f[7 + base_idx_0]));
      asm volatile("vslide1down.vx v10, v8, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v20, %0, v8" ::"r"(f[42 + base_idx_0]));
      sld_elem = *i_slide_ptr_3++;
      asm volatile("vmacc.vx v22, %0, v8" ::"r"(f[35 + base_idx_0]));
      asm volatile("vmacc.vx v24, %0, v8" ::"r"(f[28 + base_idx_0]));
      asm volatile("vmacc.vx v26, %0, v8" ::"r"(f[21 + base_idx_0]));
      asm volatile("vmacc.vx v28, %0, v8" ::"r"(f[14 + base_idx_0]));
      asm volatile("vslide1down.vx v14, v12, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v22, %0, v12" ::"r"(f[42 + base_idx_0]));
      sld_elem = *i_slide_ptr_0++;
      asm volatile("vmacc.vx v24, %0, v12" ::"r"(f[35 + base_idx_0]));
      asm volatile("vmacc.vx v26, %0, v12" ::"r"(f[28 + base_idx_0]));
      asm volatile("vmacc.vx v28, %0, v12" ::"r"(f[21 + base_idx_0]));

      asm volatile("vslide1down.vx v0, v2, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v16, %0, v2" ::"r"(f[42 + base_idx_1]));
      sld_elem = *i_slide_ptr_1++;
      asm volatile("vmacc.vx v18, %0, v2" ::"r"(f[35 + base_idx_1]));
      asm volatile("vmacc.vx v20, %0, v2" ::"r"(f[28 + base_idx_1]));
      asm volatile("vmacc.vx v22, %0, v2" ::"r"(f[21 + base_idx_1]));
      asm volatile("vmacc.vx v24, %0, v2" ::"r"(f[14 + base_idx_1]));
      asm volatile("vmacc.vx v26, %0, v2" ::"r"(f[7 + base_idx_1]));
      asm volatile("vmacc.vx v28, %0, v2" ::"r"(f[0 + base_idx_1]));
      asm volatile("vslide1down.vx v4, v6, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v18, %0, v6" ::"r"(f[42 + base_idx_1]));
      sld_elem = *i_slide_ptr_2++;
      asm volatile("vmacc.vx v20, %0, v6" ::"r"(f[35 + base_idx_1]));
      asm volatile("vmacc.vx v22, %0, v6" ::"r"(f[28 + base_idx_1]));
      asm volatile("vmacc.vx v24, %0, v6" ::"r"(f[21 + base_idx_1]));
      asm volatile("vmacc.vx v26, %0, v6" ::"r"(f[14 + base_idx_1]));
      asm volatile("vmacc.vx v28, %0, v6" ::"r"(f[7 + base_idx_1]));
      asm volatile("vslide1down.vx v8, v10, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v20, %0, v10" ::"r"(f[42 + base_idx_1]));
      sld_elem = *i_slide_ptr_3++;
      asm volatile("vmacc.vx v22, %0, v10" ::"r"(f[35 + base_idx_1]));
      asm volatile("vmacc.vx v24, %0, v10" ::"r"(f[28 + base_idx_1]));
      asm volatile("vmacc.vx v26, %0, v10" ::"r"(f[21 + base_idx_1]));
      asm volatile("vmacc.vx v28, %0, v10" ::"r"(f[14 + base_idx_1]));
      asm volatile("vslide1down.vx v12, v14, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v22, %0, v14" ::"r"(f[42 + base_idx_1]));
      asm volatile("vmacc.vx v24, %0, v14" ::"r"(f[35 + base_idx_1]));
      asm volatile("vmacc.vx v26, %0, v14" ::"r"(f[28 + base_idx_1]));
      asm volatile("vmacc.vx v28, %0, v14" ::"r"(f[21 + base_idx_1]));
    }

    if (ch != C - 1) {
      int32_t base_idx_0 = (F - 1) + (ch * fch_len);

      asm volatile("vmacc.vx v16, %0, v0" ::"r"(f[42 + base_idx_0]));
      asm volatile("vmacc.vx v18, %0, v0" ::"r"(f[35 + base_idx_0]));
      asm volatile("vmacc.vx v20, %0, v0" ::"r"(f[28 + base_idx_0]));
      asm volatile("vmacc.vx v22, %0, v0" ::"r"(f[21 + base_idx_0]));
      asm volatile("vmacc.vx v24, %0, v0" ::"r"(f[14 + base_idx_0]));
      asm volatile("vmacc.vx v26, %0, v0" ::"r"(f[7 + base_idx_0]));
      asm volatile("vmacc.vx v28, %0, v0" ::"r"(f[0 + base_idx_0]));
      asm volatile("vmacc.vx v18, %0, v4" ::"r"(f[42 + base_idx_0]));
      asm volatile("vmacc.vx v20, %0, v4" ::"r"(f[35 + base_idx_0]));
      asm volatile("vmacc.vx v22, %0, v4" ::"r"(f[28 + base_idx_0]));
      asm volatile("vmacc.vx v24, %0, v4" ::"r"(f[21 + base_idx_0]));
      asm volatile("vmacc.vx v26, %0, v4" ::"r"(f[14 + base_idx_0]));
      asm volatile("vmacc.vx v28, %0, v4" ::"r"(f[7 + base_idx_0]));
      asm volatile("vmacc.vx v20, %0, v8" ::"r"(f[42 + base_idx_0]));
      asm volatile("vmacc.vx v22, %0, v8" ::"r"(f[35 + base_idx_0]));
      asm volatile("vmacc.vx v24, %0, v8" ::"r"(f[28 + base_idx_0]));
      asm volatile("vmacc.vx v26, %0, v8" ::"r"(f[21 + base_idx_0]));
      asm volatile("vmacc.vx v28, %0, v8" ::"r"(f[14 + base_idx_0]));
      asm volatile("vmacc.vx v22, %0, v12" ::"r"(f[42 + base_idx_0]));
      asm volatile("vmacc.vx v24, %0, v12" ::"r"(f[35 + base_idx_0]));
      asm volatile("vmacc.vx v26, %0, v12" ::"r"(f[28 + base_idx_0]));
      asm volatile("vmacc.vx v28, %0, v12" ::"r"(f[21 + base_idx_0]));
    }
  }

  asm volatile("vmacc.vx v16, %0, v0" ::"r"(fl6));
  asm volatile("vmacc.vx v18, %0, v0" ::"r"(fl5));
  asm volatile("vmacc.vx v20, %0, v0" ::"r"(fl4));
  asm volatile("vse32.v  v16, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
  asm volatile("vmacc.vx v22, %0, v0" ::"r"(fl3));
  asm volatile("vmacc.vx v24, %0, v0" ::"r"(fl2));
  asm volatile("vmacc.vx v26, %0, v0" ::"r"(fl1));
  asm volatile("vmacc.vx v28, %0, v0" ::"r"(fl0));
  asm volatile("vmacc.vx v18, %0, v4" ::"r"(fl6));
  asm volatile("vmacc.vx v20, %0, v4" ::"r"(fl5));
  asm volatile("vse32.v  v18, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
  asm volatile("vmacc.vx v22, %0, v4" ::"r"(fl4));
  asm volatile("vmacc.vx v24, %0, v4" ::"r"(fl3));
  asm volatile("vmacc.vx v26, %0, v4" ::"r"(fl2));
  asm volatile("vmacc.vx v28, %0, v4" ::"r"(fl1));
  asm volatile("vmacc.vx v20, %0, v8" ::"r"(fl6));
  asm volatile("vmacc.vx v22, %0, v8" ::"r"(fl5));
  asm volatile("vse32.v  v20, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
  asm volatile("vmacc.vx v24, %0, v8" ::"r"(fl4));
  asm volatile("vmacc.vx v26, %0, v8" ::"r"(fl3));
  asm volatile("vmacc.vx v28, %0, v8" ::"r"(fl2));
  asm volatile("vmacc.vx v22, %0, v12" ::"r"(fl6));
  asm volatile("vse32.v  v22, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
  asm volatile("vmacc.vx v24, %0, v12" ::"r"(fl5));
  asm volatile("vmacc.vx v26, %0, v12" ::"r"(fl4));
  asm volatile("vmacc.vx v28, %0, v12" ::"r"(fl3));

  // Bump the input ptr
  i_ += 4 * (N + F - 1);

  //////////////////////////
  // Row (I-1)-3 -> (I-1) //
  //////////////////////////

  for (int32_t ch = 0; ch < C; ++ch) {

    // Point to the first element of the channel ch
    i__ = i_ + ch * ich_len;

    // Start calculating the next pointers to the elements to be slided in
    i_slide_ptr_0 = i__ + n_ + 0 * (N + F - 1);
    i_slide_ptr_1 = i__ + n_ + 1 * (N + F - 1);
    i_slide_ptr_2 = i__ + n_ + 2 * (N + F - 1);

    asm volatile("vle32.v v2, (%0); add %0, %0, %1"
                 : "+&r"(i__)
                 : "r"(lwi_pad));
    asm volatile("vle32.v v6, (%0); add %0, %0, %1"
                 : "+&r"(i__)
                 : "r"(lwi_pad));
    asm volatile("vle32.v v10, (%0); add %0, %0, %1"
                 : "+&r"(i__)
                 : "r"(lwi_pad));

    // Main kernel, unrolled by 2
    for (int k = 0; k < F / 2; ++k) {
      int32_t sld_elem = *i_slide_ptr_0++;
      // Two base indexes because of the unrolling
      // Point to the first element of the current column (k) of the current
      // channel (ch) of the filter (f)
      int32_t base_idx_0 = (2 * k) + (ch * fch_len);
      // Point to the first element of the current column (k+1) of the current
      // channel (ch) of the filter (f)
      int32_t base_idx_1 = (2 * k + 1) + (ch * fch_len);

      asm volatile("vslide1down.vx v0, v2, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v24, %0, v2" ::"r"(f[42 + base_idx_0]));
      sld_elem = *i_slide_ptr_1++;
      asm volatile("vmacc.vx v26, %0, v2" ::"r"(f[35 + base_idx_0]));
      asm volatile("vslide1down.vx v4, v6, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v28, %0, v2" ::"r"(f[28 + base_idx_0]));
      sld_elem = *i_slide_ptr_2++;
      asm volatile("vmacc.vx v26, %0, v6" ::"r"(f[42 + base_idx_0]));
      asm volatile("vslide1down.vx v8, v10, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v28, %0, v6" ::"r"(f[35 + base_idx_0]));
      sld_elem = *i_slide_ptr_0++;
      asm volatile("vmacc.vx v28, %0, v10" ::"r"(f[42 + base_idx_0]));

      asm volatile("vslide1down.vx v2, v0, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v24, %0, v0" ::"r"(f[42 + base_idx_1]));
      sld_elem = *i_slide_ptr_1++;
      asm volatile("vmacc.vx v26, %0, v0" ::"r"(f[35 + base_idx_1]));
      asm volatile("vslide1down.vx v6, v4, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v28, %0, v0" ::"r"(f[28 + base_idx_1]));
      sld_elem = *i_slide_ptr_2++;
      asm volatile("vmacc.vx v26, %0, v4" ::"r"(f[42 + base_idx_1]));
      asm volatile("vslide1down.vx v10, v8, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v28, %0, v4" ::"r"(f[35 + base_idx_1]));
      asm volatile("vmacc.vx v28, %0, v8" ::"r"(f[42 + base_idx_1]));
    }

    if (ch != C - 1) {
      int32_t base_idx_0 = (F - 1) + (ch * fch_len);

      asm volatile("vmacc.vx v24, %0, v2" ::"r"(f[42 + base_idx_0]));
      asm volatile("vmacc.vx v26, %0, v2" ::"r"(f[35 + base_idx_0]));
      asm volatile("vmacc.vx v28, %0, v2" ::"r"(f[28 + base_idx_0]));
      asm volatile("vmacc.vx v26, %0, v6" ::"r"(f[42 + base_idx_0]));
      asm volatile("vmacc.vx v28, %0, v6" ::"r"(f[35 + base_idx_0]));
      asm volatile("vmacc.vx v28, %0, v10" ::"r"(f[42 + base_idx_0]));
    }
  }

  asm volatile("vmacc.vx v24, %0, v2" ::"r"(fl6));
  asm volatile("vse32.v  v24, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
  asm volatile("vmacc.vx v26, %0, v2" ::"r"(fl5));
  asm volatile("vmacc.vx v28, %0, v2" ::"r"(fl4));
  asm volatile("vmacc.vx v26, %0, v6" ::"r"(fl6));
  asm volatile("vse32.v  v26, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
  asm volatile("vmacc.vx v28, %0, v6" ::"r"(fl5));
  asm volatile("vmacc.vx v28, %0, v10" ::"r"(fl6));
  asm volatile("vse32.v  v28, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
}

/*
  ////////////////////
  // MAIN ALGOMITHM //
  ////////////////////
  // Start calculating the pointer to the next element to be slided in
  i_slide_ptr_0 = i + N;
  // Load one input row
  asm volatile("vle32.v v0, (%0); add %0, %0, %1" : "+&r"(i) : "r"(lwi_pad));
  // Kernel
  for (int k = 0; k < F; ++k) {
    // Nalculate F*F contributions of the input rows, on F different output rows
    // v28 should be initialized during the first iteration
    asm volatile("vmacc.vx v16, %0, v0" :: "r"(f[42  + (2*k)]));
    asm volatile("vmacc.vx v18, %0, v0" :: "r"(f[35  + (2*k)]));
    asm volatile("vmacc.vx v20, %0, v0" :: "r"(f[28 + (2*k)]));
    asm volatile("vmacc.vx v22, %0, v0" :: "r"(f[21 + (2*k)]));
    asm volatile("vmacc.vx v24, %0, v0" :: "r"(f[14 + (2*k)]));
    asm volatile("vmacc.vx v26, %0, v0" :: "r"(f[7 + (2*k)]));
    if (k == 0)
      asm volatile("vmul.vx v28, v0, %0" :: "r"(f[0 + (2*k)]));
    else
      asm volatile("vmacc.vx v28, %0, v0" :: "r"(f[0 + (2*k)]));
    // Slide the input row by one, and inject the next scalar element of the row
    asm volatile("vslide1down.vx v0, v0, %0" :: "r"(*i_slide_ptr_0++));
  }
  // Store one output row
  asm volatile("vse32.v  v16, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
  // Move all the input rows to return to the initial situation
  // To avoid these operations, unroll the loop via software, renaming the
  registers manually asm volatile("vmv.v.v v16, v18"); asm volatile("vmv.v.v
  v18, v20"); asm volatile("vmv.v.v v20, v22"); asm volatile("vmv.v.v v22,
  v24"); asm volatile("vmv.v.v v24, v26"); asm volatile("vmv.v.v v26, v28");
*/

#undef M
#undef N
#undef C
#undef F
