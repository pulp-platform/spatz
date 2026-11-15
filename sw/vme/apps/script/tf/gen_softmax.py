#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
"""Emit kernel/softmax_cols.c: column softmax of buf[R][64] (FP32, in place) on the Spatz VFU.

Same arithmetic as baseline tf16_softmax_cols (opope_tf16.h): column max, x - max, TF16_EXP_NEG (degree-5
Taylor + six squarings), row-ordered column sum, TF_RECIP (magic constant + 3 Newton steps), x * (1/sum).
Only the schedule differs: loads are issued ahead of their use (several rows in flight), the max pass uses two
accumulators (max is exact in any order), the normalisation uses e32,m8 (two rows per instruction).
"""
import argparse
import pathlib
import struct

ap = argparse.ArgumentParser()
ap.add_argument("--rows", type=int, default=192)
ap.add_argument("--name", default="vme_softmax_cols")
ap.add_argument("--stats", type=int, default=0, help="1: also store column max / sum (a1, a2)")
ap.add_argument("--r0", type=int, default=0, help="rows [0, r0) already max-reduced into v0 / v4 by the caller")
ap.add_argument("--ce", type=int, default=0,
                help="P7: gather target logits (a3 = tok, a4 = tgt) before the exp pass and compute the cross-entropy "
                     "loss (a5 = loss) in 16-token chunks interleaved with the normalisation (implies --stats 1)")
ap.add_argument("--out", default=None)
ap.add_argument("--loadat", type=int, default=3, help="exp pass: next-pair loads after this many op pairs")
A = ap.parse_args()
if A.ce:
    A.stats = 1
ARGS_LOADAT = A.loadat
R = A.rows
assert R % 12 == 0

GPR = {"t0": 5, "t1": 6, "t2": 7, "a0": 10, "a1": 11, "a2": 12, "a6": 13, "a7": 14, "a5": 15, "a6": 16, "a7": 17}


def rr(vd, rs1, rs2):
    w = (0b110000 << 26) | (1 << 25) | (GPR[rs2] << 20) | (GPR[rs1] << 15) | (0b110 << 12) | (vd << 7) | 0x7B
    return f".word 0x{w:08x} /* p.vle32.v.rrpost v{vd}, ({rs1}), {rs2} */"


L = []


def e(*x):
    L.extend(x)


# float constants live in fa/ft registers loaded from a small table
CONSTS = {"m16": -16.0, "s64": 0.015625, "c5": 0.0083333333, "c4": 0.0416666667, "c3": 0.1666666667,
          "c2": 0.5, "one": 1.0, "two": 2.0}
FREG = {"m16": "ft0", "s64": "ft1", "c5": "ft2", "c4": "ft3", "c3": "ft4", "c2": "ft5", "one": "ft6", "two": "ft7"}

# a0 = buf, t2 = 256 (row stride), t1 = 512 (two rows)
VM4 = "vsetvli zero, t0, e32, m4, ta, ma"      # t0 = 64
e("li t0, 64", "li t2, 256", "li t1, 512", VM4)
for k, v in CONSTS.items():
    import struct
    bits = struct.unpack("<I", struct.pack("<f", v))[0]
    e(f"li t3, 0x{bits:08x}", f"fmv.w.x {FREG[k]}, t3")

if A.ce:
    # target logits tgt[t] = buf[tok[t]][t] (the GeMM has finished; its last tile stores must have landed):
    # one indexed load, byte offset tok << 8 | t << 2
    # (indexed vector loads hang the VLSU here, vid.v is not implemented: scalar gather as in O-POPE)
    # unrolled by 4: the four index loads first (one outstanding integer load), then the four FP loads
    # (several outstanding), then the stores
    e("fence rw, rw", "mv t5, a3", "mv t6, a4", "mv t0, a0", "addi t1, a3, 256", ".Lga:",
      "lw t3, 0(t5)", "lw t4, 4(t5)", "lw a6, 8(t5)", "lw a7, 12(t5)",
      "slli t3, t3, 8", "slli t4, t4, 8", "slli a6, a6, 8", "slli a7, a7, 8",
      "add t3, t3, t0", "add t4, t4, t0", "add a6, a6, t0", "add a7, a7, t0",
      "flw ft8, 0(t3)", "flw ft9, 4(t4)", "flw ft10, 8(a6)", "flw ft11, 12(a7)",
      "fsw ft8, 0(t6)", "fsw ft9, 4(t6)", "fsw ft10, 8(t6)", "fsw ft11, 12(t6)",
      "addi t5, t5, 16", "addi t6, t6, 16", "addi t0, t0, 16", "bne t5, t1, .Lga",
      "li t0, 64", "li t1, 512", VM4)

# ---- max pass: acc v0 / v4, ring of 6 buffers, loads 4 rows ahead ----
BUF = [8, 12, 16, 20, 24, 28]
AH = 4
e(f"li t3, {A.r0 * 256}", "add a6, a0, t3")       # load pointer (first row not yet reduced)
for r in range(AH):
    e(rr(BUF[r % 6], "a6", "t2"))
if A.r0 == 0:
    e("li t3, 0xf149f2ca", "fmv.w.x ft8, t3", "vfmv.v.f v0, ft8", "vfmv.v.f v4, ft8")  # -1e30


def max_row(r, load):
    e(f"vfmax.vv v{4 * (r % 2)}, v{4 * (r % 2)}, v{BUF[r % 6]}")
    if load:
        e(rr(BUF[(r + AH) % 6], "a6", "t2"))


# loop: 6 rows per iteration (static ring), R/6 iterations, last one without the run-ahead loads past R
n_it = (R - A.r0) // 6
assert (R - A.r0) % 6 == 0 and A.r0 % 2 == 0
e(f"li t4, {n_it - 1}", ".Lmx:")
for r in range(6):
    max_row(r, True)
e("addi t4, t4, -1", "bnez t4, .Lmx")
for r in range(6):
    max_row(r, r + AH < 6)
e("vfmax.vv v0, v0, v4")              # column max M in v0

# ---- exp + sum pass: pairs of rows; M = v0, S = v4, temps v8 / v12, row pairs P0 = v16/v20, P1 = v24/v28 ----
e("vmv.v.i v4, 0", "mv a6, a0", "mv a7, a0")          # a3 load pointer, a4 store pointer
P = [(16, 20), (24, 28)]
e(rr(16, "a6", "t2"), rr(20, "a6", "t2"))


def exp_pair(p, load_next):
    xa, xb = P[p % 2]
    ta, tb = 8, 12
    e(f"vfsub.vv v{xa}, v{xa}, v0", f"vfsub.vv v{xb}, v{xb}, v0")
    seq = [("vfmax.vf", "X", "m16"), ("vfmul.vf", "X", "s64"), ("vfmv.v.f", "T", "c5")]
    for c in ("c4", "c3", "c2", "one", "one"):
        seq += [("vfmul.vv", "T", "X"), ("vfadd.vf", "T", c)]
    seq += [("sq0", None, None)] + [("sq", None, None)] * 5
    for i_op, (op, d, c) in enumerate(seq):
        if load_next and i_op == ARGS_LOADAT:
            na, nb = P[(p + 1) % 2]
            e(rr(na, "a6", "t2"), rr(nb, "a6", "t2"))
        for x, t in ((xa, ta), (xb, tb)):
            if op == "vfmv.v.f":
                e(f"vfmv.v.f v{t}, {FREG[c]}")
            elif op == "vfmul.vv" and d == "T":
                e(f"vfmul.vv v{t}, v{t}, v{x}")
            elif op == "vfadd.vf":
                e(f"vfadd.vf v{t}, v{t}, {FREG[c]}")
            elif op in ("vfmax.vf", "vfmul.vf"):
                e(f"{op} v{x}, v{x}, {FREG[c]}")
            elif op == "sq0":
                e(f"vfmul.vv v{x}, v{t}, v{t}")
            elif op == "sq":
                e(f"vfmul.vv v{x}, v{x}, v{x}")
    e(f"vse32.v v{xa}, (a7)", "add a7, a7, t2", f"vse32.v v{xb}, (a7)", "add a7, a7, t2",
      f"vfadd.vv v4, v4, v{xa}", f"vfadd.vv v4, v4, v{xb}")


n_pairs = R // 2
e(f"li t4, {n_pairs // 2 - 1}", ".Lex:")
exp_pair(0, True)
exp_pair(1, True)
e("addi t4, t4, -1", "bnez t4, .Lex")
exp_pair(0, True)
exp_pair(1, False)
if A.stats:
    e("vse32.v v0, (a1)", "vse32.v v4, (a2)")
# ---- 1/S: TF_RECIP (vrsub magic + 3 Newton steps) into v8 ----
e("li t3, 0x7EF311C7", "vrsub.vx v8, v4, t3")
for _ in range(3):
    e("vfmul.vv v12, v4, v8", "vfrsub.vf v12, v12, ft7", "vfmul.vv v8, v8, v12")
# ---- normalise: e32,m8 two rows per instruction, R/2 pairs, loads one pair ahead (bufs v16 / v24) ----
# 1/S must be replicated over both rows of an m8 group: v8..v11 = 1/S, v12..v15 = copy
e("vmv.v.v v12, v8", "li t5, 128", "vsetvli zero, t5, e32, m8, ta, ma", "mv a6, a0", "mv a7, a0")
NB = [16, 24]


def ce_ops():
    """Cross entropy (same op sequence as tf16_ce_loss) in 4 chunks of 16 tokens, e32,m1, registers v0..v7:
    loss = e*ln2 + log(m) + colmax - tgt, sum = 2^e * m, log(m) = 2 s (1 + s^2/3 + s^4/5 + s^6/7 + s^8/9),
    s = (m - 1) / (m + 1)."""
    ops = []
    for c in range(4):
        o = 64 * c
        ops += [[f"addi t3, a2, {o}", "vle32.v v0, (t3)"],
                ["vsrl.vi v1, v0, 23"], ["li t3, 0x4B000000", "vor.vx v1, v1, t3"], ["vfsub.vf v1, v1, fa0"],
                ["li t3, 0x007FFFFF", "vand.vx v2, v0, t3"], ["li t3, 0x3F800000", "vor.vx v2, v2, t3"],
                ["vfadd.vf v3, v2, ft6"], ["vfsub.vf v2, v2, ft6"],
                ["li t3, 0x7EF311C7", "vrsub.vx v4, v3, t3"]]
        for _ in range(3):
            ops += [["vfmul.vv v5, v3, v4"], ["vfrsub.vf v5, v5, ft7"], ["vfmul.vv v4, v4, v5"]]
        ops += [["vfmul.vv v2, v2, v4"], ["vfmul.vv v4, v2, v2"], ["vfmv.v.f v5, fa1"], ["vfmul.vv v5, v5, v4"],
                ["vfadd.vf v5, v5, fa2"], ["vfmul.vv v5, v5, v4"], ["vfadd.vf v5, v5, fa3"], ["vfmul.vv v5, v5, v4"],
                ["vfadd.vf v5, v5, fa4"], ["vfmul.vv v5, v5, v4"], ["vfadd.vf v5, v5, ft6"], ["vfmul.vv v5, v5, v2"],
                ["vfmul.vf v5, v5, ft7"], ["vfmul.vf v1, v1, fa5"], ["vfadd.vv v1, v1, v5"],
                [f"addi t3, a1, {o}", "vle32.v v6, (t3)"], [f"addi t3, a4, {o}", "vle32.v v7, (t3)"],
                ["vfadd.vv v1, v1, v6"], ["vfsub.vv v1, v1, v7"], [f"addi t3, a5, {o}", "vse32.v v1, (t3)"]]
    return ops


if A.ce:
    for k, v in {"fa0": 8388735.0, "fa1": 0.1111111111, "fa2": 0.1428571429, "fa3": 0.2, "fa4": 0.3333333333,
                 "fa5": 0.69314718056}.items():
        bits = struct.unpack("<I", struct.pack("<f", v))[0]
        e(f"li t3, 0x{bits:08x}", f"fmv.w.x {k}, t3")
    CE = ce_ops()
    per = -(-len(CE) // (n_pairs - 4))           # CE ops per normalisation pair
    e(rr(16, "a6", "t1"))
    k = 0
    for p in range(n_pairs):                      # straight-line: one m8 pair (two rows) per step
        if p + 1 < n_pairs:
            e(rr(NB[(p + 1) % 2], "a6", "t1"))
        e(f"vfmul.vv v{NB[p % 2]}, v{NB[p % 2]}, v8", f"vse32.v v{NB[p % 2]}, (a7)", "add a7, a7, t1")
        if p >= 2 and k < len(CE):
            e("li t6, 16", "vsetvli zero, t6, e32, m1, ta, ma")
            for _ in range(per):
                if k < len(CE):
                    e(*CE[k]); k += 1
            e("vsetvli zero, t5, e32, m8, ta, ma")
    assert k == len(CE)
else:
    e(rr(16, "a6", "t1"))
    e(f"li t4, {n_pairs // 2 - 1}", ".Lnm:")
    for p in range(2):
        e(rr(NB[(p + 1) % 2], "a6", "t1"), f"vfmul.vv v{NB[p % 2]}, v{NB[p % 2]}, v8",
          f"vse32.v v{NB[p % 2]}, (a7)", "add a7, a7, t1")
    e("addi t4, t4, -1", "bnez t4, .Lnm")
    e(rr(NB[1], "a6", "t1"), f"vfmul.vv v{NB[0]}, v{NB[0]}, v8", f"vse32.v v{NB[0]}, (a7)", "add a7, a7, t1")
    e(f"vfmul.vv v{NB[1]}, v{NB[1]}, v8", f"vse32.v v{NB[1]}, (a7)")
e("fence rw, rw", "lw t3, -4(a7)", "ret")

N = A.name
body = "\n".join(f'    "{x}\\n"' for x in L)
src = (f"// GENERATED by script/gen_softmax.py -- column softmax of buf[{R}][64], same math as tf16_softmax_cols\n"
       f"#include <stdint.h>\nvoid {N}(float *buf{', float *colmax, float *colsum' if A.stats else ''}"
       f"{', const uint32_t *tok, float *tgt, float *loss' if A.ce else ''});\n"
       f'__asm__(\n    ".text\\n"\n    ".balign 64\\n"\n    ".globl {N}\\n"\n    ".type {N}, @function\\n"\n'
       f'    "{N}:\\n"\n{body}\n    ".size {N}, .-{N}\\n");\n')
dst = pathlib.Path(A.out) if A.out else pathlib.Path("kernel") / "softmax_cols.c"
dst.write_text(src)
print(f"wrote {dst} ({sum(1 for x in L if not x.endswith(':'))} instructions)")
