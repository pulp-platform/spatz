# FP16 causal attention head (O-POPE P8: D=64, T=96)

Score GeMM (causal blocks skipped, diagonal blocks seeded with the mask), causal softmax-pack (O-POPE op sequence), PV GeMM with causal K truncation. Kernels and data: `script/gen_attention.py`.

Build and run (from `hw/system/spatz_cluster`; the checks after the measured region can need more than 60 s):

```
(cd sw/build && cmake . && make test-vme-fp16-attention_D64_T96)
bin/spatz_cluster.vsim sw/build/vme/apps/test-vme-fp16-attention_D64_T96
```

Results against O-POPE: `runtime/opope_summary.md` (repository root).
