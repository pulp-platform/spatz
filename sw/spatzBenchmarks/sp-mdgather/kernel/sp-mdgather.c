// Copyright 2026 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

// Molecular-dynamics cluster-pair gather (GROMACS-style cluster-pair
// algorithm, Pall et al. 2020), fp32, simplified FMA-only pair force.
//
// Atoms are grouped in clusters of 4. Coordinates live cluster-blocked in
// the padded xyzq X4 layout: per cluster c, 64 bytes at coords + c*64:
//   x[4] fp32 | y[4] fp32 | z[4] fp32 | q[4] fp32
// A pair list gives, for each i-cluster, LIST j-cluster indices (uint16).
// For every i-atom (4 per i-cluster) against every atom of every listed
// j-cluster the kernels compute (no div/sqrt):
//   dx = xi - xj; dy = yi - yj; dz = zi - zj
//   r2 = dx*dx + dy*dy + dz*dz
//   w  = max(CUT2 - r2, 0)            // branch-free via vfmax.vf
//   s  = qj * w * w
//   fx_i += s*dx; fy_i += s*dy; fz_i += s*dz
//
// FLOP-count convention (per (i-atom, j-atom) pair), fixed at
//   subs 3 + r2 5 + w 1 (incl. max, counted as 1) + w*w 1 + s 1 + accum 6
//   = 17 FLOP/pair
// and used consistently by the perf printout in main.c. (The implementation
// realizes the accumulation as 3 vector multiplies + reduction adds instead
// of 3 vector FMAs + final reduce; the mathematical work is the same.)
//
// Structure (identical for both variants, only the gather differs):
// per i-cluster, the j-list is processed in vector-length chunks at
// e32/LMUL=4 (VLMAX = 64 elements = 16 j-clusters for VLEN=512). The four
// field gathers (xj, yj, zj, qj) are HOISTED outside the 4-i-atom loop and
// the gathered j-data is reused for all 4 i-atoms (the GROMACS reuse
// structure; this is the key optimization). Per-i-atom forces are
// accumulated with vfredusum.vs into scalar accumulators.
//
// Vector register map (e32, m4):
//   v8-11 xj | v12-15 yj | v16-19 zj | v20-23 qj      (per-chunk j-data)
//   v24-27 dx | v28-31 dy | v0-3 dz | v4-7 r2/w/s     (per-i-atom temps)
//   v4(-7) doubles as index scratch at chunk start and reduction scratch
//   at the end of each i-atom (dead as s by then). v24-25 holds the raw
//   index load of the RVV variant before widening (dx is not yet live).
//
// Reduction note: 12 full-width vector force accumulators (4 i-atoms x 3
// components) do not fit in the register file next to the hoisted j-data,
// so instead of a single vfredusum at the very end of the list loop each
// (chunk, i-atom) partial force vector is reduced with the proven
// vfmv.s.f-zero + vfredusum.vs + vfmv.f.s sequence (cf. dp-fdotp,
// hp-vqdotp) and accumulated in scalar fp registers across chunks.

#include "sp-mdgather.h"

// j-atoms (fp32 elements) per gathered block: one j-cluster = 4 elements
// = 16 bytes. Power of two, multiple of 8 bytes, as VLXBLK requires.
#define MDG_CL 4

static inline unsigned int mdg_vlmax_e32m4(void) {
  unsigned int vlmax;
  asm volatile("vsetvli %0, zero, e32, m4, ta, ma" : "=r"(vlmax));
  return vlmax;
}

// One i-atom against the gathered j-data of the current chunk (vtype must
// be e32/m4 with vl = gvl). Reads v8/v12/v16/v20, clobbers v0-7/v24-31,
// accumulates the reduced (fx, fy, fz) into the scalar accumulators.
static inline void mdg_iatom_body(const float xi, const float yi,
                                  const float zi, const float cut2,
                                  float *fx, float *fy, float *fz) {
  float ax = *fx, ay = *fy, az = *fz;
  const float fzero = 0.0f;

  asm volatile(
      "vfrsub.vf v24, v8,  %[xi]\n"  // dx = xi - xj
      "vfrsub.vf v28, v12, %[yi]\n"  // dy = yi - yj
      "vfrsub.vf v0,  v16, %[zi]\n"  // dz = zi - zj
      "vfmul.vv  v4,  v24, v24\n"    // r2  = dx*dx
      "vfmacc.vv v4,  v28, v28\n"    // r2 += dy*dy
      "vfmacc.vv v4,  v0,  v0\n"     // r2 += dz*dz
      "vfrsub.vf v4,  v4,  %[c2]\n"  // CUT2 - r2
      "vfmax.vf  v4,  v4,  %[z]\n"   // w = max(CUT2 - r2, 0)
      "vfmul.vv  v4,  v4,  v4\n"     // w*w
      "vfmul.vv  v4,  v4,  v20\n"    // s = qj * w*w
      "vfmul.vv  v24, v24, v4\n"     // s*dx
      "vfmul.vv  v28, v28, v4\n"     // s*dy
      "vfmul.vv  v0,  v0,  v4\n"     // s*dz
      :
      : [xi] "f"(xi), [yi] "f"(yi), [zi] "f"(zi), [c2] "f"(cut2),
        [z] "f"(fzero)
      : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v24", "v25", "v26",
        "v27", "v28", "v29", "v30", "v31");

  // v4-7 (s) is dead now; reuse v4 as the reduction scratch register.
  // Each reduction is SEEDED with the running per-i-atom accumulator via
  // its scalar operand (vs1), which removes the dependent scalar fadd
  // chain of a separate `acc += red` step. The three reductions stay
  // fully serialized on v4 with an immediate vfmv.f.s read-back each --
  // the proven fdotp/vqdotp pattern. (Batching several in-flight
  // reductions into distinct scratch registers before reading them back
  // deadlocks the VFU reduction FSM in RTL; do not "optimize" this.)
  asm volatile("vfmv.s.f v4, %[ax]\n"
               "vfredusum.vs v4, v24, v4\n"
               :
               : [ax] "f"(ax)
               : "v4");
  asm volatile("vfmv.f.s %0, v4" : "=f"(ax));
  asm volatile("vfmv.s.f v4, %[ay]\n"
               "vfredusum.vs v4, v28, v4\n"
               :
               : [ay] "f"(ay)
               : "v4");
  asm volatile("vfmv.f.s %0, v4" : "=f"(ay));
  asm volatile("vfmv.s.f v4, %[az]\n"
               "vfredusum.vs v4, v0, v4\n"
               :
               : [az] "f"(az)
               : "v4");
  asm volatile("vfmv.f.s %0, v4" : "=f"(az));

  *fx = ax;
  *fy = ay;
  *fz = az;
}

// VLXBLK variant: gather one 16-byte field block (4 fp32) per pair-list
// index with vlxblkei16.v. The record is 64 B but the block is 16 B, so
// the index granularity is 16-B fields: field f of cluster c starts at
// (c*4 + f) * 16 B. One vsll.vi turns the raw cluster index into the
// field-granular index (idx4 = idx << 2); the four gathers then use the
// SAME index vector with base pointers coords + f*16 (f = 0..3).
void mdgather_vlxblk(float *forces, const float *coords,
                     const uint16_t *pairlist, const unsigned int NC,
                     const unsigned int LIST, const float cut2) {
  const unsigned int vlmax = mdg_vlmax_e32m4();
  const unsigned int chunk_cl_max = vlmax / MDG_CL;  // j-clusters per chunk

  // Block length: 4 fp32 elements (16 B). Power of two as required.
  asm volatile("vsetblklen %0" ::"r"(MDG_CL));

  for (unsigned int c = 0; c < NC; ++c) {
    const float *ci = coords + c * 16;
    const uint16_t *lp = pairlist + c * LIST;

    float fx[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float fy[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float fz[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    for (unsigned int j = 0; j < LIST;) {
      const unsigned int n_cl =
          ((LIST - j) < chunk_cl_max) ? (LIST - j) : chunk_cl_max;
      const unsigned int gvl = n_cl * MDG_CL;

      // Load the raw uint16 j-cluster indices, scale to 16-B field
      // granularity, and gather all four fields (hoisted out of the
      // i-atom loop; reused by all 4 i-atoms).
      asm volatile(
          "vsetvli zero, %[n_cl], e16, m1, ta, ma\n"
          "vle16.v v4, (%[idxp])\n"
          "vsll.vi v4, v4, 2\n"  // idx4 = cluster * 4 (16-B fields)
          "vsetvli zero, %[gvl], e32, m4, ta, ma\n"
          "vlxblkei16.v v8,  (%[cx]), v4\n"  // xj
          "vlxblkei16.v v12, (%[cy]), v4\n"  // yj
          "vlxblkei16.v v16, (%[cz]), v4\n"  // zj
          "vlxblkei16.v v20, (%[cq]), v4\n"  // qj
          :
          : [n_cl] "r"(n_cl), [gvl] "r"(gvl), [idxp] "r"(lp + j),
            [cx] "r"(coords + 0), [cy] "r"(coords + 4), [cz] "r"(coords + 8),
            [cq] "r"(coords + 12)
          : "v4", "v5", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
            "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "memory");

      for (unsigned int a = 0; a < 4; ++a)
        mdg_iatom_body(ci[0 + a], ci[4 + a], ci[8 + a], cut2, &fx[a], &fy[a],
                       &fz[a]);

      j += n_cl;
    }

    float *fo = forces + c * 12;
    for (unsigned int a = 0; a < 4; ++a) {
      fo[3 * a + 0] = fx[a];
      fo[3 * a + 1] = fy[a];
      fo[3 * a + 2] = fz[a];
    }
  }
}

// Plain-RVV baseline: identical structure, but each field gather is a
// canonical element-granular vluxei32.v. Element e of a chunk needs byte
// offset idx[e/4]*64 + f*16 + (e%4)*4; the field offset f*16 folds into
// the base pointer. Spatz has neither vid.v nor vrgather(ei16) (verified
// against the spatz_vpu decoder), so the per-element part is provided by a
// PRECOMPUTED EXPANDED index array (one uint16 per gathered element,
// value = cluster*16 + lane, built by the data generator alongside the
// pair list). It is loaded with vle16 INSIDE the timed region, so its
// (4x larger) index traffic is charged to the baseline fairly. One
// widening add + one shift then yield the byte offsets
//   (cluster*16 + lane) << 2 = cluster*64 + lane*4.
void mdgather_rvv(float *forces, const float *coords,
                  const uint16_t *pairlist_exp, const unsigned int NC,
                  const unsigned int LIST, const float cut2) {
  const unsigned int vlmax = mdg_vlmax_e32m4();
  const unsigned int list_el = LIST * MDG_CL;  // elements per i-cluster list

  for (unsigned int c = 0; c < NC; ++c) {
    const float *ci = coords + c * 16;
    const uint16_t *lp = pairlist_exp + c * list_el;

    float fx[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float fy[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float fz[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    for (unsigned int e = 0; e < list_el;) {
      const unsigned int gvl =
          ((list_el - e) < vlmax) ? (list_el - e) : vlmax;

      // Load the expanded per-element indices (uint16), widen to e32,
      // scale to byte offsets, and gather all four fields (hoisted out
      // of the i-atom loop; reused by all 4 i-atoms). v24-25 is free
      // here (dx is not live between chunks).
      asm volatile(
          "vsetvli zero, %[gvl], e16, m2, ta, ma\n"
          "vle16.v v24, (%[idxp])\n"
          "vwaddu.vx v4, v24, zero\n"  // u16 -> u32
          "vsetvli zero, %[gvl], e32, m4, ta, ma\n"
          "vsll.vi v4, v4, 2\n"  // byte offs = cluster*64 + lane*4
          "vluxei32.v v8,  (%[cx]), v4\n"  // xj
          "vluxei32.v v12, (%[cy]), v4\n"  // yj
          "vluxei32.v v16, (%[cz]), v4\n"  // zj
          "vluxei32.v v20, (%[cq]), v4\n"  // qj
          :
          : [gvl] "r"(gvl), [idxp] "r"(lp + e), [cx] "r"(coords + 0),
            [cy] "r"(coords + 4), [cz] "r"(coords + 8), [cq] "r"(coords + 12)
          : "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
            "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22",
            "v23", "v24", "v25", "memory");

      for (unsigned int a = 0; a < 4; ++a)
        mdg_iatom_body(ci[0 + a], ci[4 + a], ci[8 + a], cut2, &fx[a], &fy[a],
                       &fz[a]);

      e += gvl;
    }

    float *fo = forces + c * 12;
    for (unsigned int a = 0; a < 4; ++a) {
      fo[3 * a + 0] = fx[a];
      fo[3 * a + 1] = fy[a];
      fo[3 * a + 2] = fz[a];
    }
  }
}
