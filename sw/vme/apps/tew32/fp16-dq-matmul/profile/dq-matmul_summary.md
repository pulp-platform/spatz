# DQ-MatMul summary

## Workload and naming

The generated data and the retained kernel compute

```text
C[M, Nout] = A[M, Kred] x decode(Wmeta)[Kred, Nout]
M = 128, Kred = 64, Nout = 128
```

This is `M128 x N128 x K64` in the conventional GEMM naming where `N` is the
output-column dimension and `K` is the reduction dimension. The current build
target calls the same shape `M128_N64_K128`, following the O-POPE benchmark
convention `X[M,N] * W[N,K]`. Using the explicit names `Kred=64` and
`Nout=128` avoids that ambiguity.

The M64 component is correspondingly `M64, Kred64, Nout128`, conventionally
`M64 x N128 x K64`.

## Component process

### 1. Input preparation

- `A` contains packed FP16 activation pairs.
- Two metadata-index streams select vectors from `CB0` and `CB1`.
- Adding the two selected FP16 codebook vectors reconstructs one decoded
  weight group.
- FP32 output and an FP16 decoded-weight L1 workspace are allocated before the
  timed call. DMA and output initialization are outside the measured kernel.

### 2. First M64 panel: decode, compute, and `cacheW`

For each of the eight N16 output-column blocks:

1. Clear `mt0`, `mt4`, `mt8`, and `mt12` for four M16 row tiles.
2. Decode the first K8 weight group from the two codebooks.
3. Alternate decoded weights between two VRF banks.
4. While the current bank feeds `vtfmm.tvv`, resolve and load the successor
   group into the other bank.
5. Load the four activation panels and accumulate four M16xN16 tiles.
6. Store each decoded group into the L1 `cacheW` workspace. Address generation
   and codebook loads are placed between the cache store and the next matrix
   use to reduce exposed VRF/VLSU waiting.
7. On the final reduction group, interleave `vtse32` drains of completed tiles
   with computation of the remaining tiles.

This phase produces output rows 0--63 and a complete decoded-weight cache.

### 3. Second M64 panel: cached-weight consumer

Rows 64--127 reuse `cacheW`. The kernel no longer performs codebook lookup or
FP16 codebook addition for these rows. It pipelines cached weight loads,
activation loads, and four-tile matrix execution. A decoded K8 group is
consumed by all four accumulator tiles before its VRF registers are reused for
the successor group.

The combined `MT8x2` name represents the eight M16 row tiles across the two
M64 panels. The full result is produced by one assembly entry rather than by
adding independently timed component kernels.

### 4. FP32 output store

The final K8 group uses compute/store macros that place `vtse32` operations
between `vtfmm.tvv` operations. The last tile is then drained, and a dependent
load ensures the stores have completed before returning.

## End-to-end RTL results

| Workload | Implementation | RTL cycles | Reduction vs O-POPE | Speedup | Validation |
|---|---|---:|---:|---:|---|
| M64, Kred64, Nout128 | O-POPE | 9,709 | -- | 1.000x | reference |
| M64, Kred64, Nout128 | VME MT4x1 direct | 7,521 | 2,188 | 1.291x | PASS |
| M128, Kred64, Nout128 | O-POPE | 13,785 | -- | 1.000x | reference |
| M128, Kred64, Nout128 | VME MT8x2 historical | 13,087 | 698 | 1.053x | PASS; historical ELF |
| M128, Kred64, Nout128 | VME MT8x2 current | 13,015 | 770 | 1.059x | PASS; zero simulator errors |

The current reproducible full-workload comparison is **13,785 / 13,015 =
1.059x**. Address/copy cleanup increased the prior 12,928-cycle result by 87
cycles (0.67%), while preserving golden correctness.

## Component measurements

| Component | Cycles | Scope |
|---|---:|---|
| `MT4x1` direct | 7,521 | complete M64 direct-decode benchmark |
| `MT4x1_cacheW` | 7,887 | M64 producer that also materializes decoded weights |
| `MT2x2` | 5,314 | cached-weight consumer/component measurement |
| `MT8x2` | 13,087 historical / 13,015 current | complete M128 two-panel kernel |

The 7,887- and 5,314-cycle component measurements are diagnostic entry-point
results, not additive phases of the 13,015-cycle kernel. The integrated kernel
uses different boundaries and overlaps cache production/consumption support,
matrix issue, activation loads, and output stores.

## Shared VQ scheduling optimization

The retained DQ kernel keeps the dependency-aware scheduling used by the
optimized VQ kernel. `DQ_DECODE4_PRE_LAG` places independent index/address and
codebook-load work after each decoded-weight cache store. The final reduction
group interleaves `vtse32` output drains with independent `vtfmm.tvv`
operations. These schedules avoid exposing the decoded-bank producer latency
and reduce a separate output-drain tail.

For direct comparison, the retained component and full-kernel measurements
are:

| Kernel | Producer/cache component | Cycles | Consumer component | Cycles | Full kernel | Cycles |
|---|---|---:|---|---:|---|---:|
| VQ-MatMul | `MT4x1_cacheW` | 14,869 | `MT2x2` | 5,200 | `MT6x2` | 19,075 historical / 18,554 current |
| DQ-MatMul | `MT4x1_cacheW` | 7,887 | `MT2x2` | 5,314 | `MT8x2` | 13,015 current |

## Reproduction

```text
target: test-vme-fp16-dq-matmul-MT8x2_M128_N64_K128
actual dimensions: M=128, Kred=64, Nout=128
timed region: dq_matmul_MT8x2(...)
```

The current driver validates all `128 x 128` FP32 output elements after the
timed region.
