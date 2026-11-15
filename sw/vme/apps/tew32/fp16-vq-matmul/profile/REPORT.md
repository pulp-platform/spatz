# VQ MatMul: retained serial schedule

Workload: M96 N128 K128, FP16 decoded weights and FP32 output. The retained
`vq_gemm_MT6x2` computes the M64 decoded-weight producer, caches W, then calls
the M32 `MT2x2` consumer. The cached-weight handoff is serial.

| Schedule | RTL cycles | Golden |
|---|---:|---|
| VME MT6x2 current | 18,554 | PASS |
| O-POPE optimized | 23,089 | reference |

The 2026-11-14 RTL rerun of the current target measured **18,554 cycles**,
golden PASS. It still reported the two stream-arbiter assertions seen in the
earlier runs. The saved 19,075-cycle result is retained as a historical
measurement.

The tested last-N32 overlap schedules passed golden at 19,277–19,972 cycles,
so the current 18,554-cycle serial schedule remains the fastest measured
choice. The
older non-fused VME schedule took 30,708 cycles. The prior RTL transcript
also contained two stream-arbiter assertions; the golden result is therefore
assertion-qualified.

See [vq-matmul_summary.md](vq-matmul_summary.md) for the component process,
schedule comparison, and reproduction details.
