# Sparse-DMA gather — how to reproduce

This branch (`sparse-dma`) extends the Spatz cluster with a **hardware indexed-gather
DMA engine** and runs it against a **DRAMSys HBM2E** memory model. It lets you reproduce
the two benchmark settings for an indexed KV-cache gather (2048×128 fp16 matrix in DRAM,
128 scattered 256 B row reads into L1):

1. **Core-issued** disaggregated DMA — the Snitch core computes each row address and
   issues one DMA per row (`gather`, and the optimized `gather-opt`).
2. **Hardware gather engine** — one `DMGATHER` instruction stream configures the on-chip
   engine, which reads the index stream from L1 and generates all row reads (`gather-hw`).

Everything needed is committed, **including the benchmark data header** (no regeneration
required). The DRAMSys shared library is the one external artifact you build once.

> Environment: this flow targets the IIS toolchains (QuestaSim, LLVM RISC-V, gcc-11.2)
> via `util/iis-env.sh`, and needs **cmake ≥ 3.28.1** to build DRAMSys. On a non-IIS
> machine, provide equivalent tools and adjust `util/iis-env.sh`.

## 1. Clone and check out the branch

```sh
git clone git@github.com:pulp-platform/spatz.git
cd spatz
git checkout sparse-dma
```

## 2. Fetch dependencies and build the DRAMSys library (one-time)

```sh
bender update                                   # fetch dram_rtl_sim, idma, axi, ...
cd "$(bender path dram_rtl_sim)"                # the DRAMSys co-sim dependency
make dramsys                                    # builds libsystemc.so + libDRAMSys_Simulator.so (needs cmake>=3.28.1)
cd -                                            # back to the spatz root
```

## 3. Source the toolchain environment (every shell)

```sh
source util/iis-env.sh                          # QuestaSim + LLVM RISC-V + gcc on PATH
```

## 4. Build software and hardware

```sh
cd hw/system/spatz_cluster
make generate                                   # regenerate the cluster wrapper from the cfg
make sw                                         # LLVM RISC-V build -> sw/build/... (data header already committed)
make bin/spatz_cluster.vsim                     # QuestaSim build of the cluster + DRAMSys DPI
```

The default config is `cfg/spatz_cluster.default.dram.hjson` (HBM2E, 57.55 GB/s peak,
1 GHz, 512-bit AXI). Relevant DMA/AXI outstanding knobs are `trans` (crossbar
`MaxMstTrans`) and `dma_axi_req_fifo_depth`/`dma_req_fifo_depth` (iDMA `NumAxInFlight`),
all = 64 by default. To change them, edit the cfg then rebuild:
```sh
make -B generate && make clean.vsim && make bin/spatz_cluster.vsim
```

## 5. Run the two settings

From `hw/system/spatz_cluster`:

```sh
# (1) core-issued gather
./bin/spatz_cluster.vsim sw/build/spatzBenchmarks/test-spatzBenchmarks-gather_R2048_D128_G128        # baseline (snrt_dma_start_1d)
./bin/spatz_cluster.vsim sw/build/spatzBenchmarks/test-spatzBenchmarks-gather-opt_R2048_D128_G128    # optimized inline offload

# (2) hardware gather engine (DMGATHER)
./bin/spatz_cluster.vsim sw/build/spatzBenchmarks/test-spatzBenchmarks-gather-hw_R2048_D128_G128
```

Each run prints (via UART):
```
[DMA]     <t0> ns issue id=1 src=0x80003600 ...    # gather launch
[DMADONE] <t1> ns id=1                             # gather complete
[UART] The gather took <N> cycles (128 scattered DMA requests).
[UART] CORRECT!
```
`CORRECT!` = the gathered rows match the DRAM matrix bit-for-bit.

## 6. Reading the results — bandwidth methodology

The workload moves **32768 bytes** (128 rows × 256 B). Report utilization two ways:

- **DRAM/transfer utilization (engine efficiency)** — use the DMA-execution window
  `[DMADONE] − [DMA]` (both printed above), which excludes core-side startup:
  ```
  GB/s   = 32768 / (t1 - t0[ns])
  % peak = GB/s / 57.55
  ```
  This is the number to compare against the DRAM ceiling (an ideal AXI master hits ~62%).
- **End-to-end / variant comparison** — the `took <N> cycles` figure. Consistent across
  variants (all measured the same way), so use it for speedups. It includes one-time
  startup (config offloads + first-run icache warmup + `wait_all`), so at G=128 it reads
  lower than the DMA-window %; do not convert it to "% of peak".

## 7. Expected results (default cfg: `trans=64`, depths=64; HBM2E)

| setting | binary | `took` cycles | DMA-window % peak |
|---|---|---|---|
| (1) core-issued, baseline | `gather` | ~3099 | — |
| (1) core-issued, optimized | `gather-opt` | ~1952 | — |
| (2) **HW gather engine** | `gather-hw` | ~1236 | **~61%** (≈932 ns window) |

The HW engine is ~1.6× faster than the best software (end-to-end) and drives the DRAM at
~61% of peak — matching an ideal AXI master, i.e. near-optimal for this scatter pattern.

## 8. Notes & gotchas

- **Data header is committed** (`sw/spatzBenchmarks/gather/data/…`), so results are
  reproducible without regeneration. To regenerate (e.g. a different shape), edit
  `sw/spatzBenchmarks/gather/script/*.json` and run `make spatz.gendata`.
- **RTL/param changes need `make clean.vsim`** before rebuilding — `vlog -incr` can keep
  stale compiled objects and silently run the old config.
- **Verify loop is sim-time-heavy** (~hundreds of µs of per-element DRAM reads); the
  gather itself is ~µs. This does not affect the reported gather cycles/window.
- The three variants share one source (`gather/main.c`) selected by `-DGATHER_OPT` /
  `-DGATHER_HW` (see `sw/spatzBenchmarks/CMakeLists.txt`).
- Design rationale and the full investigation are in `NOTE.md`.
