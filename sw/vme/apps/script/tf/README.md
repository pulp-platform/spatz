# Shared generators of the transformer / gather-MLP VME kernels

Used by the kernel directories in `../../tew32/` (their `script/gen_*.py` import or call these):

| File | Purpose |
|---|---|
| `vmegen.py` | generic staggered (ping-pong) GeMM generator: 2x1 / 1x2h / 1x2c blocks, drains of the idle tile pair overlapped with the accumulation, seeds, bias K-pair, slot hooks |
| `fusegen.py` | fusion helpers: GELU op sequence (O-POPE TF16_GELU2 with one bit-exact rewrite), OPE work queue with slots inside VFU code, 2x2 blocks, batched OPE-only emission |
| `gen_ln.py` | software-pipelined LayerNorm + FP16 pack (O-POPE arithmetic) |
| `gen_gelu.py` | GELU + FP16 pack (O-POPE TF16_GELU2), registers renamed per row pair |
| `gen_softmax.py` | column softmax (+ cross-entropy) with O-POPE's op sequence, software pipelined |
| `gen_kernel.py` | P5-shape GeMM (bias K-pair, fused ReLU / softmax-max tail) |
| `gen_data.py` | data headers of the P5-shape kernels (gemm-relu, gemm-softmax, gemm-softmax-ce) |
| `gemm_gelu.py` | fused GeMM + GELU (p2) and LayerNorm + GeMM + GELU (p3, `--mode p3`) |
| `gathermlp.py` | gather-MLP (DLRM / GCN): gather, then GeMM with the ReLU fused into the tile drain |
