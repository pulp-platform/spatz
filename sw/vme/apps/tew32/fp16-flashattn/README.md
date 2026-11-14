# FP16 FlashAttention

This application provides fixed-shape FP16 FlashAttention kernels for the
Spatz TE16 VME configuration. Q, K, V and packed probabilities are FP16;
scores, softmax metadata and outputs are FP32.

## Workloads

| Workload | Target | Recorded RTL cycles |
|---|---|---:|
| `Tq16,T64,d64,dv16` | `test-vme-fp16-flashattn-vrf_TQ16_T64_D64_DV16` | 6,036 |
| `Tq64,T64,d64,dv64,Bc64` | `test-vme-fp16-flashattn-key64-single_TQ64_T64_D64_DV64` | 20,177 |
| `Tq64,T256,d64,dv64,Bc64` | `test-vme-fp16-flashattn-global-max4_TQ64_T256_D64_DV64` | 73,859 |

The TQ16 kernel is a small direct-VRF schedule. The TQ64 kernel uses 64-key
blocks; for T256 it makes all four score blocks resident, computes one global
maximum per query row, and accumulates four `P*V` contributions while each
output tile remains resident in OPE.

## Source layout

| File | Purpose |
|---|---|
| `kernel/flashattn.h` | Public APIs, buffer types and private math macros |
| `kernel/flashattn_tq16.c` | TQ16/T64/D64/DV16 kernel |
| `kernel/flashattn_tq64.c` | TQ64 blocked/global-max kernel |
| `kernel/qk_matmul_64x64x64.c` | Self-contained VME QK matmul used by TQ64 |
| `main_tq16.c`, `main_tq64.c` | L1 allocation, DMA, cycle timing and golden checks |
| `script/gen_data_tq16.py`, `script/gen_data_tq64.py` | Deterministic test-data generators |

Kernel files contain computation only. DMA transfers, benchmark timing and
golden-result checks remain in the corresponding `main_*.c` driver.

## Build and run

From the repository root, load the project environment and use the configured
Spatz software build directory:

```sh
source util/iis-env.sh
cmake --build hw/system/spatz_cluster/sw/build --target \
  test-vme-fp16-flashattn-global-max4_TQ64_T256_D64_DV64 -j 4
```

The build automatically regenerates its data header when its generator
changes. Run the RTL golden test with:

```sh
cmake --build hw/system/spatz_cluster/sw/build --target \
  run-rtl-test-vme-fp16-flashattn-global-max4_TQ64_T256_D64_DV64 -j 4
```

Replace the target suffix with either target from the workload table to build
or run another shape. A successful run prints its cycle count followed by
`PASS`; a mismatch prints `FAIL` and the number of differing outputs.

Production targets compile with stage instrumentation disabled. Define
`BENCHMARK=1` for a TQ64 target when per-stage QK, softmax, PV and normalization
counters are needed; these counters perturb the production cycle count.

## Results and algorithm notes

The direct TQ16 target passed RTL golden checking at 6,036 cycles on
2026-11-14; its earlier ELF measured 6,055 cycles. The exponential remains the
degree-5 Taylor approximation with six squarings used by the matched O-POPE
benchmark.

For T256, the matched optimized O-POPE result is 85,869 cycles; VME's current
73,859-cycle result is 12,010 cycles faster.
