#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator for embedding-bag -> MLP (DLRM) and GCN-layer pipelines, FP16 inputs / FP32 output.
#   table  [R][D] FP16, natural feature order: a pooled row (64 halfwords) is 32 words {f[2p], f[2p+1]}, i.e. already the
#          packed pairs of the W operand, written with ONE strided 32-bit store per bag
#   idx    [B][L/4]  u32 words, four 8-bit row indices each (R <= 256): 2 scalar loads per bag of 8 lookups (a scalar load
#          costs about 5 cycles and cannot overlap vector memory accesses); the byte offset is index << 7 (D * 2 = 128)
#   The pooled operand [D/2][BP] and Z [Dout][BP] have an odd pitch BP words (B is odd: 127 bags; otherwise BP = B + 1 with a
#   dummy bag): an odd pitch makes the strided store conflict-free in the TCDM banks (a pitch multiple of 16 words hits
#   one bank: 3x slower). K = 127 costs the same GeMM time as K = 128.
#   w1     [D/2][Dout] words {W1[2p][o], W1[2p+1][o]}  (packed X^T operand of the MLP GeMM, prepared offline)
#   bias   [Dout] FP32 (initialises Y)
#   golden Z[Dout][B] FP32, feature-major, after ReLU
# The golden emulates the FP16 sequential sum-pooling (and the FP16 mean scale of the GCN) exactly.

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "common"))
import opope_data as od  # noqa: E402


def main():
    tag, p = od.parse_cfg()
    mode, R, D, B, L, Dout = (p[k] for k in ("mode", "R", "D", "B", "L", "Dout"))
    assert mode in ("dlrm", "gcn")
    assert D == 64, "the device code holds a pooled row (64 halfwords) in one LMUL=2 e16 group (VLEN=512)"
    assert Dout % 16 == 0 and B >= 16

    rng = np.random.default_rng(1337)
    table = rng.uniform(-1, 1, (R, D)).astype(np.float16)
    if mode == "gcn":
        assert R == B
        idx = np.zeros((B, L), dtype=np.int64)
        idx[:, 0] = np.arange(B)  # self loop
        for v in range(B):
            idx[v, 1:] = rng.choice(R, L - 1, replace=False)  # neighbours
    else:
        idx = rng.integers(0, R, (B, L))
    w1 = rng.uniform(-0.5, 0.5, (D, Dout)).astype(np.float16)
    bias = rng.uniform(-0.5, 0.5, Dout).astype(np.float32)

    # FP16 sequential sum-pooling (same order as the device), then the FP16 mean scale (GCN)
    pooled = np.zeros((B, D), dtype=np.float16)
    for l in range(L):
        pooled = (pooled + table[idx[:, l]]).astype(np.float16)
    if mode == "gcn":
        pooled = (pooled * np.float16(1.0 / L)).astype(np.float16)
    z = np.maximum(w1.astype(np.float64).T @ pooled.astype(np.float64).T + bias[:, None], 0)
    z = z.astype(np.float32)

    BP = B if B % 2 else B + 1  # odd pitch (see above); an odd number of bags needs no dummy column
    assert R <= 256 and L % 4 == 0 and D * 2 == 128
    ii = idx.astype(np.uint32)
    idxw = (ii[:, 0::4] | (ii[:, 1::4] << 8) | (ii[:, 2::4] << 16) | (ii[:, 3::4] << 24)).astype(np.uint32)  # [B][L/4]
    # packed X^T operand: [D/2][Dout] words, lo = W1[2p][o], hi = W1[2p+1][o]
    w1p = od.pack_pairs(od.halves(w1).reshape(D, Dout))

    od.check_footprint({"table": 2 * R * D, "idx": 4 * B * (L // 4), "pool": 4 * (D // 2) * BP,
                        "w1": 4 * (D // 2) * Dout, "z": 4 * Dout * BP})

    defines = {"MODE_GCN": int(mode == "gcn"), "R_SIZE": R, "D_SIZE": D, "B_SIZE": B, "L_SIZE": L,
               "DOUT_SIZE": Dout, "BP_SIZE": BP}
    arrays = [od.carray("gm_table_dram", od.halves(table), ctype="uint16_t"),
              od.carray("gm_idx_dram", idxw.flatten()),
              od.carray("gm_w1_dram", w1p.flatten()),
              od.carray("gm_zinit_dram", od.words(np.repeat(bias[:, None], BP, axis=1))),  # Y = bias broadcast [Dout][BP]
              od.carray("gm_golden", od.words(z))]
    if mode == "gcn":
        defines["SCALE_F16_BITS"] = "0x%04x" % int(np.float16(1.0 / L).view(np.uint16))
    od.write_header(pathlib.Path(__file__).resolve().parents[1], tag, defines, arrays)


if __name__ == "__main__":
    main()
