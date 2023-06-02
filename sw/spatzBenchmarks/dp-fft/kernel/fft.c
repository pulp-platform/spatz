// Copyright 2021 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Matteo Perotti, ETH Zurich

#include "fft.h"

// Single-core FFT
// DIF Cooley-Tukey algorithm
// At every iteration, we store indexed
// todo: simplify the last iteration, which do not require twiddle factors
void fft_sc(double *s, double *buf, const double *twi, const uint16_t *seq_idx,
            const uint16_t *rev_idx, const unsigned int nfft) {

  // log2(nfft). We can also pass it directly as a function argument
  const unsigned int log2_nfft = 31 - __builtin_clz(nfft);

  // Real part of the twiddles
  const double *re_t = twi;
  // Img part of the twiddles
  // If the multiplication is slow, pass via func args directly
  const double *im_t = twi + (nfft >> 1) * log2_nfft;

  // Bit-reverse indices
  const uint16_t *idx_ = rev_idx;

  // Keep half of the samples in a vector register
  size_t avl;
  size_t vl;

  // Loop through the butterfly stages
  // The last loop is easily optimizable (we know the twiddle factors)
  for (unsigned int bf = 0; bf < log2_nfft; ++bf) {
    // Keep half of the samples in a vector register
    avl = nfft >> 1;
    asm volatile("vsetvli %0, %1, e64, m2, ta, ma" : "=r"(vl) : "r"(avl));

    // Swap between the two buffers
    const double *i_buf = (bf & 1) ? buf : s;
    double *o_buf = (bf & 1) ? s : buf;

    // Last iteration
    if (bf == log2_nfft - 1)
      o_buf = buf;

    // Update pointers
    const double *re_u_i = i_buf;
    const double *im_u_i = i_buf + nfft;
    const double *re_l_i = re_u_i + (nfft >> 1);
    const double *im_l_i = im_u_i + (nfft >> 1);
    double *re_u_o = o_buf;
    double *im_u_o = o_buf + nfft;
    double *re_l_o = re_u_o + (nfft >> 1);
    double *im_l_o = im_u_o + (nfft >> 1);

    // Stripmine the whole vector for this butterfly stage
    for (; avl > 0; avl -= vl) {
      // Load a portion of the vector (real part)
      asm volatile("vle64.v v0, (%0);" ::"r"(re_u_i)); // v0: Re upper wing
      re_u_i += vl;
      asm volatile("vle64.v v2, (%0);" ::"r"(re_l_i)); // v2: Re lower wing
      re_l_i += vl;

      asm volatile("vfadd.vv v8, v0, v2"); // v8: Re butterfly output upper wing
      asm volatile(
          "vfsub.vv v10, v0, v2"); // v10: Re butterfly output upper wing

      // Load a portion of the vector (imag part)
      asm volatile("vle64.v v4, (%0);" ::"r"(im_u_i)); // v4: Im upper wing
      im_u_i += vl;
      asm volatile("vle64.v v6, (%0);" ::"r"(im_l_i)); // v6: Im lower wing
      im_l_i += vl;

      // Store the results of the last iteration
      if (avl != (nfft >> 1)) {
        asm volatile("vsuxei16.v v20, (%0), v24" ::"r"(re_l_o));
        asm volatile("vsuxei16.v v22, (%0), v24" ::"r"(im_l_o));
      }

      asm volatile(
          "vfadd.vv v12, v4, v6"); // v12: Im butterfly output upper wing
      asm volatile(
          "vfsub.vv v14, v4, v6"); // v14: Im butterfly output upper wing

      // Load the twiddle vector
      asm volatile("vle64.v v16, (%0);" ::"r"(re_t)); // v16: Re twi
      re_t += vl;
      asm volatile("vle64.v v18, (%0);" ::"r"(im_t)); // v18: Im twi
      im_t += vl;

      // Twiddle the lower wing
      asm volatile("vfmul.vv v20, v10, v16");
      asm volatile("vfmul.vv v22, v14, v16");
      asm volatile("vfnmsac.vv v20, v14, v18"); // v20: Re butterfly output
                                                // twiddled lower wing
      asm volatile("vfmacc.vv v22, v10, v18");  // v22: Im butterfly output
                                                // twiddled lower wing

      // Load the index vector. If last step, it's the bitrev index vector.
      // Otherwise, it's the helper index for the permutations (this is a mask
      // vector)
      if (bf == log2_nfft - 1) {
        asm volatile(
            "vle16.v v24, (%0);" ::"r"(idx_)); // v24: bit-reversal indices
        idx_ += vl;
        re_u_o = o_buf;
        im_u_o = o_buf + nfft;
        re_l_o = re_u_o + (nfft >> 1);
        im_l_o = im_u_o + (nfft >> 1);
      } else {
        // Load the sequential indices dirctly
        asm volatile("vle16.v v24, (%0)" ::"r"(seq_idx)); // v24: index vector
        seq_idx += vl;
        re_u_o = o_buf;
        im_u_o = o_buf + nfft;
        re_l_o = re_u_o + (nfft >> 2);
        im_l_o = im_u_o + (nfft >> 2);
      }

      asm volatile("vsuxei16.v v8, (%0), v24" ::"r"(re_u_o));
      asm volatile("vsuxei16.v v12, (%0), v24" ::"r"(im_u_o));
    }

    // Store the results of the last iteration
    asm volatile("vsuxei16.v v20, (%0), v24" ::"r"(re_l_o));
    asm volatile("vsuxei16.v v22, (%0), v24" ::"r"(im_l_o));
  }
}

// The first log2(n_cores) butterflies are special, then, we fall-back into
// the single-core case Hardcoded two-core implementation of FFT Now, the
// fall-back is done directly in the main function. Therefore, this function
// implements just the first butterfly stage of a 2-core implementation.
void fft_2c(double *s, const double *twi, const unsigned int nfft,
            const unsigned int cid) {
  // Log2(nfft). We can also pass it directly as a function argument
  unsigned int log2_nfft = 31 - __builtin_clz(nfft >> 1);

  // avl = (nfft/2) / n_cores
  size_t avl = nfft >> 2;
  size_t vl;

  // Real part of the twiddles
  const double *re_t = cid ? twi + (nfft >> 2) : twi;
  // Img part of the twiddles
  // If the multiplication is slow, pass via func args directly
  const double *im_t = cid ? twi + (nfft >> 2) * (log2_nfft + 3)
                           : twi + (nfft >> 2) * (log2_nfft + 2);

  asm volatile("vsetvli %0, %1, e64, m2, ta, ma" : "=r"(vl) : "r"(avl));

  // Overwrite the buffers
  const double *i_buf = s + cid * (nfft >> 2);
  double *o_buf = s + cid * (nfft >> 2);

  // Update pointers
  const double *re_u_i = i_buf;
  const double *im_u_i = i_buf + nfft;
  const double *re_l_i = re_u_i + (nfft >> 1);
  const double *im_l_i = im_u_i + (nfft >> 1);
  double *re_u_o = (double *)re_u_i;
  double *im_u_o = (double *)im_u_i;
  double *re_l_o = (double *)re_l_i;
  double *im_l_o = (double *)im_l_i;

  // Stripmine the whole vector for this butterfly stage
  for (; avl > 0; avl -= vl) {
    asm volatile("vsetvli %0, %1, e64, m2, ta, ma" : "=r"(vl) : "r"(avl));
    // Load a portion of the vector
    asm volatile("vle64.v v0, (%0);" ::"r"(re_u_i)); // v0: Re upper wing
    re_u_i += vl;
    asm volatile("vle64.v v2, (%0);" ::"r"(re_l_i)); // v2: Re lower wing
    re_l_i += vl;
    asm volatile("vle64.v v4, (%0);" ::"r"(im_u_i)); // v4: Im upper wing
    im_u_i += vl;
    asm volatile("vle64.v v6, (%0);" ::"r"(im_l_i)); // v6: Im lower wing
    im_l_i += vl;

    // Butterfly upper wing
    asm volatile("vfadd.vv v8, v0, v2");  // v8: Re butterfly output upper wing
    asm volatile("vfadd.vv v12, v4, v6"); // v12: Im butterfly output upper wing
    // Butterfly lower wing
    asm volatile("vfsub.vv v10, v0, v2"); // v10: Re butterfly output upper wing
    asm volatile("vfsub.vv v14, v4, v6"); // v14: Im butterfly output upper wing

    // Load the twiddle vector
    asm volatile("vle64.v v16, (%0);" ::"r"(re_t)); // v16: Re twi
    re_t += vl;
    asm volatile("vle64.v v18, (%0);" ::"r"(im_t)); // v18: Im twi
    im_t += vl;

    // Twiddle the lower wing
    asm volatile("vfmul.vv v20, v10, v16");
    asm volatile("vfnmsac.vv v20, v14, v18"); // v20: Re butterfly output
                                              // twiddled lower wing
    asm volatile("vfmul.vv v22, v10, v18");
    asm volatile("vfmacc.vv v22, v14, v16"); // v22: Im butterfly output
                                             // twiddled lower wing

    // Store 1:1 the output result
    asm volatile("vse64.v v8, (%0)" ::"r"(re_u_o));
    re_u_o += vl;
    asm volatile("vse64.v v12, (%0)" ::"r"(im_u_o));
    im_u_o += vl;
    asm volatile("vse64.v v20, (%0)" ::"r"(re_l_o));
    re_l_o += vl;
    asm volatile("vse64.v v22, (%0)" ::"r"(im_l_o));
    im_l_o += vl;
  }
}
