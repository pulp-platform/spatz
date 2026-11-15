# FP16 GeMM -> GELU (O-POPE P2: C=128, T=64, COUT=192)

One fused kernel: the GELU (O-POPE TF16_GELU2 with one bit-exact rewrite) runs over z with the GeMM K-groups in its register windows and the tile drains in 2-row chunks. Generator: `../../script/tf/gemm_gelu.py` (wrapper `script/gen_gemm_gelu.py`).

Build and run (from `hw/system/spatz_cluster`; the checks after the measured region can need more than 60 s):

```
(cd sw/build && cmake . && make test-vme-fp16-gemm-gelu_C128_T64_O192)
bin/spatz_cluster.vsim sw/build/vme/apps/test-vme-fp16-gemm-gelu_C128_T64_O192
```

Results against O-POPE: `runtime/opope_summary.md` (repository root).
