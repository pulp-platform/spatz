# FP16 FlashAttention for the 128 KiB TCDM (Tq64, T256, d64, dv64, Bc64)

The global-max FlashAttention kernel of `bobo/sw/vme/apps/tew32/fp16-flashattn` (commit bf7400a): all four
64x64 score blocks resident, one row maximum over the 256 keys, then exponentials + FP16 pack per block and
P*V with the four 32x32 output tiles resident in the OPE. The original allocates ~185 KiB of L1 and stops with
"L1 allocation failed" on the 128 KiB TCDM. Changes (macro `FLASH_V_L2=1`, on in this target):

| Buffer | Size | Live | Here |
|---|---:|---|---|
| Q | 8 KiB | QK | L1 |
| K | 32 KiB | QK | L1, then reused for P |
| scores | 64 KiB | QK .. exp/pack | L1 |
| P | 32 KiB | exp/pack .. PV | in the K buffer |
| V | 32 KiB | PV | in L2 (DRAM); the kernel DMAs it into score blocks 0-1 after exp/pack of block 1, overlapped with blocks 2-3 |
| output | 16 KiB | PV | in score block 2 |

L1 total ~104.5 KiB. One correctness fix on top: a scalar load after the priming loads of the
exponentials (`kernel/flashattn_tq64.c`). Without it the first score vector of block 0 is exponentiated
wrongly in 2 of 8 code placements (padding sweep 0..28 B in front of the kernel), which is why adding or
moving code before the kernel made the original fail (2853 mismatches). The original kernel has the same issue.

Build and run (from `hw/system/spatz_cluster`):

```
(cd sw/build && cmake . && make test-vme-fp16-flashattn-l2_TQ64_T256_D64_DV64)
bin/spatz_cluster.vsim sw/build/vme/apps/test-vme-fp16-flashattn-l2_TQ64_T256_D64_DV64
```

`FA_PROBE=1` (measurement only, link `spatzBenchmarks/benchmark/benchmark.c`) puts the cluster probe around
the kernel. Results against O-POPE: `runtime/opope_summary.md`.
