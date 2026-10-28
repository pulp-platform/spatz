#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator: single-head attention O = softmax(Q K^T / sqrt(d)) V, FP16 Q/K/V, FP32 output,
# processed on the device per key tile. Layouts, chosen so that no pre-processing is needed
# (two FP16 per word along the contraction dimension of each GeMM):
#   qw      [d/2][Tq]         W operand of GeMM1 (S^T = K_t Q^T): word (p,q) = {Q[q][2p], Q[q][2p+1]}
#   kw      [T/Bc][d/2][Bc]   X^T operand of GeMM1, per tile: word (p,j) = {K[j][2p], K[j][2p+1]}
#   vw      [T/Bc][Bc/2][dv]  X^T operand of GeMM2 (O^T += V_t^T P^T), per tile: word (p,c) = {V[2p][c], V[2p+1][c]}
#   golden  [dv][Tq]          O^T FP32
# The softmax statistics and the O^T accumulator are FP32; P^T is rounded to FP16 (the GeMM2 operand) as on
# the device. The golden follows the same online algorithm (running max/sum, FP16 P, FP32 l and O).

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "common"))
import opope_data as od  # noqa: E402


def main():
    tag, p = od.parse_cfg()
    Tq, T, d, dv, Bc = (p[k] for k in ("Tq", "T", "d", "dv", "Bc"))
    assert Tq == 64, "one LMUL=4 e32 vector (VLEN=512) per row of queries"
    assert Bc % 16 == 0 and dv % 16 == 0 and d % 2 == 0 and d >= 16 and T % Bc == 0
    NT = T // Bc

    rng = np.random.default_rng(1337)
    q = rng.uniform(-1, 1, (Tq, d)).astype(np.float16)
    k = rng.uniform(-1, 1, (T, d)).astype(np.float16)
    v = rng.uniform(-1, 1, (T, dv)).astype(np.float16)
    scale = 1.0 / np.sqrt(d)
    q = (q.astype(np.float64) * scale).astype(np.float16)  # 1/sqrt(d) folded into Q (exact for d = 4^k)

    # online-softmax golden (float64 math, FP16 P)
    m = np.full(Tq, -1e30)
    l = np.zeros(Tq)
    o = np.zeros((Tq, dv))
    for t in range(NT):
        s = q.astype(np.float64) @ k[t * Bc:(t + 1) * Bc].astype(np.float64).T  # [Tq][Bc]
        mn = np.maximum(m, s.max(axis=1))
        alpha = np.exp(m - mn)
        pr = np.exp(s - mn[:, None])
        l = l * alpha + pr.sum(axis=1)
        p16 = pr.astype(np.float16).astype(np.float64)
        o = o * alpha[:, None] + p16 @ v[t * Bc:(t + 1) * Bc].astype(np.float64)
        m = mn
    o = (o / l[:, None]).astype(np.float32)  # [Tq][dv]

    qw = od.pack_pairs(od.halves(q.T).reshape(d, Tq))  # [d/2][Tq]
    kw = np.stack([od.pack_pairs(od.halves(k[t * Bc:(t + 1) * Bc].T).reshape(d, Bc))
                   for t in range(NT)])  # [NT][d/2][Bc]
    vw = np.stack([od.pack_pairs(od.halves(v[t * Bc:(t + 1) * Bc]).reshape(Bc, dv))
                   for t in range(NT)])  # [NT][Bc/2][dv]

    od.check_footprint({"qw": 4 * (d // 2) * Tq, "kw": 4 * NT * (d // 2) * Bc,
                        "vw": 4 * NT * (Bc // 2) * dv, "st": 4 * Bc * Tq,
                        "pw": 4 * (Bc // 2) * Tq, "oT": 4 * dv * Tq, "m": 4 * Tq, "l": 4 * Tq})

    defines = {"TQ": Tq, "T_SIZE": T, "D_SIZE": d, "DV_SIZE": dv, "BC": Bc, "NT": NT,
               }
    arrays = [od.carray("fa_qw_dram", qw.flatten()), od.carray("fa_kw_dram", kw.flatten()),
              od.carray("fa_vw_dram", vw.flatten()), od.carray("fa_golden", od.words(o.T.copy())),
              od.carray("fa_m_dram", od.words(np.full(Tq, -1e30, np.float32)))]
    od.write_header(pathlib.Path(__file__).resolve().parents[1], tag, defines, arrays)


if __name__ == "__main__":
    main()
