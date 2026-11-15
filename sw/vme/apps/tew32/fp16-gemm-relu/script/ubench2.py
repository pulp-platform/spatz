#!/usr/bin/env python3
"""Generate ubench2.c: steady-state 2-tile group (16 K-groups, ideal 512 cycles) plus one extra
VFU/VLSU stream per variant, to locate the resource the ReLU competes with."""
import pathlib

GPR = {"t0": 5, "t1": 6, "t2": 7, "a0": 10, "a1": 11, "a2": 12}


def rr(vd, rs1, rs2):
    w = (0b110000 << 26) | (1 << 25) | (GPR[rs2] << 20) | (GPR[rs1] << 15) | (0b110 << 12) | (vd << 7) | 0x7B
    return f".word 0x{w:08x}"


# variant: (instructions in odd groups, instructions in even groups) placed with the operand loads
V = {
    "base": ([], []),
    "relu-full": (["vse32.v v12, (t0)", "vle32.v v28, (a7)"], ["vfmax.vf v12, v28, ft0"]),
    "relu-load": (["vle32.v v28, (a7)"], []),
    "relu-vfmax": ([], ["vfmax.vf v12, v28, ft0"]),
    "relu-store": (["vse32.v v12, (t0)"], []),
    "relu-load+store": (["vse32.v v12, (t0)", "vle32.v v28, (a7)"], []),
    "relu-masked": (["vse32.v v12, (t0), v0.t", "vle32.v v28, (a7)"], ["vmflt.vf v0, v28, ft0"]),
    "vfmax-every": (["vfmax.vf v12, v28, ft0"], ["vfmax.vf v12, v28, ft0"]),
}


def group(cur, nxt, odd, extra, loadpos=4):
    a0, a1, b = cur
    n0, n1, nb = nxt
    loads = ["vsetvli zero, s0, e32, m4, ta, ma", rr(n0, "a0", "t1"), rr(n1, "a1", "t1"), rr(nb, "a2", "t2")]
    loads += extra[0] if odd else extra[1]
    loads += ["msetmtype t4, t5", "msettn zero, t6"]
    macs = []
    for k in range(4):
        macs += [f"vtfmm.tvv mt0, v{a0+k}, v{b+k}", f"vtfmm.tvv mt4, v{a1+k}, v{b+k}"]
    out = []
    for i, m in enumerate(macs):
        if i == loadpos:
            out += loads
        out.append(m)
    return out


src = ['#include <snrt.h>', '#include <printf.h>', '#include <stdint.h>',
       'static inline uint32_t cyc(void){uint32_t c; asm volatile("csrr %0, mcycle":"=r"(c)::"memory"); return c;}']
S0, S1 = (0, 4, 8), (16, 20, 24)
for name, ex in V.items():
    body = []
    for g in range(8):
        body += group(S0, S1, False, ex) + group(S1, S0, True, ex)
    fn = name.replace("-", "_").replace("+", "_")
    lines = "\n".join(f'    "{x}\\n"' for x in body)
    src.append(f'''static uint32_t run_{fn}(uint32_t *a, uint32_t *b, float *r, uint32_t *sync) {{
  register uint32_t *pa0 asm("a0") = a;
  register uint32_t *pa1 asm("a1") = a + 64;
  register uint32_t *pb asm("a2") = b;
  register float *pr asm("a7") = r;
  register float *ps asm("t0") = r + 2048;
  asm volatile("msetmtype %0, %1\\n msettn zero, %2\\n msettm zero, %2\\n vtzero mt0\\n vtzero mt4\\n fmv.w.x ft0, zero"
               :: "r"(16450u), "r"(0xC8u), "r"(16u) : "memory");
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma\\n vle32.v v0, (%1)\\n vle32.v v4, (%1)\\n vle32.v v8, (%2)\\n"
               "msetmtype %3, %4\\n msettn zero, %5\\n vtse32 %6, (%7)\\n fence rw, rw\\n lw zero, 0(%7)"
               :: "r"(64), "r"(a), "r"(b), "r"(16450u), "r"(0xC8u), "r"(16u), "r"(0x40000000u), "r"(sync) : "memory");
  uint32_t s = cyc();
  asm volatile("li s0, 64\\n li t1, 512\\n li t2, 256\\n li t4, 16450\\n li t5, 0xC8\\n li t6, 16\\n"
{lines}
    "msetmtype t4, t5\\n msettn zero, t6\\n vtse32 zero, (%4)\\n li t3, 0x20000000\\n vtse32 t3, (%4)\\n"
    "fence rw, rw\\n lw zero, 0(%4)\\n"
    : "+r"(pa0), "+r"(pa1), "+r"(pb), "+r"(pr), "+r"(sync), "+r"(ps)
    :: "s0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");
  return cyc() - s;
}}''')
src.append('int main(void) {\n  if (snrt_cluster_core_idx() != 0) return 0;\n'
           '  uint32_t *a = snrt_l1alloc(20480), *b = snrt_l1alloc(8192), *sy = snrt_l1alloc(256);\n'
           '  float *r = snrt_l1alloc(16384);\n'
           '  for (int i = 0; i < 5120; ++i) a[i] = 0x3c003c00u;\n'
           '  for (int i = 0; i < 2048; ++i) b[i] = 0x3c003c00u;\n'
           '  for (int i = 0; i < 4096; ++i) r[i] = (i & 1) ? -1.0f : 1.0f;')
for name in V:
    fn = name.replace("-", "_").replace("+", "_")
    src.append(f'  {{ uint32_t c = run_{fn}(a, b, r, sy); printf("%-18s %u\\n", "{name}", c); }}')
src.append('  printf("UBENCH2 DONE\\n");\n  return 0;\n}')
pathlib.Path(__file__).with_name("ubench2.c").write_text("\n".join(src) + "\n")
