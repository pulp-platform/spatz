# FP32 MatMul with tile stores

This microbenchmark studies FP32 matrix multiplication using tile-store output
paths. It covers TE16 and TE32 configurations with generated shape-specific
datasets.

Targets follow `test-vme-fp32-matmul-vtse-<te>-<variant>_M<M>_N<N>_K<K>`.
