# DQ MatMul: retained cached-weight schedule

Workload: M128, Kred64, Nout128, with FP16 decoded weights and FP32 output.
The retained `dq_matmul_MT8x2` computes the first M64 panel while decoding and
caching W, then computes the second M64 panel from `cacheW`. The cached-weight
handoff is serial within one assembly entry.

| Schedule | RTL cycles | Golden |
|---|---:|---|
| VME MT8x2 current | 13,015 | PASS |
| O-POPE optimized | 13,785 | reference |

The 2026-11-14 RTL rerun of the current target measured **13,015 cycles**,
golden PASS, with zero simulator errors and warnings. The saved 13,087-cycle
result is retained as a historical measurement; an intermediate ELF measured
13,012 cycles.

The retained kernel removes the separate non-fused and older schedule
experiments. Its first M64 producer decodes and caches weights, and its second
M64 consumer reuses those weights without another decode. The current result
is 770 cycles faster than optimized O-POPE, corresponding to a 1.059x speedup.

See [dq-matmul_summary.md](dq-matmul_summary.md) for the component process,
schedule comparison, and reproduction details.
