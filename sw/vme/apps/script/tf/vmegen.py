#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
"""Generic staggered (ping-pong) VME GeMM generator: Z[M][N] = Y + A^T B on Spatz + OPE (TE16, CE8, VLEN512).

Words are packed FP16 pairs {K even (hi), K odd (lo)}. Shapes (a block is always two 16x16 tiles):
  "2x1" : 32 rows x 16 cols. A stream [M/32][KG][2][4][16], B panels [N/16][KP][16].
  "1x2h": 16 rows x 32 cols. A stream [M/16][KG][1][4][16], B row-major packed [KP][N] (O-POPE layout,
          produced on chip); per K-pair a half row (32 words = both column tiles) is loaded (e32, vl=32).
  "1x2c": as 1x2h, but B is stored column-half-major [N/32][KP][32]: the 4 K-pairs of a group are 128 contiguous
          words (two e32,m4 loads, no vl switch).
Z is FP32 row-major (row stride N*4). Blocks alternate the tile pairs P0 = (mt0, mt4) / P1 = (mt8, mt12): while
one pair accumulates, the other one (previous block) is drained with vtse32 and optionally seeded with vtle32
from a Y matrix (same layout as Z) for its next block. drain="both": one row of each tile per K-group
(KG >= 16); drain="alt": one row per K-group alternating the tiles (KG >= 32, needed with seeding: the tile
path sustains about two tile transfers per 32 OPE cycles). Bias: one extra K-pair per block ({hi, lo} x {1, 1})
from bias vectors [rowgroup][X 16 | Y 16 | {1,1} x 16 | 0 x 16].
slot(block, group) -> (instrs under e32,m4, instrs under matrix config[, instrs at the start of the group, before
its first vtfmm: scalar work placed there does not wait for in-flight operand loads]) injects other work.

Registers used: a0..a6, s0..s11 (saved), t1..t6. Free for the caller's injected streams: a7, t0, ft*, and the
vector registers v12-v15 / v28-v31 (v28-v30 hold the bias operands when bias=True).
"""

GPR = {"zero": 0, "ra": 1, "sp": 2, "gp": 3, "tp": 4, "t0": 5, "t1": 6, "t2": 7, "s0": 8, "s1": 9,
       **{f"a{i}": 10 + i for i in range(8)}, **{f"s{i}": 16 + i for i in range(2, 12)},
       "t3": 28, "t4": 29, "t5": 30, "t6": 31}


def rr(vd, rs1, rs2):
    """p.vle32.v.rrpost vd, (rs1), rs2 (no toolchain mnemonic; see hw/ip/snitch/src/riscv_instr.sv)"""
    w = (0b110000 << 26) | (1 << 25) | (GPR[rs2] << 20) | (GPR[rs1] << 15) | (0b110 << 12) | (vd << 7) | 0x7B
    return f".word 0x{w:08x} /* p.vle32.v.rrpost v{vd}, ({rs1}), {rs2} */"


MMODE = ["msetmtype t4, t5", "msettn zero, t6"]
VMODE = "vsetvli zero, s0, e32, m4, ta, ma"
V32 = "vsetvli zero, s1, e32, m2, ta, ma"           # s1 = 32
PAIRS = [("mt0", "mt4", 0x00000000, 0x20000000), ("mt8", "mt12", 0x40000000, 0x60000000)]
SETS2 = [(0, 4, 8), (16, 20, 24)]                     # 2x1: A0, A1, B
SETSH = [(0, 4), (16, 20)]                            # 1x2h: A (4 regs), B (8 regs: B0/B1 of 4 K-pairs)


class Gemm:
    def __init__(self, M, N, KP, shape="2x1", order=None, seed=False, bias=False, loadpos=4, drain="both",
                 tag="g", zero=True, kp_fn=None, seed_blocks=None):
        self.M, self.N, self.KP, self.shape = M, N, KP, shape
        self.KG = -(-KP // 4)
        self.lastk = KP - 4 * (self.KG - 1)
        rows, cols = (32, 16) if shape == "2x1" else (16, 32)
        self.contig = shape == "1x2c"
        assert M % rows == 0 and N % cols == 0
        self.RB, self.CB = M // rows, N // cols
        self.order = order or [(rb, cb) for rb in range(self.RB) for cb in range(self.CB)]
        self.seed, self.bias, self.loadpos, self.drain, self.tag, self.zero = seed, bias, loadpos, drain, tag, zero
        assert drain in ("both", "alt")
        assert not (seed and drain != "alt" and seed_blocks is None), "seeding all blocks needs drain='alt'"
        assert self.KG >= (32 if drain == "alt" else 16) or True
        self.kp_fn = kp_fn or (lambda blk: KP)          # K-pairs actually reduced for a block (causal truncation)
        self.seed_blocks = seed_blocks                    # None: all blocks (if seed); else the set of seeded blocks
        self.L = []
        self.rowbytes = N * 4
        self.ablock = self.KG * (512 if shape == "2x1" else 256)
        assert not (shape == "1x2c" and KP % 4), "1x2c needs whole K-groups"
        self.gctr = 0

    def e(self, *x):
        self.L.extend(x)

    def zbase(self, blk):
        rb, cb = blk
        return (rb * 32 * self.rowbytes + cb * 64) if self.shape == "2x1" else (rb * 16 * self.rowbytes + cb * 128)

    def ydrain(self):
        return 16 * self.rowbytes if self.shape == "2x1" else 64

    def set_ops(self, blk):
        o = [f"li t3, {blk[0] * self.ablock}", "add a0, s11, t3"]
        if self.shape == "2x1":
            o += ["addi a1, a0, 256", f"li t3, {blk[1] * self.KP * 64}", "add a2, s10, t3"]
        elif self.contig:
            o += [f"li t3, {blk[1] * self.KP * 128}", "add a2, s10, t3"]   # B column half [cb][KP][32]
        else:
            o += [f"li t3, {blk[1] * 128}", "add a2, s10, t3"]     # B half-row base, stride N*4 per K-pair
        return o

    def loads(self, nxt_set):
        if self.shape == "2x1":
            n0, n1, nb = nxt_set
            return [VMODE, rr(n0, "a0", "t1"), rr(n1, "a1", "t1"), rr(nb, "a2", "t2")]
        na, nb = nxt_set
        if self.contig:
            return [VMODE, rr(na, "a0", "t1"), rr(nb, "a2", "t2"), rr(nb + 4, "a2", "t2")]
        return [VMODE, rr(na, "a0", "t1"), V32] + [rr(nb + 2 * k, "a2", "s9") for k in range(4)]

    def macs(self, cx, cy, cur, nk):
        m = []
        for k in range(nk):
            if self.shape == "2x1":
                a0, a1, b = cur
                m += [f"vtfmm.tvv {cx}, v{a0+k}, v{b+k}", f"vtfmm.tvv {cy}, v{a1+k}, v{b+k}"]
            else:
                a, b = cur
                m += [f"vtfmm.tvv {cx}, v{a+k}, v{b+2*k}", f"vtfmm.tvv {cy}, v{a+k}, v{b+2*k+1}"]
        return m

    def seeded(self, blk):
        return self.seed and blk is not None and (self.seed_blocks is None or blk in self.seed_blocks)

    def drains_for(self, g, KG=None, seed_next=None):
        """tile-row transfers of the idle pair issued in K-group g of a block (16 rows of each tile per block,
        spread evenly over the K-groups)"""
        KG = KG or self.KG
        sd = self.seed if seed_next is None else seed_next
        X = ["vtse32 a5, (a3)"] + (["vtle32 a5, (s3)", "add s3, s3, s9"] if sd else []) + \
            ["addi a5, a5, 1", "add a3, a3, s9"]
        Y = ["vtse32 a6, (a4)"] + (["vtle32 a6, (s6)", "add s6, s6, s9"] if sd else []) + \
            ["addi a6, a6, 1", "add a4, a4, s9"]
        seq = [X, Y] * 16                       # X0 Y0 X1 Y1 ... (alt and both use the same order)
        lo, hi = (32 * g) // KG, (32 * (g + 1)) // KG
        return seq[lo:hi]

    def emit(self, slot=None, tail_hook=None, pre_hook=None, name="vme_gemm", drain_last=True, final_sync=True,
             seed_hook=None):
        slot = slot or (lambda b, g: ((), ()))
        e = self.e
        SAVED = [f"s{i}" for i in range(12)]
        sets = SETS2 if self.shape == "2x1" else SETSH
        # args: a0 = A stream, a1 = B, a2 = Z, a3 = Y (seed source) / bias vectors
        e("addi sp, sp, -64", *[f"sw {r}, {4*i}(sp)" for i, r in enumerate(SAVED)],
          "mv s2, a2", "mv s10, a1", "mv s11, a0", "mv s8, a3",
          f"li t1, {512 if self.shape == '2x1' else 256}", "li t2, 256", "li s0, 64", "li s1, 32",
          f"li s9, {self.rowbytes}", f"li s4, {self.ydrain()}",
          "li t4, 16450", "li t5, 0xC8", "li t6, 16")
        if pre_hook:
            pre_hook(self)
        nb = len(self.order)
        cx0, cy0, tx0, ty0 = PAIRS[0]
        if seed_hook is not None and self.seeded(self.order[0]):
            # block 0 seeded first (unrolled), seed_hook(i) placing other scalar work between the rows, then the
            # first operand loads (the hook may produce data they read)
            e(*MMODE, "msettm zero, t6", f"li t3, {self.zbase(self.order[0])}", "add t0, s8, t3", f"li a5, {tx0}",
              f"li a6, {ty0}")
            for i in range(16):
                e("vtle32 a5, (t0)", "add a7, t0, s4", "vtle32 a6, (a7)", "addi a5, a5, 1", "addi a6, a6, 1",
                  "add t0, t0, s9", *seed_hook(i))
        e(*self.set_ops(self.order[0]), *self.loads(sets[0])[0:])
        if self.bias:
            e(VMODE, f"li t3, {self.order[0][0] * 256}", "add t3, s8, t3", "vle32.v v28, (t3)")
        e(*MMODE, "msettm zero, t6")
        if seed_hook is not None and self.seeded(self.order[0]):
            pass
        elif self.seeded(self.order[0]):     # block 0 tiles from Y (exposed); block 1 is seeded during block 0
            e(f"li t3, {self.zbase(self.order[0])}", "add t0, s8, t3", f"li a5, {tx0}", f"li a6, {ty0}",
              "li s7, 16", f".Lps_{self.tag}:", "vtle32 a5, (t0)", "add a7, t0, s4", "vtle32 a6, (a7)",
              "addi a5, a5, 1", "addi a6, a6, 1", "add t0, t0, s9", "addi s7, s7, -1", f"bnez s7, .Lps_{self.tag}")
        elif self.zero:
            e(f"vtzero {cx0}", f"vtzero {cy0}")
        # block -1 does not exist: block 0 drains the idle pair into the Y / Z region of block 0 (rewritten)
        e(f"vtzero {PAIRS[1][0]}", f"vtzero {PAIRS[1][1]}", f"li t3, {self.zbase(self.order[0])}",
          "add a3, s2, t3", "add a4, a3, s4", f"li a5, {PAIRS[1][2]}", f"li a6, {PAIRS[1][3]}")
        if self.seed:
            nn = self.order[1] if nb > 1 else self.order[0]
            e(f"li t3, {self.zbase(nn)}", "add s3, s8, t3", "add s6, s3, s4")
        for j, blk in enumerate(self.order):
            cx, cy, tx, ty = PAIRS[j % 2]
            nxt = self.order[j + 1] if j + 1 < nb else None
            if (j > 0 or not self.seeded(blk)) and not self.seeded(blk) and self.zero:
                if j > 0 or not self.seed:
                    e(f"vtzero {cx}", f"vtzero {cy}")
            if self.bias:
                e(f"vtfmm.tvv {cx}, v28, v30", f"vtfmm.tvv {cy}, v29, v30")
            kp = self.kp_fn(blk)
            KGb = -(-kp // 4)
            for g in range(KGb):
                cur, nx = sets[self.gctr % 2], sets[(self.gctr + 1) % 2]
                self.gctr += 1
                last = g == KGb - 1
                nk = kp - 4 * (KGb - 1) if last else 4
                sl = slot(j, g)
                ev, em = list(sl[0]), list(sl[1])
                eg = list(sl[2]) if len(sl) > 2 else []
                if last and nxt is not None:
                    e(*self.set_ops(nxt))
                    if self.bias:
                        ev += [f"li t3, {nxt[0] * 256}", "add t3, s8, t3", "vle32.v v28, (t3)"]
                ld = (self.loads(nx) if (nxt is not None or not last) else [])
                if ev and self.shape == "1x2h" and ld:
                    ld = ld + [VMODE]
                ld = ld + ev
                ld = ld + (MMODE if ld else []) + em
                ms = self.macs(cx, cy, cur, nk)
                dr = self.drains_for(g, KGb, self.seeded(nxt))
                lp = min(self.loadpos, len(ms))
                # spread the tile transfers over the vtfmm of the group (after vtfmm 1, then every 8/n)
                nd = len(dr)
                dpos = [1 + (len(ms) * i) // max(nd, 1) for i in range(nd)]
                done = 0
                e(*eg)
                for i, m in enumerate(ms):
                    if i == lp:
                        e(*ld)
                    e(m)
                    while done < nd and dpos[done] == i + 1:
                        e(*dr[done]); done += 1
                if lp >= len(ms):
                    e(*ld)
                while done < nd:
                    e(*dr[done]); done += 1
            if nxt is not None:
                e(f"li t3, {self.zbase(blk)}", "add a3, s2, t3", "add a4, a3, s4", f"li a5, {tx}", f"li a6, {ty}")
                if self.seed:
                    nn = self.order[j + 2] if j + 2 < nb else None
                    if nn is None:
                        e("mv s3, s8", "add s6, s8, s4")          # harmless reads (no block after)
                    else:
                        e(f"li t3, {self.zbase(nn)}", "add s3, s8, t3", "add s6, s3, s4")
        cx, cy, tx, ty = PAIRS[(nb - 1) % 2]
        e(f"li t3, {self.zbase(self.order[-1])}", "add a3, s2, t3", "add a4, a3, s4", f"li a5, {tx}", f"li a6, {ty}")
        if tail_hook:
            tail_hook(self)
        elif drain_last:
            e("li s7, 16", f".Ltd_{self.tag}:", "vtse32 a5, (a3)", "addi a5, a5, 1", "add a3, a3, s9",
              "vtse32 a6, (a4)", "addi a6, a6, 1", "add a4, a4, s9", "addi s7, s7, -1", f"bnez s7, .Ltd_{self.tag}")
        if final_sync:
            e("fence rw, rw", "lw t3, 0(s2)")
        e(*[f"lw {r}, {4*i}(sp)" for i, r in enumerate(SAVED)], "addi sp, sp, 64", "ret")
        body = "\n".join(f'    "{x}\\n"' for x in self.L)
        return (f'__asm__(\n    ".text\\n"\n    ".balign 64\\n"\n    ".globl {name}\\n"\n'
                f'    ".type {name}, @function\\n"\n    "{name}:\\n"\n{body}\n    ".size {name}, .-{name}\\n");\n')


# ---- host-side packing helpers (numpy) ----------------------------------------------------------------------
def _words(m16, swap=False):
    """swap=False: {K even (hi), K odd (lo)} (VME convention); swap=True: O-POPE order {K even (lo), K odd (hi)}.
    The FP16-pair dot product only needs A and B to use the same order."""
    import numpy as np
    b = np.asarray(m16, np.float16).view(np.uint16).astype(np.uint32)
    return (b[1::2] << 16) | b[0::2] if swap else (b[0::2] << 16) | b[1::2]


def pack_a(w16, shape="2x1", swap=False):
    """W[K][M] (FP16) -> A stream for the given shape (last K-group zero-padded to 4 K-pairs)"""
    import numpy as np
    words = _words(w16, swap)
    KP, M = words.shape
    KG = -(-KP // 4)
    pad = np.zeros((KG * 4, M), np.uint32)
    pad[:KP] = words
    tiles = 2 if shape == "2x1" else 1
    t = pad.reshape(KG, 4, M // (16 * tiles), tiles, 16).transpose(2, 0, 3, 1, 4)
    return t.ravel()


def pack_b_panels(x16):
    """X[K][N] (FP16) -> B panels [N/16][KP][16] (2x1 shape)"""
    words = _words(x16)
    KP, N = words.shape
    return words.reshape(KP, N // 16, 16).transpose(1, 0, 2).ravel()


def pack_b_rows(x16):
    """X[K][N] (FP16) -> row-major packed [KP][N] (1x2h shape, O-POPE layout but VME half order)"""
    return _words(x16).ravel()


def bias_vectors(bias, shape="2x1"):
    """bias[M] (FP32) -> [rowgroup][X 16 | Y 16 | {1,1} x 16 | 0 x 16] words, {hi, lo} = fp16 split"""
    import numpy as np
    b = np.asarray(bias, np.float32)
    hi = b.astype(np.float16)
    lo = (b.astype(np.float64) - hi.astype(np.float64)).astype(np.float16)
    w = (hi.view(np.uint16).astype(np.uint32) << 16) | lo.view(np.uint16).astype(np.uint32)
    if shape == "2x1":
        xy = w.reshape(-1, 32)
    else:
        xy = np.concatenate([w.reshape(-1, 16), w.reshape(-1, 16)], axis=1)
    n = xy.shape[0]
    return np.concatenate([xy, np.full((n, 16), 0x3c003c00, np.uint32), np.zeros((n, 16), np.uint32)], axis=1).ravel()
