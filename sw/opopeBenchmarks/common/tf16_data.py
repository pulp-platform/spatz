# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

"""Reference models + data emitters of the FP16-input / FP32-output transformer pipelines (P1..P11).

Activations are FEATURE-MAJOR A^T[C][T]; every GeMM operand that carries FP16 data is pair-packed along the
contraction dimension (rows for activations / weights): packed[R/2][C], word (p,j) = {M[2p][j] (lo), M[2p+1][j] (hi)}.
Weights W[Cin][Cout] are stored packed ([Cin/2][Cout]) = the X^T operand of Z^T = W^T A^T.
Inputs whose first operation is non-linear (LayerNorm) are stored FP32 and cast on the device; inputs that go
straight into a GeMM are stored FP16 (packed). The golden quantises to FP16 exactly where the device does.
"""

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parent))
import opope_data as od  # noqa: E402
import tf_data as tf  # noqa: E402

rnd, wgt, ln, gelu, softmax0, lin, f32, fp = tf.rnd, tf.wgt, tf.ln, tf.gelu, tf.softmax0, tf.lin, tf.f32, tf.fp


def h(x):  # FP16 quantisation, kept as float64 for the reference math
    return np.asarray(x).astype(np.float16).astype(np.float64)


def pk(m):  # [R][C] -> packed words [R/2][C]
    m = np.asarray(m)
    return od.pack_pairs(od.halves(m).reshape(m.shape))


def emit(app_dir, tag, defines, arrays, footprint):
    tf.emit(app_dir, tag, defines, arrays, footprint)


def p_lin(app_dir, tag, p, mode):
    """p1 LN->GeMM, p2 GeMM->GELU, p3 LN->GeMM->GELU, p5 GeMM->ReLU"""
    C, T, O = p["C"], p["T"], p["COUT"]
    assert T >= 16 and O % 16 == 0 and C % 2 == 0
    rng = np.random.default_rng(21)
    w = wgt(rng, C, O).astype(np.float16)
    bias = 0.1 * rnd(rng, O)
    use_ln = mode in ("p1", "p3")
    arrays = {}
    if use_ln:
        x = rnd(rng, (C, T)); g = 1 + 0.1 * rnd(rng, C); b = 0.1 * rnd(rng, C)
        a = h(ln(x, g, b))
        arrays.update(x=x, gamma=g, beta=b)
    else:
        x16 = rnd(rng, (C, T)).astype(np.float16)
        a = h(x16)
        arrays["xpk"] = pk(x16)
    z = lin(w, a, bias)
    if mode in ("p2", "p3"):
        z = gelu(z)
    if mode == "p5":
        z = np.maximum(z, 0)
    arrays.update(w=pk(w), bias=bias, golden=f32(z))
    ftp = dict(w=(C // 2) * O, z=O * T, bias=O)
    if use_ln:
        ftp.update(x=C * T, lnpk=(C // 2) * T, gamma=C, beta=C)
    else:
        ftp.update(xpk=(C // 2) * T)
    emit(app_dir, tag, {"C_SIZE": C, "T_SIZE": T, "COUT_SIZE": O}, arrays, fp(**ftp))


def p_resid(app_dir, tag, p):
    C, T = p["C"], p["T"]
    rng = np.random.default_rng(22)
    h16 = rnd(rng, (C, T)).astype(np.float16)
    res = rnd(rng, (C, T)); w = wgt(rng, C, C).astype(np.float16); bias = 0.1 * rnd(rng, C)
    z = lin(w, h(h16), bias) + res
    emit(app_dir, tag, {"C_SIZE": C, "T_SIZE": T},
         {"hpk": pk(h16), "res": res, "w": pk(w), "bias": bias, "golden": f32(z)},
         fp(hpk=(C // 2) * T, **({} if "fused" in app_dir.name else {"res": C * T}),
            w=(C // 2) * C, z=C * T, bias=C))


def p_softmax(app_dir, tag, p, ce):
    C, T, V = p["C"], p["T"], p["V"]
    assert T >= 16 and V % 16 == 0
    rng = np.random.default_rng(23)
    x16 = rnd(rng, (C, T)).astype(np.float16)
    w = (wgt(rng, C, V) * 2).astype(np.float16); bias = 0.1 * rnd(rng, V)
    probs = softmax0(lin(w, h(x16), bias))
    arrays = {"xpk": pk(x16), "w": pk(w), "bias": bias, "golden": f32(probs)}
    ftp = dict(xpk=(C // 2) * T, w=(C // 2) * V, z=V * T, bias=V)
    if ce:
        tok = rng.integers(0, V, T).astype(np.uint32)
        arrays["tok"] = tok
        arrays["loss"] = f32(-np.log(probs[tok, np.arange(T)]))
        ftp.update(tok=T, loss=T)
    emit(app_dir, tag, {"C_SIZE": C, "T_SIZE": T, "V_SIZE": V}, arrays, fp(**ftp))


def attn_core(qT, kT, v, scale):
    """qT,kT: [d][T] fp16-valued, v: [T][dv] fp16-valued -> (P16 [Tk][Tq], out^T [dv][Tq]); causal"""
    s = (np.asarray(kT, np.float64).T @ np.asarray(qT, np.float64)) * scale  # [key][query]
    T = s.shape[0]
    s = np.where(np.arange(T)[:, None] > np.arange(T)[None, :], -np.inf, s)
    p16 = h(softmax0(s))
    return p16, np.asarray(v, np.float64).T @ p16


def p_attn(app_dir, tag, p):
    d, T = p["D"], p["T"]
    assert T >= 16 and d % 16 == 0 and T % 2 == 0
    rng = np.random.default_rng(24)
    q = rnd(rng, (d, T)).astype(np.float16); k = rnd(rng, (d, T)).astype(np.float16)
    v = rnd(rng, (T, d)).astype(np.float16)
    scale = 1.0 / np.sqrt(d)
    _, out = attn_core(h(q), h(k), h(v), scale)
    emit(app_dir, tag, {"D_SIZE": d, "T_SIZE": T, "SCALE_F": "%.10ff" % scale},
         {"qpk": pk(q), "kpk": pk(k), "vpk": pk(v), "golden": f32(out)},
         fp(qpk=(d // 2) * T, kpk=(d // 2) * T, vpk=(T // 2) * d, s=T * T, ppk=(T // 2) * T, o=d * T))


def p_mlp(app_dir, tag, p):
    C, T, H = p["C"], p["T"], p["HID"]
    assert H % 16 == 0
    rng = np.random.default_rng(25)
    x = rnd(rng, (C, T)); g = 1 + 0.1 * rnd(rng, C); b = 0.1 * rnd(rng, C)
    w1 = wgt(rng, C, H).astype(np.float16); b1 = 0.1 * rnd(rng, H)
    w2 = wgt(rng, H, C).astype(np.float16); b2 = 0.1 * rnd(rng, C)
    z = x + lin(w2, h(gelu(lin(w1, h(ln(x, g, b)), b1))), b2)
    emit(app_dir, tag, {"C_SIZE": C, "T_SIZE": T, "HID_SIZE": H},
         {"x": x, "gamma": g, "beta": b, "w1": pk(w1), "b1": b1, "w2": pk(w2), "b2": b2,
          "golden": f32(z)},
         fp(z=C * T, lnpk=(C // 2) * T, w1=(C // 2) * H, h=H * T, hpk=(H // 2) * T, w2=(H // 2) * C,
            gamma=C, beta=C, b1=H, b2=C))


def p_embed(app_dir, tag, p):
    C, T, V, O = p["C"], p["T"], p["V"], p["COUT"]
    assert T >= 16 and C % 16 == 0 and O % 16 == 0
    rng = np.random.default_rng(26)
    tok = rng.integers(0, V, T).astype(np.uint32)
    wte = rnd(rng, (V, C)); wpe = 0.5 * rnd(rng, (T, C))
    g = 1 + 0.1 * rnd(rng, C); b = 0.1 * rnd(rng, C)
    w = wgt(rng, C, O).astype(np.float16); bias = 0.1 * rnd(rng, O)
    emb = (wte[tok] + wpe).astype(np.float32).T  # [C][T] FP32
    z = lin(w, h(ln(emb, g, b)), bias)
    emit(app_dir, tag, {"C_SIZE": C, "T_SIZE": T, "V_SIZE": V, "COUT_SIZE": O},
         {"tok": tok, "wte": wte, "wpe": wpe, "gamma": g, "beta": b, "w": pk(w), "bias": bias,
          "golden": f32(z)},
         fp(tok=T, wte=V * C, wpe=T * C, emb=T * C, ident=T * T, embt=C * T, lnpk=(C // 2) * T,
            w=(C // 2) * O, z=O * T, gamma=C, beta=C, bias=O))


def p_block(app_dir, tag, p):
    C, T, H = p["C"], p["T"], p["HID"]
    assert T % 16 == 0 and C % 16 == 0 and H % 16 == 0
    rng = np.random.default_rng(27)
    x = rnd(rng, (C, T))
    g1 = 1 + 0.1 * rnd(rng, C); be1 = 0.1 * rnd(rng, C)
    g2 = 1 + 0.1 * rnd(rng, C); be2 = 0.1 * rnd(rng, C)
    wq, wk, wv, wp = (wgt(rng, C, C).astype(np.float16) for _ in range(4))
    bq, bk, bv, bp = (0.1 * rnd(rng, C) for _ in range(4))
    w1 = wgt(rng, C, H).astype(np.float16); bm1 = 0.1 * rnd(rng, H)
    w2 = wgt(rng, H, C).astype(np.float16); bm2 = 0.1 * rnd(rng, C)
    scale = 1.0 / np.sqrt(C)
    a = h(ln(x, g1, be1))
    q = h(lin(wq, a, bq)); k = h(lin(wk, a, bk))
    v = h(a.T @ np.asarray(wv, np.float64) + bv.astype(np.float64))  # [T][C] token-major
    _, att = attn_core(q, k, v, scale)
    z1 = x + lin(wp, h(att), bp)
    z2 = z1 + lin(w2, h(gelu(lin(w1, h(ln(z1, g2, be2)), bm1))), bm2)
    arrays = {"x": x, "g1": g1, "be1": be1, "g2": g2, "be2": be2, "wq": pk(wq), "wk": pk(wk),
              "wv": pk(wv), "wp": pk(wp), "bq": bq, "bk": bk, "bv": bv, "bp": bp, "w1": pk(w1),
              "bm1": bm1, "w2": pk(w2), "bm2": bm2, "golden": f32(z2)}
    emit(app_dir, tag, {"C_SIZE": C, "T_SIZE": T, "HID_SIZE": H, "SCALE_F": "%.10ff" % scale},
         arrays,
         fp(x=C * T, lnpk=(C // 2) * T, wq=(C // 2) * C, wk=(C // 2) * C, wv=(C // 2) * C,
            wp=(C // 2) * C, q=C * T, k=C * T, v=T * C, qpk=(C // 2) * T, kpk=(C // 2) * T,
            vpk=(T // 2) * C, s=T * T, ppk=(T // 2) * T, att=C * T, attpk=(C // 2) * T, z=C * T,
            w1=(C // 2) * H, h=H * T, hpk=(H // 2) * T, w2=(H // 2) * C, g1=C, be1=C, g2=C, be2=C,
            bq=C, bk=C, bv=C, bp=C, bm1=H, bm2=C))
