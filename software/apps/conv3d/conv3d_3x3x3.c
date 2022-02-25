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
#define M MATRIX_DIM
#define N MATRIX_DIM
#define C 1
#define F 3

void conv3d_CHx3x3(int32_t *o, int32_t *i, int32_t *f) {

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

    conv3d_CHx3x3_block(o_, i_, M, f, n_);
  }
}

void conv3d_CHx3x3_block(int32_t *o, int32_t *i, uint32_t num_rows, int32_t *f, uint32_t n_) {

  // Helper variables
  int32_t lwo = N << 2;
  int32_t lwi = N + F - 1;
  int32_t lwi_pad = (N + F - 1) << 2;

  // Number of elements that separates two adjacent channels
  int32_t ich_len = (M + F - 1) * (N + F - 1);
  int32_t fch_len = F * F;

  int32_t *i_ = i;
  int32_t *i__ = i;

  // Buffers for coefficients preloading
  int32_t f0_buf, f1_buf, f2_buf;

  int32_t *i_slide_ptr_0;
  int32_t *i_slide_ptr_1;

  // Preload two input rows belonging to channel 0
  asm volatile("vle32.v v0, (%0); add %0, %0, %1"
               : "+&r"(i__)
               : "r"(lwi_pad));

  // Buffer some of the filter coefficients not to lose efficiency
  f0_buf = f[0];
  f1_buf = f[3];
  f2_buf = f[6];

  asm volatile("vle32.v v4, (%0)" :: "r"(i__));


  ////////////////
  // Row 0 -> 1 //
  ////////////////

  // Loop on the channels
  for (int ch = 0; ch < C; ++ch) {

    // Point to the first element of the channel ch
    i__ = i_ + ich_len * ch;

    // Point to the scalar elements to insert during a slide
    i_slide_ptr_0 = i__ + n_ + 0 * (N + F - 1);
    i_slide_ptr_1 = i__ + n_ + 1 * (N + F - 1);

    // Main kernel, unrolled by 2
    // Unrolled because of int32_t buffering
    // With HW renaming, this unroll is not needed
    // Two base indexes because of the unrolling
    // Point to the first element of the current column (k+1) of the current
    // channel (ch) of the filter (f)
    int32_t base_idx_1 = 1 + (fch_len * ch);
    // Point to the first element of the current column (k+2) of the current
    // channel (ch) of the filter (f)
    int32_t base_idx_2 = 2 + (fch_len * ch);

    int32_t sld_elem = *i_slide_ptr_0++;

    if (ch == 0)
      asm volatile("vmul.vx v24, v0, %0" ::"r"(f0_buf));
    else
      asm volatile("vmacc.vx v24, %0, v0" ::"r"(f0_buf));
    if (ch == 0)
      asm volatile("vmul.vx v26, v4, %0" ::"r"(f0_buf));
    else
      asm volatile("vmacc.vx v26, %0, v4" ::"r"(f0_buf));
    f0_buf = f[0 + base_idx_1];
    asm volatile("vslide1down.vx v2, v0, %0" ::"r"(sld_elem));
    sld_elem = *i_slide_ptr_1++;
    asm volatile("vmacc.vx v24, %0, v4" ::"r"(f1_buf));
    f1_buf = f[3 + base_idx_1];
    asm volatile("vslide1down.vx v6, v4, %0" ::"r"(sld_elem));
    sld_elem = *i_slide_ptr_0++;


    asm volatile("vmacc.vx v24, %0, v2" ::"r"(f0_buf));
    asm volatile("vmacc.vx v26, %0, v6" ::"r"(f0_buf));
    f0_buf = f[0 + base_idx_2];
    asm volatile("vslide1down.vx v0, v2, %0" ::"r"(sld_elem));
    sld_elem = *i_slide_ptr_1++;
    asm volatile("vmacc.vx v24, %0, v6" ::"r"(f1_buf));
    f1_buf = f[3 + base_idx_2];
    asm volatile("vslide1down.vx v4, v6, %0" ::"r"(sld_elem));
    sld_elem = *i_slide_ptr_0++;

    if (ch != C - 1) {
      int32_t base_idx_0 = fch_len * (ch + 1);
      i__ = i_ + ich_len * (ch + 1);

      // Don't slide during the last iteration but preload the
      // next two input rows of channel ch + 1
      asm volatile("vmacc.vx v24, %0, v0" ::"r"(f0_buf));

      asm volatile("vle32.v v0, (%0); add %0, %0, %1"
                   : "+&r"(i__)
                   : "r"(lwi_pad));

      asm volatile("vmacc.vx v26, %0, v4" ::"r"(f0_buf));
      f0_buf = f[0 + base_idx_0];
      asm volatile("vmacc.vx v24, %0, v4" ::"r"(f1_buf));

      asm volatile("vle32.v v4, (%0)" :: "r"(i__));

      f1_buf = f[3 + base_idx_0];
    }
  }

  // Bump the input ptr
  i_ += 2 * (N + F - 1);

  asm volatile("vle32.v v2, (%0)" :: "r"(i_));

  asm volatile("vmacc.vx v24, %0, v0" ::"r"(f0_buf));
  asm volatile("vmacc.vx v26, %0, v4" ::"r"(f0_buf));
  f0_buf = f[0];
  asm volatile("vmacc.vx v24, %0, v4" ::"r"(f1_buf));
  f1_buf = f[3];

  ///////////
  // Row 2 //
  ///////////

  // Loop on the channels
  for (int ch = 0; ch < C; ++ch) {

    // Point to the first element of the channel ch
    i__ = i_ + ich_len * ch;

    // Start calculating the next pointers to the elements to be slided in
    i_slide_ptr_0 = i__ + n_ + 0 * (N + F - 1);

    // Main kernel, unrolled by 2
    // Two base indexes because of the unrolling
    // Point to the first element of the current column (k+1) of the current
    // channel (ch) of the filter (f)
    int32_t base_idx_1 = 1 + (fch_len * ch);
    // Point to the first element of the current column (k+2) of the current
    // channel (ch) of the filter (f)
    int32_t base_idx_2 = 2 + (fch_len * ch);

    int32_t sld_elem = *i_slide_ptr_0++;

    // Unroll 0
    asm volatile("vmacc.vx v24, %0, v2" ::"r"(f2_buf));
    f2_buf = f[6 + base_idx_1];
    asm volatile("vslide1down.vx v0, v2, %0" ::"r"(sld_elem));
    sld_elem = *i_slide_ptr_0++;

    asm volatile("vmacc.vx v26, %0, v2" ::"r"(f1_buf));
    f1_buf = f[3 + base_idx_1];

    if (ch == 0)
      asm volatile("vmul.vx v28, v2, %0" ::"r"(f0_buf));
    else
      asm volatile("vmacc.vx v28, %0, v2" ::"r"(f0_buf));
    f0_buf = f[0 + base_idx_1];

    // Unroll 1
    asm volatile("vmacc.vx v24, %0, v0" ::"r"(f2_buf));
    asm volatile("vslide1down.vx v2, v0, %0" ::"r"(sld_elem));
    f2_buf  = f[6 + base_idx_2];

    asm volatile("vmacc.vx v26, %0, v0" ::"r"(f1_buf));
    f1_buf = f[3 + base_idx_2];

    asm volatile("vmacc.vx v28, %0, v0" ::"r"(f0_buf));
    f0_buf = f[0 + base_idx_2];

    // The very last iterations require mixing the instructions with the store
    // and the moves
    if (ch != C - 1) {
      // Point to the first element of the current column (k) of the current
      // channel (ch) of the filter (f)
      int32_t base_idx_0 = fch_len * (ch + 1);
      i__ = i_ + ich_len * (ch + 1);

      // Don't slide the elements here

      // Preload next input row of channel ch+1
      asm volatile("vle32.v v30, (%0)" :: "r"(i__));

      asm volatile("vmacc.vx v24, %0, v2" ::"r"(f2_buf));
      f2_buf = f[6 + base_idx_0];

      asm volatile("vmacc.vx v26, %0, v2" ::"r"(f1_buf));
      f1_buf = f[3 + base_idx_0];

      asm volatile("vmacc.vx v28, %0, v2" ::"r"(f0_buf));
      asm volatile("vmv.v.v v2, v30");
      f0_buf = f[0 + base_idx_0];
    }
  }


  asm volatile("vmacc.vx v24, %0, v2" ::"r"(f2_buf));
  asm volatile("vse32.v v24, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
  f2_buf = f[6];

  asm volatile("vmacc.vx v26, %0, v2" ::"r"(f1_buf));
  f1_buf = f[3];
  asm volatile("vmv.v.v v24, v26");

  // Bump the input ptr
  i_ += (N + F - 1);

  // Preload next input row
  asm volatile("vle32.v v0, (%0)" :: "r"(i_));

  i_slide_ptr_0 = i_ + n_;

  asm volatile("vmacc.vx v28, %0, v2" ::"r"(f0_buf));
  f0_buf = f[0];
  asm volatile("vmv.v.v v26, v28");

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

      //////////////
      // UNROLL 0 //
      //////////////

      // Main loop=
      // Two base indexes because of the unrolling
      // Look ahead to the first element of the current column (k+1) of the
      // current channel (ch) of the filter (f)
      int32_t base_idx_1 = 1 + (fch_len * ch);
      // Point to the first element of the current column (k+2) of the current
      // channel (ch) of the filter (f)
      int32_t base_idx_2 = 2 + (fch_len * ch);

      int32_t sld_elem = *i_slide_ptr_0++;

      // Calculate F contributions of the input rows, on F different output
      // rows
      asm volatile("vmacc.vx v24, %0, v0" ::"r"(f2_buf));
      f2_buf = f[6 + base_idx_1];
      asm volatile("vmacc.vx v26, %0, v0" ::"r"(f1_buf));
      f1_buf = f[3 + base_idx_1];
      asm volatile("vslide1down.vx v2, v0, %0" ::"r"(sld_elem));
      sld_elem = *i_slide_ptr_0++;
      if (ch == 0)
        asm volatile("vmul.vx v28, v0, %0" ::"r"(f0_buf));
      else
        asm volatile("vmacc.vx v28, %0, v0" ::"r"(f0_buf));
      f0_buf = f[0 + base_idx_1];


      // Nalculate F contributions of the input rows, on F different output
      // rows
      asm volatile("vmacc.vx v24, %0, v2" ::"r"(f2_buf));
      f2_buf = f[6 + base_idx_2];
      asm volatile("vmacc.vx v26, %0, v2" ::"r"(f1_buf));
      f1_buf = f[3 + base_idx_2];
      asm volatile("vslide1down.vx v0, v2, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v28, %0, v2" ::"r"(f0_buf));
      f0_buf = f[0 + base_idx_2];
      if (ch != C - 1) {
        // Point to the first element of the channel ch
        i__ = i_ + ich_len * (ch + 1);
        // Preload next input row
        asm volatile("vle32.v v30, (%0)" :: "r"(i__));

        // Start calculating the next pointers to the elements to be slided in

        asm volatile("vmacc.vx v24, %0, v0" ::"r"(f2_buf));

        // Calculate next base idx
        int32_t base_idx_0 = fch_len * (ch + 1);

        f2_buf = f[6 + base_idx_0];
        asm volatile("vmacc.vx v26, %0, v0" ::"r"(f1_buf));
        f1_buf = f[3 + base_idx_0];
        asm volatile("vmacc.vx v28, %0, v0" ::"r"(f0_buf));
        asm volatile("vmv.v.v v0, v30");
        f0_buf = f[0 + base_idx_0];

        i_slide_ptr_0 = i__ + n_;
      }
    }

    // The last iteration is used to mask the latency of the store and the moves
    asm volatile("vmacc.vx v24, %0, v0" ::"r"(f2_buf));
    asm volatile("vse32.v  v24, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
    asm volatile("vmacc.vx v26, %0, v0" ::"r"(f1_buf));
    asm volatile("vmv.v.v v24, v26");

    // Bump the input ptr
    i_ += N + F - 1;
    // Preload next input row of channel 0
    asm volatile("vle32.v v2, (%0)" :: "r"(i_));
    i_slide_ptr_1 = i_ + n_;

    asm volatile("vmacc.vx v28, %0, v0" ::"r"(f0_buf));
    f2_buf = f[6];
    asm volatile("vmv.v.v v26, v28");
    f1_buf = f[3];
    f0_buf = f[0];


    //////////////
    // UNROLL 1 //
    //////////////

    // Loop on the channels
    for (int ch = 0; ch < C; ++ch) {
      // Two base indexes because of the unrolling
      // Point to the first element of the current column (k+1) of the current
      // channel (ch) of the filter (f)
      int32_t base_idx_1 = 1 + (fch_len * ch);
      // Point to the first element of the current column (k+2) of the current
      // channel (ch) of the filter (f)
      int32_t base_idx_2 = 2 + (fch_len * ch);

      int32_t sld_elem = *i_slide_ptr_1++;

      asm volatile("vmacc.vx v24, %0, v2" ::"r"(f2_buf));
      f2_buf = f[6 + base_idx_1];
      asm volatile("vslide1down.vx v0, v2, %0" ::"r"(sld_elem));
      sld_elem = *i_slide_ptr_1++;
      asm volatile("vmacc.vx v26, %0, v2" ::"r"(f1_buf));
      f1_buf = f[3 + base_idx_1];
      if (ch == 0)
        asm volatile("vmul.vx v28, v2, %0" ::"r"(f0_buf));
      else
        asm volatile("vmacc.vx v28, %0, v2" ::"r"(f0_buf));
      f0_buf = f[0 + base_idx_1];

      asm volatile("vmacc.vx v24, %0, v0" ::"r"(f2_buf));
      asm volatile("vslide1down.vx v2, v0, %0" ::"r"(sld_elem));
      f2_buf = f[6 + base_idx_2];
      asm volatile("vmacc.vx v26, %0, v0" ::"r"(f1_buf));
      f1_buf = f[3 + base_idx_2];
      asm volatile("vmacc.vx v28, %0, v0" ::"r"(f0_buf));
      f0_buf = f[0 + base_idx_2];

      if (ch != C - 1) {
        // Point to the first element of the channel ch+1
        i__ = i_ + ich_len * (ch + 1);
        asm volatile("vle32.v v30, (%0)" :: "r"(i__));

        asm volatile("vmacc.vx v24, %0, v2" ::"r"(f2_buf));

        // Calculate next base idx
        int32_t base_idx_0 = fch_len * (ch + 1);

        f2_buf = f[6 + base_idx_0];
        asm volatile("vmacc.vx v26, %0, v2" ::"r"(f1_buf));
        f1_buf = f[3 + base_idx_0];
        asm volatile("vmacc.vx v28, %0, v2" ::"r"(f0_buf));
        asm volatile("vmv.v.v v2, v30");
        f0_buf = f[0 + base_idx_0];

        i_slide_ptr_1 = i__ + n_;
      }
    }

    // The last iteration is used to mask the latency of the store and the moves
    asm volatile("vmacc.vx v24, %0, v2" ::"r"(f2_buf));
    asm volatile("vse32.v  v24, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
    asm volatile("vmacc.vx v26, %0, v2" ::"r"(f1_buf));
    asm volatile("vmv.v.v v24, v26");

    // Bump the input ptr
    i_ += N + F - 1;
    // Preload next input row
    asm volatile("vle32.v v0, (%0)" :: "r"(i_));
    i_slide_ptr_0 = i_ + n_;

    asm volatile("vmacc.vx v28, %0, v2" ::"r"(f0_buf));
    f2_buf = f[6];
    asm volatile("vmv.v.v v26, v28");
    f1_buf = f[3];
    f0_buf = f[0];

  }

  i__ = i_ + lwi;
  asm volatile("vle32.v v4, (%0)" :: "r"(i__));

  ////////////////////////
  // Row I-F -> (I-1)-1 //
  ////////////////////////

  for (int32_t ch = 0; ch < C; ++ch) {

    // Point to the first element of the channel ch
    i__ = i_ + ich_len * ch;

    // Point to the scalar elements to insert during a slide
    // i_slide_ptr_0 has already been computed
    i_slide_ptr_0 = i__ + n_ + 0 * (N + F - 1);
    i_slide_ptr_1 = i__ + n_ + 1 * (N + F - 1);

    // Main kernel, unrolled by 2
    // Process 2 input rows
    int32_t sld_elem = *i_slide_ptr_0++;
    // Two base indexes because of the unrolling
    // Point to the first element of the current column (k+1) of the current
    // channel (ch) of the filter (f)
    int32_t base_idx_1 = 1 + (fch_len * ch);
    // Point to the first element of the current column (k+2) of the current
    // channel (ch) of the filter (f)
    int32_t base_idx_2 = 2 + (fch_len * ch);

    asm volatile("vmacc.vx v24, %0, v0" ::"r"(f2_buf));
    asm volatile("vslide1down.vx v2, v0, %0" ::"r"(sld_elem));
    sld_elem = *i_slide_ptr_1++;
    asm volatile("vmacc.vx v26, %0, v0" ::"r"(f1_buf));
    if (ch == 0)
      asm volatile("vmul.vx v28, v0, %0" ::"r"(f0_buf));
    else
      asm volatile("vmacc.vx v28, %0, v0" ::"r"(f0_buf));
    f0_buf = f[0 + base_idx_1];
    asm volatile("vslide1down.vx v6, v4, %0" ::"r"(sld_elem));
    sld_elem = *i_slide_ptr_0++;
    asm volatile("vmacc.vx v26, %0, v4" ::"r"(f2_buf));
    f2_buf = f[6 + base_idx_1];
    asm volatile("vmacc.vx v28, %0, v4" ::"r"(f1_buf));
    f1_buf = f[3 + base_idx_1];

    asm volatile("vslide1down.vx v0, v2, %0" ::"r"(sld_elem));
    sld_elem = *i_slide_ptr_1++;
    asm volatile("vmacc.vx v24, %0, v2" ::"r"(f2_buf));
    asm volatile("vmacc.vx v26, %0, v2" ::"r"(f1_buf));
    asm volatile("vmacc.vx v28, %0, v2" ::"r"(f0_buf));
    f0_buf = f[0 + base_idx_2];
    asm volatile("vslide1down.vx v4, v6, %0" ::"r"(sld_elem));
    asm volatile("vmacc.vx v26, %0, v6" ::"r"(f2_buf));
    f2_buf = f[6 + base_idx_2];
    asm volatile("vmacc.vx v28, %0, v6" ::"r"(f1_buf));
    f1_buf = f[3 + base_idx_2];

    if (ch != C - 1) {

      int32_t base_idx_0 = fch_len * (ch + 1);

      i__ = i_ + ich_len * (ch + 1);

      asm volatile("vmacc.vx v24, %0, v0" ::"r"(f2_buf));
      asm volatile("vmacc.vx v26, %0, v0" ::"r"(f1_buf));
      asm volatile("vmacc.vx v28, %0, v0" ::"r"(f0_buf));
      f0_buf = f[0 + base_idx_0];

      asm volatile("vle32.v v0, (%0); add %0, %0, %1"
                   : "+&r"(i__)
                   : "r"(lwi_pad));

      asm volatile("vmacc.vx v26, %0, v4" ::"r"(f2_buf));
      f2_buf = f[6 + base_idx_0];
      asm volatile("vmacc.vx v28, %0, v4" ::"r"(f1_buf));
      f1_buf = f[3 + base_idx_0];

      asm volatile("vle32.v v4, (%0)" :: "r"(i__));
    }
  }

  asm volatile("vmacc.vx v24, %0, v0" ::"r"(f2_buf));
  asm volatile("vse32.v  v24, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
  asm volatile("vmacc.vx v26, %0, v0" ::"r"(f1_buf));
  asm volatile("vmacc.vx v28, %0, v0" ::"r"(f0_buf));
  f0_buf = f[0];
  asm volatile("vmacc.vx v26, %0, v4" ::"r"(f2_buf));
  f2_buf = f[6];
  asm volatile("vse32.v  v26, (%0); add %0, %0, %1" : "+&r"(o) : "r"(lwo));
  asm volatile("vmacc.vx v28, %0, v4" ::"r"(f1_buf));
  f1_buf = f[3];

  // Bump the input ptr
  i_ += 2 * (N + F - 1);
  asm volatile("vle32.v v2, (%0)" :: "r"(i_));

  //////////////////////////
  // Row (I-1)-3 -> (I-1) //
  //////////////////////////

  for (int32_t ch = 0; ch < C; ++ch) {

    // Point to the first element of the channel ch
    i__ = i_ + ich_len * ch;

    // Start calculating the next pointers to the elements to be slided in
    i_slide_ptr_0 = i__ + n_;

    // Main kernel, unrolled by 2
    int32_t sld_elem = *i_slide_ptr_0++;
    // Two base indexes because of the unrolling
    // Point to the first element of the current column (k+1) of the current
    // channel (ch) of the filter (f)
    int32_t base_idx_1 = 1 + (fch_len * ch);
    // Point to the first element of the current column (k+2) of the current
    // channel (ch) of the filter (f)
    int32_t base_idx_2 = 2 + (fch_len * ch);

    asm volatile("vslide1down.vx v0, v2, %0" ::"r"(sld_elem));
    sld_elem = *i_slide_ptr_0++;
    asm volatile("vmacc.vx v28, %0, v2" ::"r"(f2_buf));
    f2_buf = f[6 + base_idx_1];

    asm volatile("vslide1down.vx v2, v0, %0" ::"r"(sld_elem));
    asm volatile("vmacc.vx v28, %0, v0" ::"r"(f2_buf));
    f2_buf = f[6 + base_idx_2];

    if (ch != C - 1) {
      int32_t base_idx_0 = fch_len * (ch + 1);

      i__ = i_ + ich_len * (ch + 1);
      asm volatile("vle32.v v30, (%0)" :: "r"(i__));

      asm volatile("vmacc.vx v28, %0, v2" ::"r"(f2_buf));
      f2_buf = f[6 + base_idx_0];

      asm volatile("vmv.v.v v2 v30");
    }
  }

  asm volatile("vmacc.vx v28, %0, v2" ::"r"(f2_buf));
  asm volatile("vse32.v  v28, (%0)" :: "r"(o));
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
