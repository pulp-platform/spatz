#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator: valid conv2d + bias + ReLU, FP16 input/weights, FP32 output (O-POPE SDOTP GeMM).
# No im2col: the convolution is KH*KW accumulating GeMMs (one per filter tap) whose X^T operand is the input itself, read
# at a shifted word address. The input is stored as channel-pair planes of H*W words, and the GeMM has M = H*W output
# positions, so the pitch of the operand rows (M words) equals the pitch of the planes: for the tap (ky,kx) the operand is
# in + ky*W + kx (word pointer) with N = Cin and the O-POPE reads xT[cp][m] = plane_pair[cp][m + ky*W + kx] with no copy.
#   input   [Cin/2][H*W + pad] words : word i of plane pair cp = { in[2cp][i] (lo), in[2cp+1][i] (hi) }
#   weights [KH*KW][Cin/2][Cout] words : word (tap, cp, co) = { w[co][2cp][ky][kx], w[co][2cp+1][ky][kx] } (tap = ky*KW + kx)
#   bias    [Cout] FP32 (initialises Y, which every tap job accumulates into in place)
#   golden  [Ho*Wo][Cout] FP32, spatial-major (row pitch W*Cout in Z), only the valid positions
# The positions with y >= Ho or x >= Wo are computed but unused.

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "common"))
import opope_data as od  # noqa: E402


def main():
    tag, p = od.parse_cfg()
    Cin, H, W, Cout, KH, KW = (p[k] for k in ("Cin", "H", "W", "Cout", "KH", "KW"))
    Ho, Wo = H - KH + 1, W - KW + 1
    M, N, K = H * W, Cin, Cout  # one job per tap: M = H*W positions, N = Cin
    assert M % 16 == 0 and min(M, N, K) >= 16 and (KH - 1) * W + KW - 1 <= 64
    assert Cin % 2 == 0 and K % 2 == 0

    rng = np.random.default_rng(1337)
    x = rng.uniform(-1, 1, (Cin, H, W)).astype(np.float16)
    w = rng.uniform(-0.5, 0.5, (Cout, Cin, KH, KW)).astype(np.float16)
    b = rng.uniform(-0.5, 0.5, Cout).astype(np.float32)

    out = np.zeros((Ho, Wo, Cout), dtype=np.float64)
    for ky in range(KH):
        for kx in range(KW):
            patch = x[:, ky:ky + Ho, kx:kx + Wo].astype(np.float64)  # Cin,Ho,Wo
            out += np.einsum("chw,oc->hwo", patch, w[:, :, ky, kx].astype(np.float64))
    out = np.maximum(out + b.astype(np.float64), 0).astype(np.float32).reshape(Ho * Wo, Cout)

    # channel-pair planes: [Cin/2][H*W] words (+ pad: the shifted operands read up to (KH-1)*W + KW-1 words past the last plane)
    PAD = 64
    xb = x.view(np.uint16).reshape(Cin, H * W).astype(np.uint32)
    xin = np.zeros((Cin // 2, H * W), dtype=np.uint32)
    xin[:] = xb[0::2] | (xb[1::2] << 16)
    xin = np.concatenate([xin.flatten(), np.zeros(PAD, dtype=np.uint32)])
    # W operand of every tap: [tap][Cin/2][Cout] words
    wb = w.view(np.uint16).astype(np.uint32)  # [Cout][Cin][KH][KW]
    wpair = wb[:, 0::2] | (wb[:, 1::2] << 16)  # [Cout][Cin/2][KH][KW]
    wmat = wpair.transpose(2, 3, 1, 0).reshape(KH * KW * (Cin // 2), Cout)

    od.check_footprint({"in": 4 * (Cin // 2 * H * W + PAD), "w": 4 * KH * KW * (Cin // 2) * K, "z": 4 * M * K})

    defines = {"CIN": Cin, "H_SIZE": H, "W_SIZE": W, "COUT": Cout, "KH_SIZE": KH, "KW_SIZE": KW,
               "HO_SIZE": Ho, "WO_SIZE": Wo, "M_SIZE": M, "N_SIZE": N, "K_SIZE": K, "IN_PAD": PAD}
    arrays = [od.carray("cv_in_dram", xin.flatten()), od.carray("cv_w_dram", wmat.flatten()),
              od.carray("cv_zinit_dram", od.words(np.repeat(b[None, :], M, axis=0))),  # Y = bias per column [M][K]
               od.carray("cv_golden", od.words(out))]
    od.write_header(pathlib.Path(__file__).resolve().parents[1], tag, defines, arrays)


if __name__ == "__main__":
    main()
