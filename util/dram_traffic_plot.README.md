# DRAM traffic visualization (`dram_traffic_plot.py`)

Plots the DRAM-channel traffic recorded by DRAMSys during a Spatz-cluster
simulation, aligned against software benchmark phases. Built to observe how
traffic changes before/after the sparse-DMA work.

## Prerequisites

- A simulation run with DRAMSys enabled (`USE_DRAMSYS=1`, the default), which
  writes a per-channel SQLite trace `DRAMSysRecordable0_ddr4_ddr4_ch0.tdb` into
  `hw/system/spatz_cluster/`. **The trace is overwritten by every run** — copy
  it aside before the next simulation.
- `python3` with `numpy`, `matplotlib`, and (optionally) `pyelftools` for the
  instruction-cache split. All present in the IIS `anaconda3-2023.07` python.

## Quick start

From `hw/system/spatz_cluster`:

```bash
make TEST=<binary-basename> plot
# e.g.
make TEST=test-spatzBenchmarks-dp-faxpy_M1024 plot

# zoom into a time window (ns); omit either bound to keep the default edge
make TEST=test-spatzBenchmarks-dp-faxpy_M1024 START=4000 END=5300 plot
```

This runs the binary on `bin/spatz_cluster.vsim`, captures the transcript,
snapshots the `.tdb`, and writes `plots/<TEST>_dram_plot.png` (plus the `.log`
and `.tdb` snapshots). `plots/`, `*.tdb`, and `*_dram_plot.png` are gitignored.
`START`/`END` (ns) restrict the plotted time window; unset = full first->last
burst span. The ACT rate, bank strip, and auto bin width are recomputed over the
shown window (so zooming reveals per-burst structure), but the **avg and
active-avg reference lines are always computed over the full run** and stay fixed
regardless of the window — they are a stable baseline the zoomed curve is
compared against. `make help-plot` prints usage; `make clean-plot` removes the
plots directory.

Direct invocation (e.g. to re-plot an existing snapshot without re-simulating):

```bash
python3 ../../../util/dram_traffic_plot.py plots/run.tdb \
    --transcript plots/run.log \
    --elf sw/build/.../test-... \
    -o plots/run_dram_plot.png
```

### Options

| flag | meaning |
|------|---------|
| `--transcript FILE` | run log; draws green vertical lines at `[PHASE]` markers |
| `--elf FILE` | classify reads to executable ELF sections as icache refills (gray) |
| `--dram-base ADDR` | base subtracted from ELF vaddrs (default `0x80000000`) |
| `--dma-write-range LO:HI` | writes in this DRAM-relative range are DMA (orange); repeatable |
| `--tstart NS` / `--tend NS` | plotted time window (default: first/last burst); `make` exposes these as `START`/`END` |
| `--peak-gbps F` | bus peak, y-axis ceiling (default 19.21 for DDR4-2400 x8) |
| `--window-ns F` | time-bin width (default: span/400) |
| two `.tdb` files | overlay mode: side-by-side columns (e.g. baseline vs sparse) |

## Phase markers

The testbench prints `[PHASE] <t> ns mark=<value>` whenever software writes the
`BENCHMARK_MARK` peripheral register (offset `0x68`). Call `benchmark_mark(v)`
(in `sw/spatzBenchmarks/benchmark/benchmark.c`) from **core 0** to timestamp a
phase boundary. DMA activity itself needs no marker — it is visible directly in
the DRAM trace; markers exist to delimit the *compute* phase, which the DRAM
cannot see. `dp-faxpy` marks `1`=DMA-in, `2`=compute, `0`=done as a pilot.

## DMA core-issue markers

The DMA frontend (`hw/ip/spatz_cc/src/axi_dma_tc_snitch_fe.sv`) prints
`[DMA] <t> ns issue id=<n> src=0x.. dst=0x.. bytes=<b>` when the core launches a
transfer (the `twod_req` handshake), and `[DMADONE] <t> ns id=<n>` when it
completes (the `completed_id` advance). With `--transcript`, issues are drawn as
**orange downward triangles above every subplot**, labelled `DMA#<id> (<bytes>B)`
inside the read panel; the issue/done pair also defines the time-gated DMA
window used to colour DMA traffic. The monitors are simulation-only
(`pragma translate_off` / `ifndef SYNTHESIS`).

**Timestamps.** All markers (`[PHASE]`, `[DMA]`, `[EOC]`) print
`$realtime/1ns` — nanoseconds independent of a module's local timeunit. (Do
*not* hand-divide `$time` by 1000 and format with `%t`: `%t` applies its own
`$timeformat` scaling, and the combination silently rounds times to bogus
1000-ns multiples. The clock period is 1 ns; the 1 ps only sets simulation
resolution.)

**Two time bases — aligned automatically.** The markers use the RTL testbench
`$realtime`, while the traffic curves come from DRAMSys's *own* trace clock,
which starts at 0 when the `dram_sim_engine` begins clocking (first edge after
reset — it only advances `run_ns` while `rst_ni` is high). The testbench emits
that offset as `[DRAMSYNC] <t> ns` (the SV time of DRAM t=0, ~100 ns for the
default reset/clock timing); the plot adds it to every DRAMSys timestamp so the
traffic lands on the same `$realtime` axis as the markers, restoring causal
ordering. Override with `--dram-time-offset <ns>`; if no `[DRAMSYNC]` line is
present (old trace) the tool warns and applies 0.

The read curve uses `DataStrobeBegin` (data actually on the DQ bus), so it
appears well after a DMA-issue line: measured issue->data ~20 ns for a row hit
(CL only) and ~47 ns for a row miss (on-chip propagation ~6 ns + controller
scheduling ~14 ns + ACT/RCD+CL ~27 ns). Do not confuse `DataStrobeBegin` with
the Transactions table's `TimeOfGeneration`, which is only when the request
*reaches the controller input* (~7 ns after issue) — not when data returns.

## Subplots

1. **read GB/s** — bus bandwidth of reads, split into **DMA read** (blue) vs
   **other read** (gray, = icache refills + core scalar loads). DMA traffic is
   identified by *time-gated windows*: a read counts as DMA only if it hits a
   DMA buffer **and** occurs while that transfer is active. The windows are
   built automatically from the `[DMA]` issue lines (src/dst/bytes) and the
   `[DMADONE]` completion lines the frontend prints — so no manual setup, and no
   false positives from core accesses that merely share a cache line with a DMA
   buffer (a real effect: e.g. pointer globals adjacent to a DMA array). Supply
   ranges explicitly with `--dma-read-range LO:HI` (address-only, no time gate);
   without any DMA info, falls back to `--elf`'s code/non-code split.
2. **write GB/s** — writes. `core/other` (gray) by default; `DMA write`
   (orange) only for addresses in a declared `--dma-write-range` (see caveat).
3. **ACT / us** — DRAM row **ACTIVATE** commands per microsecond. This is the
   row-conflict signal: high ACT rate at fixed bytes = row thrashing. The key
   metric for gather/scatter — coalescing should reduce ACT per byte.
4. **bank strip** — heatmap of accesses per (bank, time-window), 16 banks.
   Colorbar `accesses / window` (magma: black=0, pink=low, yellow=high).

Each bandwidth subplot has three reference lines: **peak** (dashed), **avg**
(dotted, over the first→last-burst span), and **active avg** (dash-dot). The
curves are step (staircase) plots: each series is a saturated step outline over
a light fill (`--style area`, default) or just the outline (`--style line`).
The default display bin (≈ span/250) is deliberately coarse so sparse isolated
accesses (e.g. icache refills) average into a calm low baseline rather than a
spike forest; a dense DMA burst still stands out as a clean stepped block.
Plots render at 200 dpi. Tune with `--window-ns`; zooming (`--tstart/--tend`) auto-refines
the bins. The avg/active-avg numbers are computed on a *fixed* fine reference bin
(`REF_BIN_NS`, 10 ns), decoupled from the display bin, so they stay stable and
meaningful when you change the visual granularity or zoom.

Phase boundaries are labelled with the phase **name** (e.g. `dma-in`,
`compute`, `idle`) rather than the raw mark value. The value->name map defaults
to the dp-faxpy convention and is overridable with
`--phase-names "1=load,2=kernel,0=done"`.

## How bandwidth is computed (and why it never exceeds peak)

Each DRAM burst (BL8 = 64 B) has a data-strobe interval `[DataStrobeBegin,
DataStrobeEnd]` (~3.33 ns) during which it occupies the DQ bus. Per window,
bandwidth = (sum of strobe-interval overlap with the window) / window-width ×
peak. Because strobe intervals never overlap (single shared bus), a curve can
never exceed peak.

> Do **not** compute bandwidth by counting whole bursts at their start time and
> dividing by window width — in a narrow window (~13.7 ns, comparable to the
> ~3.33 ns burst spacing) that overshoots peak by catching one extra burst.
> This was a real earlier bug; occupancy-based accumulation fixes it.

## Reconciling the plot with the DRAMSys end-of-run summary

DRAMSys prints, e.g.:

```
AVG BW:        28.66 Gb/s |  3.58 GB/s |  18.65 %
AVG BW\IDLE:   63.60 Gb/s |  7.95 GB/s |  41.39 %
MAX BW:       153.66 Gb/s | 19.21 GB/s | 100.00 %
```

The plot's numbers and these do **not** match by design — same transferred
bytes (numerator), different time denominators and read/write grouping. Units:
`Gb/s` = giga*bits* (÷8 for `GB/s`); `%` is of the 19.21 GB/s peak. Worked
example (dp-faxpy_M1024, 23,872 bytes transferred):

| quantity | value | = bytes ÷ time | denominator meaning |
|----------|-------|----------------|---------------------|
| DRAMSys **AVG BW** | 3.58 GB/s (18.65%) | 23872 ÷ **6663 ns** | full recorded sim time (incl. ~1.15 us startup idle before first access) |
| plot **avg** (read+write) | 4.35 GB/s | 23872 ÷ **5486 ns** | first burst → last burst |
| DRAMSys **AVG BW\IDLE** | 7.95 GB/s (41.4%) | 23872 ÷ **3002 ns** | time with a request *pending* (excludes idle gaps, **includes** ACT/precharge/CL latency where the bus isn't strobing) |
| pure DQ-bus occupancy | 19.21 GB/s (100%) | 23872 ÷ **1243 ns** | time the data bus is actually strobing (= plot curve height during the burst) |
| plot **active avg** (read) | 9.06 GB/s | — | mean of per-window GB/s over non-empty windows; read-only; window-size dependent |

Key points:
- `AVG BW` vs plot `avg`: identical concept, different clock window. DRAMSys
  divides by the whole sim time (includes the leading idle before the first
  access); the plot divides by the active first→last-burst span.
- `\IDLE` means "**without** idle". It divides by the time the controller had
  work pending (3002 ns here), which still includes latency overhead — so the
  gap between pure bus occupancy (1243 ns, 100% of peak) and non-idle time
  (3002 ns) is the DRAM latency/row-conflict cost. **`BW\IDLE` is the number to
  watch for the sparse work**: a good mapping/coalescer pushes it toward peak
  for the same bytes.
- The plot's `active avg` is a coarser, window-dependent, read-only heuristic —
  useful visually but the least physically rigorous; it is *not* the same
  estimator as `BW\IDLE` and will not match it numerically.

If exact agreement with the DRAMSys dump is ever needed, the tool can be
extended to divide `avg` by full recorded time and to add a
strobe-occupancy-% and a pending-time `BW\IDLE` figure.

## Known limitations

- **Write source cannot be auto-classified.** The trace's `Thread` column is
  uniformly 0, so DMA-engine writes cannot be distinguished from core writes
  automatically. Register spills in Spatz go to the **L1/TCDM stack** and never
  reach DRAM at all; the DRAM writes seen in simple kernels are scalar global
  stores and the HTIF `tohost` store. Declare DMA destinations explicitly with
  `--dma-write-range` (we control those buffers in the sparse benchmarks), or
  teach DRAMSys to record the AXI ID.
- `active avg` depends on `--window-ns`.
- Assumes a single DRAM channel (the current cluster TB configuration).
