# FP16 GEMM unit

This application benchmarks FP16-to-FP32 GEMM at M64 x N64 with K128, K256,
and K512. The kernel, generated data, generator, and host entry point are kept
in one application directory; there is no rolled/unrolled distinction.

The default target is `test-vme-fp16-gemm-unit_M64_N64_K<K>`. K128 also
provides `staggered` and `lockstep` scheduling variants.
