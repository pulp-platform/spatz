#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator: valid conv2d + bias + ReLU, FP16 input/weights, FP32 output (O-POPE SDOTP GeMM).
# The data are laid out so that the on-device pre-processing is just unit-stride word copies:
#   input   [Cin/2][H*W] words : word i of plane pair cp = { in[2cp][i] (lo), in[2cp+1][i] (hi) }
#   weights [N/2][Cout]  words : row p=(cp*KH+ky)*KW+kx, word co = { w[co][2cp][ky][kx], w[co][2cp+1][ky][kx] }
#                                (the O-POPE W operand: two FP16 per word along N = Cin*KH*KW)
#   bias    [Cout] FP32 (initialises Y)
#   golden  [Ho*Wo][Cout] FP32, spatial-major, only the valid positions
# The on-device im2col builds xT [N/2][M] (the packed X^T operand) as shifted copies of the plane
# pairs: xT[p][m] = plane_pair[cp][m + ky*W + kx].

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "common"))
import opope_data as od  # noqa: E402


def main():
    tag, p = od.parse_cfg()
    Cin, H, W, Cout, KH, KW = (p[k] for k in ("Cin", "H", "W", "Cout", "KH", "KW"))
    Ho, Wo = H - KH + 1, W - KW + 1
    M, N, K = Ho * W, Cin * KH * KW, Cout
    assert W % 16 == 0 and M % 16 == 0 and min(M, N, K) >= 16
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

    # channel-pair planes: [Cin/2][H*W] words
    xb = x.view(np.uint16).reshape(Cin, H * W).astype(np.uint32)
    xin = xb[0::2] | (xb[1::2] << 16)
    # W operand rows p=(cp*KH+ky)*KW+kx, columns co
    wb = w.view(np.uint16).astype(np.uint32)  # [Cout][Cin][KH][KW]
    wpair = wb[:, 0::2] | (wb[:, 1::2] << 16)  # [Cout][Cin/2][KH][KW]
    wmat = wpair.transpose(1, 2, 3, 0).reshape(N // 2, Cout)

    # L1: plane pairs (+4 words: the shifted copies read slightly past the last plane), xT [N/2][M],
    # W [N/2][K], Z/Y [M][K] FP32, bias [K]
    od.check_footprint({"in": 4 * (Cin // 2 * H * W + 4), "xT": 4 * (N // 2) * M,
                        "w": 4 * (N // 2) * K, "z": 4 * M * K, "bias": 4 * K})

    defines = {"CIN": Cin, "H_SIZE": H, "W_SIZE": W, "COUT": Cout, "KH_SIZE": KH, "KW_SIZE": KW,
               "HO_SIZE": Ho, "WO_SIZE": Wo, "M_SIZE": M, "N_SIZE": N, "K_SIZE": K}
    arrays = [od.carray("cv_in_dram", xin.flatten()), od.carray("cv_w_dram", wmat.flatten()),
              od.carray("cv_bias_dram", od.words(b)), od.carray("cv_golden", od.words(out))]
    od.write_header(pathlib.Path(__file__).resolve().parents[1], tag, defines, arrays)


if __name__ == "__main__":
    main()
