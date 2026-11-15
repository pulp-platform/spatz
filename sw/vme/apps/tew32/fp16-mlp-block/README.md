# FP16 MLP block (O-POPE P9)

`main.c`: sequential phases (LayerNorm, GeMM1, GELU-pack, GeMM2 with the residual seeded; `script/gen_mlp_block.py`, `../../script/tf/gen_ln.py`, `gen_gelu.py`). `main_fused.c`: one fused kernel, GeMM K-groups and tile transfers in register-free slots of the LayerNorm / GELU code (`script/gen_mlp_block_fused.py`).

Build and run (from `hw/system/spatz_cluster`; the checks after the measured region can need more than 60 s):

```
(cd sw/build && cmake . && make test-vme-fp16-mlp-block_C64_T64_H144 test-vme-fp16-mlp-block-fused_C64_T64_H144)
bin/spatz_cluster.vsim sw/build/vme/apps/test-vme-fp16-mlp-block_C64_T64_H144
bin/spatz_cluster.vsim sw/build/vme/apps/test-vme-fp16-mlp-block-fused_C64_T64_H144
```

Results against O-POPE: `runtime/opope_summary.md` (repository root).
