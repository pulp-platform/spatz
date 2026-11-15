#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
"""P9 MLP block (LN -> GeMM1 + b1 -> GELU -> GeMM2 + b2 + residual), C=64, T=64, HID=144: FUSED VME kernel.

The VFU always runs at vl = 64 (e32, m4); the OPE work (a queue of K-groups and tile-transfer chunks, consumed in
order) is placed into slots of the VFU code:
  1. LN over all 64 tokens: passes 1-2 alone; pass 3 double-buffered (narrowed in place), GeMM1(A) block 0 operands
     in v20/v24, its K-groups tracking the LN output (operand loads and vtfmm in different slots).
  2. GELU(A) (token half A, elementwise on 2-row x 32-token vectors: h is stored [half][160][32]); TF16_GELU2 op
     sequence with one bit-exact rewrite (vfmv.v.f + vfmul.vv -> vfmul.vf, fusegen.py), 4 rows per step. Its T3 registers (v12-v15, v28-v31) are dead from the end of a step to op 53 of the
     next: K-groups (A in v12-v15, B in v28-v31) are placed in slots after ops 2, 4, ..., 40 (operand loads in one
     slot, vtfmm in a later one, >= 13 VFU ops before op 53 rewrites the registers). Tile transfers go in 2-row
     chunks into the same slots and after ops 42, 44, ..., 66: a longer scalar burst would starve the VFU (measured:
     2-row chunks are free, 8-row chunks cost ~3 cycles per row). OPE meanwhile: rest of GeMM1(A), GeMM1(B).
  3. GELU(B) with GeMM2(A) and GeMM2(B) block 0 (K-major, tracking the GELU output) in its slots.
  4. tail: GeMM2(B) block 1 as two 2-tile halves (block 0 lower drain and half seeds/drains overlap the MACs of the
     other half), two K-groups per vector/matrix config switch.
OPE blocks are 2x2 (32 rows x 32 tokens = all 4 tiles), 2 K-pairs per K-group (one e32,m4 load for A, one for B);
GeMM1 rows 144..159 (padding) are neither computed nor drained. Biases are extra reduction K-pairs (constant operand
rows placed outside the measured region), the residual is seeded into the GeMM2 tiles.
Layouts (packed words in O-POPE half order {K even (lo), K odd (hi)}):
  lnpk [2][33][32] (row 32 = {1,1}), hpk [2][73][32] (row 72 = {1,0}), h [2][160][32] FP32, z [64][64] FP32
  A1 stream [5][17][2 tiles][2 K-pairs][16] (W1' padded to 160 rows, K-pair 32 = {b1 hi, b1 lo}), A2 [2][37][2][2][16]
"""
import argparse, pathlib, re, struct, sys
import numpy as np

F32 = np.float32
ap = argparse.ArgumentParser()
ap.add_argument("--data", required=True)
ap.add_argument("--kernel", required=True)
ap.add_argument("--header", default=str(pathlib.Path(__file__).resolve().parents[3] / "include" / "opope_tf16_pairs.h"))
ap.add_argument("--rewrite", type=int, default=1, help="1: GELU with the bit-exact vfmv+vfmul -> vfmul.vf rewrite")
ap.add_argument("--win", default="", help="GELU ops after which OPE chunks may be placed")
ap.add_argument("--per", type=int, default=1, help="OPE K-group chunks per window slot")
ap.add_argument("--ipu", type=int, default=0, help="1: one K-group per step in T2 registers, vtfmm during the vrsub ops")
ap.add_argument("--margin", type=int, default=2, help="GELU steps between a GeMM1 drain and its consumption")
ap.add_argument("--xr", type=int, default=2, help="tile rows per transfer chunk")
ap.add_argument("--xfers", type=int, default=1, help="transfer chunks per window slot")
ap.add_argument("--sxfers", type=int, default=-1, help="transfer chunks in a MAC slot (-1: = --xfers)")
ap.add_argument("--xwin", default="", help="extra GELU ops after which only transfer chunks are placed")
ap.add_argument("--p3", type=int, default=1, help="1: double-buffered LN pass 3 with split OPE slots")
ap.add_argument("--tail", type=int, default=1, help="1: last GeMM2 block as two overlapped 2-tile halves")
ap.add_argument("--txr", type=int, default=8, help="rows per transfer chunk in the tail block")
ap.add_argument("--tbatch", type=int, default=2, help="K-groups per config switch in the tail")
A = ap.parse_args()
C, T, H, HP = 64, 64, 144, 160
KP1, KP2 = C // 2 + 1, H // 2 + 1
KG1, KG2 = -(-KP1 // 2), -(-KP2 // 2)
WIN = [int(x) for x in A.win.split(",") if x]
XWIN = [int(x) for x in A.xwin.split(",") if x]
XR = A.xr

# ------------------------------------------------------------------ data (same draws as tf16_data.p_mlp) -------
def rnd(rng, shape, scale=1.0): return (rng.standard_normal(shape) * scale).astype(F32)
def wgt(rng, cin, cout): return (rng.standard_normal((cin, cout)) / np.sqrt(cin)).astype(F32)
def h16(x): return np.asarray(x).astype(np.float16).astype(np.float64)
def ln_na(x):
    x = x.astype(np.float64); m = x.mean(axis=0, keepdims=True); v = ((x - m) ** 2).mean(axis=0, keepdims=True)
    return (x - m) / np.sqrt(v + 1e-5)
def fold_ln(w, g, b, bias):
    w64 = w.astype(np.float64)
    return (w64 * g.astype(np.float64)[:, None]).astype(np.float16), (bias.astype(np.float64) + w64.T @ b.astype(np.float64)).astype(F32)
def gelu(x): return 0.5 * x * (1.0 + np.tanh(np.sqrt(2.0 / np.pi) * (x + 0.044715 * x ** 3)))
def lin(w, a, b=None):
    z = w.astype(np.float64).T @ np.asarray(a, dtype=np.float64)
    return z if b is None else z + b.astype(np.float64)[:, None]
rng = np.random.default_rng(25)
x = rnd(rng, (C, T)); g = 1 + 0.1 * rnd(rng, C); b = 0.1 * rnd(rng, C)
w1 = wgt(rng, C, H).astype(np.float16); b1 = 0.1 * rnd(rng, H)
w2 = wgt(rng, H, C).astype(np.float16); b2 = 0.1 * rnd(rng, C)
w1, b1 = fold_ln(w1, g, b, b1)
b2 = b2.astype(np.float16).astype(np.float64)
z = x + lin(w2, h16(gelu(lin(w1, h16(ln_na(x)), b1))), b2)
def bits(m): return np.asarray(m, np.float16).view(np.uint16).astype(np.uint32)
def pack22(wext, M):
    bb = bits(wext); words = bb[0::2] | (bb[1::2] << 16)
    KP = words.shape[0]; KG = -(-KP // 2)
    pad = np.zeros((KG * 2, M), np.uint32); pad[:KP] = words
    return pad.reshape(KG, 2, M // 32, 2, 16).transpose(2, 0, 3, 1, 4).ravel()
hi = b1.astype(np.float16); lo = (b1.astype(np.float64) - hi.astype(np.float64)).astype(np.float16)
w1ext = np.zeros((C + 2, HP), np.float16); w1ext[:C, :H] = w1; w1ext[C, :H] = hi; w1ext[C + 1, :H] = lo
w2ext = np.zeros((H + 2, C), np.float16); w2ext[:H] = w2; w2ext[H] = b2.astype(np.float16)
arrays = {"d_x": x.view(np.uint32), "d_a1": pack22(w1ext, HP), "d_a2": pack22(w2ext, C),
          "d_lnone": np.full(32, 0x3C003C00, np.uint32), "d_hone": np.full(32, 0x00003C00, np.uint32),
          "d_golden": z.astype(F32).view(np.uint32)}
out = ["// GENERATED by gen_mlp_block_fused.py", "#pragma once", "#include <stdint.h>", f"#define C_SIZE {C}", f"#define T_SIZE {T}",
       f"#define HID_SIZE {H}", f"#define HID_PAD {HP}", f"#define KP1 {KP1}", f"#define KP2 {KP2}"]
for k, v in arrays.items():
    v = np.asarray(v).ravel()
    out.append(f"static uint32_t {k}[{len(v)}] __attribute__((section(\".data\"), aligned(64))) = {{" +
               ",".join(f"0x{int(q):08x}" for q in v) + "};")
pathlib.Path(A.data).write_text("\n".join(out) + "\n")

# ------------------------------------------------------------------ helpers -------------------------------------
GPR = {"zero": 0, "t0": 5, "t1": 6, "t2": 7, "s0": 8, "s1": 9, **{f"a{i}": 10 + i for i in range(8)},
       **{f"s{i}": 16 + i for i in range(2, 12)}, "t3": 28, "t4": 29, "t5": 30, "t6": 31}
def rr(vd, rs1, rs2):
    w = (0b110000 << 26) | (1 << 25) | (GPR[rs2] << 20) | (GPR[rs1] << 15) | (0b110 << 12) | (vd << 7) | 0x7B
    return f".word 0x{w:08x} /* p.vle32.v.rrpost v{vd}, ({rs1}), {rs2} */"
def fbits(v): return struct.unpack("<I", struct.pack("<f", v))[0]
# scalar registers. OPE: a0 A ptr, a1 B ptr, a2 addr, a3 TSS, a4 scratch, t1 = 256, s1 = 128, t4/t5/t6 mtype/vtype/16,
#   s0 = 64, s2 z, s3 h, s4 lnpk, s5 hpk, s6 A1, s7 A2.   VFU: a5 load, a6 store, a7 aux, t2 = 4, t3 scratch, s8 = 256.
VM64 = "vsetvli zero, s0, e32, m4, ta, ma"
MM = ["msetmtype t4, t5", "msettn zero, t6"]
V16_64 = "vsetvli zero, s0, e16, m2, ta, ma"
V16_32 = "vsetvli zero, s11, e16, m1, ta, ma"        # s11 = 32
txt = pathlib.Path(A.header).read_text()
body = txt[txt.index("#define TF16_GELU2()"):]; body = body[:body.index("while (0)")]
GOPS = []
for m in re.finditer(r'asm volatile\("([^"]*)"\s*(?:::\s*"([fr])"\(([^)]*)\))?\)', body):
    ins, kind, val = m.group(1), m.group(2), m.group(3)
    mn, ops = ins.split(None, 1)
    GOPS.append((mn, [o.strip() for o in ops.split(",")], kind, val))
assert len(GOPS) == 68
if A.rewrite:
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[3] / "script" / "tf"))
    import fusegen as _fg
    GOPS = [tuple(o) for o in _fg.gelu_ops(A.header)]
# register windows from the op sequence: T3 (v12 / v28) first written at op T3W; K-group vtfmm >= 12 ops before it
T3W = min(i for i, o in enumerate(GOPS) if o[1][0] in ("v12", "v28")) + 1
LOADAT = max(i for i, o in enumerate(GOPS) if any(r in o[1][1:] for r in ("v4", "v20"))) + 1
if not WIN:
    WIN = list(range(2, T3W - 12, 2))
if not A.xwin:
    XWIN = list(range(WIN[-1] + (4 if A.rewrite else 2), len(GOPS) - 1, 2))
FC = {}
FNAMES = [f"ft{i}" for i in range(12)] + [f"fa{i}" for i in range(8)]
def freg(v):
    if v not in FC:
        FC[v] = FNAMES[len(FC)]
    return FC[v]
for mn, ops, kind, val in GOPS:
    if kind == "f":
        freg(float(val.rstrip("f")))
for v in (1.0 / C, 1e-5, 0.5, 1.5):
    freg(v)
ICONST = {0x7EF311C7: "s9", 0x5F3759DF: "s10"}

L = []
def e(*x): L.extend(x)
def mark(n): L.extend([f".globl p9m_{n}", f"p9m_{n}:"])

# ------------------------------------------------------------------ OPE work queue ------------------------------
TS = [0x00000000, 0x20000000, 0x40000000, 0x60000000]
TILES = ["mt0", "mt4", "mt8", "mt12"]


class Op:
    """one OPE chunk: kind in init/group/xfer; render(regs) -> instructions (regs = (A base, B base) for groups)"""
    def __init__(self, kind, render, req=None, prov=None, cost=0):
        self.kind, self.render, self.req, self.prov, self.cost = kind, render, req, prov, cost


def block_ops(hf, rb, abase, astride, bbase, kp, KG, zreg, zoff, zstride_reg, seed, req_fn, prov, ntiles=4):
    ops = []
    def init(regs, hf=hf, rb=rb):
        return [f"li a4, {rb * astride}", f"add a0, {abase}, a4", f"li a4, {hf * kp * 128}", f"add a1, {bbase}, a4"] + \
               ([] if seed else [f"vtzero {t}" for t in TILES])
    ops.append(Op("init", init))
    if seed:
        for ti in range(4):
            for r0 in range(0, 16, XR):
                def sd(regs, ti=ti, r0=r0):
                    o = [f"li a3, {TS[ti] + r0}", f"li a4, {zoff + ((ti // 2) * 16 + r0) * 256 + (ti % 2) * 64}",
                         f"add a2, {zreg}, a4"] if r0 == 0 else []
                    for _ in range(XR):
                        o += ["vtle32 a3, (a2)", "addi a3, a3, 1", f"add a2, a2, {zstride_reg}"]
                    return o
                ops.append(Op("xfer", sd))
    for gi in range(KG):
        nk = min(2, kp - 2 * gi)
        def grp(regs, nk=nk, part="all"):
            a, bb = regs
            if part == "load":                                  # issued under the VFU config (e32, m4, vl = 64)
                return [rr(a, "a0", "t1"), rr(bb, "a1", "t1")]
            o = [VM64, rr(a, "a0", "t1"), rr(bb, "a1", "t1"), *MM] if part == "all" else []
            for k in range(nk):
                o += [f"vtfmm.tvv mt0, v{a + k}, v{bb + 2 * k}", f"vtfmm.tvv mt4, v{a + k}, v{bb + 2 * k + 1}"]
                if ntiles == 4:
                    o += [f"vtfmm.tvv mt8, v{a + 2 + k}, v{bb + 2 * k}", f"vtfmm.tvv mt12, v{a + 2 + k}, v{bb + 2 * k + 1}"]
            return o
        ops.append(Op("group", grp, req_fn(gi), None, ntiles * 4 * nk))
    for ti in range(ntiles):
        for r0 in range(0, 16, XR):
            def dr(regs, ti=ti, r0=r0):
                if zreg == "s3":       # h [half][160][32]: row stride 128
                    off = zoff + ((ti // 2) * 16 + r0) * 128 + (ti % 2) * 64
                else:                  # z [64][64]: row stride 256
                    off = zoff + ((ti // 2) * 16 + r0) * 256 + (ti % 2) * 64
                o = [f"li a3, {TS[ti] + r0}", f"li a4, {off}", f"add a2, {zreg}, a4"] if r0 == 0 else []
                for _ in range(XR):
                    o += ["vtse32 a3, (a2)", "addi a3, a3, 1", f"add a2, a2, {zstride_reg}"]
                return o
            ops.append(Op("xfer", dr))
    ops[-1].prov = prov
    return ops


Q = []
for hf in (0, 1):
    for rb in range(HP // 32):
        Q += block_ops(hf, rb, "s6", KG1 * 256, "s4", KP1, KG1, "s3", hf * HP * 128 + rb * 32 * 128, "s1", False,
                       lambda gi: ("ln", min(2 * gi + 2, C // 2 - 1)), ("h", hf, rb),
                       ntiles=4 if (rb + 1) * 32 <= H else 2)
TXR = A.txr
def xchunk(kind, ti, r0, zoff, n=None):
    """stand-alone transfer chunk (own pointer prologue) for z [64][64] (row stride 256 = t1)"""
    n = TXR if n is None else n
    def f(regs, ti=ti, r0=r0):
        o = [f"li a3, {TS[ti] + r0}", f"li a4, {zoff + ((ti // 2) * 16 + r0) * 256 + (ti % 2) * 64}", "add a2, s2, a4"]
        for _ in range(n):
            o += [f"{'vtle32' if kind == 'sd' else 'vtse32'} a3, (a2)", "addi a3, a3, 1", "add a2, a2, t1"]
        return o
    return Op("xfer", f)


def half_groups(hf, rb, upper):
    """GeMM2 K-groups of one 2-tile half-block (upper: mt0/mt4 rows 0-15, else mt8/mt12 rows 16-31)"""
    ops = [Op("init", lambda regs: [f"li a4, {rb * KG2 * 256}", "add a0, s7, a4", f"li a4, {hf * KP2 * 128}",
                                     "add a1, s5, a4"])]
    for gi in range(KG2):
        nk = min(2, KP2 - 2 * gi)
        def grp(regs, nk=nk, part="all"):
            a, bb = regs
            if part == "load":
                return [rr(a, "a0", "t1"), rr(bb, "a1", "t1")]
            o = [VM64, rr(a, "a0", "t1"), rr(bb, "a1", "t1"), *MM] if part == "all" else []
            for k in range(nk):
                ar = a + k if upper else a + 2 + k
                t0, t1_ = ("mt0", "mt4") if upper else ("mt8", "mt12")
                o += [f"vtfmm.tvv {t0}, v{ar}, v{bb + 2 * k}", f"vtfmm.tvv {t1_}, v{ar}, v{bb + 2 * k + 1}"]
            return o
        ops.append(Op("group", grp, ("gelu", hf, min(gi, H // 4 - 1)), None, 8 * nk))
    return ops


def interleave(groups, xfers):
    out = [groups[0]]
    for g in groups[1:]:
        out.append(g)
        if xfers:
            out.append(xfers.pop(0))
    return out + xfers


for hf in (0, 1):
    for rb in range(C // 32):
        if A.tail and hf == 1:
            continue
        Q += block_ops(hf, rb, "s7", KG2 * 256, "s5", KP2, KG2, "s2", 128 * hf + rb * 32 * 256, "t1", True,
                       lambda gi, hf=hf: ("gelu", hf, min(gi, H // 4 - 1)), ("z", hf, rb))
if A.tail:
    # GeMM2(B): block 0 tracks GELU(B) as a full 2x2 block; block 1 runs after it as two 2-tile halves, so that the
    # transfers (block 0 lower drain, half seeds/drains) overlap with the MACs of the other half.
    z0, z1 = 128, 128 + 32 * 256
    b0 = block_ops(1, 0, "s7", KG2 * 256, "s5", KP2, KG2, "s2", z0, "t1", True,
                   lambda gi: ("gelu", 1, min(gi, H // 4 - 1)), None)
    Q += b0[:-2 * (16 // XR)]                                  # without the drains of tiles 2, 3
    rows = range(0, 16, TXR)
    Q += [Op("init", lambda regs: [])] + [xchunk("sd", ti, r0, z1) for ti in (0, 1) for r0 in rows]
    L1 = [xchunk("dr", ti, r0, z0) for ti in (2, 3) for r0 in rows] + [xchunk("sd", ti, r0, z1) for ti in (2, 3) for r0 in rows]
    Q += interleave(half_groups(1, 1, True), L1)
    L2 = [xchunk("dr", ti, r0, z1) for ti in (0, 1) for r0 in rows]
    Q += interleave(half_groups(1, 1, False), L2)
    Q += [xchunk("dr", ti, r0, z1) for ti in (2, 3) for r0 in rows]
    Q[-1].prov = ("z", 1, 1)
done = set()
qi = [0]


def ope_ready():
    if qi[0] >= len(Q):
        return False
    r = Q[qi[0]].req
    return r is None or r in done


pending = [None]          # a K-group whose operands are loaded and whose vtfmm are not issued yet
cool = [0]                # slots to wait after vtfmm before the next operand load (WAR on the window registers)


def emit_xfer_only(n):
    body = []
    for _ in range(n):
        if not (pending[0] is None and ope_ready() and Q[qi[0]].kind == "xfer"):
            break
        op = Q[qi[0]]; body += op.render(None)
        if op.prov:
            done.add(op.prov)
        qi[0] += 1
    if body:
        e(*MM, *body, VM64)


def emit_ope(regs, n=1, groups_only=False, split=False, xfers=None):
    xfers = A.xfers if xfers is None else xfers
    """emit up to n OPE chunks. split=True (inside VFU windows): a K-group's operand loads go into one slot (under
    the VFU config, no switch) and its vtfmm into a later slot, together with up to `xfers` tile-transfer chunks,
    so a slot costs at most one matrix-config switch and the OPE never waits on its operands."""
    if not split:
        cnt = 0
        while cnt < n and (pending[0] is not None or ope_ready()):
            if pending[0] is not None:
                e(*MM, *pending[0].render(regs, part="macs")); pending[0] = None; cnt += 1; continue
            op = Q[qi[0]]
            e(*MM, *op.render(regs))
            if op.prov:
                done.add(op.prov)
            qi[0] += 1; cnt += 1
        return cnt
    body = []
    if pending[0] is not None:
        body += pending[0].render(regs, part="macs"); pending[0] = None; cool[0] = 1
    nx = 0
    sx = A.sxfers if A.sxfers >= 0 else xfers
    while nx < sx and ope_ready() and Q[qi[0]].kind != "group":
        op = Q[qi[0]]
        body += op.render(regs)
        if op.prov:
            done.add(op.prov)
        qi[0] += 1; nx += 1
    if body:
        e(*MM, *body, VM64)
        return 1
    if ope_ready() and Q[qi[0]].kind == "group":
        if cool[0] > 0:
            cool[0] -= 1
            return 0
        e(*Q[qi[0]].render(regs, part="load"))
        pending[0] = Q[qi[0]]; qi[0] += 1
        return 1
    return 0


def flush_pending(regs):
    if pending[0] is not None:
        e(*MM, *pending[0].render(regs, part="macs"))
        pending[0] = None


# ------------------------------------------------------------------ program ---------------------------------------
SAVED = [f"s{i}" for i in range(12)]
e("addi sp, sp, -64", *[f"sw {r}, {4 * i}(sp)" for i, r in enumerate(SAVED)],
  "mv s6, a0", "mv s7, a1", "mv s2, a2", "mv s3, a3", "mv s4, a4", "mv s5, a5",
  "li t1, 256", "li s1, 128", "li s8, 256", "li s0, 64", "li s11, 32", "li t2, 4", "li t4, 16450", "li t5, 0xC8",
  "li t6, 16", "li s9, 0x7EF311C7", "li s10, 0x5F3759DF")
for v, r in FC.items():
    e(f"li t3, 0x{fbits(v):08x}", f"fmv.w.x {r}, t3")
e(*MM, "msettm zero, t6")

# ---- 1. LayerNorm over 64 tokens (vl = 64) ----
R0 = [8, 12, 16, 20, 24, 28]
e(VM64, "mv a5, s2", "vmv.v.i v0, 0", "vmv.v.i v4, 0")
for r in range(4):
    e(rr(R0[r], "a5", "s8"))
for r in range(C):
    e(f"vfadd.vv v{4 * (r % 2)}, v{4 * (r % 2)}, v{R0[r % 6]}")
    if r + 4 < C:
        e(rr(R0[(r + 4) % 6], "a5", "s8"))
e("vfadd.vv v0, v0, v4", f"vfmul.vf v0, v0, {freg(1.0 / C)}")                       # M = v0
P2 = [(12, 16), (20, 24)]
e("mv a5, s2", "vmv.v.i v4, 0", "vmv.v.i v8, 0", rr(12, "a5", "s8"), rr(16, "a5", "s8"))
for p in range(C // 2):
    xa, xb = P2[p % 2]
    e(f"vfsub.vv v{xa}, v{xa}, v0", f"vfsub.vv v{xb}, v{xb}, v0")
    if p + 1 < C // 2:
        na, nb = P2[(p + 1) % 2]
        e(rr(na, "a5", "s8"), rr(nb, "a5", "s8"))
    e(f"vfmul.vv v{xa}, v{xa}, v{xa}", f"vfmul.vv v{xb}, v{xb}, v{xb}", f"vfadd.vv v4, v4, v{xa}", f"vfadd.vv v8, v8, v{xb}")
e("vfadd.vv v4, v4, v8", f"vfmul.vf v4, v4, {freg(1.0 / C)}", f"vfadd.vf v4, v4, {freg(1e-5)}")
e("vsrl.vi v8, v4, 1", "vrsub.vx v8, v8, s10", f"vfmul.vf v16, v4, {freg(0.5)}")
for _ in range(3):
    e("vfmul.vv v12, v8, v8", "vfmul.vv v12, v12, v16", f"vfrsub.vf v12, v12, {freg(1.5)}", "vfmul.vv v8, v8, v12")
e("vmv.v.v v4, v8")                                                                  # R = v4
mark("ln_p3")
# pass 3: double-buffered pairs (v8, v12) / (v16, v28), narrowed in place; GeMM1(A) block 0 operands in v20 / v24
if A.p3 == 1:
    P3 = [(8, 12), (16, 28)]
    e(VM64, "mv a5, s2", "mv a6, s4", rr(8, "a5", "s8"), rr(12, "a5", "s8"))
    for p in range(C // 2):
        xa, xb = P3[p % 2]
        e(f"vfsub.vv v{xa}, v{xa}, v0", f"vfsub.vv v{xb}, v{xb}, v0")
        if p + 1 < C // 2:
            na, nb = P3[(p + 1) % 2]
            e(rr(na, "a5", "s8"), rr(nb, "a5", "s8"))
        e(f"vfmul.vv v{xa}, v{xa}, v4", f"vfmul.vv v{xb}, v{xb}, v4")
        if p >= 3:
            emit_ope((20, 24), 1, split=True)
        e(V16_64, f"vfncvt.f.f.w v{xa}, v{xa}", f"vfncvt.f.f.w v{xb}, v{xb}",
          V16_32, f"vsse16.v v{xa}, (a6), t2", f"li t3, {KP1 * 128}", "add a7, a6, t3", f"vsse16.v v{xa + 1}, (a7), t2",
          "addi a7, a7, 2", f"vsse16.v v{xb + 1}, (a7), t2", "addi a7, a6, 2", f"vsse16.v v{xb}, (a7), t2",
          "addi a6, a6, 128", VM64)
        done.add(("ln", p))
        if p >= 3:
            emit_ope((20, 24), 1, split=True)
    flush_pending((20, 24))
else:
  e("mv a5, s2", "mv a6, s4")
  for p in range(C // 2):
    e(VM64, rr(8, "a5", "s8"), rr(12, "a5", "s8"), "vfsub.vv v8, v8, v0", "vfsub.vv v12, v12, v0",
      "vfmul.vv v8, v8, v4", "vfmul.vv v12, v12, v4", V16_64, "vfncvt.f.f.w v16, v8", "vfncvt.f.f.w v18, v12",
      V16_32, "vsse16.v v16, (a6), t2", f"li t3, {KP1 * 128}", "add a7, a6, t3", "vsse16.v v17, (a7), t2",
      "addi a7, a7, 2", "vsse16.v v19, (a7), t2", "addi a7, a6, 2", "vsse16.v v18, (a7), t2",
      "addi a6, a6, 128")
    done.add(("ln", p))
    if p >= 3:
        emit_ope((20, 24), 1)
mark("ln_end")
# ---- 2/3. GELU per half, OPE K-groups in the T3 windows ----
def gm(s):
    if s % 2 == 0:
        return {"v0": 0, "v4": 4, "v8": 8, "v12": 12, "v16": 16, "v20": 20, "v24": 24, "v28": 28}
    return {"v0": 4, "v4": 0, "v8": 8, "v12": 12, "v16": 20, "v20": 16, "v24": 24, "v28": 28}
OPE_REGS = (12, 28)
NS = H // 4
for hf in (0, 1):
    # rows of step s: 4s..4s+3 of half hf (two vectors of 2 rows x 32 tokens), block of row r = r // 32
    def need(s, hf=hf):
        return ("h", hf, min((4 * s + 3) // 32 + 0, HP // 32 - 1))
    while need(0) not in done or need(1) not in done:
        if not emit_ope(OPE_REGS, 1):
            if qi[0] < len(Q) and Q[qi[0]].kind != "group":
                pass
            if not ope_ready():
                raise RuntimeError(f"stuck before GELU {hf}: {Q[qi[0]].req if qi[0] < len(Q) else None}")
    mark(f"gelu{hf}")
    e(VM64, f"li t3, {hf * HP * 128}", "add a5, s3, t3", f"li t3, {hf * KP2 * 128}", "add a6, s5, t3",
      rr(0, "a5", "s8"), rr(16, "a5", "s8"))
    ipu_op = None
    for s in range(NS):
        m, nm = gm(s), gm(s + 1)
        # data for the prefetch of step s+1 must be drained (margin: GeMM1 runs ahead)
        while s + 1 < NS and need(s + 1 + A.margin if s + 1 + A.margin < NS else NS - 1) not in done and ope_ready():
            emit_ope(OPE_REGS, 1)
            e(VM64)
        if s + 1 < NS and need(s + 1) not in done:
            raise RuntimeError(f"GELU {hf} step {s + 1} data not produced")
        for i, (mn, ops, kind, val) in enumerate(GOPS):
            o = [f"v{m[q]}" if q in m else q for q in ops]
            o = [(freg(float(val.rstrip('f'))) if kind == "f" else ICONST[int(val.rstrip('uU'), 0)]) if q == "%0" else q
                 for q in o]
            e(f"{mn} {', '.join(o)}")
            if i + 1 in WIN:
                emit_ope(OPE_REGS, A.per, split=True)
            elif i + 1 in XWIN:
                emit_xfer_only(A.xfers)
            if i + 1 == WIN[-1] + 2 and pending[0] is not None:
                flush_pending(OPE_REGS)        # nothing may live in the T3 registers at op 53
                e(VM64)
            if A.ipu and i + 1 == 41 and pending[0] is None and ope_ready() and Q[qi[0]].kind == "group":
                # T2 (v8-v11, v24-v27) is dead from op 41 to op 54: load a K-group's operands now ...
                ipu_op = Q[qi[0]]; qi[0] += 1
                e(*ipu_op.render((8, 24), part="load"))
            elif A.ipu and i + 1 == 41:
                ipu_op = None
            if A.ipu and i + 1 == 54 and ipu_op is not None:
                # ... and issue its vtfmm right after the two vrsub (integer unit, operand ports mostly idle)
                e(*MM, *ipu_op.render((8, 24), part="macs"), VM64)
                ipu_op = None
            if i + 1 == LOADAT and s + 1 < NS:
                e(rr(nm["v0"], "a5", "s8"), rr(nm["v16"], "a5", "s8"))
        xa, xb = m["v0"], m["v16"]
        # narrow both vectors; vector a = rows 4s, 4s+1 -> K-pair 2s (lo, hi), vector b -> K-pair 2s+1
        e(V16_64, f"vfncvt.f.f.w v8, v{xa}", f"vfncvt.f.f.w v24, v{xb}", V16_32,
          "vsse16.v v8, (a6), t2", "addi a7, a6, 2", "vsse16.v v9, (a7), t2", "addi a7, a6, 128",
          "vsse16.v v24, (a7), t2", "addi a7, a6, 130", "vsse16.v v25, (a7), t2", "addi a6, a6, 256", VM64)
        done.add(("gelu", hf, s))
mark("tail")
# ---- 4. tail: remaining OPE work ----
flush_pending(OPE_REGS)
TSETS = [[(0, 16), (4, 20)], [(8, 24), (12, 28)]]
cfg = "V"
bi = 0
while qi[0] < len(Q):
    op = Q[qi[0]]
    if op.kind == "group":
        groups, xf = [], []
        while qi[0] < len(Q) and len(groups) < A.tbatch and Q[qi[0]].kind in ("group", "xfer"):
            (groups if Q[qi[0]].kind == "group" else xf).append(Q[qi[0]])
            qi[0] += 1
        rs = TSETS[bi % 2] if A.tbatch <= 2 else [(0, 16), (4, 20), (8, 24), (12, 28)][:A.tbatch]
        bi += 1
        e(VM64)
        for g, r in zip(groups, rs):
            e(*g.render(r, part="load"))
        e(*MM)
        for g, r in zip(groups, rs):
            e(*g.render(r, part="macs"))
        cfg = "M"
        for x in xf:
            e(*x.render(None))
    else:
        if op.kind == "xfer" and cfg == "V":
            e(*MM); cfg = "M"
        e(*op.render(None))
        qi[0] += 1
e("fence rw, rw", "lw t3, 0(s2)", *[f"lw {r}, {4 * i}(sp)" for i, r in enumerate(SAVED)], "addi sp, sp, 64", "ret")

mark("end")
bodyasm = "\n".join(f'    "{x}\\n"' for x in L)
src = ("// GENERATED by script/gen_mlp_block_fused.py -- fused P9 (see the generator for the schedule)\n#include <stdint.h>\n"
       "void p9_fused(const uint32_t *a1, const uint32_t *a2, float *z, float *h, uint32_t *lnpk, uint32_t *hpk);\n"
       '__asm__(\n    ".text\\n"\n    ".balign 64\\n"\n    ".globl p9_fused\\n"\n    ".type p9_fused, @function\\n"\n'
       f'    "p9_fused:\\n"\n{bodyasm}\n    ".size p9_fused, .-p9_fused\\n");\n')
pathlib.Path(A.kernel).write_text(src)
print(f"p9 fused: {len(L)} instructions, OPE chunks {len(Q)}")
