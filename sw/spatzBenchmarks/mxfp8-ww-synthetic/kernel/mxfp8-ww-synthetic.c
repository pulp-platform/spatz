// Copyright 2025 ETH Zurich and University of Bologna.
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

// Author: Max Wipfli <mwipfli@ethz.ch>

#include "mxfp8-ww-synthetic.h"

float mxfp8_mxdotp_ww_synthetic(const uint32_t N) {
  if (N == 0)
    return 0.0f;

  // v0-v2: accumulator
  uint32_t n_vl;
  asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(n_vl) : "r"(N));
  asm volatile("vmv.v.i v0, 0");

  // 2x FP8 operands   (a):       v8-v11,  v12-v15 (EMUL=4)
  // 2x FP8 operands   (b):       v16-v19, v20-v23 (EMUL=4)
  // 2x scale operands (a_scale): v24, v26 (EMUL=1/2)
  // 2x scale operands (b_scale): v28, v30 (EMUL=1/2)
  // NOTE: Scale register chosen s.t. there are no bank conflict with the
  //       element registers
  // - vs1 should not conflict with vs4
  // - vs2 should not conflict with vs3

  // dummy values for elements and scales
  asm volatile("vfadd.vv  v8, v0, v0");
  asm volatile("vfadd.vv v10, v0, v0");
  asm volatile("vfadd.vv v12, v0, v0");
  asm volatile("vfadd.vv v14, v0, v0");
  asm volatile("vfadd.vv v16, v0, v0");
  asm volatile("vfadd.vv v18, v0, v0");
  asm volatile("vfadd.vv v20, v0, v0");
  asm volatile("vfadd.vv v22, v0, v0");
  asm volatile("vfadd.vv v24, v0, v0");
  asm volatile("vfadd.vv v26, v0, v0");


  for (uint32_t i = 0; i < N; i += 4 * 8 * n_vl) {
    // banks for all instructions: vd=0, vs1=1, vs2=2, vs3=3, vs4=3
    asm volatile("vmxdotp.ww v6,  v8, v16, v24, v28");
    asm volatile("vmxdotp.ww v4, v12, v20, v26, v30");
    asm volatile("vmxdotp.ww v2,  v8, v16, v24, v28");
    asm volatile("vmxdotp.ww v0, v12, v20, v26, v30");
  }

  float result;
  asm volatile("vfmv.f.s %0, v0" : "=f"(result));
  return result;
}
