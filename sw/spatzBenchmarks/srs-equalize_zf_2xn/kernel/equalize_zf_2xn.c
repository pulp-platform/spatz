#include "equalize_zf_2xn.h"
#include <stddef.h>
#include <snrt.h>
#include <float.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

void equalize_zf_2xn(float *eq_symbols, float *noise_vars, const uint16_t *ch_symbols,
    const uint16_t *ch_estimates, const float noise_var_est, float tx_scaling, uint32_t nof_re, uint32_t rx_ports) {
  uint32_t remaining = nof_re;
  uint32_t vl;
  uint32_t stride = 4;
  uint32_t stride2 = 8;
  uint32_t stride4 = 16;
  uint32_t pos_normal = 1 << 6;
  uint32_t inf = 0x7F800000;
  uint32_t normal = 0x42;
  float one = 1.0f;
  const float cls_pos_normal = 0x1p-143f;
  // temp var
  uint32_t sub_min = 0xFF800000;
  uint32_t lim     = 0x7F000000;
  uint32_t nof_layers = 2;
  uint32_t bits;
  memcpy(&bits, &noise_var_est, sizeof(bits));
  if ((bits - 0x00800000u) >= 0x7F000000u) {
    asm volatile("vsetvli  %0, %1, e32, m8, ta, mu" : "=r"(vl): "r"(remaining));
    asm volatile("vmv.v.i v8,  0");
    asm volatile("vmv.v.x v16,  %0" :: "r"(inf));
    do {
      asm volatile("vsetvli  %0, %1, e32, m8, ta, mu" : "=r"(vl): "r"(remaining));
      // eq_symbols = 0, noise_vars = inf,
      asm volatile("vse32.v v8, (%0)" :: "r"(eq_symbols) : "memory");
      asm volatile("vse32.v v8, (%0)" :: "r"(eq_symbols + nof_re) : "memory");
      asm volatile("vse32.v v8, (%0)" :: "r"(eq_symbols + 2 * nof_re) : "memory");
      asm volatile("vse32.v v8, (%0)" :: "r"(eq_symbols + 3 * nof_re) : "memory");
      asm volatile("vse32.v v16, (%0)" :: "r"(noise_vars) : "memory");
      asm volatile("vse32.v v16, (%0)" :: "r"(noise_vars + nof_re) : "memory");

      remaining  -= vl;
      eq_symbols += vl;
      noise_vars += vl;
      } while (remaining > 0);
    return;
  }
  float nv_scaled = noise_var_est / tx_scaling;

  asm volatile("csrwi 0x800, 0x2");
  do {
  asm volatile("vsetvli %0, %1, e16, m1, ta, mu" : "=r"(vl) : "r"(remaining));
  // For port 0 - vfwmul
  // load ch estimates layer 0, Re part 
  asm volatile("vlse16.v v1, (%0), %1" :: "r"(ch_estimates), "r"(stride) : "memory");
  // load ch estimates layer 1, Re part 
  asm volatile("vlse16.v v3, (%0), %1" :: "r"(ch_estimates + rx_ports * 2 * nof_re), "r"(stride) : "memory");
 
  // Calculate norm layer 1
  asm volatile("vfwmul.vv v6, v1, v1");
  asm volatile("vfwmul.vv v8, v3, v3");
  // load ch estimates layer 1, Im part 
  asm volatile("vlse16.v v4, (%0), %1" :: "r"(ch_estimates + rx_ports * 2 * nof_re + 1), "r"(stride) : "memory");
  // load ch estimates layer 0, Im part 
  asm volatile("vlse16.v v2, (%0), %1" :: "r"(ch_estimates + 1), "r"(stride) : "memory");
  asm volatile("vfwmul.vv v10, v1, v3");
  asm volatile("vfwmul.vv v12, v1, v4");
  asm volatile("vfwmacc.vv v8, v4, v4");
  // Calculate norm layer 2
  asm volatile("vfwmacc.vv v6, v2, v2");
  // load re_in layer 0, Re part 
  asm volatile("vlse16.v v5, (%0), %1" :: "r"(ch_symbols), "r"(stride) : "memory");
  // load re_in layer 0, Im part
  asm volatile("vlse16.v v14, (%0), %1" :: "r"(ch_symbols + 1), "r"(stride) : "memory");
;
  // Calculate the anti-diagonal coefficients, which are xi and xi_conj.
  // xi.re
  asm volatile("vfwmacc.vv v10, v2, v4");
  // xi.im
  asm volatile("vfwnmsac.vv v12, v2, v3");

  // matched_input channel 0
  asm volatile("vfwmul.vv v18, v1, v5");
  asm volatile("vfwmul.vv v22, v3, v5");
  asm volatile("vfwmul.vv v20, v1, v14");
  asm volatile("vfwmul.vv v24, v3, v14");
  asm volatile("vfwmacc.vv v18, v2, v14");
  // load ch estimates layer 0, Re part
  asm volatile("vlse16.v v1, (%0), %1" :: "r"(ch_estimates + 2 * nof_re), "r"(stride) : "memory");
  asm volatile("vfwnmsac.vv v20, v2, v5");
  // load ch estimates layer 1, Re part
  asm volatile("vlse16.v v3, (%0), %1" :: "r"(ch_estimates + (rx_ports + 1) * 2 * nof_re), "r"(stride) : "memory");
  // matched_input channel 1
  asm volatile("vfwmacc.vv v22, v4, v14");
  asm volatile("vfwnmsac.vv v24, v4, v5");
  // load ch estimates layer 0, Im part
  asm volatile("vlse16.v v2, (%0), %1" :: "r"(ch_estimates + 2 * nof_re + 1), "r"(stride) : "memory");

  for (uint32_t i_port = 1; i_port < rx_ports; i_port++) {
    bool nx = (i_port + 1) < rx_ports;
    asm volatile("vfwmacc.vv v6, v1, v1");
    asm volatile("vfwmacc.vv v8, v3, v3");
    // load ch estimates layer 1, Im part
    asm volatile("vlse16.v v4, (%0), %1" :: "r"(ch_estimates + (rx_ports + i_port) * 2 * nof_re + 1), "r"(stride) : "memory");
    asm volatile("vfwmacc.vv v10, v1, v3");
    asm volatile("vfwmacc.vv v6, v2, v2");
    // load re_in layer 0, Re part
    asm volatile("vlse16.v v5, (%0), %1" :: "r"(ch_symbols + i_port * 2 * nof_re), "r"(stride) : "memory");
    asm volatile("vfwmacc.vv v12, v1, v4");
    // load re_in layer 0, Im part
    asm volatile("vlse16.v v14, (%0), %1" :: "r"(ch_symbols + i_port * 2 * nof_re + 1), "r"(stride) : "memory");
    asm volatile("vfwmacc.vv v8, v4, v4");
    asm volatile("vfwmacc.vv v10, v2, v4");
    asm volatile("vfwnmsac.vv v12, v2, v3");
    // matched_input channel 0
    asm volatile("vfwmacc.vv v18, v1, v5");
    asm volatile("vfwmacc.vv v20, v1, v14");
    asm volatile("vfwmacc.vv v22, v3, v5");
    asm volatile("vfwmacc.vv v24, v3, v14");
    if (nx) asm volatile("vlse16.v v1, (%0), %1" :: "r"(ch_estimates + (i_port + 1) * 2 * nof_re), "r"(stride) : "memory");
    asm volatile("vfwmacc.vv v18, v2, v14");
    asm volatile("vfwnmsac.vv v20, v2, v5");
    if (nx) asm volatile("vlse16.v v3, (%0), %1" :: "r"(ch_estimates + (rx_ports + i_port + 1) * 2 * nof_re), "r"(stride) : "memory");
    // matched_input channel 1
    asm volatile("vfwmacc.vv v22, v4, v14");
    asm volatile("vfwnmsac.vv v24, v4, v5");
    if (nx) asm volatile("vlse16.v v2, (%0), %1" :: "r"(ch_estimates + (i_port + 1) * 2 * nof_re + 1), "r"(stride) : "memory");
  }
  
   asm volatile("vsetvli  %0, %1, e32, m2, ta, mu" : "=r"(vl): "r"(remaining));
  asm volatile("vmv.v.i v28, 0");
  asm volatile("vfmul.vv v26, v10, v10");
  asm volatile("vfmul.vv v16, v8, v18");
  asm volatile("vfmacc.vv v26, v12, v12");
  asm volatile("vmv.v.i v14, 0");
  asm volatile("vfnmsac.vv v16, v10, v22");
  asm volatile("vfmsac.vv v26, v6, v8");
  asm volatile("vfmacc.vv v16, v12, v24");
  asm volatile("vfmv.v.f v4, %0" :: "f"(one));
  asm volatile("vfmul.vf v26, v26, %0" :: "f"(tx_scaling));
  asm volatile("vfmul.vv v30, v8, v20");
  asm volatile("vfsgnjx.vv v2, v26, v26");
  asm volatile("vfnmsac.vv v30, v10, v24");
  asm volatile("vfclass.v v2, v2");
  asm volatile("vfnmsac.vv v30, v12, v22");
  asm volatile("vmfeq.vf v0, v2, %0" :: "f"(cls_pos_normal));
  // d_pinv_rcp, in place
  asm volatile("vfdiv.vv v4, v4, v26");
  // Re0, Im0
    asm volatile("vfmul.vv v28, v16, v4, v0.t");
  asm volatile("vfmul.vv v14, v30, v4, v0.t");
  // Re1 / Im1 pre-products, interleaved
  asm volatile("vfmul.vv v16, v6, v22");
  asm volatile("vsse32.v v28, (%0), %1" :: "r"(eq_symbols), "r"(stride4) : "memory");
  asm volatile("vfmul.vv v30, v6, v24");
  asm volatile("vsse32.v v14, (%0), %1" :: "r"(eq_symbols+1), "r"(stride4) : "memory");
  asm volatile("vfnmsac.vv v16, v10, v18");
  asm volatile("vmv.v.i v26, 0");
  asm volatile("vfnmsac.vv v30, v10, v20");
  asm volatile("vmv.v.i v24, 0");
  asm volatile("vfnmsac.vv v16, v12, v20");
  asm volatile("vfmacc.vv v30, v12, v18");
  // Re1, Im1
  asm volatile("vfmul.vv v26, v16, v4, v0.t");
  asm volatile("vmv.v.x v20, %0" :: "r"(inf));
  asm volatile("vfmul.vv v24, v30, v4, v0.t");
  asm volatile("vmv.v.x v18, %0" :: "r"(inf));
  asm volatile("vsse32.v v26, (%0), %1" :: "r"(eq_symbols+2), "r"(stride4) : "memory");
  // noise_vars L1 = nv * norm0 * d_pinv_rcp, L0 = nv * norm1 * d_pinv_rcp
  asm volatile("vfmul.vf v20, v6, %0, v0.t" :: "f"(nv_scaled));
  asm volatile("vsse32.v v24, (%0), %1" :: "r"(eq_symbols+3), "r"(stride4) : "memory");
  asm volatile("vfmul.vf v18, v8, %0, v0.t" :: "f"(nv_scaled));
  asm volatile("vfmul.vv v20, v20, v4, v0.t");
  asm volatile("vfmul.vv v18, v18, v4, v0.t");
  asm volatile("vsse32.v v20, (%0), %1" :: "r"(noise_vars + 1), "r"(stride2) : "memory");
  asm volatile("vsse32.v v18, (%0), %1" :: "r"(noise_vars), "r"(stride2) : "memory");

  remaining    -= vl;
  ch_symbols   += 2 * vl;
  ch_estimates += 2 * vl;
  eq_symbols   += 4 * vl;
  noise_vars   += 2 * vl;

  } while (remaining > 0);
}