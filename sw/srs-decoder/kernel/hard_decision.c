#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

bool hard_decision_packed(uint8_t *packed, const int8_t *soft, uint32_t nbits)
{
  const uint32_t c33 = 0x33, c55 = 0x55;
  uint32_t vl, nbytes, result;

  asm volatile("vsetivli zero, 1, e8, m1, ta, ma");
  asm volatile("vmv.v.i v16, -1");

  while (nbits > 0) {
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(nbits));
    asm volatile("vle8.v v0, (%0)" :: "r"(soft) : "memory");
    asm volatile("vredminu.vs v16, v0, v16");
    asm volatile("vmsle.vi v8, v0, 0");

    nbytes = (vl + 7) >> 3;
    asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(nbytes));

    asm volatile("vsrl.vi v9, v8, 4");
    asm volatile("vsll.vi v8, v8, 4");
    asm volatile("vor.vv v8, v8, v9");

    asm volatile("vsrl.vi v9, v8, 2");
    asm volatile("vand.vx v9, v9, %0" :: "r"(c33));
    asm volatile("vand.vx v8, v8, %0" :: "r"(c33));
    asm volatile("vsll.vi v8, v8, 2");
    asm volatile("vor.vv v8, v8, v9");

    asm volatile("vsrl.vi v9, v8, 1");
    asm volatile("vand.vx v9, v9, %0" :: "r"(c55));
    asm volatile("vand.vx v8, v8, %0" :: "r"(c55));
    asm volatile("vsll.vi v8, v8, 1");
    asm volatile("vor.vv v8, v8, v9");

    asm volatile("vse8.v v8, (%0)" :: "r"(packed) : "memory");

    soft   += vl;
    packed += nbytes;
    nbits  -= vl;
  }

  asm volatile("vsetivli zero, 1, e8, m1, ta, ma");
  asm volatile("vmv.x.s %0, v16" : "=r"(result));
  return (result & 0xFF) != 0;
}
