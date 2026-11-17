# FP16 MatMul with tile stores

This microbenchmark studies FP16-to-FP32 matrix multiplication using tile-store
output paths. TE16 and TE32 configurations provide rolled and explicitly
unrolled scheduling variants where their source trees are available.

Targets follow `test-vme-fp16-matmul-vtse-<te>-<variant>_M<M>_N<N>_K<K>`.
