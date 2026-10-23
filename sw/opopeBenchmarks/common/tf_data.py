# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

"""Reference models + data emitters of the transformer pipelines (P1..P11) on O-POPE + Spatz.

All activations are FEATURE-MAJOR: A^T[C][T] (see include/opope_transformer.h). Weights are stored
"in x out" row-major W[Cin][Cout] (the transposed X operand of the O-POPE job). Everything is FP32
on the device; the references are computed in float64 on the FP32 inputs.
"""

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parent))
import opope_data as od  # noqa: E402

F32 = np.float32


def rnd(rng, shape, scale=1.0):
    return (rng.standard_normal(shape) * scale).astype(F32)


def wgt(rng, cin, cout):
    return (rng.standard_normal((cin, cout)) / np.sqrt(cin)).astype(F32)


def ln(x, g, b):  # statistics over axis 0 (channels), per token
    x = x.astype(np.float64)
    m = x.mean(axis=0, keepdims=True)
    v = ((x - m) ** 2).mean(axis=0, keepdims=True)
    return (x - m) / np.sqrt(v + 1e-5) * g[:, None].astype(np.float64) + b[:, None].astype(np.float64)


def gelu(x):
    return 0.5 * x * (1.0 + np.tanh(np.sqrt(2.0 / np.pi) * (x + 0.044715 * x ** 3)))


def softmax0(x):
    e = np.exp(x - x.max(axis=0, keepdims=True))
    return e / e.sum(axis=0, keepdims=True)


def lin(w, a, b=None):  # Z^T[Cout][T] = W^T a + b
    z = w.astype(np.float64).T @ np.asarray(a, dtype=np.float64)
    return z if b is None else z + b.astype(np.float64)[:, None]


def f32(x):
    return np.asarray(x, dtype=np.float64).astype(F32)


def emit(app_dir, tag, defines, arrays, footprint):
    od.check_footprint(footprint)
    arr = [od.carray("d_" + k, od.words(v) if v.dtype != np.uint32 else v.flatten())
           for k, v in arrays.items()]
    od.write_header(app_dir, tag, defines, arr)


def fp(**kw):  # footprint dict: name -> number of FP32/uint32 words
    return {k: 4 * v for k, v in kw.items()}


# ---------------------------------------------------------------------------------------------
# Pipelines. Each returns nothing; it writes data/data_<tag>.h
# ---------------------------------------------------------------------------------------------

def p_lin(app_dir, tag, p, mode):
    """mode: 'p1' LN->GeMM, 'p2' GeMM->GELU, 'p3' LN->GeMM->GELU, 'p5' GeMM->ReLU"""
    C, T, O = p["C"], p["T"], p["COUT"]
    assert T >= 16 and O % 16 == 0
    rng = np.random.default_rng(11)
    x = rnd(rng, (C, T)); g = (1 + 0.1 * rnd(rng, C)); b = 0.1 * rnd(rng, C)
    w = wgt(rng, C, O); bias = 0.1 * rnd(rng, O)
    a = ln(x, g, b) if mode in ("p1", "p3") else x.astype(np.float64)
    z = lin(w, a, bias)
    if mode in ("p2", "p3"):
        z = gelu(z)
    if mode == "p5":
        z = np.maximum(z, 0)
    arrays = {"x": x, "gamma": g, "beta": b, "w": w, "bias": bias, "golden": f32(z)}
    use_ln = mode in ("p1", "p3")
    emit(app_dir, tag, {"C_SIZE": C, "T_SIZE": T, "COUT_SIZE": O},
         arrays, fp(x=C * T, ln=C * T if use_ln else 0, w=C * O, z=O * T, bias=O,
                    gamma=C if use_ln else 0, beta=C if use_ln else 0) if True else None)


def p_resid(app_dir, tag, p, fused):
    C, T = p["C"], p["T"]
    rng = np.random.default_rng(12)
    h = rnd(rng, (C, T)); res = rnd(rng, (C, T)); w = wgt(rng, C, C); bias = 0.1 * rnd(rng, C)
    z = lin(w, h, bias) + res
    arrays = {"h": h, "res": res, "w": w, "bias": bias, "golden": f32(z)}
    emit(app_dir, tag, {"C_SIZE": C, "T_SIZE": T}, arrays,
         fp(h=C * T, res=C * T, w=C * C, z=C * T, bias=C))


def p_softmax(app_dir, tag, p, ce):
    C, T, V = p["C"], p["T"], p["V"]
    assert T >= 16 and V % 16 == 0
    rng = np.random.default_rng(13)
    x = rnd(rng, (C, T)); w = wgt(rng, C, V) * 2; bias = 0.1 * rnd(rng, V)
    probs = softmax0(lin(w, x, bias))
    arrays = {"x": x, "w": w, "bias": bias, "golden": f32(probs)}
    ftp = dict(x=C * T, w=C * V, z=V * T, bias=V)
    if ce:
        tok = rng.integers(0, V, T).astype(np.uint32)
        loss = -np.log(probs[tok, np.arange(T)])
        arrays["tok"] = tok
        arrays["loss"] = f32(loss)
        ftp.update(tok=T, loss=T)
    emit(app_dir, tag, {"C_SIZE": C, "T_SIZE": T, "V_SIZE": V}, arrays, fp(**ftp))


def attention_ref(q, k, v, scale):
    """q,k: [d][T] feature-major, v: [T][d] token-major -> out^T [d][T]"""
    s = (k.astype(np.float64).T @ q.astype(np.float64)) * scale  # s[key][query]
    T = s.shape[0]
    mask = np.arange(T)[:, None] > np.arange(T)[None, :]  # key > query
    s = np.where(mask, -np.inf, s)
    pr = softmax0(s)
    return v.astype(np.float64).T @ pr  # [d][query]


def p_attn(app_dir, tag, p):
    d, T = p["D"], p["T"]
    assert T >= 16 and d % 16 == 0
    rng = np.random.default_rng(14)
    q = rnd(rng, (d, T)); k = rnd(rng, (d, T)); v = rnd(rng, (T, d))
    scale = 1.0 / np.sqrt(d)
    out = attention_ref(q, k, v, scale)
    emit(app_dir, tag, {"D_SIZE": d, "T_SIZE": T, "SCALE_F": "%.10ff" % scale},
         {"q": q, "k": k, "v": v, "golden": f32(out)},
         fp(q=d * T, k=d * T, v=d * T, s=T * T, o=d * T))


def p_mlp(app_dir, tag, p):
    C, T, H = p["C"], p["T"], p["HID"]
    assert H % 16 == 0
    rng = np.random.default_rng(15)
    x = rnd(rng, (C, T)); g = 1 + 0.1 * rnd(rng, C); b = 0.1 * rnd(rng, C)
    w1 = wgt(rng, C, H); b1 = 0.1 * rnd(rng, H); w2 = wgt(rng, H, C); b2 = 0.1 * rnd(rng, C)
    z = x + lin(w2, gelu(lin(w1, ln(x, g, b), b1)), b2)
    emit(app_dir, tag, {"C_SIZE": C, "T_SIZE": T, "HID_SIZE": H},
         {"x": x, "gamma": g, "beta": b, "w1": w1, "b1": b1, "w2": w2, "b2": b2,
          "golden": f32(z)},
         fp(z=C * T, ln=C * T, w1=C * H, h=H * T, w2=H * C, gamma=C, beta=C, b1=H, b2=C))


def p_embed(app_dir, tag, p):
    C, T, V, O = p["C"], p["T"], p["V"], p["COUT"]
    assert T >= 16 and C % 16 == 0 and O % 16 == 0
    rng = np.random.default_rng(16)
    tok = rng.integers(0, V, T).astype(np.uint32)
    wte = rnd(rng, (V, C)); wpe = 0.5 * rnd(rng, (T, C))
    g = 1 + 0.1 * rnd(rng, C); b = 0.1 * rnd(rng, C)
    w = wgt(rng, C, O); bias = 0.1 * rnd(rng, O)
    emb = (wte[tok].astype(np.float64) + wpe).T  # [C][T]
    z = lin(w, ln(emb, g, b), bias)
    emit(app_dir, tag, {"C_SIZE": C, "T_SIZE": T, "V_SIZE": V, "COUT_SIZE": O},
         {"tok": tok, "wte": wte, "wpe": wpe, "gamma": g, "beta": b, "w": w, "bias": bias,
          "golden": f32(z)},
         fp(tok=T, wte=V * C, wpe=T * C, emb=T * C, ident=T * T, embt=C * T, w=C * O, z=O * T,
            gamma=C, beta=C, bias=O))


def p_block(app_dir, tag, p):
    C, T, H = p["C"], p["T"], p["HID"]
    assert T % 16 == 0 and C % 16 == 0 and H % 16 == 0
    rng = np.random.default_rng(17)
    x = rnd(rng, (C, T))
    g1 = 1 + 0.1 * rnd(rng, C); be1 = 0.1 * rnd(rng, C)
    g2 = 1 + 0.1 * rnd(rng, C); be2 = 0.1 * rnd(rng, C)
    wq, wk, wv, wp = (wgt(rng, C, C) for _ in range(4))
    bq, bk, bv, bp = (0.1 * rnd(rng, C) for _ in range(4))
    w1 = wgt(rng, C, H); bm1 = 0.1 * rnd(rng, H); w2 = wgt(rng, H, C); bm2 = 0.1 * rnd(rng, C)
    a = ln(x, g1, be1)
    q = lin(wq, a, bq); k = lin(wk, a, bk)
    v = (a.T @ wv.astype(np.float64)) + bv.astype(np.float64)  # [T][C] token-major
    att = attention_ref(q, k, v, 1.0 / np.sqrt(C))
    z1 = x + lin(wp, att, bp)
    z2 = z1 + lin(w2, gelu(lin(w1, ln(z1, g2, be2), bm1)), bm2)
    arrays = {"x": x, "g1": g1, "be1": be1, "g2": g2, "be2": be2, "wq": wq, "wk": wk, "wv": wv,
              "wp": wp, "bq": bq, "bk": bk, "bv": bv, "bp": bp, "w1": w1, "bm1": bm1, "w2": w2,
              "bm2": bm2, "golden": f32(z2)}
    emit(app_dir, tag, {"C_SIZE": C, "T_SIZE": T, "HID_SIZE": H,
                        "SCALE_F": "%.10ff" % (1.0 / np.sqrt(C))}, arrays,
         fp(x=C * T, ln=C * T, wq=C * C, wk=C * C, wv=C * C, wp=C * C, q=C * T, k=C * T, v=C * T,
            s=T * T, att=C * T, z=C * T, w1=C * H, h=H * T, w2=H * C, g1=C, be1=C, g2=C, be2=C,
            bq=C, bk=C, bv=C, bp=C, bm1=H, bm2=C))
