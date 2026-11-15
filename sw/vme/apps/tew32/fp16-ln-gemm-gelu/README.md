# FP16 LayerNorm -> GeMM -> GELU (O-POPE P3: C=96, T=64, COUT=176)

One fused kernel: the GELU (O-POPE TF16_GELU2 with one bit-exact rewrite) runs over z with the GeMM K-groups in its register windows and the tile drains in 2-row chunks after the software-pipelined LayerNorm. Generator: `../../script/tf/gemm_gelu.py` (wrapper `script/gen_ln_gemm_gelu.py`).

Build and run (from `hw/system/spatz_cluster`; the checks after the measured region can need more than 60 s):

```
(cd sw/build && cmake . && make test-vme-fp16-ln-gemm-gelu_C96_T64_O176)
bin/spatz_cluster.vsim sw/build/vme/apps/test-vme-fp16-ln-gemm-gelu_C96_T64_O176
```

Results against O-POPE: `runtime/opope_summary.md` (repository root).
