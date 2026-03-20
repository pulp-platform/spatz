// Copyright 2021 ETH Zurich and University of Bologna.
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

// Author: Diyou Shen, ETH Zurich

#include <benchmark.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/db-fft.c"

// #define DEBUG

// ---------------------------------------------------------------------------
// DMA ownership
//
// On Snitch, ONLY Core 0 has a DMA unit. Core 1 is a pure compute core and
// will trap with an illegal instruction if it executes any dmsrc/dmdst/dmcpyi.
// All snrt_dma_* calls are therefore guarded by (cid == 0).
//
// Double-buffering model (both phases):
//   Sections are processed in pairs: pair k = (section k*2, section k*2+1).
//   Core 0 always computes the even section, Core 1 the odd section.
//
//   For pair k:
//     [C0]  dma_start(pair k+1 load)   \
//     [C0]  compute section k*2         } overlapped
//     [C1]  compute section k*2+1      /
//     barrier
//     [C0]  dma_wait_all               -- next pair data now ready
//     [C0]  dma_start(store pair k)    -- write results back (Phase 1 only)
//     [C0]  dma_wait_all
//     barrier
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// L1 layout
//
// 4 slots total: 2 cores × 2 ping-pong slots.
// Core c, slot s: l1_base + (c*2 + s) * slot_doubles
//
// Phase 1 slot (6 * nfft_sec doubles):
//   [Re_upper: nfft_sec][Re_lower: nfft_sec]
//   [Im_upper: nfft_sec][Im_lower: nfft_sec]
//   [Re_twi:   nfft_sec][Im_twi:   nfft_sec]
//
// Phase 2 slot (4*nfft_sec + 2*ntwi_p2 doubles):
//   offset 0:           ping Re  (nfft_sec elements)
//   offset nfft_sec:    ping Im  (nfft_sec elements)
//   offset 2*nfft_sec:  pong Re  (nfft_sec elements)
//   offset 3*nfft_sec:  pong Im  (nfft_sec elements)
//   offset 4*nfft_sec:  twiddles [Re: ntwi_p2][Im: ntwi_p2]
//
//   fft_p2_kernel uses nfft_sec as the Re-to-Im stride (tight packing).
// ---------------------------------------------------------------------------

static double   *l1_base;
static uint16_t *l1_store_idx;

static inline int fp_check(const double a, const double b) {
  const double threshold = 0.00001;
  double comp = a - b;
  if (comp < 0) comp = -comp;
  return comp > threshold;
}

// ---------------------------------------------------------------------------
// DMA helpers — call only from Core 0
// ---------------------------------------------------------------------------

static void dma_load_p1_section(
    double       *l1_slot,
    const double *samples_dram,
    const double *twi_p1_dram,
    const unsigned int sec,
    const unsigned int stage,
    const unsigned int nfft,
    const unsigned int nfft_sec,
    const unsigned int log2_S,
    const unsigned int S,
    const unsigned int ntwi_p1)
{
  const unsigned int T_S = sizeof(double);

  // Pointer arithmetic for this section at this stage
  const unsigned int group    = sec >> (log2_S - stage);
  const unsigned int idx      = sec - group * (S >> stage);
  const unsigned int offset   = (nfft >> stage) * group;
  const unsigned int wing_gap = nfft >> (stage + 1);

  const double *re_upper = samples_dram + offset + idx * nfft_sec;
  const double *re_lower = re_upper + wing_gap;
  const double *im_upper = re_upper + nfft;
  const double *im_lower = re_lower + nfft;

  // Twiddle offset: skip stage 0..stage-1 sizes, then idx*nfft_sec within stage
  unsigned int twi_off = 0;
  for (unsigned int i = 0; i < stage; ++i)
    twi_off += nfft >> (i + 1);
  // Each section consumes nfft_sec/2 twiddles (one per butterfly pair)
  twi_off += idx * (nfft_sec / 2);

  const double *re_twi = twi_p1_dram           + twi_off;
  const double *im_twi = twi_p1_dram + ntwi_p1 + twi_off;

  // Each wing has nfft_sec/2 elements (upper and lower are separate halves)
  snrt_dma_start_1d(l1_slot,                re_upper, (nfft_sec/2) * T_S);
  snrt_dma_start_1d(l1_slot +   nfft_sec/2, re_lower, (nfft_sec/2) * T_S);
  snrt_dma_start_1d(l1_slot + 2*nfft_sec/2, im_upper, (nfft_sec/2) * T_S);
  snrt_dma_start_1d(l1_slot + 3*nfft_sec/2, im_lower, (nfft_sec/2) * T_S);
  snrt_dma_start_1d(l1_slot + 4*nfft_sec, re_twi,   (nfft_sec/2) * T_S);
  snrt_dma_start_1d(l1_slot + 5*nfft_sec, im_twi,   (nfft_sec/2) * T_S);
}

static void dma_store_p1_section(
    const double *l1_slot,
    double       *samples_dram,
    const unsigned int sec,
    const unsigned int stage,
    const unsigned int nfft,
    const unsigned int nfft_sec,
    const unsigned int log2_S,
    const unsigned int S)
{
  const unsigned int T_S = sizeof(double);

  const unsigned int group    = sec >> (log2_S - stage);
  const unsigned int idx      = sec - group * (S >> stage);
  const unsigned int offset   = (nfft >> stage) * group;
  const unsigned int wing_gap = nfft >> (stage + 1);

  double *re_upper = samples_dram + offset + idx * nfft_sec;
  double *re_lower = re_upper + wing_gap;
  double *im_upper = re_upper + nfft;
  double *im_lower = re_lower + nfft;

  snrt_dma_start_1d(re_upper, l1_slot,                (nfft_sec/2) * T_S);
  snrt_dma_start_1d(re_lower, l1_slot +   nfft_sec/2, (nfft_sec/2) * T_S);
  snrt_dma_start_1d(im_upper, l1_slot + 2*nfft_sec/2, (nfft_sec/2) * T_S);
  snrt_dma_start_1d(im_lower, l1_slot + 3*nfft_sec/2, (nfft_sec/2) * T_S);
}

static void dma_load_p2_section(
    double       *l1_slot,
    const double *samples_dram,
    const double *twi_p2_dram,
    const unsigned int sec,
    const unsigned int nfft,
    const unsigned int nfft_sec,
    const unsigned int ntwi_p2)
{
  const unsigned int T_S = sizeof(double);

  const double *re_src     = samples_dram +           sec * nfft_sec;
  const double *im_src     = samples_dram + nfft     + sec * nfft_sec;
  const double *re_twi_src = twi_p2_dram  + sec * 2 * ntwi_p2;
  const double *im_twi_src = re_twi_src   + ntwi_p2;

  // Re at slot+0, Im at slot+nfft_sec (tight packing, matches fft_p2_kernel)
  snrt_dma_start_1d(l1_slot,                      re_src,     nfft_sec * T_S);
  snrt_dma_start_1d(l1_slot + nfft_sec,           im_src,     nfft_sec * T_S);
  // Twiddles after both ping (2*nfft_sec) and pong (2*nfft_sec) buffers
  snrt_dma_start_1d(l1_slot + 2*nfft_sec,         re_twi_src, ntwi_p2  * T_S);
  snrt_dma_start_1d(l1_slot + 2*nfft_sec + ntwi_p2, im_twi_src, ntwi_p2 * T_S);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
  const unsigned int num_cores = snrt_cluster_core_num();  // 2
  const unsigned int cid       = snrt_cluster_core_idx();  // 0 or 1

  // Static parameters from DATAHEADER
  const unsigned int nfft     = NFFT;
  const unsigned int S        = NSEC;
  const unsigned int log2_S   = LOG2_NSEC;
  const unsigned int nfft_sec = NFFT_SEC;
  const unsigned int log2_sec = LOG2_SEC;
  const unsigned int ntwi_p1  = NTWI_P1;
  const unsigned int ntwi_p2  = NTWI_P2;
  // Stride for vsse64 final output: S sections × 8 bytes
  const unsigned int stride   = S * sizeof(double);

  // Sections per core: core c processes sections c, c+2, c+4, ...
  const unsigned int sec_per_core = S / num_cores;

  // L1 slot sizes (in doubles)
  const unsigned int p1_slot_d = 6 * nfft_sec;
  // 2 ping-pong buffers × 2*nfft_sec (Re+Im tightly packed) + twiddles
  const unsigned int p2_slot_d = 4 * nfft_sec + 2 * ntwi_p2;
  const unsigned int slot_d    = (p1_slot_d > p2_slot_d) ? p1_slot_d : p2_slot_d;

  if (cid == 0) {
    l1d_init(120);
  }
  snrt_cluster_hw_barrier();

  // L1 allocation: only Core 0 allocates, pointer shared via global
  if (cid == 0) {
    const unsigned int l1_slots_bytes = 2 * num_cores * slot_d * sizeof(double);
    l1_base = (double *)snrt_l1alloc(l1_slots_bytes);
    if (l1_base == NULL) {
      printf("ERROR: L1 alloc failed for slots: requested %u bytes (NFFT=%u, NSEC=%u, slot_d=%u doubles)\n",
              l1_slots_bytes, nfft, S, slot_d);
      snrt_cluster_hw_barrier();
      set_eoc();
      return -1;
    }

    // store_idx for Phase 2 intermediate stages (not needed for final stage)
    const unsigned int n_sidx = (log2_sec > 1) ? (log2_sec - 1) * (nfft_sec / 2) : 0;
    if (n_sidx > 0) {
      const unsigned int l1_sidx_bytes = n_sidx * sizeof(uint16_t);
      l1_store_idx = (uint16_t *)snrt_l1alloc(l1_sidx_bytes);
      if (l1_store_idx == NULL) {
        printf("ERROR: L1 alloc failed for store_idx: requested %u bytes (n_sidx=%u)\n",
                l1_sidx_bytes, n_sidx);
        snrt_cluster_hw_barrier();
        set_eoc();
        return -1;
      }
      snrt_dma_start_1d(l1_store_idx, store_idx_dram, l1_sidx_bytes);
      snrt_dma_wait_all();
    } else {
      l1_store_idx = NULL;
    }
  }
  snrt_cluster_hw_barrier();

  // Convenient slot pointer for this core
  // core c, slot s  →  l1_base + (c*2 + s)*slot_d
  #define SLOT(c, s)  (l1_base + ((c)*2 + (s)) * slot_d)

  // Twiddle DRAM layout: [P1_Re:ntwi_p1][P1_Im:ntwi_p1][P2_sec0..secS-1]
  const double *twi_p1_dram = twiddle_dram;
  const double *twi_p2_dram = twiddle_dram + 2 * ntwi_p1;

  // working_samples: the DRAM array Phase 1 reads/writes in-place
  double *working_samples = samples_dram;

  unsigned int timer     = (unsigned int)-1;
  unsigned int timer_tmp = 0;

  snrt_cluster_hw_barrier();
  if (cid == 0) {
    timer_tmp = benchmark_get_cycle();
    start_kernel();
  }

  // =========================================================================
  // PHASE 1
  //
  // Outer loop: stages 0 .. log2_S-1 (full barrier between stages).
  // Inner loop: pairs k = 0 .. sec_per_core-1.
  //   pair k → core 0 computes section k*2, core 1 computes section k*2+1.
  //
  // Barrier protocol within a stage:
  //   (A) C0: pre-load pair 0 → wait
  //   barrier  ← both cores see pair 0 data in their slot
  //   loop k:
  //     (B) C0: start load pair k+1 (no wait — overlaps compute)
  //         Both: compute pair k
  //     barrier  ← compute done; if C0 started load, it may still be running
  //     (C) C0: dma_wait_all (load done), store pair k, dma_wait_all
  //     barrier  ← stores flushed to DRAM before next pair's compute
  //   end loop
  //   barrier  ← stage fully written back; next stage may begin
  // =========================================================================

  // Save originals before stage loop (working_samples aliases samples_dram)
  double dbg_orig_0   = samples_dram[0];
  double dbg_orig_128 = samples_dram[128];

  for (unsigned int stage = 0; stage < log2_S; ++stage) {

    // (A) Pre-load pair 0 into slot 0
    if (cid == 0) {
      for (unsigned int c = 0; c < num_cores; ++c) {
        dma_load_p1_section(SLOT(c, 0),
                            working_samples, twi_p1_dram,
                            c, stage,
                            nfft, nfft_sec, log2_S, S, ntwi_p1);
      }
      snrt_dma_wait_all();

      #ifdef DEBUG
      // DEBUG: print load addresses for stage 0, pair 0
      if (stage == 0 && cid == 0) {
        for (unsigned int c = 0; c < num_cores; ++c) {
          unsigned int sec   = c;
          unsigned int group = sec >> (log2_S - 0);       // = 0
          unsigned int idx   = sec - group * (S >> 0);    // = sec
          unsigned int offset   = (nfft >> 0) * group;    // = 0
          unsigned int wing_gap = nfft >> 1;
          printf("sec=%u group=%u idx=%u offset=%u wing_gap=%u\n",
                 sec, group, idx, offset, wing_gap);
          printf("  re_upper=%u re_lower=%u im_upper=%u im_lower=%u\n",
                 (unsigned)(idx * nfft_sec),
                 (unsigned)(idx * nfft_sec + wing_gap),
                 (unsigned)(idx * nfft_sec + nfft),
                 (unsigned)(idx * nfft_sec + nfft + wing_gap));
          unsigned int twi_off = idx * nfft_sec;
          printf("  twi_off=%u (re_twi[0]=%u im_twi[0]=%u)\n",
                 twi_off, twi_off, (unsigned)(ntwi_p1 + twi_off));
        }
      }

      #endif
    }
    snrt_cluster_hw_barrier();  // (A) barrier: pair 0 ready in slot 0

    for (unsigned int k = 0; k < sec_per_core; ++k) {
      const unsigned int cur_s   = k & 1;
      const unsigned int nxt_s   = 1 - cur_s;
      const unsigned int cur_sec = k * num_cores + cid;

      // (B) C0: kick next-pair load into nxt_s (overlaps compute below)
      if (cid == 0 && k + 1 < sec_per_core) {
        for (unsigned int c = 0; c < num_cores; ++c) {
          unsigned int nxt_sec = (k + 1) * num_cores + c;
          dma_load_p1_section(SLOT(c, nxt_s),
                              working_samples, twi_p1_dram,
                              nxt_sec, stage,
                              nfft, nfft_sec, log2_S, S, ntwi_p1);
        }
      }

      // (B) Both cores compute their current section (in-place in cur_s slot)
      double *cur_slot = SLOT(cid, cur_s);
      fft_p1_kernel(cur_slot,              // src (in-place)
                    cur_slot,              // dst
                    cur_slot + 4*nfft_sec, // twiddles
                    nfft_sec);

      snrt_cluster_hw_barrier();  // (B) barrier: compute done

      // (C) C0: wait for next-pair load, then store cur pair back to DRAM.
      if (cid == 0) {
        snrt_dma_wait_all();
        for (unsigned int c = 0; c < num_cores; ++c) {
          dma_store_p1_section(SLOT(c, cur_s),
                               working_samples,
                               k * num_cores + c, stage,
                               nfft, nfft_sec, log2_S, S);
        }
        snrt_dma_wait_all();
      }
      snrt_cluster_hw_barrier();  // (C) barrier: DRAM updated, next iter safe

      #ifdef DEBUG
      // Print AFTER (C) barrier so store is fully visible to all cores
      if (stage == 0 && k == 0 && cid == 0) {
          printf("After stage 0 pair 0 store (post-barrier):\n");
          printf("  samples[0]=%f  expected=%.6f (re[0]+re[128])\n",
                 working_samples[0], dbg_orig_0 + dbg_orig_128);
          printf("  samples[128]=%f\n", working_samples[128]);
          printf("  SLOT(0,0)[0]=%f (should match samples[0])\n",
                 SLOT(0,0)[0]);
      }
      #endif
    }

    // No extra barrier needed here: (C) barrier at end of last k already
    // ensures all stores are flushed before the stage loop increments.
  }

  // =========================================================================
  // PHASE 2
  //
  // After Phase 1, section sec's data is contiguous in working_samples:
  //   Re: working_samples[sec*nfft_sec .. (sec+1)*nfft_sec - 1]
  //   Im: working_samples[nfft + sec*nfft_sec ..]
  //
  // fft_p2_kernel writes its output via vsse64 directly to out_dram —
  // no store-back DMA needed.
  //
  // Barrier protocol:
  //   (A) C0: pre-load pair 0 → wait
  //   barrier
  //   loop k:
  //     (B) C0: start load pair k+1 (overlaps compute)
  //         Both: fft_p2_kernel on pair k  (writes to out_dram via vsse64)
  //     barrier
  //     (C) C0: dma_wait_all (ensure next pair loaded for next iteration)
  //     barrier
  //   end loop
  // =========================================================================

  // (A) Pre-load pair 0
  if (cid == 0) {
    for (unsigned int c = 0; c < num_cores; ++c) {
      unsigned int sec = 0 * num_cores + c;
      dma_load_p2_section(SLOT(c, 0),
                          working_samples, twi_p2_dram,
                          sec, nfft, nfft_sec, ntwi_p2);
    }
    snrt_dma_wait_all();
  }
  snrt_cluster_hw_barrier();  // (A) barrier

  for (unsigned int k = 0; k < sec_per_core; ++k) {
    const unsigned int cur_s   = k & 1;
    const unsigned int nxt_s   = 1 - cur_s;
    const unsigned int cur_sec = k * num_cores + cid;

    // (B) C0: start loading next pair
    if (cid == 0 && k + 1 < sec_per_core) {
      for (unsigned int c = 0; c < num_cores; ++c) {
        unsigned int nxt_sec = (k + 1) * num_cores + c;
        dma_load_p2_section(SLOT(c, nxt_s),
                            working_samples, twi_p2_dram,
                            nxt_sec, nfft, nfft_sec, ntwi_p2);
      }
    }

    // (B) Both cores run Phase 2 on their current section
    double *cur_slot = SLOT(cid, cur_s);
    double *p2_s     = cur_slot;               // ping: Re@0, Im@nfft_sec
    double *p2_buf   = cur_slot + 2*nfft_sec;  // pong: Re@0, Im@nfft_sec
    double *p2_twi   = cur_slot + 4*nfft_sec;  // [Re:ntwi_p2][Im:ntwi_p2]
    double *out_sec  = out_dram + cur_sec; // section's output base (strided)

    fft_p2_kernel(p2_s, p2_buf, p2_twi, out_sec,
                  l1_store_idx,
                  nfft_sec, nfft,
                  log2_sec, stride,
                  log2_S, ntwi_p2);

    snrt_cluster_hw_barrier();  // (B) barrier

    // (C) C0: ensure next pair is loaded before next iteration's compute
    if (cid == 0 && k + 1 < sec_per_core) {
      snrt_dma_wait_all();
    }
    snrt_cluster_hw_barrier();  // (C) barrier
  }

  // =========================================================================
  // Results
  // =========================================================================
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    stop_kernel();
    timer_tmp = benchmark_get_cycle() - timer_tmp;
    timer = timer_tmp;
    write_cyc(timer);

    long unsigned int performance =
        1000 * 5 * nfft * (LOG2_NFFT + 1) / timer;
    long unsigned int utilization =
        (1000 * performance) / (1250 * num_cores * 4);

    #ifdef PRINT_RESULT
    printf("\n----- db fft on %d samples -----\n", nfft);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
    #endif

    #ifdef DEBUG
      unsigned int rerr = 0, ierr = 0;
      for (unsigned int i = 0; i < nfft; i++) {
        if (fp_check(out_dram[i], gold_out_dram[2 * i]))
          rerr++;
      }
      for (unsigned int i = 0; i < nfft; i++) {
        if (fp_check(out_dram[i + nfft], gold_out_dram[2 * i + 1]))
          ierr++;
      }
      #ifdef PRINT_RESULT
      if (rerr || ierr)
        printf("Errors: Re=%u Im=%u\n", rerr, ierr);
      else
        printf("Result OK\n");
      #endif
    #endif
  }

  snrt_cluster_hw_barrier();
  set_eoc();
  return 0;
}
