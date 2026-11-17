// SPDX-License-Identifier: Apache-2.0

// One timed application call. The nonlinear producers write FP16 directly in
// the following GEMM's tiled operand layout, avoiding layout-conversion calls.
static __attribute__((noinline)) void run_mlp_pipeline(void) {
  // Constant layout offsets are staged in otherwise-unused scratch before
  // timing. LN consumes the first vector before overwriting lnpk.
  transformer_load_tile_offsets((const uint16_t *)lnpk);
  transformer_layernorm_na_pack(lnpk, z, C_SIZE, 1.0f / C_SIZE, T_SIZE);
  gather_gemm_1x4(w1, lnpk, h, HID_SIZE, T_SIZE, C_SIZE, 0);
#if !APP_FUSED
  wait_for_vector_store(h);
#endif
  uint16_t *offsets = (uint16_t *)w1; // first GEMM has consumed W1
  // hpk holds the second precomputed vector. Copy it into consumed W1 before
  // GELU starts overwriting hpk with packed activations.
  asm volatile("vsetvli zero, %0, e16, m2, ta, ma\n"
               "vle16.v v26, (%1)\n"
               "vse16.v v26, (%2)"
               :: "r"((uintptr_t)64), "r"(hpk), "r"(offsets)
               : "v26", "v27", "memory");
  gelu_pair_pack(hpk, h, offsets);
  wait_for_vector_store(hpk);
  // post_op=3 seeds every accumulator lane from the FP32 residual.  Bias2 is
  // the final FP16 reduction pair in hpk/w2, matching current O-POPE.
  gather_gemm_1x4(w2, hpk, z, C_SIZE, T_SIZE, GEMM2_K_SIZE, 3);
#if !APP_FUSED
  wait_for_vector_store(z);
#endif
}
