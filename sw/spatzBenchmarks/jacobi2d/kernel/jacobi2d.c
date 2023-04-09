/*
OHIO STATE UNIVERSITY SOFTWARE DISTRIBUTION LICENSE

PolyBench/C, a collection of benchmarks containing static control
parts (the "Software")
Copyright (c) 2010-2016, Ohio State University. All rights reserved.

The Software is available for download and use subject to the terms
and conditions of this License.  Access or use of the Software
constitutes acceptance and agreement to the terms and conditions of
this License.  Redistribution and use of the Software in source and
binary forms, with or without modification, are permitted provided
that the following conditions are met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the capitalized paragraph below.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the capitalized paragraph below in
the documentation and/or other materials provided with the
distribution.

3. The name of Ohio State University, or its faculty, staff or
students may not be used to endorse or promote products derived from
the Software without specific prior written permission.

This software was produced with support from the U.S. Defense Advanced
Research Projects Agency (DARPA), the U.S. Department of Energy (DoE)
and the U.S. National Science Foundation. Nothing in this work should
be construed as reflecting the official policy or position of the
Defense Department, the United States government or Ohio State
University.

THIS SOFTWARE HAS BEEN APPROVED FOR PUBLIC RELEASE, UNLIMITED
DISTRIBUTION.  THE SOFTWARE IS PROVIDED ?AS IS? AND WITHOUT ANY
EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, WARRANTIES OF ACCURACY, COMPLETENESS, NONINFRINGEMENT,
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
ACCESS OR USE OF THE SOFTWARE IS ENTIRELY AT THE USER?S RISK.  IN NO
EVENT SHALL OHIO STATE UNIVERSITY OR ITS FACULTY, STAFF OR STUDENTS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  THE SOFTWARE USER SHALL
INDEMNIFY, DEFEND AND HOLD HARMLESS OHIO STATE UNIVERSITY AND ITS
FACULTY, STAFF AND STUDENTS FROM ANY AND ALL CLAIMS, ACTIONS, DAMAGES,
LOSSES, LIABILITIES, COSTS AND EXPENSES, INCLUDING ATTORNEYS? FEES AND
COURT COSTS, DIRECTLY OR INDIRECTLY ARISING OUT OF OR IN CONNECTION
WITH ACCESS OR USE OF THE SOFTWARE.
*/

/**
 * This version is stamped on May 10, 2016
 *
 * Contact:
 *   Louis-Noel Pouchet <pouchet.ohio-state.edu>
 *   Tomofumi Yuki <tomofumi.yuki.fr>
 *
 * Web address: http://polybench.sourceforge.net
 */
/* jacobi-2d.c: this file is part of PolyBench/C */

/*************************************************************************
 * RISC-V Vectorized Version
 * Author: Cristóbal Ramírez Lazo
 * email: cristobal.ramirez@bsc.es
 * Barcelona Supercomputing Center (2020)
 *************************************************************************/

// Porting to Spatz SW environment and Optimization
// Author: Matteo Perotti, ETH Zurich, <mperotti@iis.ee.ethz.ch>

#include "jacobi2d.h"

// Optimized version of the jacobi2d kernel
// 1) Preload the coefficients, before each vstore
// 2) Eliminate WAW and WAR hazards
// 3) Unroll the loop and use multiple buffers
void jacobi2d(double *a, double *b, const unsigned int start_y,
              const unsigned int start_x, const unsigned int size_y,
              const unsigned int size_x, const unsigned int C) {
  double izq_0, izq_1, izq_2;
  double der_0, der_1, der_2;

  // Avoid division. 1/5 == 0.2
  const double five_ = 0.2;

  size_t gvl;

  asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(gvl) : "r"(size_x));

  for (unsigned int j = start_x; j < size_x + start_x; j = j + gvl) {
    asm volatile("vsetvli %0, %1, e64, m4, ta, ma"
                 : "=r"(gvl)
                 : "r"(size_x - j + start_x));
    asm volatile("vle64.v v0, (%0)" ::"r"(&a[0 * C + j])); // v0 top
    asm volatile("vle64.v v4, (%0)" ::"r"(&a[1 * C + j])); // v4 middle
    asm volatile("vle64.v v8, (%0)" ::"r"(&a[2 * C + j])); // v8 bottom

    // Look ahead and load the next coefficients
    // Do it before vector stores
    izq_0 = a[1 * C + j - 1];
    der_0 = a[1 * C + j + gvl];

    for (unsigned int i = start_y; i <= size_y; i += 3) {
      asm volatile("vfslide1up.vf v24, v4, %0" ::"f"(izq_0));
      asm volatile("vfslide1down.vf v28, v4, %0" ::"f"(der_0));
      asm volatile("vfadd.vv v12, v4, v0");   // middle - top
      asm volatile("vfadd.vv v12, v12, v8");  // bottom
      asm volatile("vfadd.vv v12, v12, v24"); // left
      if ((i + 1) <= size_y) {
        asm volatile(
            "vle64.v v0, (%0)" ::"r"(&a[((i + 1) + 1) * C + j])); // v0 top
      }
      asm volatile("vfadd.vv v12, v12, v28"); // right
      asm volatile("vfmul.vf v12, v12, %0" ::"f"(five_));
      if ((i + 1) <= size_y) {
        izq_1 = a[(i + 1) * C + j - 1];
        der_1 = a[(i + 1) * C + j + gvl];
      }
      asm volatile("vse64.v v12, (%0)" ::"r"(&b[i * C + j]));

      if ((i + 1) <= size_y) {
        asm volatile("vfslide1up.vf v24, v8, %0" ::"f"(izq_1));
        asm volatile("vfslide1down.vf v28, v8, %0" ::"f"(der_1));
        asm volatile("vfadd.vv v16, v4, v8");   // middle - top
        asm volatile("vfadd.vv v16, v16, v0");  // bottom
        asm volatile("vfadd.vv v16, v16, v24"); // left
        if ((i + 2) <= size_y) {
          asm volatile(
              "vle64.v v4, (%0)" ::"r"(&a[((i + 2) + 1) * C + j])); // v4 middle
        }
        asm volatile("vfadd.vv v16, v16, v28"); // right
        asm volatile("vfmul.vf v16, v16, %0" ::"f"(five_));
        if ((i + 2) <= size_y) {
          izq_2 = a[(i + 2) * C + j - 1];
          der_2 = a[(i + 2) * C + j + gvl];
        }
        asm volatile("vse64.v v16, (%0)" ::"r"(&b[(i + 1) * C + j]));

        if ((i + 2) <= size_y) {
          asm volatile("vfslide1up.vf v24, v0, %0" ::"f"(izq_2));
          asm volatile("vfslide1down.vf v28, v0, %0" ::"f"(der_2));
          asm volatile("vfadd.vv v20, v0, v8");   // middle - top
          asm volatile("vfadd.vv v20, v20, v4");  // bottom
          asm volatile("vfadd.vv v20, v20, v24"); // left
          if ((i + 3) <= size_y) {
            asm volatile("vle64.v v8, (%0)" ::"r"(
                &a[((i + 3) + 1) * C + j])); // v8 bottom
          }
          asm volatile("vfadd.vv v20, v20, v28"); // right
          asm volatile("vfmul.vf v20, v20, %0" ::"f"(five_));
          if ((i + 3) <= size_y) {
            izq_0 = a[(i + 3) * C + j - 1];
            der_0 = a[(i + 3) * C + j + gvl];
          }
          asm volatile("vse64.v v20, (%0)" ::"r"(&b[(i + 2) * C + j]));
        }
      }
    }
  }
}
