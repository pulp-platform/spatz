// Microbenchmarks for scheduling the fused GeMM->ReLU on Spatz+OPE (TE16, CE8, VLEN512).
// Each test is timed with mcycle and ends with a completion sync (tile row store + fence + load).
#include <snrt.h>
#include <printf.h>
#include <stdint.h>

#define MTYPE16 16450u /* TE16, TWIDEN2, TK2 */
#define VTYPE16 0xC8u  /* e16 m1 ta ma */

static inline uint32_t cyc(void) { uint32_t c; asm volatile("csrr %0, mcycle" : "=r"(c)::"memory"); return c; }
static inline void mmode(void) {
  asm volatile("msetmtype %0, %1\n msettn zero, %2\n msettm zero, %2" :: "r"(MTYPE16), "r"(VTYPE16), "r"(16u) : "memory");
}
static uint32_t *sync_buf;
// wait until OPE work on mt0/mt4/mt8/mt12 has retired and memory is consistent
static inline void sync_all(void) {
  asm volatile("vtse32 %0, (%1)\n vtse32 %2, (%1)\n vtse32 %3, (%1)\n vtse32 %4, (%1)\n fence rw, rw\n lw zero, 0(%1)"
               :: "r"(0u), "r"(sync_buf), "r"(0x20000000u), "r"(0x40000000u), "r"(0x60000000u) : "memory");
}
#define REP4(x) x x x x
#define REP16(x) REP4(REP4(x))
#define T(name, ...) do { mmode(); sync_all(); uint32_t _s = cyc(); __VA_ARGS__; mmode(); sync_all(); uint32_t _e = cyc(); \
    printf("%-28s %6u\n", name, _e - _s - base); } while (0)

int main(void) {
  if (snrt_cluster_core_idx() != 0) return 0;
  uint32_t *buf = snrt_l1alloc(32768);
  sync_buf = snrt_l1alloc(256);
  float *zout = snrt_l1alloc(16384);
  for (int i = 0; i < 8192; ++i) buf[i] = 0x3c003c00u; // {1.0,1.0} fp16
  uint32_t base = 0;
  mmode();
  asm volatile("vsetvli zero, %0, e32, m8, ta, ma\n vle32.v v0,(%1)\n vle32.v v8,(%1)\n vle32.v v16,(%1)\n vle32.v v24,(%1)" :: "r"(128), "r"(buf) : "memory");
  mmode();
  asm volatile("vtzero mt0\n vtzero mt4\n vtzero mt8\n vtzero mt12" ::: "memory");
  { mmode(); sync_all(); uint32_t s = cyc(); sync_all(); base = cyc() - s; }
  printf("sync overhead %u\n", base);

  T("64 vtfmm same tile", REP16(asm volatile("vtfmm.tvv mt0, v0, v8\n vtfmm.tvv mt0, v1, v9\n vtfmm.tvv mt0, v2, v10\n vtfmm.tvv mt0, v3, v11" ::: "memory");));
  T("64 vtfmm 2 tiles", REP16(asm volatile("vtfmm.tvv mt0, v0, v8\n vtfmm.tvv mt4, v4, v8\n vtfmm.tvv mt0, v1, v9\n vtfmm.tvv mt4, v5, v9" ::: "memory");));
  T("64 vtfmm 4 tiles", REP16(asm volatile("vtfmm.tvv mt0, v0, v8\n vtfmm.tvv mt4, v0, v12\n vtfmm.tvv mt8, v4, v8\n vtfmm.tvv mt12, v4, v12" ::: "memory");));
  T("64 vtfmm 2t same regs", REP16(asm volatile("vtfmm.tvv mt0, v0, v8\n vtfmm.tvv mt4, v4, v8\n vtfmm.tvv mt0, v0, v8\n vtfmm.tvv mt4, v4, v8" ::: "memory");));

  // memory bandwidth
  T("32 vle32 m4 (8KB)", { asm volatile("vsetvli zero, %0, e32, m4, ta, ma" :: "r"(64)); uint32_t *p = buf;
     REP16(asm volatile("vle32.v v16,(%0)\n vle32.v v20,(%1)" :: "r"(p), "r"(p + 64) : "memory"); p += 128;); });
  T("16 vle32 m8 (8KB)", { asm volatile("vsetvli zero, %0, e32, m8, ta, ma" :: "r"(128)); uint32_t *p = buf;
     REP16(asm volatile("vle32.v v16,(%0)" :: "r"(p) : "memory"); p += 128;); });
  T("64 vle32 m1@mmode (4KB)", { uint32_t *p = buf;
     REP16(asm volatile("vle32.v v16,(%0)\n vle32.v v18,(%1)\n vle32.v v20,(%2)\n vle32.v v22,(%3)" :: "r"(p), "r"(p+16), "r"(p+32), "r"(p+48) : "memory"); p += 64;); });
  T("32 vse32 m4 (8KB)", { asm volatile("vsetvli zero, %0, e32, m4, ta, ma" :: "r"(64)); uint32_t *p = (uint32_t*)zout;
     REP16(asm volatile("vse32.v v16,(%0)\n vse32.v v20,(%1)" :: "r"(p), "r"(p + 64) : "memory"); p += 128; if (p >= (uint32_t*)zout + 4096) p = (uint32_t*)zout;); });

  // tile moves
  T("32 vtse32 rows", { uint32_t t = 0; float *p = zout;
     REP16(asm volatile("vtse32 %0,(%1)\n addi %0,%0,1\n add %1,%1,%2\n vtse32 %0,(%1)\n addi %0,%0,1\n add %1,%1,%2" : "+r"(t), "+r"(p) : "r"(256) : "memory"); if (t >= 16) t = 0x20000000u;); });
  T("32 vtle32 rows", { uint32_t t = 0x40000000u; float *p = zout;
     REP16(asm volatile("vtle32 %0,(%1)\n addi %0,%0,1\n add %1,%1,%2\n vtle32 %0,(%1)\n addi %0,%0,1\n add %1,%1,%2" : "+r"(t), "+r"(p) : "r"(256) : "memory"); if ((t & 0xff) >= 16) t = 0x60000000u;); });
  T("32 vtmv.v.t rows (e32)", { asm volatile("msetmtypei 1, 2, 0\n msettn zero, %0\n msettm zero, %0" :: "r"(16u)); uint32_t t = 0;
     REP16(asm volatile("vtmv.v.t v16, %0\n addi %0,%0,1\n vtmv.v.t v17, %0\n addi %0,%0,1" : "+r"(t) :: "memory"); if (t >= 16) t = 0x20000000u;); mmode(); });

  // configuration switches
  T("32x vsetvli+msetmtype+msettn", REP16(asm volatile("vsetvli zero, %0, e32, m4, ta, ma\n msetmtype %1, %2\n msettn zero, %3\n"
     "vsetvli zero, %0, e32, m4, ta, ma\n msetmtype %1, %2\n msettn zero, %3" :: "r"(64), "r"(MTYPE16), "r"(VTYPE16), "r"(16u) : "memory");));

  // ReLU from memory, 4096 floats
  T("ReLU 4096f m8 (vle,vfmax,vse)", { asm volatile("vsetvli zero, %0, e32, m8, ta, ma" :: "r"(128)); float *p = zout;
     for (int i = 0; i < 32; ++i) { asm volatile("vle32.v v16,(%0)\n vfmax.vf v16, v16, %1\n vse32.v v16,(%0)" :: "r"(p), "f"(0.0f) : "memory"); p += 128; } });
  T("ReLU 4096f m8 2-stream", { asm volatile("vsetvli zero, %0, e32, m8, ta, ma" :: "r"(128)); float *p = zout;
     for (int i = 0; i < 16; ++i) { asm volatile("vle32.v v16,(%0)\n vle32.v v24,(%1)\n vfmax.vf v16, v16, %2\n vfmax.vf v24, v24, %2\n vse32.v v16,(%0)\n vse32.v v24,(%1)" :: "r"(p), "r"(p+128), "f"(0.0f) : "memory"); p += 256; } });

  // steady state: 2-tile group (mt0: A0xB, mt4: A1xB), 16 K-groups of 4 pairs, double-buffered operand regs. Ideal = 512.
  T("2t-group 16 K-groups", { const uint32_t *pa0 = buf, *pa1 = buf + 1024, *pb = buf + 2048;
     for (int g = 0; g < 8; ++g) {
       asm volatile("vsetvli zero, %3, e32, m4, ta, ma\n vle32.v v16,(%0)\n vle32.v v20,(%1)\n vle32.v v24,(%2)\n msetmtype %4, %5\n msettn zero, %6\n"
                    "vtfmm.tvv mt0, v0, v8\n vtfmm.tvv mt4, v4, v8\n vtfmm.tvv mt0, v1, v9\n vtfmm.tvv mt4, v5, v9\n"
                    "vtfmm.tvv mt0, v2, v10\n vtfmm.tvv mt4, v6, v10\n vtfmm.tvv mt0, v3, v11\n vtfmm.tvv mt4, v7, v11\n"
                    "vsetvli zero, %3, e32, m4, ta, ma\n vle32.v v0,(%0)\n vle32.v v4,(%1)\n vle32.v v8,(%2)\n msetmtype %4, %5\n msettn zero, %6\n"
                    "vtfmm.tvv mt0, v16, v24\n vtfmm.tvv mt4, v20, v24\n vtfmm.tvv mt0, v17, v25\n vtfmm.tvv mt4, v21, v25\n"
                    "vtfmm.tvv mt0, v18, v26\n vtfmm.tvv mt4, v22, v26\n vtfmm.tvv mt0, v19, v27\n vtfmm.tvv mt4, v23, v27\n"
                    :: "r"(pa0), "r"(pa1), "r"(pb), "r"(64), "r"(MTYPE16), "r"(VTYPE16), "r"(16u) : "memory");
       pa0 += 128; pa1 += 128; pb += 128; } });
  T("2t-group + 32 vtse other pair", { const uint32_t *pa0 = buf, *pa1 = buf + 1024, *pb = buf + 2048; uint32_t t0 = 0x40000000u, t1 = 0x60000000u; float *q0 = zout, *q1 = zout + 16;
     uint32_t dummy = 64, dummy2 = MTYPE16, dummy3 = VTYPE16, dummy4 = 16;
     for (int g = 0; g < 8; ++g) {
       asm volatile("vsetvli zero, %3, e32, m4, ta, ma\n vle32.v v16,(%0)\n vle32.v v20,(%1)\n vle32.v v24,(%2)\n msetmtype %4, %5\n msettn zero, %6\n"
                    "vtfmm.tvv mt0, v0, v8\n vtfmm.tvv mt4, v4, v8\n vtse32 %7,(%9)\n vtfmm.tvv mt0, v1, v9\n addi %7,%7,1\n vtfmm.tvv mt4, v5, v9\n add %9,%9,%11\n"
                    "vtfmm.tvv mt0, v2, v10\n vtse32 %8,(%10)\n vtfmm.tvv mt4, v6, v10\n addi %8,%8,1\n vtfmm.tvv mt0, v3, v11\n add %10,%10,%11\n vtfmm.tvv mt4, v7, v11\n"
                    "vsetvli zero, %3, e32, m4, ta, ma\n vle32.v v0,(%0)\n vle32.v v4,(%1)\n vle32.v v8,(%2)\n msetmtype %4, %5\n msettn zero, %6\n"
                    "vtfmm.tvv mt0, v16, v24\n vtfmm.tvv mt4, v20, v24\n vtse32 %7,(%9)\n vtfmm.tvv mt0, v17, v25\n addi %7,%7,1\n vtfmm.tvv mt4, v21, v25\n add %9,%9,%11\n"
                    "vtfmm.tvv mt0, v18, v26\n vtse32 %8,(%10)\n vtfmm.tvv mt4, v22, v26\n addi %8,%8,1\n vtfmm.tvv mt0, v19, v27\n add %10,%10,%11\n vtfmm.tvv mt4, v23, v27\n"
                    : "+r"(pa0), "+r"(pa1), "+r"(pb), "+r"(dummy), "+r"(dummy2), "+r"(dummy3), "+r"(dummy4), "+r"(t0), "+r"(t1), "+r"(q0), "+r"(q1) : "r"(256) : "memory");
       pa0 += 128; pa1 += 128; pb += 128; } });
  printf("UBENCH DONE\n");
  return 0;
}
