# FP32 MatMul with vector stores

This microbenchmark studies FP32 matrix multiplication whose tile results are
moved to vector registers and written with vector stores. It covers TE16 and
TE32 configurations and selects one or two operand buffers according to TCDM
capacity.

Targets follow `test-vme-fp32-matmul-vse-<te>-<variant>_M<M>_N<N>_K<K>`.
