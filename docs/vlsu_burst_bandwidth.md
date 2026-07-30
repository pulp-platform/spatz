# Spatz VLSU: burst requests and sustaining full load bandwidth

How the Spatz vector load/store unit was changed to issue **burst** memory requests and to keep
enough of them in flight to sustain bandwidth, what each change is worth in measured cycles, and
where the remaining ceilings are.

Target system: Spatz + Snitch core complexes in a TeraNoC / MemPool-style shared-L1 cluster
(`terapool_spatz4_fpu`: 256 cores, 16 groups x 16 tiles, 1 core/tile, VLEN=512, 4 FPUs/core).
Benchmark throughout: `sp-fmatmul-opt-burst-merge`, `e32,m2` (VL=32 words), 256x32x256 unless
stated.

---

## 1. Why this work exists

A vector load of VL=32 words used to leave the core as **32 independent single-word requests**.
Each one occupies a request slot, a NoC flit, an MSHR lookup, and a response beat. The interconnect
is word-granular, so nothing downstream could tell that those 32 words were one contiguous line.

Two consequences:

* **Request-side cost.** 32 requests/load, all needing IDs, arbitration and NoC bandwidth.
* **No coalescing opportunity.** Cores sharing a matrix row issue overlapping *sets* of words with
  no marker saying "this is one 16-word line", so the group MSHR could not merge them cheaply.

The fix has two halves: make the VLSU **emit bursts**, and make sure it can keep **enough bursts in
flight** that the memory system stays busy. The second half turned out to be the harder one.

---

## 2. Burst emission

The VLSU splits an aligned vector load into **16-word bursts** (`MaxBurstWords = 16`) and sends one
request per burst, carrying a length field, instead of 16 single-word requests. At `e32,m2`
(VL = 32 words) one vector load becomes **2 bursts**.

### 2.1 Admission rule, and why `vl=64` cannot burst

Burst emission is only taken when

```
vl <= NrOutstandingLoads * MemDataWidthB
```

At `NrOutstandingLoads = 32` that is 128 B, so `e32,m2` (128 B) qualifies and `e32,m4` (VL=64,
256 B) does **not** — an m4 load falls back to the multi-port word-interleaved path.

The reason is deadlock avoidance, not bookkeeping. A burst reserves its full ID range up front. If a
load needed more IDs than the ROB has, the second half of the load could not be reserved until the
first half retired — but VRF writeback is scoreboard-blocked behind the *whole* instruction, so the
ROB could never drain between batches. The rule keeps a whole load's ID range simultaneously
reservable.

### 2.2 Bursts are port-0 only

Bursts are emitted **only** on port 0. The other VLSU ports carry the word-interleaved path. An
early bug had `burst_mode_req[port]` asserted on interleaved-path ports as well, producing
burst-tagged requests on a path whose responses are not burst-ordered. The fix ANDs in
`mem_use_port0_burst`, and a `TARGET_MEMPOOL`-gated tripwire keeps it that way:

```systemverilog
gen_burst_only_in_port0_mode:
  assert property (spatz_mem_req_valid[port] && burst_len > 1 |-> mem_use_port0_burst);
```

---

## 3. Keeping bursts in flight

Emitting bursts is not enough. Little's law over the measured numbers said the loop needed
**N = L/W = 109/64 ≈ 1.70** loads in flight and was achieving **1.23**. Three changes closed most of
that gap.

### 3.1 Block ROB-ID reservation — `SPATZ_VLSU_BLOCK_ALLOC` (default **on**)

**Problem.** The port-0 burst allocator walked its ROB IDs **one per cycle**. A 16-beat burst
therefore waited 18 cycles between becoming eligible and its request handshake
(1 decide + 16 walk + 1 send).

**Change.** The ROB grants the whole 16-ID window in a single cycle: decide → reserve → send = **3
cycles**. That removes 15 cycles per burst, i.e. **30 cycles per vector load**, straight out of the
load recurrence.

Implementation notes:

* `burst_len` is binary here (16 or 1), so `wp + 16 mod 32` is a single inverter — no adder.
* The room check must be **non-strict**: `status_cnt_q <= NumWords - BlockWords`. A strict `<`
  silently re-serialises the second burst of every load.
* The window mask is quadrant-generalised (`BlkLoW = idx_width(BlockWords)`, one-hot `eq_wp`/`eq_wp1`
  selects) and was verified exhaustively to be bit-identical to the MSB-XOR form at
  `NumWords/2`.

**Measured: 3821 → 3589 cycles (−6.1%), reproduced on two independent runs.**

Worth knowing: the instruction-active count dropped by 488 cycles but wall-clock only by 232 — a
0.48 ratio. The 36-cycle walk had been partly overlapped with the 64-cycle compute window, so about
half the saving turned into idle turnaround rather than speed. Netlist is bit-identical when off.

### 3.2 ROB depth 64 — `SPATZ_VLSU_ROB_DEPTH` (unset = 32)

Doubling the ROB gives room for **two** m2 loads in flight (2 x 32 IDs). This is a **system-wide
width change**, driven from a single root so the widths cannot drift apart:

```
snitch_pkg::RobDepth  ->  meta_id_t  ->  every TCDM struct
                      ->  both FlooNoC flit metas
                      ->  the group MSHR
spatz NrOutstandingLoads
spatz_mem_rsp_t.id   (generated pkg AND its .tpl -- edit both or regeneration reverts it)
```

`MetaIdWidth` goes 5 → 6 bits, which **widens every mesh link by one bit** (PNR must re-close). No
floogen regeneration is needed.

A "contained-ID" variant that hid the 6th bit from the NoC was evaluated and is **unsound**: the
MSHR's `req_meta_ovlp_map` relies on per-core meta-ID uniqueness, which the hidden bit breaks.

**Measured: cycle-identical (3589) on its own.** It is purely an enabler for the next item.

### 3.3 H1 dual-load runahead — `SPATZ_VLSU_DUAL_LOAD` (unset/1 = legacy, 2 = on)

**Legacy:** the next load starts only when the previous one has fully **retired**.
**H1:** the next burst-safe load starts as soon as the previous one's requests are all **issued**,
so its flight overlaps the elder's drain.

Mechanism: `dual_adv` re-keys `mem_spatz_req_ready` to fire on `mem_req_all_issued`, capped by
`commit_insn_q.id == mem_spatz_req.id`. Commit attribution stays **positional**, so no extra ID tags
are needed. Requires ROB64 for the room.

**Measured with ROB64: 3589 → 3488 cycles (−2.8%)**, `dual_adv` firing on 32 of 40 instructions,
all assertions silent.

**Why the gain is small.** Block-alloc had already pulled the required in-flight count down to
≈1.23, which is what the loop was already achieving — so there was little concurrency left to
recover. The measured `blk_stall = 64` shows load B is rate-limited by A's drain, not by issue. The
remaining idle time is warm-up, I-cache, barrier and inter-core skew, none of which more
memory-level parallelism can fix.

---

## 4. Response bandwidth: the other half

Requests were no longer the limit; **responses** were.

A per-core burst response was capped at roughly **2 words/cycle** by two independent constraints:
the MSHR drains **one beat per entry per cycle**, and only **2 response ports** are usable. Any
VLSU-side or port-selection-only fix is a no-op against that ceiling — several were tried and
measured zero.

**ParityDrain (2-wide receive)** addresses the first constraint. Beat `b` is delivered on tile
response port `1 + (b & 1)` with `core_id + (b & 1)`. The wire contract is unchanged — `meta_id`
stays `base + b`, one contiguous range — so entries remain **fully mergeable** and coalescing is
untouched. On the VLSU side a `burst_odd_expected` classifier feeds a **dual-ported ROB0** doing
2-wide commit.

**Measured: 4453 → 4009 cycles (1.11x), and 3836 (1.16x)** together with the bypass-retag table.

Going beyond ~2 words/cycle per core needs `noc_resp_channel_num = 4`; it is a NoC provisioning
limit, not a VLSU one.

---

## 5. Area-only cleanups (no cycle change)

Both are deliberate netlist changes that delete provably unreachable logic, so they are **not**
bit-identical to the off build. Keep them off until each has its own LEC/timing run.

| Knob | Effect | Saving |
|---|---|---|
| `SPATZ_ROB_CNT_IDVALID` | derive `id_valid_o` from `status_cnt_q` instead of the free-ID bitmap | −`NumWords` flops, −1 decoder, −2 32:1 muxes per ROB (x4 ROBs/core), ~6 fewer logic levels on `id_valid_o -> mem_req_lvalid` |
| `SPATZ_VLSU_COMMIT_QMIN` | commit-metadata FIFO depth `NrOutstandingLoads`(32) → `NrParallelInstructions`(4) — the most that can ever be resident, since the push is gated on `mem_insn_pending_q` | −28 x 37 flops and a 37b 32:1 read mux |

Both measured **cycle-identical** at ROB32 and ROB64.

---

## 6. Results summary

256x32x256, `e32,m2`, one core-complex per tile:

| Configuration | Cycles | Δ |
|---|---|---|
| baseline (pre-campaign) | 3905 | |
| + per-type bank shift and burst fixes | 3821 | −2.2% |
| + `SPATZ_VLSU_BLOCK_ALLOC=1` | **3589** | −6.1% |
| + `SPATZ_VLSU_ROB_DEPTH=64` alone | 3589 | 0 (enabler) |
| + `SPATZ_VLSU_DUAL_LOAD=2` | **3488** | −2.8% |

FPU floor for this shape is **2048 cycles** (8192 MACs/core ÷ 4 lanes), so 3488 is **58.7%** of
peak. A realistic target is 85–90%; the gap is not memory-level parallelism.

**Enabling the measured-best configuration:**

```make
spatz_vlsu_block_alloc ?= 1
spatz_vlsu_rob_depth   ?= 64      # required for dual_load to have ROB room
spatz_vlsu_dual_load   ?= 2
```

`dual_load` without `rob_depth=64` has nowhere to put the second load and does nothing.

---

## 7. Known ceilings

1. **Response bandwidth**, not requests, is the current per-core limit (§4). Beyond ~2 words/cycle
   needs `noc_resp_channel_num = 4`.
2. **`e32,m4` cannot burst** (§2.1) — it exceeds the ID-range admission rule and falls back to the
   word-interleaved path.
3. **The kernel is compute-bound at this shape.** Coalescing saves NoC traffic, which is not the
   scarce resource here; it does not raise throughput on its own.
4. **Bursts do not share a response cache.** A burst request can never hit a `CACHED` MSHR entry
   (that path requires `req_len == 1`) and burst entries never finalise to `CACHED`, so two cores
   loading the same line at different times fetch it twice. A burst-line response cache is the
   remedy, not more MSHR ways.
5. **Inter-core skew dominates the residual idle time.** Measured skew between the column-lanes of
   the matmul work split exceeds 1024 cycles, which is larger than any merge or rendezvous window —
   fix the skew before adding more concurrency machinery.

---

## 8. Verification notes

* **`vlog`-clean is not `vopt`-clean.** Assertion and packed-struct-array syntax has passed `vlog`
  and failed elaboration more than once here (`vopt-13276` on a field select across a packed struct
  array dimension). Only reaching `run` proves elaboration.
* **Cycle counts are layout-sensitive.** ±60 cycles from code-layout changes alone; the H1 delta
  measured −101 and −23 on two binaries of the same source. Always A/B the same binary.
* **Check a file is actually compiled** (`Bender.yml`) before trusting what it says. A conclusion
  about `NumOutstandingMem` was once drawn from a `snitch_lsu.sv` that is commented out of the
  build; the compiled FP-LSU is ID-based out-of-order with 16 outstanding.
* `[VPERF]` counters (`c_dual`, `c_blkstall`) report kernel-window receive behaviour and are the
  quickest check that `dual_adv` is actually firing.
