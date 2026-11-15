#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
"""Gather-MLP (DLRM embedding-bag or GCN layer, FP16 -> FP32), O-POPE shapes R=144/127, D=64, B=127, L=8, Dout=128:
VME kernel + data.

Inputs drawn exactly as baseline widening-hp-gathermlp/script/gen_data.py (default_rng(1337)); table and index words
(four 8-bit row indices per word) unchanged. One kernel, two phases (default, --gemm5 1):
  1. gather + pooling, O-POPE order (FP16 sequential sum of the 8 rows, then the FP16 1/L scale for GCN): one stream
     of row loads (ring of 6 e16,m2 buffers, loads 4 rows ahead), two alternating accumulators; index words through FP
     registers (flw burst every 14 bags + fmv.x.w: scalar integer loads wait for every outstanding vector memory
     access). The pooled row of a bag (32 feature-pair words) is stored with one strided store into
     pool [4 bag tiles][33 K-pairs][32 bags] (K-pair 32 = {1, 1}, placed outside: the bias K-pair).
  2. GeMM z[Dout][bags] = bias + W1^T pool with the ReLU fused into the drain: 1x2c blocks (16 outputs x 32 bags,
     two tiles, 4 K-pairs per K-group) alternating the tile pairs; while block j accumulates, the rows of block j-1
     go tile -> VRF (vtmv.v.t, under the widening matrix config), are clamped (vfmax, VFU) and stored once (vse32):
     the ReLU runs on the VFU in parallel with the OPE and the z pass of a separate ReLU (128 KB of VLSU traffic)
     disappears. The tile -> VRF moves sit between the two halves of the 8 vtfmm (the OPE queue holds ~4).
z is stored with a row pitch of 128 (bag 127 is padding).
Measured alternatives (slower on this Spatz: single in-order dispatch, ~0.7 instructions per cycle): GeMM K-groups
inside the gather (v1: 27k; gathermlp v3/v4/v6 (session prototypes, not kept): 26-31k: the configuration switches and per-step bookkeeping cost
more instructions than the overlap hides); ReLU as load / max / store vectors in the GeMM (v2: 24.0k).
"""
import argparse, pathlib, sys
import numpy as np
sys.path.insert(0, str(pathlib.Path(__file__).parent))
import fusegen as fg
import vmegen as vg

ap = argparse.ArgumentParser()
ap.add_argument("--data", required=True)
ap.add_argument("--kernel", required=True)
ap.add_argument("--mode", default="dlrm", choices=["dlrm", "gcn"])
ap.add_argument("--slots", default="0,1,2,3,4,5,6,7", help="lookups (0..7) after whose add an OPE slot is placed")
ap.add_argument("--fuse", type=int, default=1)
ap.add_argument("--mc32", type=int, default=0, help="gemm5: switch to the SEW32 matrix config for vtmv.v.t")
ap.add_argument("--gemm5", type=int, default=1, help="1: after the gather, 1x2c ping-pong GeMM whose drains apply the ReLU "
                "(tile rows -> vtmv.v.t -> vfmax -> vse32: no ReLU pass over z)")
ap.add_argument("--fbuf", type=int, default=14, help="v2: bags of index words buffered in FP registers per burst (0: off)")
ap.add_argument("--ring", type=int, default=6, help="v2: row buffers of the gather ring (loads ring-2 rows ahead)")
ap.add_argument("--relu_in", type=int, default=1, help="v2: ReLU of finished row bands inside the GeMM")
ap.add_argument("--v2", type=int, default=1, help="1: gather alone, then the vmegen 2x1 ping-pong GeMM (bias K-pair), ReLU")
ap.add_argument("--cool", type=int, default=1)
ap.add_argument("--xfers", type=int, default=2)
ap.add_argument("--quiet", default="6,7", help="lookups whose slot issues only vtfmm (no memory traffic before the index loads)")
A = ap.parse_args()
GCN = A.mode == "gcn"
R, D, B, L, Dout = (127 if GCN else 144), 64, 127, 8, 128
BPAD = 128
KP = D // 2 + 1
KG = -(-KP // 2)
SLOTS = {int(x) for x in A.slots.split(",") if x}
QUIET = {int(x) for x in A.quiet.split(",") if x}

rng = np.random.default_rng(1337)
table = rng.uniform(-1, 1, (R, D)).astype(np.float16)
if GCN:
    idx = np.zeros((B, L), dtype=np.int64)
    idx[:, 0] = np.arange(B)
    for v in range(B):
        idx[v, 1:] = rng.choice(R, L - 1, replace=False)
else:
    idx = rng.integers(0, R, (B, L))
w1 = rng.uniform(-0.5, 0.5, (D, Dout)).astype(np.float16)
bias = rng.uniform(-0.5, 0.5, Dout).astype(np.float32)
pooled = np.zeros((B, D), dtype=np.float16)
for l in range(L):
    pooled = (pooled + table[idx[:, l]]).astype(np.float16)
if GCN:
    pooled = (pooled * np.float16(1.0 / L)).astype(np.float16)
z = np.maximum(w1.astype(np.float64).T @ pooled.astype(np.float64).T + bias[:, None], 0).astype(np.float32)
ii = idx.astype(np.uint32)
idxw = (ii[:, 0::4] | (ii[:, 1::4] << 8) | (ii[:, 2::4] << 16) | (ii[:, 3::4] << 24)).astype(np.uint32)
hi = bias.astype(np.float16); lo = (bias.astype(np.float64) - hi.astype(np.float64)).astype(np.float16)
wext = np.zeros((D + 2, Dout), np.float16); wext[:D] = w1; wext[D] = hi; wext[D + 1] = lo
arrays = {"gm_table": table.view(np.uint16).astype(np.uint32).ravel(), "gm_idx": idxw.ravel(),
          "gm_w": (vg.pack_a(wext, "1x2h", swap=True) if A.gemm5 else
                   vg.pack_a(w1, "2x1", swap=True) if A.v2 else fg.pack22(fg.packwords(wext))),
          "gm_golden": z.view(np.uint32).ravel()}
if A.gemm5:
    A.v2 = 0
    A.fuse = 0
if A.v2:
    arrays["gm_bv"] = vg.bias_vectors(bias, "2x1")
hdr = [f"// GENERATED by gathermlp.py --mode {A.mode}", "#pragma once", "#include <stdint.h>",
       f"#define MODE_GCN {int(GCN)}", f"#define R_SIZE {R}", f"#define D_SIZE {D}", f"#define B_SIZE {B}",
       f"#define L_SIZE {L}", f"#define DOUT_SIZE {Dout}", f"#define BPAD {BPAD}", f"#define KP_SIZE {KP}",
       f"#define GM_V2 {A.v2}", f"#define POOL_WORDS {(BPAD // 16) * (D // 2) * 16 if A.v2 else (BPAD // 32) * KP * 32 + 128}"]
for k, v in arrays.items():
    if k == "gm_table":
        hdr.append(f"static uint16_t {k}[{len(v)}] __attribute__((section(\".data\"), aligned(64))) = {{" +
                   ",".join(f"0x{int(q):04x}" for q in v) + "};")
    else:
        hdr.append(f"static uint32_t {k}[{len(v)}] __attribute__((section(\".data\"), aligned(64))) = {{" +
                   ",".join(f"0x{int(q):08x}" for q in v) + "};")
pathlib.Path(A.data).write_text("\n".join(hdr) + "\n")

# ------------------------------------------------------------------ kernel ----------------------------------------
# s2 z, s3 table, s4 idx words, s5 pool, s6 W stream, s7 = 0x7f80 (row index mask << 7), s8 = 128 (pool K-pair stride),
# s9 = 512 (z row pitch), s0 = 64, s1 = 32, t1 = 256, t4/t5/t6 matrix config. Gather: a4/a5 and a6/a7 index words
# (current / next bag), t0 / t2 address temps, t3 scratch. OPE: a0/a1 operand pointers, a2/a3 drain pointers.
L_ = []
def E(*x): L_.extend(x)
V16 = "vsetvli zero, s0, e16, m2, ta, ma"
VS = "vsetvli zero, s1, e32, m2, ta, ma"
q = fg.Queue()
q.cool_slots = A.cool
NCB = BPAD // 32
for cb in range(NCB):
    for rb in range(Dout // 32):
        fg.block22(q, abase="s6", aoff=rb * KG * 256, bbase="s5", boff=cb * KP * 128, kp=KP, zreg="s2",
                   zoff=rb * 32 * 512 + cb * 128, zstride="s9", zrow_bytes=512, req_fn=lambda gi, cb=cb: ("pool", cb),
                   prov=("z", rb, cb))
E(*fg.prologue(), "mv s2, a0", "mv s3, a1", "mv s4, a2", "mv s5, a3", "mv s6, a4", "li s7, 0x7f80", "li s8, 128",
  f"li s9, {64 if A.v2 else 512}", "li s0, 64", "li s1, 32", "li t1, 256", "li t4, 16450", "li t5, 0xC8", "li t6, 16", *fg.MM,
  "msettm zero, t6")
if GCN:
    E(f"li t3, 0x{int(np.float16(1.0 / L).view(np.uint16)):04x}", "fmv.h.x ft1, t3")
RING = [0, 2, 4, 6, 8, 10] if not A.v2 else list(range(0, 2 * A.ring, 2))
ACC = [12, 14] if not A.v2 else [2 * A.ring, 2 * A.ring + 2]
AH = 4 if not A.v2 else A.ring - 2
NQ = B * L
FREG = [f"fs{i}" for i in range(12)] + [f"fa{i}" for i in range(8)] + [f"ft{i}" for i in range(2, 12)]
IDXR = [("s10", "s11"), ("a5", "a6"), ("a7", "ra")]   # a0..a4 are used by the OPE chunks; ra saved on the stack


def addr(qq, dst):
    """t0/t2 <- table + 128 * index of lookup qq (bag qq // L, lookup qq % L) from the bag's index words"""
    b, l = divmod(qq, L)
    w = IDXR[b % 3][l // 4]
    k = l % 4
    sh = 8 * k - 7
    return ([f"slli {dst}, {w}, 7"] if sh < 0 else [f"srli {dst}, {w}, {sh}"]) + [f"and {dst}, {dst}, s7",
                                                                                   f"add {dst}, {dst}, s3"]


def load(qq):
    tmp = "t0" if qq % 2 == 0 else "t2"
    return addr(qq, tmp) + [f"vle16.v v{RING[qq % len(RING)]}, ({tmp})"]


E("sw ra, 48(sp)", "addi sp, sp, -96", *[f"fsd fs{i}, {8 * i}(sp)" for i in range(12)],   # fs0..fs11: callee-saved
  V16, "lw s10, 0(s4)", "lw s11, 4(s4)")
for qq in range(AH):
    E(*load(qq))
for b in range(B):
    if A.fbuf:
        # index words through FP registers: a burst of flw per FB bags, one fmv.x.w per word (no memory wait)
        FB = A.fbuf
        if b % FB == 0:
            E(*[f"flw {FREG[2 * (nb - b - 1) + i]}, {8 * nb + 4 * i}(s4)" for nb in range(b + 1, min(b + 1 + FB, B))
                for i in (0, 1)])
        nb = b + 1
        if nb < B:
            r = ((nb - 1) // FB) * FB
            nw = IDXR[nb % 3]
            E(f"fmv.x.w {nw[0]}, {FREG[2 * (nb - r - 1)]}", f"fmv.x.w {nw[1]}, {FREG[2 * (nb - r - 1) + 1]}")
    elif b % 2 == 0:                  # index words of the next two bags (the first load waits for the vector
        for nb in (b + 1, b + 2):     # memory accesses in flight, the others do not)
            if nb < B:
                nw = IDXR[nb % 3]
                E(f"lw {nw[0]}, {8 * nb}(s4)", f"lw {nw[1]}, {8 * nb + 4}(s4)")
    acc = ACC[b % 2]
    for l in range(L):
        qq = b * L + l
        if l == 1:
            E(f"vfadd.vv v{acc}, v{RING[(qq - 1) % len(RING)]}, v{RING[qq % len(RING)]}")
        elif l >= 2:
            E(f"vfadd.vv v{acc}, v{acc}, v{RING[qq % len(RING)]}")
        if qq + AH < NQ:
            E(*load(qq + AH))
        if A.fuse and not A.v2 and l in SLOTS:
            fg.slot2(q, E, ((16, 20), (24, 28)), V16, fg.VM64, xfers=A.xfers, macs_only=l in QUIET)
    if GCN:
        E(f"vfmul.vf v{acc}, v{acc}, ft1")
    if A.v2:
        pnl, col = divmod(b, 16)
        E(VS, f"li t3, {pnl * (D // 2) * 64 + col * 4}", "add t3, s5, t3", f"vsse32.v v{acc}, (t3), s9", V16)
        cb = b // 32
    else:
        cb, col = divmod(b, 32)
        E(VS, f"li t3, {cb * KP * 128 + col * 4}", "add t3, s5, t3", f"vsse32.v v{acc}, (t3), s8", V16)
    if col == 31 or b == B - 1:
        q.done.add(("pool", cb))
q.done.add(("pool", NCB - 1))
def gemm5():
    """1x2c ping-pong GeMM (16 outputs x 32 bags per block, 4 K-pairs per K-group, K-pair 32 = bias) whose drains
    apply the ReLU: block j-1's rows go tile -> VRF (vtmv.v.t, SEW32 matrix config) -> vfmax -> vse32, two rows
    (z row segments of 32 floats) per K-group of block j. Sets: A v0-3 / B v4-11 and A v12-15 / B v16-23;
    staging v24..v31 (4 segments)."""
    KG5 = -(-KP // 4)
    V32 = "vsetvli zero, s1, e32, m2, ta, ma"
    MC32 = ["msetmtypei 1, 2, 0", "msettn zero, t6", "msettm zero, t6"] if A.mc32 else []
    PRS = [("mt0", "mt4", fg.TS[0], fg.TS[1]), ("mt8", "mt12", fg.TS[2], fg.TS[3])]
    SETS5 = [(0, 4), (12, 16)]
    blks = [(c, rb) for c in range(NCB) for rb in range(Dout // 16)]
    groups = [(j, g) for j in range(len(blks)) for g in range(KG5)]
    def ptrs(j):
        c, rb = blks[j]
        return [f"li t3, {rb * KG5 * 256}", "add a0, s6, t3", f"li t3, {c * KP * 128}", "add a1, s5, t3"]
    def loads(gi):
        a, b = SETS5[gi % 2]
        return [fg.rr(a, "a0", "t1"), fg.rr(b, "a1", "t1"), fg.rr(b + 4, "a1", "t1")]
    def macs(gi):
        j, g = groups[gi]
        a, b = SETS5[gi % 2]
        tx, ty = PRS[j % 2][:2]
        nk = min(4, KP - 4 * g)
        o = []
        for k in range(nk):
            o += [f"vtfmm.tvv {tx}, v{a + k}, v{b + 2 * k}", f"vtfmm.tvv {ty}, v{a + k}, v{b + 2 * k + 1}"]
        return o
    seg_m = []                     # pending tile -> VRF moves: (staging reg, block)
    seg_v = []                     # moved, to be clamped and stored: staging reg
    nseg = [0]
    def drain_setup(j):
        c, rb = blks[j]
        tsx, tsy = PRS[j % 2][2:]
        return [f"li a3, {tsx}", f"li a4, {tsy}", f"li t3, {rb * 16 * BPAD * 4 + c * 128}", "add a2, s2, t3"]
    def m_part(n):
        o = []
        for _ in range(n):
            if not seg_m:
                break
            setup = seg_m.pop(0)
            st = 24 + 2 * (nseg[0] % 4); nseg[0] += 1
            o += setup + [f"vtmv.v.t v{st}, a3", "addi a3, a3, 1", f"vtmv.v.t v{st + 1}, a4", "addi a4, a4, 1"]
            seg_v.append(st)
        return o
    def v_part():
        sts = list(seg_v); seg_v.clear()
        o = [f"vfmax.vf v{st}, v{st}, ft0" for st in sts]          # all clamps first, then the stores
        for st in sts:
            o += [f"vse32.v v{st}, (a2)", f"addi a2, a2, {BPAD * 4}"]
        return o
    E(fg.VM64, *ptrs(0), *loads(0), *fg.MM, *[f"vtzero {t}" for t in PRS[0][:2]])
    for gi, (j, g) in enumerate(groups):
        if g == 0 and j > 0:
            seg_m.extend([drain_setup(j - 1)] + [[] for _ in range(15)])
        # V part: next group's operands (pointers of the next block first), clamp + store of the moved rows
        v = [fg.VM64]
        if gi + 1 < len(groups):
            if groups[gi + 1][1] == 0:
                v += ptrs(groups[gi + 1][0])
            v += loads(gi + 1)
        vp = v_part()
        if vp:
            v += [V32, *vp]
        E(*v)
        # M part: two tile rows of the previous block -> VRF, then this group's vtfmm (zero the pair at block start)
        mp = m_part(2)
        if A.mc32:
            if mp:
                E(*MC32, *mp)
            E(*fg.MM, *([f"vtzero {t}" for t in PRS[j % 2][:2]] if (g == 0 and j > 0) else []), *macs(gi))
        else:
            # the OPE queue holds ~4 vtfmm: the tile -> VRF moves (other pair) go between the two halves
            mc = macs(gi)
            vz = [f"vtzero {t}" for t in PRS[j % 2][:2]] if (g == 0 and j > 0) else []
            E(*fg.MM, *vz, *mc[:4], *mp, *mc[4:])
    # last block's drain
    seg_m.extend([drain_setup(len(blks) - 1)] + [[] for _ in range(15)])
    while seg_m or seg_v:
        mp = m_part(2)
        if mp:
            E(*(MC32 if A.mc32 else fg.MM), *mp)
        vp = v_part()
        if vp:
            E(V32, *vp)
    E("fence rw, rw")


if A.gemm5:
    gemm5()
elif not A.v2:
    if q.pending is not None:
        E(*fg.MM, *[x for op, r in q.pending for x in op.render(r, "macs")], V16)
        q.pending = None
    q.batched(E, sets=((0, 16), (8, 24)))
    E(*fg.MM, "li t3, 0x60000000", "vtmv.v.t v30, t3", VS, "vmv.x.s t3, v30")     # sync before the ReLU writes v0..v15
    V8 = "vsetvli zero, t3, e32, m8, ta, ma"
    E("li t3, 128", V8, "fmv.w.x ft0, zero", "mv t0, s2")
    for i in range(Dout * BPAD // 256):
        E("vle32.v v0, (t0)", "addi t2, t0, 512", "vle32.v v8, (t2)", "vfmax.vf v0, v0, ft0", "vfmax.vf v8, v8, ft0",
          "vse32.v v0, (t0)", "vse32.v v8, (t2)", "addi t0, t0, 1024")
E(*[f"fld fs{i}, {8 * i}(sp)" for i in range(12)], "addi sp, sp, 96", "lw ra, 48(sp)", *fg.epilogue("s2"))
NAME = "gm_vme"
src = f"// GENERATED by script/tf/gathermlp.py --mode {A.mode}\n" + fg.wrap(
    NAME, f"void {NAME}(float *z, const uint16_t *table, const uint32_t *idx, uint32_t *pool, const uint32_t *w)", L_)
if A.v2:
    # GeMM (vmegen 2x1 ping-pong, rb-major) with the ReLU of row band rb-1 in the K-groups of band rb: one vector
    # (half a z row, 64 floats) per K-group, single buffer v12 (vfmax + store of vector k-1, load of vector k);
    # columns 0..63 first, so the band's last block (columns 112..127, drained during the first block of band rb)
    # is done before its columns are read. The last band: m8 after the final drain.
    NB = BPAD // 16
    def rvec(band, k):
        row, half = (k, 0) if k < 32 else (k - 32, 1)
        return (band * 32 + row) * BPAD * 4 + half * 256
    def slot(j, g):
        rb = j // NB
        if rb == 0 or not A.relu_in:
            return ((), ())
        k = (j % NB) * 8 + g
        ev = [vg.VMODE]                  # the final K-group has no operand loads (config still matrix)
        if k > 0 or rb >= 2:
            ev += ["vfmax.vf v12, v12, ft0", "vse32.v v12, (t0)"]
        ev += [f"li t3, {rvec(rb - 1, k)}", "add t0, s2, t3", "vle32.v v12, (t0)"]
        return (ev, ())
    def tail(gm):
        gm.e("li s7, 16", ".Ltd_gm:", "vtse32 a5, (a3)", "addi a5, a5, 1", "add a3, a3, s9",
             "vtse32 a6, (a4)", "addi a6, a6, 1", "add a4, a4, s9", "addi s7, s7, -1", "bnez s7, .Ltd_gm")
        first = (Dout // 32 - 1) * 32 if A.relu_in else 0
        gm.e(vg.VMODE)
        if A.relu_in:
            gm.e("vfmax.vf v12, v12, ft0", "vse32.v v12, (t0)")
        gm.e("li t3, 128", "vsetvli zero, t3, e32, m8, ta, ma", f"li t3, {first * BPAD * 4}", "add t0, s2, t3")
        for i in range((Dout - first) * BPAD // 256):
            gm.e("vle32.v v0, (t0)", "addi t2, t0, 512", "vle32.v v8, (t2)", "vfmax.vf v0, v0, ft0",
                 "vfmax.vf v8, v8, ft0", "vse32.v v0, (t0)", "vse32.v v8, (t2)", "addi t0, t0, 1024")
    gm = vg.Gemm(Dout, BPAD, D // 2, shape="2x1", bias=True, tag="gm")
    src += ("void gm_gemm(const uint32_t *a, const uint32_t *pool, float *z, const uint32_t *bv);\n" +
            gm.emit(name="gm_gemm", slot=slot, tail_hook=tail, pre_hook=lambda g: g.e("fmv.w.x ft0, zero")))
pathlib.Path(A.kernel).write_text(src)
print(f"gathermlp {A.mode}: {len(L_)} instructions, OPE chunks {len(q.q)}, done in gather {q.i}")
