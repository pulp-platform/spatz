# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

"""Pair-block codebooks for the dequantised-weight GeMMs (VQ / DQ), FP16.

A codeword covers TWO consecutive weight rows (2p, 2p+1) x CBW consecutive columns and is stored as CBW 32-bit words
{row 2p (bits 15:0), row 2p+1 (bits 31:16)}, i.e. already in the packed layout of the W operand of the widening GeMM.
W[2p..2p+1][g*CBW .. +CBW) = cb0[i0] + cb1[i1] (FP16 add, the two rows at once). The indices of two consecutive groups (g = 2h, 2h+1) of pair p are
stored in ONE 32-bit word of 7-bit fields {cb0 index of g (bits 6:0), cb1 index of g (13:7), cb0 index of g+1 (20:14), cb1 index of g+1 (27:21)}; the byte
offset of an entry is index * CBW * 4 (relative to the start of
the codebooks, so the device decode is: 2 vector loads, 1 FP16 add, 1 store per group and ONE scalar load per two groups,
with no integer vector op, no zip and no strided access.
An optional per-row scale (VQ) is folded into the activations offline: X'[m][n] = X[m][n] * scale[n] (FP16), which is the
same product Z = X * diag(scale) * W.
"""

import numpy as np

import opope_data as od


def G_EVEN(K, CBW):
    return (K // CBW) % 2 == 0


def make(rng, M, N, K, CBW, CBN, with_scale):
    NCB = 2
    assert N % 2 == 0 and K % CBW == 0 and CBW == 16, "one group = 16 words = 32 halfwords = one e16 LMUL=1 register"
    assert CBN <= 128 and G_EVEN(K, CBW)
    P, G = N // 2, K // CBW
    cb = rng.uniform(-0.5, 0.5, (NCB, CBN, 2, CBW)).astype(np.float16)  # [codebook][entry][row parity][column]
    idx = rng.integers(0, CBN, (NCB, P, G))
    x = rng.uniform(-1, 1, (M, N)).astype(np.float16)
    if with_scale:
        scale = rng.uniform(0.5, 1.5, N).astype(np.float16)
        x = (x * scale[None, :]).astype(np.float16)
    e = (cb[0][idx[0]] + cb[1][idx[1]]).astype(np.float16)  # [P][G][2][CBW]
    w = np.zeros((N, K), dtype=np.float16)
    w[0::2, :] = e[:, :, 0, :].reshape(P, K)
    w[1::2, :] = e[:, :, 1, :].reshape(P, K)
    z = (x.astype(np.float64) @ w.astype(np.float64)).astype(np.float32)
    xT = od.pack_pairs(od.halves(x.T).reshape(N, M))  # [N/2][M] words
    h = od.halves(cb).reshape(NCB, CBN, 2, CBW).astype(np.uint32)
    cbw = (h[:, :, 0, :] | (h[:, :, 1, :] << 16)).astype(np.uint32)  # [NCB][CBN][CBW] words
    i = idx.astype(np.uint32)  # [2][P][G]
    idxw = (i[0][:, 0::2] | (i[1][:, 0::2] << 7) | (i[0][:, 1::2] << 14) | (i[1][:, 1::2] << 21)).astype(np.uint32)  # [P][G/2]
    return dict(xT=xT, cbw=cbw, idxw=idxw, z=z, P=P, G=G)
