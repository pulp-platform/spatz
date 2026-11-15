# FP16 LayerNorm -> GeMM (O-POPE P1: C=96, T=64, COUT=176)

Software-pipelined LayerNorm (`../../script/tf/gen_ln.py`, output token-half-major) then the 1x2c staggered GeMM with the bias as one extra K-pair. Kernels and data: `script/gen_ln_gemm.py`.

Build and run (from `hw/system/spatz_cluster`; the checks after the measured region can need more than 60 s):

```
(cd sw/build && cmake . && make test-vme-fp16-ln-gemm_C96_T64_O176)
bin/spatz_cluster.vsim sw/build/vme/apps/test-vme-fp16-ln-gemm_C96_T64_O176
```

Results against O-POPE: `runtime/opope_summary.md` (repository root).
