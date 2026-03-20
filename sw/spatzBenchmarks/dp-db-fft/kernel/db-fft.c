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

// Author: Diyou Shen, ETH Zurich

#include "db-fft.h"

// ---------------------------------------------------------------------------
// Phase 1 kernel: one butterfly stage for one section
//
// Operates entirely on L1 data. Reads upper/lower wings from l1_src,
// writes results to l1_dst (both contiguous in L1).
//
// Parameters:
//   l1_src   : pointer to section's input in L1
//              layout: [Re_upper: N/S][Re_lower: N/S][Im_upper: N/S][Im_lower: N/S]
//   l1_dst   : pointer to section's output in L1 (same layout as l1_src)
//   l1_twi   : pointer to twiddles in L1
//              layout: [Re_twi: N/S][Im_twi: N/S]
//   nfft_sec : number of elements per section = N/S
// ---------------------------------------------------------------------------
void fft_p1_kernel(const double *l1_src, double *l1_dst,
                   const double *l1_twi, const unsigned int nfft_sec) {

  // nfft_sec/2 butterfly pairs per section
  size_t avl = nfft_sec / 2;
  size_t vl;

  // Input pointers: upper wing [0..nfft_sec/2-1], lower wing [nfft_sec/2..nfft_sec-1]
  const double *re_u_i = l1_src;
  const double *re_l_i = l1_src +     nfft_sec / 2;
  const double *im_u_i = l1_src + 2 * nfft_sec / 2;
  const double *im_l_i = l1_src + 3 * nfft_sec / 2;

  // Output pointers (same layout)
  double *re_u_o = l1_dst;
  double *re_l_o = l1_dst +     nfft_sec / 2;
  double *im_u_o = l1_dst + 2 * nfft_sec / 2;
  double *im_l_o = l1_dst + 3 * nfft_sec / 2;

  // Twiddle pointers: nfft_sec/2 Re then nfft_sec/2 Im
  const double *re_twi = l1_twi;
  const double *im_twi = l1_twi + nfft_sec / 2;

  for (; avl > 0; avl -= vl) {
    asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));

    // Load upper and lower wings (real)
    asm volatile("vle64.v v0, (%0)" ::"r"(re_u_i)); // v0: Re upper
    re_u_i += vl;
    asm volatile("vle64.v v4, (%0)" ::"r"(re_l_i)); // v4: Re lower
    re_l_i += vl;

    // Butterfly real
    asm volatile("vfadd.vv v16, v0, v4"); // v16: Re upper out
    asm volatile("vfsub.vv v0,  v0, v4"); // v0:  Re lower out (pre-twiddle)

    // Load upper and lower wings (imag)
    asm volatile("vle64.v v8,  (%0)" ::"r"(im_u_i)); // v8:  Im upper
    im_u_i += vl;
    asm volatile("vle64.v v12, (%0)" ::"r"(im_l_i)); // v12: Im lower
    im_l_i += vl;

    // Butterfly imag
    asm volatile("vfadd.vv v20, v8, v12"); // v20: Im upper out
    asm volatile("vfsub.vv v4,  v8, v12"); // v4:  Im lower out (pre-twiddle)

    // Load twiddles
    asm volatile("vle64.v v8,  (%0)" ::"r"(re_twi)); // v8:  Re twiddle
    re_twi += vl;
    asm volatile("vle64.v v12, (%0)" ::"r"(im_twi)); // v12: Im twiddle
    im_twi += vl;

    // Twiddle lower wing:
    // re_l = v0 * v8  - v4 * v12
    // im_l = v0 * v12 + v4 * v8
    asm volatile("vfmul.vv  v24, v0, v8");
    asm volatile("vfnmsac.vv v24, v4, v12"); // v24: Re lower out twiddled
    asm volatile("vfmul.vv  v28, v0, v12");
    asm volatile("vfmacc.vv v28, v4, v8");   // v28: Im lower out twiddled

    // Store all four outputs contiguously in L1
    asm volatile("vse64.v v16, (%0)" ::"r"(re_u_o));
    re_u_o += vl;
    asm volatile("vse64.v v24, (%0)" ::"r"(re_l_o));
    re_l_o += vl;
    asm volatile("vse64.v v20, (%0)" ::"r"(im_u_o));
    im_u_o += vl;
    asm volatile("vse64.v v28, (%0)" ::"r"(im_l_o));
    im_l_o += vl;
  }
}

// ---------------------------------------------------------------------------
// Phase 2 kernel: full sub-FFT on one section (log2(N/S) stages)
//
// Operates entirely on L1 data. Final stage uses strided store to write
// results interleaved into the global output buffer.
//
// Im part of each buffer is addressed at base + nfft_sec (tight packing),
// so each ping/pong buffer occupies exactly 2*nfft_sec doubles in L1.
//
// Parameters:
//   s          : pointer to section samples in L1 (ping buffer)
//                layout: [Re: nfft_sec][Im: nfft_sec]  (Im at base+nfft_sec)
//   buf        : pointer to second buffer in L1 (pong buffer, same layout)
//   twi        : pointer to section twiddles in L1
//                layout: [Re: NTWI_P2][Im: NTWI_P2]
//                where NTWI_P2 = log2(nfft_sec) * nfft_sec / 2
//   out        : base pointer for strided output (section's start in output)
//   seq_idx    : precomputed shuffle indices for intermediate stages
//   nfft_sec   : number of elements per section = N/S
//   nfft       : total FFT size N (used only for final strided output Im offset)
//   log2_sec   : log2(nfft_sec) = number of Phase 2 stages
//   stride     : byte stride for final vsse64 (= S * sizeof(double))
//   log2_S     : log2(S), used to advance output pointer after each vsse
//   ntwi       : NTWI_P2 (number of twiddle elements per Re or Im block)
// ---------------------------------------------------------------------------
void fft_p2_kernel(double *s, double *buf, const double *twi, double *out,
                   const uint16_t *seq_idx,
                   const unsigned int nfft_sec, const unsigned int nfft,
                   const unsigned int log2_sec,  const unsigned int stride,
                   const unsigned int log2_S,    const unsigned int ntwi) {

  const double *re_twi = twi;
  const double *im_twi = twi + ntwi;

  size_t avl;
  size_t vl;

  for (unsigned int bf = 0; bf < log2_sec; ++bf) {
    avl = nfft_sec >> 1;

    // Ping-pong between s and buf each stage
    // On even stages: read from buf, write to s
    // On odd  stages: read from s,   write to buf
    // Last stage always writes to buf (final result)
    const double *i_buf = !(bf & 1) ? buf : s;
    double       *o_buf = !(bf & 1) ? s   : buf;
    if (bf == log2_sec - 1)
      o_buf = buf;

    // Input pointers
    // Im part is at base + nfft_sec (tight packing within L1 slot)
    const double *re_u_i = i_buf;
    const double *im_u_i = i_buf + nfft_sec;
    const double *re_l_i = re_u_i + (nfft_sec >> 1);
    const double *im_l_i = im_u_i + (nfft_sec >> 1);

    // Output pointers (for non-final indexed stores)
    double *re_u_o = o_buf;
    double *im_u_o = o_buf + nfft_sec;
    double *re_l_o = re_u_o + (nfft_sec >> 1);
    double *im_l_o = im_u_o + (nfft_sec >> 1);

    // Strided store pointers for final stage (write into global out array)
    double *re_u_s = out;
    double *im_u_s = out + nfft;
    double *re_l_s = re_u_s + (nfft >> 1);
    double *im_l_s = im_u_s + (nfft >> 1);

    for (; avl > 0; avl -= vl) {
      asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));

      // Load upper and lower wings (real)
      asm volatile("vle64.v v0, (%0)" ::"r"(re_u_i)); // v0: Re upper
      re_u_i += vl;
      asm volatile("vle64.v v4, (%0)" ::"r"(re_l_i)); // v4: Re lower
      re_l_i += vl;

      // Butterfly real
      asm volatile("vfadd.vv v16, v0, v4"); // v16: Re upper out
      asm volatile("vfsub.vv v0,  v0, v4"); // v0:  Re lower out (pre-twiddle)

      // Load upper and lower wings (imag)
      asm volatile("vle64.v v8,  (%0)" ::"r"(im_u_i)); // v8:  Im upper
      im_u_i += vl;
      asm volatile("vle64.v v12, (%0)" ::"r"(im_l_i)); // v12: Im lower
      im_l_i += vl;

      // Butterfly imag
      asm volatile("vfadd.vv v20, v8, v12"); // v20: Im upper out
      asm volatile("vfsub.vv v4,  v8, v12"); // v4:  Im lower out (pre-twiddle)

      if (bf == log2_sec - 1) {
        // Final stage: strided store directly into output (bit-reversal
        // ordering comes for free from the section's strided write pattern)
        asm volatile("vsse64.v v16, (%0), %1" ::"r"(re_u_s), "r"(stride));
        asm volatile("vsse64.v v20, (%0), %1" ::"r"(im_u_s), "r"(stride));
        asm volatile("vsse64.v v0,  (%0), %1" ::"r"(re_l_s), "r"(stride));
        asm volatile("vsse64.v v4,  (%0), %1" ::"r"(im_l_s), "r"(stride));
        // Advance strided output pointers by vl * S elements
        re_u_s += (vl << log2_S);
        im_u_s += (vl << log2_S);
        re_l_s += (vl << log2_S);
        im_l_s += (vl << log2_S);
      } else {
        // Non-final stage: twiddle lower wing, then indexed scatter
        asm volatile("vle64.v v8,  (%0)" ::"r"(re_twi)); // v8:  Re twiddle
        re_twi += vl;
        asm volatile("vle64.v v12, (%0)" ::"r"(im_twi)); // v12: Im twiddle
        im_twi += vl;

        // re_l = v0 * v8  - v4 * v12
        // im_l = v0 * v12 + v4 * v8
        asm volatile("vfmul.vv  v24, v0, v8");
        asm volatile("vfnmsac.vv v24, v4, v12"); // v24: Re lower out twiddled
        asm volatile("vfmul.vv  v28, v0, v12");
        asm volatile("vfmacc.vv v28, v4, v8");   // v28: Im lower out twiddled

        // Indexed scatter using precomputed seq_idx
        asm volatile("vle16.v v12, (%0)" ::"r"(seq_idx));
        seq_idx += vl;

        // Reset output base pointers for indexed store
        re_u_o = o_buf;
        im_u_o = o_buf + nfft_sec;
        re_l_o = re_u_o + (nfft_sec >> 2);
        im_l_o = im_u_o + (nfft_sec >> 2);

        asm volatile("vsuxei16.v v16, (%0), v12" ::"r"(re_u_o));
        asm volatile("vsuxei16.v v20, (%0), v12" ::"r"(im_u_o));
        asm volatile("vsuxei16.v v24, (%0), v12" ::"r"(re_l_o));
        asm volatile("vsuxei16.v v28, (%0), v12" ::"r"(im_l_o));
      }
    }
  }
}
