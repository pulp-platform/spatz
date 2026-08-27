# Sparse-DMA (indexed gather) — design & discussion log

Working branch: `sparse-dma`. Goal: extend the Spatz cluster DMA + memory path
to accelerate **indexed (gather/scatter) transfers** (e.g. LLM KV-cache gather),
studied on a DRAMSys-backed testbench.

## Setup
- DRAM: **HBM2E-3600** (DramType "HBM2"), MAX BW **57.55 GB/s**, 2 pseudo-channels,
  64 banks. On-chip wide AXI: 512-bit @1 GHz = 64 GB/s. (DDR4-2400 = 19.2 GB/s
  was a bottleneck vs the on-chip bus; HBM4 not available in DRAMSys.)
- Observability: `util/dram_traffic_plot.py` + `make plot` (per-burst R/W, DMA-vs-
  other via time-gated windows from `[DMA]`/`[DMADONE]` monitors, ACT rate, bank
  strip, peak/bank auto-detect). See `util/dram_traffic_plot.README.md`.

## Baseline benchmark: `sw/spatzBenchmarks/gather/`
KV gather: 2048×128 fp16 matrix in DRAM; 128 unique, sorted indices; core issues
128 scattered 1D DMAs (256 B each) into L1; verify bit-exact vs DRAM. Two SW
variants (compile `-DGATHER_OPT` for the second): `gather_baseline` (calls
snrt_dma_start_1d) and `gather_opt` (inlined offload, hoisted invariants,
running pointers, ~12 vs ~19 instr/iteration).

## KEY FINDING (2026-08-06) — bottleneck is OUTSTANDING DEPTH, not the core
Measured gather (128 DMAs, HBM2E):
- `gather_baseline`: **3948 cyc**
- `gather_opt`:      **3948 cyc**  ← IDENTICAL despite ~40% fewer instructions.

=> The gather is **NOT core-instruction-bound.** Cutting the loop had zero effect
because the core stalls on the `dmcpyi` offload: the DMA request FIFO is only
`DMAReqFifoDepth = 3` and `DMAAxiReqFifoDepth = NumAxInFlight = 3`, so at most
**3 scattered AXI reads are in flight**. With 3 in flight, the ~row-open latency
per scattered 256 B read cannot be hidden across HBM's 64 banks — accesses
serialize (~30 cyc/transfer) and HBM bandwidth is unreachable.

(This corrects the earlier "core-issue-bound / ~22 cyc-per-instruction" reading —
the steady issue cadence was the depth-3 FIFO *drain* rate, not core execution.
The optimized-baseline experiment disambiguated it.)

Corroborating: DDR4 gather (3498 cyc) was *faster* than HBM2E (3948) — consistent
with a low-outstanding, **latency-bound** regime (per-access latency matters, not
bandwidth/banks), where HBM's slightly higher per-access latency loses.

## Proposed HW: indexed-gather DMA engine
Core configures once: `src_addr`, `tgt_addr`, `idx_addr` (index stream in L1 SPM
for the first experiment), `index_width`, `xfer_size`, and `num_idx` (=G). The
engine reads the index stream and emits a **stream of per-transfer requests**
(`src = src_addr + idx[i]*xfer_size`, `dst = tgt_addr + i*xfer_size`,
`len = xfer_size`) to the memory controller — no per-access core instructions.

Fits the existing frontend as a sibling of `axi_dma_twod_ext`: today
`DMSRC/DMDST/DMCPY → twod_req → twod_ext → burst_req stream → iDMA backend`; the
indexed engine is the same "descriptor → burst_req stream" pattern, driven by an
index array instead of stride/repeat. Backend, crossbar, DRAM path all reused.

### Open design decisions
1. **Index-fetch path** (the one new piece): engine must read `idx_addr` from L1.
   Lean toward prefetching the (small, 512 B) index array into an internal FIFO
   at launch, so it doesn't share the data read port. Alt: dedicated TCDM port.
2. **Config/launch interface**: reuse offload path — reuse DMSRC/DMDST for
   src/tgt, add DMIDX (idx_addr+width) + DMGATHER (launch); or CSRs.
3. **One request per index for the first cut** (defer coalescing) — keeps the DRAM
   access pattern identical to the baseline so speedup is attributable to
   issue-rate + outstanding-depth alone. Coalescing (merge indices sharing a
   row/line, à la axi-pack `coalesce_unit`) is a later, separately-measured step.
4. Index width: uint32 unpacked first; packed sub-word needs a bit-extractor.
5. Gather (reads) first; scatter (indexed writes) later.
6. **Crucially, pair the engine with sufficient outstanding depth** (see below) —
   fast request generation alone won't help if only 3 can be in flight.

## SWEEP RESULT (2026-08-06) — depth is NOT the bottleneck; LAUNCH RATE is
DMAAxiReqFifoDepth = DMAReqFifoDepth swept 3→8→16→32→64 (HBM2E, gather 128×256B, all CORRECT):

| depth | baseline cyc | opt cyc |
|-------|-------------|---------|
| 3     | 3948        | 3948    |
| 8     | 3942        | 3608    |
| 16    | 3941        | 3609    |
| 32    | 3942        | 3607    |
| 64    | 3942        | 3609    |

Findings (hypothesis "deeper FIFO → approach HBM peak" was WRONG):
- **Baseline is flat** (~3942) across ALL depths → never depth-limited; it is issue-bound by its ~19-instr sequence, which is slower than even a depth-3 FIFO drains.
- **Opt improves only 3→8** (3948→3608, ~9%) then saturates → at depth 3 the shallow FIFO throttled the faster (~12-instr) opt down to baseline; depth≥8 unmasks opt's instruction advantage, and beyond 8 nothing helps.
- => The gather is **LAUNCH-RATE-bound**: core+offload can only launch ~1 transfer / ~28-31 cyc (baseline 3942/128≈30.8; opt 3608/128≈28.2). Best SW = opt@≥8 = **3608 cyc = 9.08 GB/s = 16% of HBM 57.55**.
- **Why depth is moot for SW**: launch (~28-31 cyc) is SLOWER than DRAM service (~30-40 cyc/read), so only ~1-2 reads are ever in flight regardless of FIFO depth → the outstanding window never fills → HBM's 64-bank parallelism unused.
- **Implication (KEY):** depth only becomes the knob ONCE launch is fast. A HW gather engine that launches ~1-2 cyc/transfer WOULD fill the outstanding window, and THEN NumAxInFlight (~30 to cover latency×banks) is what buys HBM bandwidth. So the design needs BOTH fast request generation AND deep outstanding — neither alone suffices. (Config restored to depth 3.)

## CYCLE-LEVEL TRACE (2026-08-06, opt @depth 64≈8) — refines to DRAM-SERIAL-DRAIN-bound
Snitch tracer (logs/trace_hart_00000.dasm, per-instr cycle+stall). Opt inner loop = 12 instr
(mv,add,lw,addi,addi,slli,add,dmsrc,li,dmdst,dmcpyi,bne). Steady loop = **13 cyc/iter** (12 instr
+ 1 load-use stall: lw a1 → slli a1; offloads dmsrc/dmdst/dmcpyi do NOT stall in the loop). First
iter 79 cyc (icache warmup).
Decomposition of the 3608-cyc gather:
- Issue phase (128 iters) = **1731 cyc** (~13.5/iter) — core races ahead filling the DMA FIFO.
- wait_all drain tail = **~1771 cyc** — core spins in dmstati waiting for the DRAM backlog.
- => effective **28 cyc per 256B scattered read** = the DRAM SERIAL SERVICE rate (one row-miss
  latency per read; reads do NOT overlap across HBM's 64 banks).
**Bottleneck is the DRAM serially servicing scattered row-miss reads (~28 cyc each), NOT the core.**
The loop is fast (13 cyc); the core just waits. This is why: (a) fewer instructions (opt) barely
helped, (b) deeper FIFO barely helped — the reads serialize regardless. opt (issue 13<28 drain) is
drain-bound @28.2; baseline (issue ~19-20, still <28ish) ends up @30.8 (slightly worse overlap).
9.08 GB/s = 16% of HBM 57.55.
**KEY OPEN QUESTION for the HW design:** WHY don't the scattered reads overlap across banks even
with NumAxInFlight=64? (single AXI ID → in-order return? iDMA serializes transfers?) If the engine
can issue reads that the DRAM controller overlaps across banks (distinct IDs / reorderable), the
row-miss latencies hide and throughput approaches burst-limited — that's the real prize, bigger than
just moving launch to HW. Investigate the iDMA AXI-ID / outstanding behavior next.

## DEPTH-8 INSTRUCTION-LEVEL (2026-08-06, traced, evidence-backed)
Rebuilt at depth 8 (cfg=8, wrapper DMAAxi/ReqFifoDepth=8). gather-opt=3608 cyc (matches sweep).
Opt loop = 12 instr; pure execution = 13 cyc = 12 instr + 2 stalls (net 1 extra cyc):
- STALL#1 load-use: lw a1 (b08) → slli a1 (b14), 1 cyc every iter (trace: b14 stall=0x1). Only 2 filler addi between load & use; load latency needs 3.
- STALL#2 dmcpyi (b28): 0–~50 cyc VARIABLE. Trace (saturated iter): b24 dmdst retires @8983, b28 dmcpyi retires @8994 = 11-cyc gap, core frozen. RTL cause: DMCPY sets acc_qready_o=twod_req_ready; axi_dma_twod_ext `assign twod_req_ready_o = !req_fifo_full` (REQ_FIFO_DEPTH=DMAReqFifoDepth=8). FIFO full → dmcpyi stalls. dmsrc/dmdst set acc_qready_o=1'b1 unconditionally → never stall (trace: b1c→b20→b24 all 1cyc apart).
Per-iter deltas: first ~23 @13cyc (FIFO filling), then irregular 13–65 (FIFO full, paced by DRAM drain; variability = row hit vs miss). Avg 28.2 = DRAM scattered-read service rate.
RECONCILIATION: same 3608 at d8 & d64, stall just MOVES — d8: dmcpyi stall inside loop; d64: no loop stall, wait in snrt_dma_wait_all tail. Drain-bound either way; buffer depth relocates the wait, doesn't remove it (reads don't overlap across banks). Trace: logs/trace_hart_00000.dasm (1 line per retired instr; multi-cyc stalls show as timestamp gaps, e.g. dmcpyi; load-use shows as stall=1 lines). cfg currently at depth 8.

## DRAM-DIRECT "IDEAL CASE" (2026-08-06/07) — DECISIVE: bottleneck is the ISSUE PATH, not DRAM/pattern
Colleague ran the standalone DRAMSys AXI testbench at `/scratch2/bowwang/spatz_nf/sparse-dma/dram_rtl_sim`
(`make all`). This is `.bender/.../dram_rtl_sim-*/test/axi_to_dram_tb.sv` — a synthetic AXI master
driving DRAMSys directly (NO core, NO iDMA). We modified `speedTestRead` to our EXACT gather pattern:
- 256-B reads (ax_len=3 → 4-beat burst), addresses = BASE + GatherIdx[i]*256 using the REAL indices
  copied from the benchmark header (`gather_index_dram`, 128 sorted values 17..2029), embedded as a SV
  localparam `GatherIdx[0:127]`. DRAMType "HBM2". Fork fires ALL ARs back-to-back (no throttle), single
  AXI ID. Clock 1 GHz. (Our .bender copy also has WINDOW/NIDS knobs but the RUN version dropped them.)
- **RESULT: 34.69 GB/s = 60.2% of 57.55 peak** (DRAMSys banner confirms 60.19% AVG BW). = 7.38 cyc/read.
- vs core-issued gather: 9.08 GB/s = 15.8% = 28.2 cyc/read. **3.8× gap, SAME DRAM, SAME addresses, SAME clock.**

WHY the gap (per-read decomposition; 256 B = 4 beats = 4-cyc data floor):
- Ideal 7.4 cyc/read = 4 data + ~3.4 overhead: ~100 ARs queued at the controller → it ACTIVATEs future
  rows/banks while bursting the current read → row-miss latency HIDDEN behind data bursts (bank overlap).
- Core 28.2 cyc/read = 4 data + ~24 EXPOSED row-miss latency: only ~1 read in flight → controller queue
  ~1 deep → every ACT+CAS (~28 ns = full row-miss) paid serially, one bank at a time.
The DRAM's bank pipelining WORKS under our exact scatter; the core/iDMA just never fills its queue.

RTL ROOT-CAUSE INVESTIGATION (2026-08-07, verified in idma/frontend code):
- **NOT an AXI-ID problem.** `next_id` increments per transfer (fe.sv:288) → burst_req.id → idma_req.opt.axi_id.
  So transfers carry DISTINCT ids (mod IdWidth); ordering does NOT force serialization. (Earlier single-ID
  hypothesis REFUTED.) The standalone TB reaches 60% with a SINGLE id → outstanding DEPTH is the lever, not ids.
  In-order return is fine here because all reads are ~equal-latency row-misses (no head-of-line benefit from OoO).
- **The gather is a COPY (DRAM read → L1 write), rw COUPLED.** dmcpyi cfg=0 → decouple_rw=cfg[0]=0 (fe.sv:422);
  RAWCouplingAvail(1). TB does PURE reads (no write side) — key asymmetry.
- **NumAxInFlight pipelines BEATS WITHIN one descriptor, not across descriptors.** Our descriptor is tiny
  (4 beats), so 60 of 64 slots are idle; sweeping depth 3→64 did nothing because no single transfer has
  enough beats to fill the pipeline. The per-descriptor turnaround (legalize → couple → complete) is serial,
  so transfer N+1's read doesn't launch into the gap while N is in DRAM → effective ~1 outstanding.
- **DEBURSTING (Q1, surprising):** frontend forces `src_max_llen=dst_max_llen='0'` (fe.sv:145-148, comment
  "only supports completely debursting"). So each 256-B row is split into **4 single-beat (64B) AXI bursts**,
  NOT one 4-beat burst. Core path issues 4× the AR commands vs the TB's clean 4-beat burst. (The 4 beats hit
  the same row → 1 miss + 3 hits, so not 4× latency, but strictly worse AXI behavior.)
=> The iDMA is a bulk-copy engine (few LARGE descriptors; NumAxInFlight pipelines their many beats → full BW).
   Our workload (128 × 4-beat descriptors) is the pathological opposite. NOT a bug — a design-INTENT MISMATCH.
   That mismatch IS the motivation for a dedicated gather engine.

DESIGN Q&A / DIRECTION (2026-08-07):
- No AXI/DRAM protocol barrier to continuous issue (AXI = built for outstanding/OoO txns; TB proves it).
  An engine that issues continuously WILL approach ~60%. Sizing only: useful outstanding ≈ #DRAM banks
  (HBM2E ~32; ~16-32 in flight hides latency, more buys nothing); buffering = outstanding × 256 B (~8 KB);
  issue real bursts (undo debursting). **60% is the NO-COALESCING ceiling** (each row still needs 1 ACT);
  coalescing same-row/page indices is the lever BEYOND 60%.
- TWO architectures considered:
  A. Augment the CLUSTER-side DMA engine to fire a deep, deburst-free outstanding read stream over the
     existing AXI path. Smaller change, reuses writeback path; must cover long round-trip latency (more
     buffering); coarse (no bank knowledge). Good as a quick VALIDATION of 16%→60% in-cluster.
  B. Send the index stream to the MEMORY CONTROLLER; it generates DRAM requests directly (like the TB
     master). Reuses axi-pack `axi_pack_indirect_read_conv`; closer to DRAM (less buffering, lower latency
     to sustain deep outstanding); native bank/row knowledge → bank-aware COALESCING past 60%; cluster
     sends compact indices (no request storm), debursting problem disappears. NEW piece: index delivery
     from cluster side (vs axi-pack's memory-side fetch) + WRITEBACK of gathered rows into L1/TCDM.
  Both reach ~60% on the first experiment. **RECOMMEND: target B** (matches project goal + axi-pack reuse +
  coalescing headroom); optionally do A-lite first to de-risk the bandwidth thesis quickly.

## CROSSBAR OUTSTANDING CAP (2026-08-07) — MaxMstTrans=4 was a BINDING limiter (measured)
Colleague spotted axi_xbar MaxTrans. Investigation:
- The `MaxTrans(4)` at axi_xbar.sv:234 is the axi_err_slv (decode-ERROR sink, unmapped addrs) — NOT our path. Red herring.
- The REAL cap: `axi_demux.MaxTrans = Cfg.MaxMstTrans`. Spatz sets **MaxMstTrans=MaxSlvTrans=4** (spatz_cluster.sv:94-95;
  cfg schema key `trans` default 4 = "Outstanding transactions on the AXI network"; wrapper passes 4, not overridden).
  The DMA reads traverse the WIDE `DmaXbarCfg` xbar (DMA is a wide master).
- Mechanism: axi_demux_simple keeps a PER-AXI-ID counter (depth idx_width(MaxTrans)=2 → 4). When an id has 4 reads
  outstanding, `ar_id_cnt_full` de-asserts ar_ready → stalls new ARs for that id. So ≤4 outstanding PER ID.
- ID width twist: wide DMA id width `WideIdWidthIn = AxiIdWidthOut(=IwcAxiIdOutWidth=3) - clog2(NrWideMasters=3) = 1 bit`
  → only **2 distinct ids**. So total DMA outstanding ≤ 4×2 = 8 beats. With debursting (4 same-id single-beat ARs/row),
  that is ~2 rows in flight — far below the ~16-32 needed to hide HBM latency. THIS is why the DMAAxiReqFifoDepth sweep
  did nothing: iDMA offered more, xbar refused.
MEASURED (raise trans via cfg `"trans"`, regen+rebuild; opt kernel):
| config             | opt cyc | GB/s | %peak | cyc/row | binding limiter |
| trans=4,  depth=8  | 3608    | 9.08 | 15.8% | 28.2 | crossbar (4 outstanding) |
| trans=4,  depth=32 | 3607    | 9.08 | 15.8% | 28.2 | crossbar |
| trans=32, depth=32 | 1952    | 16.79| 29.2% | 15.2 | CORE issue rate |
| trans=64, depth=64 | 1952    | 16.79| 29.2% | 15.2 | CORE issue rate (PLATEAU: 32→64 no change) |
=> Crossbar cap CONFIRMED binding: trans 4→32 gave **1.85× (3607→1952, 16%→29%)**. But then it PLATEAUS (32→64 identical).
TWO signals we are now CORE-ISSUE-BOUND, not DRAM/xbar-bound: (1) the plateau; (2) opt vs baseline DIVERGE at
trans=64/depth=64: opt=1952 vs **baseline=3099** (24.2 cyc/row) — when DRAM-bound they were ~equal (~3900), now the
kernel instr count sets the time.
LAUNCH-RATE CEILING (colleague, confirmed): core issues ~13 cyc per DMA transfer (opt loop steady state) → 128×13 =
**1664 cyc ≈ 19.7 GB/s ≈ 34% peak** floor for SW. We hit 15.2 cyc/row (1952), right against it (~2 cyc/row residual drain).
CEILING HIERARCHY: 60% (DRAM real, clean bursts + deep outstanding, no core) / 34% (SW core-issue ceiling) /
29% (reached by opening crossbar) / 16% (original, crossbar-capped).
=> SW CANNOT pass ~34% (per-transfer 13-cyc loop). Gap 34→60% = core issue tax + debursting. **Only a HW frontend that
   interprets the index stream (no per-transfer core loop, clean bursts, deep outstanding) breaks past 34% toward 60%.**
   The experiment MEASURES the motivation for the engine. Also: raising `trans` is a worthwhile standalone RTL default
   change (real 1.85× win). Config state: cfg + built vsim now trans=64/depth=64; ORIGINAL was trans(default 4)/depth 8.

## Experiment plan
1. **[PRIORITY] DMAAxiReqFifoDepth / NumAxInFlight sensitivity sweep.** Sweep
   3 → 8 → 16 → 32 → 64 (regen cfg + rebuild), re-run the gather (baseline and
   opt) on HBM2E, plot BW/ACT/bank-occupancy. Hypothesis: gather time drops
   sharply as more scattered reads pipeline across banks, until the *core issue
   rate* becomes the new limiter (at which point opt > baseline should finally
   diverge, and the HW engine becomes necessary to go further).
2. Build the indexed-gather HW engine; compare vs the SW baselines at matched
   outstanding depth. Expect it to remove the core-issue limit so deep-FIFO +
   HW-engine approaches HBM bandwidth.
3. Later: coalescing; scatter; indices-from-memory-channel; address-mapping
   sensitivity for gather (am_hbm2e).

## Slide deck (in progress, 2026-08-07)
Building a deck to explain the finding. Agreed structure:
- Slide 1 "Ideal case" — DRAM-direct TB hits 60% under our exact gather pattern (deep outstanding, banks
  overlap). Diagram: overlapped bank timeline. Ceiling 60% (no coalescing).
- Slide 2 "Reality" — core-issued gather with OPTIMIZED kernel still 16%. Kernel is near-perfect (12 instr,
  ~13 cyc, ~1 IPC) but loop was never the bottleneck; iDMA keeps ~1 read outstanding → serial row-misses.
  Diagram: serial bank timeline (contrast slide 1). ~9% better than un-opt kernel = optimizing kernel can't help.
- (planned) Slide 3 "Why the gap" (debursting + per-descriptor serialization + coupled copy, NOT ids);
  Slide 4 "HW engine" (A vs B, recommend B). Keep the two bank-timeline diagrams visually parallel.

## Status / files
- Committed: DRAMSys integration, DDR4 study, observability suite (git b76f8259).
- Uncommitted: gather benchmark (`sw/spatzBenchmarks/gather/`, incl. gather_opt),
  HBM2E switch, plot auto-detect + `make plot` tdb-glob fix, this NOTE.md, PROMPT.md.
- Standalone DRAM-direct TB: `.bender/git/checkouts/dram_rtl_sim-*/test/axi_to_dram_tb.sv` (our edit:
  sparse gather pattern + GatherIdx from header + HBM2 + WINDOW/NIDS knobs). Colleague's RUN copy at
  `/scratch2/bowwang/spatz_nf/sparse-dma/dram_rtl_sim` (1 GHz clock, no knobs) → `make all` → 60.2%.
  NOTE: `.bender` checkouts are NOT tracked; port the TB into the repo before committing if we want to keep it.
