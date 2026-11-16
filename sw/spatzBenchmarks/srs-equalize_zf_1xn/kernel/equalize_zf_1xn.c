#include "equalize_zf_1xn.h"
#include <stddef.h>
#include <snrt.h>
#include <float.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

void equalize_zf_1xn(float *symbols_out, float *nvars_out, const uint16_t *ch_symbols,
    const uint16_t *ch_estimates, const float *noise_var_est, float tx_scaling, uint32_t nof_re, uint32_t rx_ports) {
  uint32_t remaining = nof_re;
  uint32_t vl;
  uint32_t stride = 4;
  uint32_t stride2 = 8;
  uint32_t pos_normal = 1 << 6;
  const float cls_pos_normal = 0x1p-143f;
  uint32_t inf = 0x7F800000;
  float one = 1.0f;
  // temp var
  uint32_t sub_min = 0xFF800000;
  uint32_t lim     = 0x7F000000;
  asm volatile("csrwi 0x800, 0x2");
  do {
  asm volatile("vsetvli %0, %1, e32, m4, ta, mu" : "=r"(vl) : "r"(remaining));
  asm volatile("vmv.v.i v8,  0");
  asm volatile("vmv.v.i v12, 0");
  asm volatile("vmv.v.i v16, 0");
  asm volatile("vmv.v.i v20, 0");
  asm volatile("vsetvli %0, %1, e16, m2, ta, mu" : "=r"(vl) : "r"(remaining));
  //asm volatile("vlse16.v v4, (%0)" :: "r"(ch_estimates) : "memory");
  // Get a norm
  //asm volatile("vfwdotp.vv v8, v4, v4");
  //Unforunately I need many extra instructions to do th next step:
  //re_out += re_in * std::conj(ch_est);
  //Solution: Complex multiplication.
  // I don't know yet how to identify that odd symbols are Im -> different mul
  for (uint32_t i_port = 0; i_port < rx_ports; i_port++) {
    uint32_t bits = *(const volatile uint32_t *)&noise_var_est[i_port];
    if ((bits - 0x00800000u) >= 0x7F000000u) continue;
    float nv = noise_var_est[i_port];
    asm volatile("vlse16.v v2, (%0), %1" :: "r"(ch_estimates + i_port * 2 * nof_re), "r"(stride) : "memory");    // load Re part
    asm volatile("vlse16.v v4, (%0), %1" :: "r"(ch_estimates + i_port * 2 * nof_re + 1), "r"(stride) : "memory"); //load Im part
    asm volatile("vlse16.v v6, (%0), %1" :: "r"(ch_symbols + i_port * 2 * nof_re + 1), "r"(stride) : "memory"); //load Im part
    //Calculate norm
    asm volatile("vfwmul.vv v24, v2, v2");
    asm volatile("vfwmacc.vv v24, v4, v4"); 
    asm volatile("vsetvli %0, %1, e32, m4, ta, mu" : "=r"(vl) : "r"(remaining));
    //check the norm
    asm volatile("vfclass.v v28, v24");
    asm volatile("vmfeq.vf v0, v28, %0" :: "f"(cls_pos_normal));
    //asm volatile("vadd.vx   v28, v24, %0" :: "r"(sub_min));
    //asm volatile("vmsltu.vx v0, v28, %0"  :: "r"(lim));
    //ch_mod_sq
    asm volatile("vfadd.vv v8, v8, v24, v0.t");
    //nvar_acc
    asm volatile("vfmacc.vf v12, %0, v24, v0.t" :: "f"(nv));

    asm volatile("vsetvli %0, %1, e16, m2, ta, mu" : "=r"(vl) : "r"(remaining));
    // load re_in symbols
    asm volatile("vlse16.v v28, (%0), %1" :: "r"(ch_symbols + i_port * 2 * nof_re), "r"(stride) : "memory");    // load Re part
    asm volatile("vfwmacc.vv v20, v6, v2, v0.t");
    asm volatile("vfwmacc.vv v16, v28, v2, v0.t");
    // Im part
    asm volatile("vfwnmsac.vv v20, v28, v4, v0.t");
    // Re part
    asm volatile("vfwmacc.vv v16, v6, v4, v0.t");
    
  }
  // v16 - re_out Re part, v20 - Im part, v12 - nvar_acc, v8 - ch_mod_sq
  asm volatile("vsetvli zero, %0, e32, m4, ta, mu" :: "r"(vl));
  asm volatile("vfmv.v.f  v4, %0" :: "f"(one));
  // d_pinv
  asm volatile("vfmul.vf v24, v8, %0" :: "f"(tx_scaling));
  // isnormal(d_pinv)
  asm volatile("vfclass.v v28, v24");
  asm volatile("vfclass.v v8, v12");
  asm volatile("vmfeq.vf v1, v28, %0" :: "f"(cls_pos_normal));
  //asm volatile("vadd.vx   v28, v24, %0" :: "r"(sub_min));
  //asm volatile("vmsltu.vx v1, v28, %0"  :: "r"(lim));

  //asm volatile("vadd.vx   v28, v12, %0" :: "r"(sub_min));
  //asm volatile("vmsltu.vx v2, v28, %0"  :: "r"(lim));
  asm volatile("vmfeq.vf v2, v8, %0" :: "f"(cls_pos_normal));
  asm volatile("vfdiv.vv v28, v4, v24");
  asm volatile("vmand.mm v0, v1, v2");
  // symbols_out = 0
  asm volatile("vmv.v.i v4, 0");
  asm volatile("vmv.v.i v24, 0");
  // symbols_out[i_re] = re_out * d_pinv_rcp;
  // Re part
  asm volatile("vfmul.vv v4, v16, v28, v0.t");
  asm volatile("vsse32.v v4, (%0), %1" :: "r"(symbols_out), "r"(stride2) : "memory");
  asm volatile("vmv.v.x v16, %0" :: "r"(inf));
  // Im part
  asm volatile("vfmul.vv  v24, v20, v28, v0.t");
  asm volatile("vsse32.v v24, (%0), %1" :: "r"(symbols_out+1), "r"(stride2) : "memory");
  //  nvars_out[i_re] = nvar_acc * d_pinv_rcp * d_pinv_rcp;
  asm volatile("vfmul.vv v8, v12, v28");

  // nvars_out - inf
  asm volatile("vfmul.vv v16, v8, v28, v0.t");
  asm volatile("vse32.v v16, (%0)" :: "r"(nvars_out));

  remaining    -= vl;
  ch_symbols   += 2 * vl;
  ch_estimates += 2 * vl;
  symbols_out  += 2 * vl;
  nvars_out    += vl;

  } while (remaining > 0);
}
