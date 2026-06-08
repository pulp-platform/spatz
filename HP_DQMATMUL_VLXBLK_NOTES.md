# hp-dqmatmul: vlxblk benchmark extensions — build & data-gen notes

Notes for the VQ-GEMM (`vlxblk`) dequant-matmul benchmark extensions.
Benchmark dir: `sw/spatzBenchmarks/hp-dqmatmul/`

## What was added

Two new dequant configurations alongside the existing blk32 kernel:

| Case | Mnemonic       | CB_D | Index | Codebooks      | Entries (= 2^idx_bits) | Codebook footprint |
|------|----------------|------|-------|----------------|------------------------|--------------------|
| 0 (existing) | `vlxblk32ei8`  | 32 | EI8  | 2, additive    | 256 (config-dependent) | small              |
| 1 (case 1)   | `vlxblk4ei8`   | 4  | EI8  | 1, no addition | 256 (2^8)              | 256*4*2 = 2 KB     |
| 2 (case 2)   | `vlxblk8ei16`  | 8  | EI16 | 2, additive    | 65536 (2^16)           | 65536*8*2 = 1 MiB each (2 MiB total) |

Codebook size is intentionally aligned to the index width: 8-bit -> 2^8 entries,
16-bit -> 2^16 entries.

### Files
- `kernel/hp-dqmatmul-blk4.c` / `.h`        — case 1 kernels (`dq_matmul_4xVL_blk4`, `dq_matmul_4xVL_dp_blk4`)
- `kernel/hp-dqmatmul-blk8ei16.c` / `.h`     — case 2 kernels (`dq_matmul_4xVL_blk8ei16`, `dq_matmul_4xVL_dp_blk8ei16`)
- `main.c`                                   — `DQ_CASE` selector (0=blk32 default, 1=blk4, 2=blk8ei16) + `CB_IN_DRAM` knob
- `script/dqmatmul_blk4.json`                — case 1 data config (CB0_N=256)
- `script/dqmatmul_blk8ei16.json`            — case 2 data config (CB0_N=CB1_N=65536)
- `script/gen_data.py`                       — extended: u16 indices, single-codebook mode, `tag`, fast join-based emit
- `data/data_dq_blk4_128_128_128.h`          — generated (≈0.7 MB)
- `data/data_dq_blk8ei16_128_128_128.h`      — generated (≈16 MB)
- `sw/spatzBenchmarks/CMakeLists.txt`        — `add_spatz_test_dq_variant` macro + 2 targets + 2 libs

### CMake targets
- `test-spatzBenchmarks-hp-dqmatmul_blk4_M128_N128_K128`      — case 1
- `test-spatzBenchmarks-hp-dqmatmul_blk8ei16_M128_N128_K128`  — case 2
- `test-spatzBenchmarks-hp-dqmatmul_M128_N128_K128` / `_M32_N32_K32` — existing blk32

## Regenerating data headers

Run from `sw/spatzBenchmarks/hp-dqmatmul/`. Needs python3 with `torch`, `numpy`, `hjson`
(available via `/usr/local/anaconda3-2023.07/bin/python3`).

```bash
cd sw/spatzBenchmarks/hp-dqmatmul
python3 script/gen_data.py -c script/dqmatmul_blk4.json     -v
python3 script/gen_data.py -c script/dqmatmul_blk8ei16.json -v
```

Output file name is `data/data_dq_<tag>_<M>_<N>_<K>.h` (the `tag` field in the JSON
disambiguates configs that share M/N/K). To change sizes/entries, edit the JSON
(`CB0_N`, `CB0_D`, `CB0_IDX_WIDTH`, `num_codebooks`, `M/N/K`) and rerun.
Generation takes a few seconds even for the 2^16 case.

Config knobs in the JSON:
- `num_codebooks`: 1 (single, no addition) or 2 (additive)
- `CB0_IDX_WIDTH` / `CB1_IDX_WIDTH`: 1 (uint8 / EI8) or 2 (uint16 / EI16)
- `tag`: filename tag

## Building the software

`USE_CACHE` is a **compile-time** make/CMake variable (default 0), NOT a simulator
runtime arg. It selects the kernel + memory layout baked into the ELF:
- `USE_CACHE=0`: L1 = 120 KB SPM + 8 KB cache; calls `dq_matmul_4xVL_*` (plain 4xVL)
- `USE_CACHE=1`: L1 = 4 KB SPM + 124 KB cache; calls `dq_matmul_4xVL_dp_*` (aligned-load)

Build from `hw/system/spatz_cluster/` (the Makefile passes `-DUSE_CACHE` to CMake):

### Case 1 — blk4 (8-bit, single codebook, 256 entries, 2 KB)
Codebook fits in L1 SPM, so the normal SPM/cache split applies:
```bash
# no-cache baseline (codebook DMA'd into SPM)
make sw.vsim                                       # USE_CACHE=0 default
# cache version
make clean sw.vsim -B USE_CACHE=1 SNRT_LINK=dram
```

### Case 2 — blk8ei16 (16-bit, 2 codebooks, 65536 entries, 2 MiB total)
2 MiB codebooks do NOT fit the 128 KB L1, so they stay resident in DRAM
(`CB_IN_DRAM=1` in main.c). A / indices / C are still DMA'd to SPM.
**`SNRT_LINK=dram` is required** so the codebook symbols land in DRAM:
```bash
# no-cache baseline: codebooks in DRAM, gathered via 8 KB residual cache (~uncached)
make clean sw.vsim -B USE_CACHE=0 SNRT_LINK=dram
# cache version: 124 KB cache captures codebook reuse
make clean sw.vsim -B USE_CACHE=1 SNRT_LINK=dram
```
Note: the 16 MB data header makes compiling this one target slow (large static init).

Replace `sw.vsim` with `sw.vlt` (Verilator) or `sw.vcs` (VCS) as needed.

## Running

The run command is the same regardless of USE_CACHE (the choice is in the ELF):
```bash
./bin/spatz_cluster.vsim.gui sw/build/spatzBenchmarks/test-spatzBenchmarks-hp-dqmatmul_blk8ei16_M128_N128_K128
```
The binary path/name does NOT encode USE_CACHE, so rebuild (with `make clean ... -B`)
before running when switching cache modes, or you'll run a stale binary.

## RTL prerequisite (one-time)

The `vlxblk` family was only partially wired into the FPU sequencer's vector-load
list. Fixed in `hw/ip/spatz/src/spatz_fpu_sequencer.sv` (the `is_vector_load`
list at ~line 593) to include the full `blk{4,6,8,12,16}ei{8,16} + blk32ei8` set,
matching `spatz_decoder.sv`. Without it, `vlxblk4ei8`/`vlxblk8ei16` trigger the
`MemoryOperationCounterRollover` assertion (acc_mem_cnt underflow), because the
op completes in the VLSU but was never counted as issued.
**Recompile the RTL/simulator** after this fix.

(`vlxblk32ei16` is still missing from the decoder/snitch/sequencer lists — not used
by these two cases, but would need adding if ever used.)
