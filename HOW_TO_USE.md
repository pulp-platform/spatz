# Sparse-DMA gather — how to reproduce

This branch (`sparse-dma`) extends the Spatz cluster with a **hardware indexed-gather
DMA engine** and runs it against a **DRAMSys HBM2E-3600** memory model. It reproduces the
two benchmark settings for an indexed KV-cache gather (2048×128 fp16 matrix in DRAM, 128
scattered 256 B row reads into L1):

1. **Core-issued** disaggregated DMA — the Snitch core computes each row address and
   issues one DMA per row (`gather`, and the optimized `gather-opt`).
2. **Hardware gather engine** — one `DMGATHER` instruction stream configures the on-chip
   engine, which reads the index stream from L1 and generates all row reads (`gather-hw`).

> **This flow was verified end-to-end on a fresh clone** (numbers in §7 are what you should
> get). It targets the IIS toolchains via `util/iis-env.sh` and needs **cmake ≥ 3.28.1**
> to build DRAMSys. The benchmark data header is committed (no regeneration required).

## 0. Prerequisites — the compiler gotcha (read first)

DRAMSys and QuestaSim's DPI must be built with the **same** host compiler, and it must be
**gcc-11.2.0-af** (Questa's runtime `libstdc++`). `iis-env.sh` does *not* set this, so
export it for the whole session, or DRAMSys fails at load with `std::filesystem` symbol
errors:
```sh
export CC=/usr/pack/gcc-11.2.0-af/linux-x64/bin/gcc
export CXX=/usr/pack/gcc-11.2.0-af/linux-x64/bin/g++
```

## 1. Clone and check out the branch
```sh
git clone git@github.com:pulp-platform/spatz.git
cd spatz
git checkout sparse-dma
source util/iis-env.sh          # QuestaSim + LLVM RISC-V on PATH
# (and the CC/CXX exports from §0)
```

## 2. Dependencies
```sh
bender update                   # fetch dram_rtl_sim, idma, axi, ...
```

## 3. Generate `encoding.h` (needed by snRuntime) — WITHOUT clobbering DMIDX
`sw/toolchain/` is not tracked, so `encoding.h` is missing on a clone. Generate **only**
it — do **NOT** run `make init` / `make update_opcodes`: those regenerate
`hw/ip/snitch/src/riscv_instr.sv` from upstream opcodes and would **delete the committed
`DMIDX` instruction**.
```sh
make sw/toolchain/riscv-opcodes || true      # clone riscv-opcodes (the version-pin
                                             #   checkout may error; the default branch it
                                             #   lands on is fine — proceed)
make sw/toolchain/riscv-opcodes/encoding.h   # generate encoding.h only
grep DMIDX hw/ip/snitch/src/riscv_instr.sv   # sanity: DMIDX must still be present
```

## 4. Build and configure DRAMSys (use the Makefile targets — do NOT `make dramsys` by hand)
```sh
cd hw/system/spatz_cluster
make dram-init      # builds libDRAMSys_Simulator.so with $(CXX)=gcc-11.2.0-af (from §0)
make dram-config    # installs the HBM2E-3600 configs into the DRAMSys config tree
                    #   (replaces the pristine slow default hbm2-example.json)
```

## 5. Build software and hardware
```sh
make -B generate            # clustergen: wrapper + bootdata_bootrom.cc etc. (-B forces a
                            #   complete generation; a plain `make generate` can leave gaps)
make sw                     # LLVM RISC-V build -> sw/build/... (data header already committed)
make bin/spatz_cluster.vsim # QuestaSim build of the cluster + DRAMSys DPI
```
Default config: `cfg/spatz_cluster.default.dram.hjson` (HBM2E-3600, 57.55 GB/s peak, 1 GHz,
512-bit AXI; `trans=64`, `dma_*_fifo_depth=64`). To sweep the DMA/AXI outstanding knobs,
edit the cfg then `make -B generate && make clean.vsim && make bin/spatz_cluster.vsim`.

## 6. Run the two settings (from `hw/system/spatz_cluster`)
```sh
# (1) core-issued
./bin/spatz_cluster.vsim sw/build/spatzBenchmarks/test-spatzBenchmarks-gather_R2048_D128_G128
./bin/spatz_cluster.vsim sw/build/spatzBenchmarks/test-spatzBenchmarks-gather-opt_R2048_D128_G128
# (2) hardware gather engine
./bin/spatz_cluster.vsim sw/build/spatzBenchmarks/test-spatzBenchmarks-gather-hw_R2048_D128_G128
```
Each prints `[DMA] <t0> ns issue id=1 ...`, `[DMADONE] <t1> ns id=1`, `The gather took <N>
cycles`, and `CORRECT!` (gathered rows match the DRAM matrix bit-for-bit).

## 7. Expected results (verified on a fresh clone; default cfg)

| setting | binary | `took` cycles | DMA window `[DMADONE]−[DMA]` | % peak |
|---|---|---|---|---|
| (1) core-issued, optimized | `gather-opt` | **1952** | — | — |
| (2) **HW gather engine** | `gather-hw` | **1236** | **932 ns** (9081→10013) | **~61%** |

Bandwidth (§ methodology): workload = 32768 B. **DRAM/transfer utilization** = `32768 /
(t1−t0[ns])` GB/s, `/57.55` for % peak — use the `[DMA]→[DMADONE]` window (excludes the
one-time config-offload + `wait_all` startup that inflates the raw cycle count at G=128).
The HW engine reaches ~61% — matching an ideal AXI master, i.e. near-optimal for this
scatter. Use the `took` cycles only to compare variants measured the same way.

## 8. Notes & gotchas (all hit during fresh-clone verification)
- **gcc-11.2.0-af for DRAMSys** (§0) — the single most important gotcha.
- **Don't run `make init`/`update_opcodes`** — regenerates `riscv_instr.sv` and drops
  `DMIDX`. Generate `encoding.h` only (§3).
- **`make -B generate`** (force) — a plain `make generate` can leave `bootdata_bootrom.cc`
  missing and the HW build fails with "No rule to make target 'test/bootdata_bootrom.cc'".
- **Use `make dram-init` / `make dram-config`**, not a hand-rolled `make dramsys` — the
  targets set the compiler and install the HBM2E-3600 configs.
- **RTL/param changes need `make clean.vsim`** before rebuilding (`vlog -incr` staleness).
- The verify loop is sim-time-heavy (~hundreds of µs); the gather itself is ~µs. Doesn't
  affect the reported gather cycles/window.
- Design rationale and the full investigation are in `NOTE.md`.
