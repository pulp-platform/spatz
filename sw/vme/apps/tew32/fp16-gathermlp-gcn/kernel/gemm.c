// SPDX-License-Identifier: Apache-2.0
#include "gemm.h"
static __attribute__((noinline)) void run_gather_pipeline(unsigned fused) {
#if APP_FUSED
  (void)fused;
  gather_pool();
  gather_gemm_1x4(w1,pool,z,DOUT_SIZE,B_SIZE,D_SIZE,1);
#else
  (void)fused;
  gather_pool();
  gather_gemm_1x4(w1,pool,z,DOUT_SIZE,B_SIZE,D_SIZE,0);
#endif
}
