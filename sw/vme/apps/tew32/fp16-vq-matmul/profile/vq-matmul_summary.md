# VQ-MatMul summary

## Workload and operation

The retained end-to-end workload computes

```text
C[M, Nout] = A[M, Kred] x decode(Wmeta)[Kred, Nout]
M = 96, Kred = 128, Nout = 128
```

`A` and the decoded weights are FP16; the matrix tiles accumulate and store
FP32 output. Each weight vector is reconstructed from two residual-VQ
codebooks. An index selects one vector from each codebook and the two FP16
vectors are added to form the weight consumed by `vtfmm.tvv`.

The implementation is `vq_gemm_MT6x2`. It divides the 96 output rows into an
M64 producer and an M32 consumer. The producer decodes each weight group once,
uses it for its own matrix multiplication, and writes the decoded group to the
L1 `cacheW` workspace. The consumer then reuses `cacheW`; it does not repeat
the codebook lookup and addition.

## Component process

### 1. Input preparation

- `A` is stored as packed FP16 pairs for matrix-tile consumption.
- `Wmeta` contains the two residual-VQ indices for every decoded weight group.
- `CB0` and `CB1` contain the two FP16 codebooks.
- The output and the decoded-weight L1 workspace are allocated before the
  timed kernel call. DMA and output initialization are outside the cycle count.

### 2. M64 producer: `MT4x1_cacheW`

For each N16 output-column block:

1. Read four packed metadata words.
2. Resolve the corresponding `CB0` and `CB1` addresses.
3. Load both codebook vectors and add them in FP16.
4. Keep two decoded-weight banks in the VRF so decoding can alternate with
   matrix execution.
5. Load four M16 activation panels into vector-register groups.
6. Accumulate the four M16xN16 output tiles in `mt0`, `mt4`, `mt8`, and
   `mt12` using `vtfmm.tvv`.
7. Store the decoded weight to `cacheW` while useful decode/address work and
   matrix operations are in flight.
8. Interleave final `vtse32` output drains with the last matrix operations.

This phase covers rows 0--63 and produces the decoded-weight cache used by the
remaining rows.

### 3. M32 consumer: `MT2x2`

The consumer covers rows 64--95. It loads decoded FP16 weights directly from
`cacheW`, loads two M16 activation panels, and accumulates two M16xN16 tiles.
The validated serial schedule calls the consumer only after the relevant
producer data is available. Tested producer/consumer overlap variants were
slower than this schedule.

### 4. FP32 output store

Each completed matrix tile is drained with `vtse32` to its row-major output
position. The stores are scheduled around the final `vtfmm.tvv` operations so
tile draining does not become a fully separate phase.

## End-to-end RTL results

| Workload | Implementation | RTL cycles | Reduction vs O-POPE | Speedup | Validation |
|---|---|---:|---:|---:|---|
| M64, Kred128, Nout128 | O-POPE | 19,000 | -- | 1.000x | reference |
| M64, Kred128, Nout128 | VME historical best | 14,483 | 4,517 | 1.312x | historical result |
| M96, Kred128, Nout128 | O-POPE | 23,089 | -- | 1.000x | reference |
| M96, Kred128, Nout128 | VME MT6x2 historical | 19,075 | 4,014 | 1.210x | PASS; historical ELF |
| M96, Kred128, Nout128 | VME MT6x2 optimized | 18,554 | 4,535 | 1.244x | golden PASS; two pre-existing stream-arbiter assertions |

The current reproducible M96 comparison is therefore **23,089 / 18,554 =
1.244x**. This supersedes the historical 19,075-cycle result. Relative to the
19,278-cycle baseline, dependency-aware scheduling and address/copy cleanup
remove 724 cycles (3.76%). The latest cleanup removes 181 cycles (0.97%) from
the previous 18,735-cycle result.

The decisive change delays the LMUL4 decoded-weight `vse32` read with three
independent activation address/load instructions. This prevents the store
from requesting the whole register group while its last `vfadd` result is
still pending, directly shortening `vrf_re && !vfu_rvalid` intervals. Final
matrix operations also retire one completed tile row between each `vtfmm`,
using `vtse32` plus its address and selector updates as a three-instruction
gap instead of leaving a serial drain tail.

## Component measurements

| Component | Cycles | Scope |
|---|---:|---|
| `MT4x1_cacheW` | 14,869 | M64 producer/cache benchmark |
| `MT2x2` | 5,200 | M32 consumer benchmark |
| `MT6x2` serial | **18,554 current best** | complete M96 kernel, golden PASS |

The component numbers must not be added to reconstruct the full-kernel cycle
count. They were measured with different entry points and timer boundaries,
and the full kernel interleaves decode, cache traffic, matrix work, and tile
stores.

## VQ- and DQ-MatMul comparison

Both retained kernels use the same scheduling principle: decoded-weight cache
stores are separated from their producer `vfadd` by independent address/load
work, and final `vtse32` tile drains are interleaved with independent
`vtfmm.tvv` operations. DQ-MatMul keeps that optimization in
`dq_matmul_MT8x2`.

| Kernel | Workload | Implementation | O-POPE cycles | VME cycles | Cycle reduction | Speedup |
|---|---|---|---:|---:|---:|---:|
| VQ-MatMul | M64 x N128 x K128 | `MT4x1` | 19,000 | 14,483 | 4,517 | 1.312x |
| VQ-MatMul | M96 x N128 x K128 | `MT6x2` historical | 23,089 | 19,075 | 4,014 | 1.210x |
| VQ-MatMul | M96 x N128 x K128 | `MT6x2` optimized | 23,089 | 18,554 | 4,535 | 1.244x |
| DQ-MatMul | M64 x N128 x K64 | `MT4x1` | 9,709 | 7,521 | 2,188 | 1.291x |
| DQ-MatMul | M128 x N128 x K64 | `MT8x2` | 13,785 | 13,015 | 770 | 1.059x |

The component measurements below are useful for locating costs, but are not
additive because their standalone entry points use different timer boundaries
from the integrated kernels.

| Kernel | Producer/cache component | Cycles | Consumer component | Cycles | Full kernel | Cycles |
|---|---|---:|---|---:|---|---:|
| VQ-MatMul | `MT4x1_cacheW` | 14,869 | `MT2x2` | 5,200 | `MT6x2` | 19,075 historical / 18,554 current |
| DQ-MatMul | `MT4x1_cacheW` | 7,887 | `MT2x2` | 5,314 | `MT8x2` | 13,015 current |

## Reproduction

```text
target: test-vme-fp16-vq-matmul-MT6x2-serial_M96_N128_K128
timed region: vq_gemm_MT6x2(...)
```

The current driver checks all FP32 output elements against the generated
golden result after the timed region.
