# FP16 embedding + LayerNorm + GeMM (O-POPE P10)

`main.c`: sequential phases (one-hot embedding GeMM, LayerNorm, projection). `main_fused.c`: one-hot built inside the embedding GeMM, LayerNorm in the same kernel absorbing the last drain, projection with contiguous operand loads (`script/gen_embed_ln_gemm_fused.py`).

Build and run (from `hw/system/spatz_cluster`; the checks after the measured region can need more than 60 s):

```
(cd sw/build && cmake . && make test-vme-fp16-embed-ln-gemm_C64_T64_V256_O64 test-vme-fp16-embed-ln-gemm-fused_C64_T64_V256_O64)
bin/spatz_cluster.vsim sw/build/vme/apps/test-vme-fp16-embed-ln-gemm_C64_T64_V256_O64
bin/spatz_cluster.vsim sw/build/vme/apps/test-vme-fp16-embed-ln-gemm-fused_C64_T64_V256_O64
```

Results against O-POPE: `runtime/opope_summary.md` (repository root).
