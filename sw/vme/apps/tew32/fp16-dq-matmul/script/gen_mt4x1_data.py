#!/usr/bin/env python3
import argparse
import pathlib
import numpy as np


def carray(name, values):
    flat = np.asarray(values, dtype=np.uint32).reshape(-1)
    body = ",".join(hex(int(v)) for v in flat)
    return (f"static const uint32_t {name}[{flat.size}] "
            f"__attribute__((section(\".data\"), aligned(64)))={{{body}}};\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", required=True, type=pathlib.Path)
    ap.add_argument("--m", required=True, type=int)
    args = ap.parse_args()
    # Baseline convention MxNxK: X[M,N] * W[N,K], pair-block CBW16.
    m, red, n, cbw, cbn, ncb = args.m, 64, 128, 16, 128, 2
    pairs, groups = red // 2, n // cbw
    rng = np.random.default_rng(1337)
    cb = rng.uniform(-.5, .5, (ncb, cbn, 2, cbw)).astype(np.float16)
    idx = rng.integers(0, cbn, (ncb, pairs, groups), dtype=np.uint32)
    x = rng.uniform(-1, 1, (m, red)).astype(np.float16)
    decoded = (cb[0][idx[0]] + cb[1][idx[1]]).astype(np.float16)
    w = np.empty((red, n), dtype=np.float16)
    w[0::2] = decoded[:, :, 0, :].reshape(pairs, n)
    w[1::2] = decoded[:, :, 1, :].reshape(pairs, n)
    golden = (x.astype(np.float64) @ w.astype(np.float64)).astype(np.float32)

    xb = x.view(np.uint16)
    xw = ((xb[:, 0::2].astype(np.uint32) << 16) |
          xb[:, 1::2].astype(np.uint32))
    apack = xw.reshape(m // 16, 16, pairs).transpose(0, 2, 1).copy()
    cbh = cb.view(np.uint16).reshape(ncb, cbn, 2, cbw).astype(np.uint32)
    cbpack = (cbh[:, :, 0, :] << 16) | cbh[:, :, 1, :]
    idxpack = (idx[0, :, 0::2] | (idx[1, :, 0::2] << 7) |
               (idx[0, :, 1::2] << 14) | (idx[1, :, 1::2] << 21))
    lines = ["#pragma once", "#include <stdint.h>",
             f"#define DQ_M {m}", "#define DQ_RED 64", "#define DQ_N 128",
             "#define DQ_CB_W 16", "#define DQ_CBN 128", "#define DQ_NCB 2",
             carray("dq_mt4_a", apack), carray("dq_mt4_cb", cbpack),
             carray("dq_mt4_idx", idxpack),
             carray("dq_mt4_golden", golden.view(np.uint32))]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines))


if __name__ == "__main__":
    main()
