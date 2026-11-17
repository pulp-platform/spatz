#!/usr/bin/env python3
"""Current Cin32 H16 W16 Cout64 3x3 O-POPE conv workload."""
import argparse
import pathlib
import numpy as np

p = argparse.ArgumentParser()
p.add_argument("--output", required=True)
a = p.parse_args()

ci, h, w, co, kh, kw = 32, 16, 16, 64, 3, 3
ho, wo, m = h - kh + 1, w - kw + 1, h * w
rng = np.random.default_rng(1337)
x = rng.uniform(-1, 1, (ci, h, w)).astype(np.float16)
weight = rng.uniform(-.5, .5, (co, ci, kh, kw)).astype(np.float16)
bias = rng.uniform(-.5, .5, co).astype(np.float32)
out = np.zeros((ho, wo, co), dtype=np.float64)
for ky in range(kh):
    for kx in range(kw):
        out += np.einsum("chw,oc->hwo",
                         x[:, ky:ky + ho, kx:kx + wo].astype(np.float64),
                         weight[:, :, ky, kx].astype(np.float64))
out = np.maximum(out + bias.astype(np.float64), 0).astype(np.float32)

# Pair-major operands used in place by each accumulating GeMM.  Keep the
# baseline halfword order; both operands use it, so each dot product matches.
xb = x.view(np.uint16).reshape(ci, h * w).astype(np.uint32)
xin = xb[0::2] | (xb[1::2] << 16)
pad = 64
xin = np.concatenate((xin.ravel(), np.zeros(pad, dtype=np.uint32)))
wb = weight.view(np.uint16).astype(np.uint32)
wpair = wb[:, 0::2] | (wb[:, 1::2] << 16)
wmat = wpair.transpose(2, 3, 1, 0).reshape(kh * kw * (ci // 2), co)
zinit = np.repeat(bias[None, :], m, axis=0)

in_bytes, weight_bytes, z_bytes = xin.nbytes, wmat.nbytes, zinit.nbytes
assert (in_bytes, weight_bytes, z_bytes) == (16640, 36864, 65536)
buffer_bytes = in_bytes + weight_bytes + z_bytes
assert buffer_bytes <= 120 * 1024

defines = dict(CIN=ci, H_SIZE=h, W_SIZE=w, COUT=co, KH_SIZE=kh,
               KW_SIZE=kw, HO_SIZE=ho, WO_SIZE=wo, M_SIZE=m,
               N_SIZE=ci, K_SIZE=co, IN_PAD=pad,
               BUFFER_BYTES=buffer_bytes)
lines = ["#pragma once", "#include <stdint.h>"]
lines += [f"#define {key} {value}" for key, value in defines.items()]
for name, arr in (("cv_in_dram", xin), ("cv_w_dram", wmat),
                  ("cv_zinit_dram", zinit.view(np.uint32)),
                  ("cv_golden", out.view(np.uint32))):
    body = ",".join(f"0x{int(v):08x}u" for v in arr.ravel())
    lines.append(f"static const uint32_t {name}[] __attribute__((aligned(256))) = {{{body}}};")
pathlib.Path(a.output).write_text("\n".join(lines) + "\n")
print(f"Cin{ci} H{h} W{w} Cout{co}: {buffer_bytes} bytes")
