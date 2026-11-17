# FP16 MatMul with vector stores

This microbenchmark studies FP16-to-FP32 matrix multiplication whose tile
results are moved to vector registers and written with vector stores. TE16 and
TE32 configurations expose rolled scheduling variants and generated datasets.

Targets follow `test-vme-fp16-matmul-vse-<te>-<variant>_M<M>_N<N>_K<K>`.
