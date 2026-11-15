#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
"""Shared pieces of the fused VME kernel generators (gemm_gelu.py, gen_p3f.py, ...).

* GELU: the O-POPE TF16_GELU2 op sequence parsed from opope_tf16_pairs.h, with one exact rewrite: `vfmv.v.f vX, c`
  followed by `vfmul.vv vX, vX, vY` becomes `vfmul.vf vX, vY, c` (bit-identical, two VFU ops less per step).
  Two vectors per step (roles v0 / v16 as in O-POPE); per step the x registers swap with v4 / v20 so the next step
  is loaded while the current one finishes. Register windows: T3 (v12-v15, v28-v31) is dead from the end of a step
  to its first write in the next one: OPE K-groups use it.
* OPE work queue: chunks (init / group / xfer) consumed in order; inside VFU code a K-group's operand loads go into
  one slot and its vtfmm into a later one; tile transfers in 2-row chunks (longer scalar bursts starve the VFU).
  A vtfmm must be issued >= 13 VFU ops before a VFU write of its operand registers (no WAR protection in HW).
* Batched OPE-only emission (no VFU work around): two K-groups per vector/matrix config switch.
"""
import re
import struct

GPR = {"zero": 0, "ra": 1, "sp": 2, "t0": 5, "t1": 6, "t2": 7, "s0": 8, "s1": 9, **{f"a{i}": 10 + i for i in range(8)},
       **{f"s{i}": 16 + i for i in range(2, 12)}, "t3": 28, "t4": 29, "t5": 30, "t6": 31}


def rr(vd, rs1, rs2):
    """p.vle32.v.rrpost vd, (rs1), rs2 (encoding only; no toolchain mnemonic)"""
    w = (0b110000 << 26) | (1 << 25) | (GPR[rs2] << 20) | (GPR[rs1] << 15) | (0b110 << 12) | (vd << 7) | 0x7B
    return f".word 0x{w:08x} /* p.vle32.v.rrpost v{vd}, ({rs1}), {rs2} */"


def fbits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


VM64 = "vsetvli zero, s0, e32, m4, ta, ma"          # s0 = 64
MM = ["msetmtype t4, t5", "msettn zero, t6"]        # t4 = 16450, t5 = 0xC8, t6 = 16
TS = [0x00000000, 0x20000000, 0x40000000, 0x60000000]
TILES = ["mt0", "mt4", "mt8", "mt12"]


def gelu_ops(header):
    txt = open(header).read()
    body = txt[txt.index("#define TF16_GELU2()"):]
    body = body[:body.index("while (0)")]
    ops = []
    for m in re.finditer(r'asm volatile\("([^"]*)"\s*(?:::\s*"([fr])"\(([^)]*)\))?\)', body):
        ins, kind, val = m.group(1), m.group(2), m.group(3)
        mn, o = ins.split(None, 1)
        ops.append([mn, [x.strip() for x in o.split(",")], kind, val])
    assert len(ops) == 68
    # exact rewrite: vfmv.v.f vX, c ; ... ; vfmul.vv vX, vX, vY  ->  vfmul.vf vX, vY, c
    out, skip = [], set()
    for i, op in enumerate(ops):
        if i in skip:
            continue
        if op[0] == "vfmv.v.f":
            x = op[1][0]
            j = next(j for j in range(i + 1, len(ops)) if x in ops[j][1])
            nxt = ops[j]
            assert nxt[0] == "vfmul.vv" and nxt[1][0] == x and nxt[1][1] == x, nxt
            ops[j] = ["vfmul.vf", [x, nxt[1][2], "%0"], op[2], op[3]]
            continue
        out.append(op)
    return out


class Gelu:
    """emits GELU steps (two vectors) with per-step register renaming and hook positions"""

    def __init__(self, header, iconst):
        self.ops = gelu_ops(header)
        self.N = len(self.ops)
        self.fc = {}
        self.iconst = iconst                         # {int value: register}
        names = [f"ft{i}" for i in range(12)] + [f"fa{i}" for i in range(8)] + [f"fs{i}" for i in range(12)]
        for mn, o, kind, val in self.ops:
            if kind == "f":
                v = float(val.rstrip("f"))
                if v not in self.fc:
                    self.fc[v] = names[len(self.fc)]
        def first_write(reg):
            return next(i for i, (mn, o, k, v) in enumerate(self.ops) if o[0] == reg)
        def last_read(regs):
            return max(i for i, (mn, o, k, v) in enumerate(self.ops) if any(r in o[1:] for r in regs))
        self.t3_first = min(first_write("v12"), first_write("v28"))      # 0-based op index
        self.loadat = last_read(["v4", "v20"]) + 1                        # hook index after which x_next can load
        self.t2_dead = (last_read_before(self.ops, ["v8", "v24"], self.t3_first) + 1,
                        min(i for i, (mn, o, k, v) in enumerate(self.ops) if o[0] in ("v8", "v24") and i > self.t3_first))

    def const_init(self, tmp="t3"):
        o = []
        for v, r in self.fc.items():
            o += [f"li {tmp}, 0x{fbits(v):08x}", f"fmv.w.x {r}, {tmp}"]
        for v, r in self.iconst.items():
            o += [f"li {r}, 0x{v:08x}"]
        return o

    @staticmethod
    def pmap(s):
        if s % 2 == 0:
            return {"v0": 0, "v4": 4, "v8": 8, "v12": 12, "v16": 16, "v20": 20, "v24": 24, "v28": 28}
        return {"v0": 4, "v4": 0, "v8": 8, "v12": 12, "v16": 20, "v20": 16, "v24": 24, "v28": 28}

    def step(self, e, s, hook):
        """emit step s; hook(i) is called after op i (1-based count of emitted ops)"""
        m = self.pmap(s)
        for i, (mn, ops, kind, val) in enumerate(self.ops):
            o = [f"v{m[q]}" if q in m else q for q in ops]
            if "%0" in o:
                c = self.fc[float(val.rstrip("f"))] if kind == "f" else self.iconst[int(val.rstrip("uU"), 0)]
                o = [c if q == "%0" else q for q in o]
            e(f"{mn} {', '.join(o)}")
            hook(i + 1)


def last_read_before(ops, regs, lim):
    return max(i for i, (mn, o, k, v) in enumerate(ops[:lim]) if any(r in o[1:] for r in regs))


class Op:
    def __init__(self, kind, render, req=None, prov=None):
        self.kind, self.render, self.req, self.prov = kind, render, req, prov


class Queue:
    """in-order OPE work queue with dependency tags"""

    def __init__(self):
        self.q, self.i, self.done = [], 0, set()
        self.pending, self.cool = None, 0
        self.cool_slots = 1                      # slots to wait after a group's vtfmm before reloading its registers

    def ready(self):
        if self.i >= len(self.q):
            return False
        r = self.q[self.i].req
        if r is None:
            return True
        if isinstance(r, list):
            return all(x in self.done for x in r)
        return r in self.done

    def head(self):
        return self.q[self.i]

    def take(self):
        op = self.q[self.i]
        self.i += 1
        if isinstance(op.prov, list):
            self.done.update(op.prov)
        elif op.prov is not None:
            self.done.add(op.prov)
        return op

    def empty(self):
        return self.i >= len(self.q)

    # ---- inside VFU code ----
    def slot(self, e, regs, restore=VM64, xfers=1, allow_group=True, lcfg=None):
        body = []
        if self.pending is not None:
            body += [*MM, *self.pending.render(regs, "macs")]
            self.pending = None
            self.cool = self.cool_slots
            n = 0
            while n < xfers and self.ready() and self.head().kind == "xfer":
                body += self.take().render(None); n += 1
        elif self.ready() and self.head().kind in ("xfer", "init"):
            body = [*MM]
            n = 0
            while n < xfers and self.ready() and self.head().kind in ("xfer", "init"):
                op = self.take()
                body += op.render(None)
                n += op.kind == "xfer"
        elif allow_group and self.ready() and self.head().kind == "group":
            if self.cool > 0:
                self.cool -= 1
                return
            op = self.take()
            if lcfg:                              # operand loads need e32, m4, vl = 64
                e(lcfg, *op.render(regs, "load"), restore)
            else:
                e(*op.render(regs, "load"))       # under the VFU config (e32, m4, vl = 64)
            self.pending = op
            return
        if body:
            e(*body)
            if restore:
                e(restore)

    def xfer_slot(self, e, n=1, restore=VM64):
        body = []
        while n > 0 and self.pending is None and self.ready() and self.head().kind in ("xfer", "init"):
            op = self.take()
            body += op.render(None)
            n -= op.kind == "xfer"
        if body:
            e(*MM, *body)
            if restore:
                e(restore)

    def flush(self, e, regs, restore=VM64):
        if self.pending is not None:
            e(*MM, *self.pending.render(regs, "macs"))
            self.pending = None
            if restore:
                e(restore)

    # ---- OPE only ----
    def batched(self, e, until=None, sets=((0, 16), (8, 24)), batch=2):
        """emit queue chunks two K-groups per config switch until `until()` is true (or the queue is empty)"""
        bi = 0
        cfg = "V"
        while not self.empty() and self.ready() and not (until and until()):
            op = self.head()
            if op.kind == "group":
                groups, xf = [], []
                while not self.empty() and self.ready() and len(groups) < batch and self.head().kind in ("group", "xfer"):
                    (groups if self.head().kind == "group" else xf).append(self.take())
                rs = [sets[(bi + k) % 2] for k in range(len(groups))]
                bi += 1
                e(VM64)
                for g, r in zip(groups, rs):
                    e(*g.render(r, "load"))
                e(*MM)
                for g, r in zip(groups, rs):
                    e(*g.render(r, "macs"))
                cfg = "M"
                for x in xf:
                    e(*x.render(None))
            else:
                op = self.take()
                if cfg != "M":
                    e(*MM); cfg = "M"
                e(*op.render(None))
        e(VM64)


def block22(q, *, abase, aoff, bbase, boff, kp, zreg, zoff, zstride, zrow_bytes, req_fn, prov, seed=False,
            ntiles=4, xr=2, drain_order=None, rowprov=None):
    """2x2 OPE block (32 rows x 32 cols = 4 tiles), 2 K-pairs per K-group (A: one e32,m4 load [2 tiles][2 kp][16],
    B: one load [2 kp][32]); drained (and optionally seeded) in xr-row chunks with running pointers a2 (addr) /
    a3 (TSS); zrow_bytes = byte stride between Z rows (zstride: register holding it)."""
    KG = -(-kp // 2)
    def init(regs):
        o = [f"li a4, {aoff}", f"add a0, {abase}, a4", f"li a4, {boff}", f"add a1, {bbase}, a4"]
        return o + ([] if seed else [f"vtzero {t}" for t in TILES[:4]])
    q.q.append(Op("init", init))
    def chunks(kind):
        out = []
        tiles = drain_order or list(range(ntiles))
        for ti in tiles:
            for r0 in range(0, 16, xr):
                def f(regs, ti=ti, r0=r0):
                    off = zoff + ((ti // 2) * 16 + r0) * zrow_bytes + (ti % 2) * 64
                    o = [f"li a3, {TS[ti] + r0}", f"li a4, {off}", f"add a2, {zreg}, a4"] if r0 == 0 else []
                    ins = "vtle32" if kind == "sd" else "vtse32"
                    for _ in range(xr):
                        o += [f"{ins} a3, (a2)", "addi a3, a3, 1", f"add a2, a2, {zstride}"]
                    return o
                out.append(Op("xfer", f))
        return out
    if seed:
        q.q += chunks("sd")
    for gi in range(KG):
        nk = min(2, kp - 2 * gi)
        def grp(regs, part, nk=nk):
            a, b = regs
            if part == "load":
                return [rr(a, "a0", "t1"), rr(b, "a1", "t1")]
            o = []
            for k in range(nk):
                o += [f"vtfmm.tvv mt0, v{a + k}, v{b + 2 * k}", f"vtfmm.tvv mt4, v{a + k}, v{b + 2 * k + 1}"]
                if ntiles == 4:
                    o += [f"vtfmm.tvv mt8, v{a + 2 + k}, v{b + 2 * k}", f"vtfmm.tvv mt12, v{a + 2 + k}, v{b + 2 * k + 1}"]
            return o
        q.q.append(Op("group", grp, req_fn(gi)))
    if rowprov is None:
        dr = chunks("dr")
        dr[-1].prov = prov
    else:
        # row-major drain (tiles 0/1 for rows 0-15, then 2/3), each chunk with its own pointers; after the chunk of
        # the second tile of a row pair, rowprov(last row of the pair) is provided
        dr = []
        for half in (0, 1):
            for r0 in range(0, 16, xr):
                for ti in (2 * half, 2 * half + 1):
                    def f(regs, ti=ti, r0=r0):
                        off = zoff + ((ti // 2) * 16 + r0) * zrow_bytes + (ti % 2) * 64
                        o = [f"li a3, {TS[ti] + r0}", f"li a4, {off}", f"add a2, {zreg}, a4"]
                        for _ in range(xr):
                            o += ["vtse32 a3, (a2)", "addi a3, a3, 1", f"add a2, a2, {zstride}"]
                        return o
                    dr.append(Op("xfer", f, None, rowprov(16 * half + r0 + xr - 1) if ti % 2 else None))
        dr[-1].prov = [dr[-1].prov] + (list(prov) if isinstance(prov, list) else [prov])
    q.q += dr


def pack22(words_kp_m):
    """packed words [KP][M] -> A stream [M/32][KG][2 tiles][2 kp][16] (KG = ceil(KP/2), zero padded)"""
    import numpy as np
    KP, M = words_kp_m.shape
    KG = -(-KP // 2)
    pad = np.zeros((KG * 2, M), np.uint32)
    pad[:KP] = words_kp_m
    return pad.reshape(KG, 2, M // 32, 2, 16).transpose(2, 0, 3, 1, 4).ravel()


def packwords(m16):
    """FP16 [K][N] -> words [K/2][N], O-POPE half order {K even (lo), K odd (hi)}"""
    import numpy as np
    b = np.asarray(m16, np.float16).view(np.uint16).astype(np.uint32)
    return b[0::2] | (b[1::2] << 16)


def wrap(name, proto, lines):
    body = "\n".join(f'    "{x}\\n"' for x in lines)
    return (f"#include <stdint.h>\n{proto};\n"
            f'__asm__(\n    ".text\\n"\n    ".balign 64\\n"\n    ".globl {name}\\n"\n    ".type {name}, @function\\n"\n'
            f'    "{name}:\\n"\n{body}\n    ".size {name}, .-{name}\\n");\n')


SAVED = [f"s{i}" for i in range(12)]


def prologue():
    return ["addi sp, sp, -64", *[f"sw {r}, {4 * i}(sp)" for i, r in enumerate(SAVED)]]


def epilogue(sync_reg="s2"):
    return ["fence rw, rw", f"lw t3, 0({sync_reg})", *[f"lw {r}, {4 * i}(sp)" for i, r in enumerate(SAVED)],
            "addi sp, sp, 64", "ret"]


def halfblock22(q, *, pair, half, abase, aoff, bbase, boff, kp, zreg, zoff, zstride, zrow_bytes, req_fn, prov,
                xr=2, interleave=None):
    """16 rows x 32 cols on one tile pair (pair 0: mt0/mt4, pair 1: mt8/mt12); rows half*16.. of a 32-row A block
    (the A load holds both halves: upper uses v[a+k], lower v[a+2+k]); 2 K-pairs per K-group. Drain chunks
    (xr rows of both tiles, own pointers) are returned, not queued: the caller interleaves them with later work."""
    ta, tb = ("mt0", "mt4") if pair == 0 else ("mt8", "mt12")
    tsa, tsb = (TS[0], TS[1]) if pair == 0 else (TS[2], TS[3])
    KG = -(-kp // 2)
    q.q.append(Op("init", lambda regs: [f"li a4, {aoff}", f"add a0, {abase}, a4", f"li a4, {boff}", f"add a1, {bbase}, a4",
                                        f"vtzero {ta}", f"vtzero {tb}"]))
    xs = list(interleave or [])
    for gi in range(KG):
        nk = min(2, kp - 2 * gi)
        def grp(regs, part, nk=nk):
            a, b = regs
            if part == "load":
                return [rr(a, "a0", "t1"), rr(b, "a1", "t1")]
            o = []
            for k in range(nk):
                ar = a + k + 2 * half
                o += [f"vtfmm.tvv {ta}, v{ar}, v{b + 2 * k}", f"vtfmm.tvv {tb}, v{ar}, v{b + 2 * k + 1}"]
            return o
        q.q.append(Op("group", grp, req_fn(gi)))
        if xs and gi >= 1:
            q.q.append(xs.pop(0))
    q.q += xs
    dr = []
    for r0 in range(0, 16, xr):
        def f(regs, r0=r0):
            off = zoff + (half * 16 + r0) * zrow_bytes
            o = [f"li a3, {tsa + r0}", f"li a4, {off}", f"add a2, {zreg}, a4", f"addi a7, a2, 64", f"li t0, {tsb + r0}"]
            for _ in range(xr):
                o += ["vtse32 a3, (a2)", "addi a3, a3, 1", f"add a2, a2, {zstride}",
                      "vtse32 t0, (a7)", "addi t0, t0, 1", f"add a7, a7, {zstride}"]
            return o
        dr.append(Op("xfer", f))
    dr[-1].prov = prov
    return dr


def slot2(q, e, sets, restore, lcfg, xfers=2, macs_only=False):
    """queue slot for VFU code whose own config is `restore`: up to two K-groups per config switch (register sets
    `sets`), operand loads in one slot and the vtfmm (plus up to `xfers` transfer chunks) in a later one"""
    if q.pending is not None:
        body = [*MM]
        for op, r in q.pending:
            body += op.render(r, "macs")
        q.pending = None
        q.cool = q.cool_slots
        n = 0
        while not macs_only and n < xfers and q.ready() and q.head().kind == "xfer":
            body += q.take().render(None); n += 1
        e(*body, restore)
        return
    if macs_only:
        return
    if q.ready() and q.head().kind in ("xfer", "init"):
        body = [*MM]
        n = 0
        while n < xfers and q.ready() and q.head().kind in ("xfer", "init"):
            op = q.take()
            body += op.render(None)
            n += op.kind == "xfer"
        e(*body, restore)
        return
    if q.ready() and q.head().kind == "group":
        if q.cool > 0:
            q.cool -= 1
            return
        batch = [(q.take(), sets[0])]
        if q.ready() and q.head().kind == "group":
            batch.append((q.take(), sets[1]))
        body = [lcfg]
        for op, r in batch:
            body += op.render(r, "load")
        e(*body, restore)
        q.pending = batch
