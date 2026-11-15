# FP16 GeMM + column softmax / + cross-entropy (O-POPE P6, P7)

GeMM (`../../script/tf/gen_kernel.py --post smax`: bias K-pair, the tail drain overlaps the first rows of the column max), then the softmax of `../../script/tf/gen_softmax.py` (`--r0 156`; P7: `--ce 1`): O-POPE's op sequence, software pipelined. Kept separate from `../fp16-gemm-softmax` (a different 64x64 app).

Build and run (from `hw/system/spatz_cluster`; the checks after the measured region can need more than 60 s):

```
(cd sw/build && cmake . && make test-vme-fp16-gemm-softmax_C128_T64_V192 test-vme-fp16-gemm-softmax-ce_C128_T64_V192)
bin/spatz_cluster.vsim sw/build/vme/apps/test-vme-fp16-gemm-softmax_C128_T64_V192
bin/spatz_cluster.vsim sw/build/vme/apps/test-vme-fp16-gemm-softmax-ce_C128_T64_V192
```

Results against O-POPE: `runtime/opope_summary.md` (repository root).
