#!/usr/bin/env python3
# Copyright 2026 ETH Zurich.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# RVV / custom encoding-space visualizer.
#
# Builds a single self-contained HTML page showing how the RISC-V Vector
# encoding space (and the adjacent custom major opcodes) is occupied by:
#   * the official RVV spec         (upstream riscv/riscv-opcodes)
#   * the custom extensions we ship  (local sw/toolchain/riscv-opcodes/opcodes-*)
#   * local additions inside opcodes-rvv itself (entries absent upstream, e.g.
#     vlx*, vfwdotp, legacy v0.9 leftovers) -> shown as their own pseudo-ext;
#     same-name entries whose fixed bits differ from upstream are listed in an
#     encoding-mismatch panel
#   * what our hardware actually decodes (riscv_instr:: refs in the primary
#     decoders: spatz_decoder.sv + snitch.sv)
#
# Color code (see LEGEND):
#   blue   -> spec instruction that our primary decoders implement
#   gray   -> spec instruction we do NOT implement
#   <hue>  -> a custom extension instruction (one hue per extension)
#   empty  -> free encoding space
#   red    -> conflict: >1 source fixes overlapping bits compatibly
#
# Usage:
#   python3 gen_encoding_viz.py            # auto-locates repo, writes encoding_map.html
#   python3 gen_encoding_viz.py --help
#
# The script does not modify the repo; it only reads. The upstream spec repo is
# cached under util/enc_viz/.cache (git-ignored) on first run.

import argparse
import os
import re
import subprocess
import sys
from collections import defaultdict, OrderedDict

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Pin upstream riscv-opcodes for reproducibility (override with --spec-commit).
UPSTREAM_URL = "https://github.com/riscv/riscv-opcodes.git"
UPSTREAM_COMMIT = "c6edca7d8c3f92694963a0a0baeb511930fb2af4"

# Spec canvas: the RVV v1.0 ground truth = extensions/rv_v in upstream
# riscv/riscv-opcodes. Pass --crypto to additionally draw the ratified
# vector-crypto extensions (rv_zv*): they claim OP-V funct6 slots too (e.g.
# Zvfbfwma's vfwmaccbf16 sits on funct6 0x3b, which our vfwdotp reuses).
SPEC_FILES = ["rv_v"]  # extended with rv_zv* only when --crypto is given

# Custom / shipped extensions: read live from the top-level Makefile OPCODES
# variable, so the map always reflects the currently-enabled build. This list
# is only the fallback if the Makefile can't be parsed. opcodes-rvv is the
# spec copy and handled separately.
EXT_FILES_FALLBACK = [
    "opcodes-frep_CUSTOM",
    "opcodes-dma_CUSTOM",
    "opcodes-smallfloat",
    "opcodes-vfx_CUSTOM",
]

_OPCODES_RE = re.compile(r'^\s*OPCODES\s*:?=\s*"([^"]+)"', re.M)


def parse_makefile_opcodes(repo):
    """Return the opcode files enabled in the top-level Makefile OPCODES var
    (minus opcodes-rvv), or None if the variable can't be found."""
    try:
        with open(os.path.join(repo, "Makefile")) as fh:
            m = _OPCODES_RE.search(fh.read())
    except OSError:
        return None
    if not m:
        return None
    return [f for f in m.group(1).split() if f != RVV_LOCAL_FILE]

# The local copy of the RVV spec fed to the riscv_instr.sv generator. It is
# diffed against the upstream canvas: entries whose name is absent upstream are
# local additions riding in the RVV space (e.g. vlx*, vfwdotp, legacy v0.9
# leftovers) and are rendered as a pseudo-extension of their own; same-name
# entries whose fixed bits differ are reported in a mismatch panel.
RVV_LOCAL_FILE = "opcodes-rvv"
ORIGIN_RVV_LOCAL = "opcodes-rvv (not in spec v1.0)"

# One distinct, accessible hue per extension (color-blind-aware categorical set).
EXT_COLORS = OrderedDict([
    ("opcodes-rv32b_CUSTOM", "#e6820a"),  # orange
    ("opcodes-ipu_CUSTOM",   "#12897d"),  # teal
    ("opcodes-frep_CUSTOM",  "#b5179e"),  # magenta
    ("opcodes-dma_CUSTOM",   "#7048e8"),  # violet
    ("opcodes-ssr_CUSTOM",   "#c9184a"),  # crimson
    ("opcodes-smallfloat",   "#5c7f00"),  # olive
    ("opcodes-vfx_CUSTOM",   "#0b7285"),  # deep cyan
    (ORIGIN_RVV_LOCAL,       "#8a5a2b"),  # brown
])

COLOR_IMPL = "#1f6fd6"   # blue  : spec, handled by HW
COLOR_UNIMPL = "#b7bdc6"  # gray  : spec, NOT handled by HW
COLOR_DEAD = "#101317"   # black : custom, defined in build but NOT handled by HW
COLOR_CONFLICT = "#e03131"  # red border/hatch
COLOR_FREE = "#f5f6f8"   # empty slot

# "Implemented in hardware" = the instruction name is referenced
# (riscv_instr::NAME) in one of the DECODER files below. This is a *decode*
# check only — it deliberately does not verify the logical implementation
# behind the decoder. Different instruction groups decode in different
# front-ends: RVV + custom vector in snitch/spatz_decoder, small-float offload
# in the FPU sequencer, DMA in the AXI DMA front-end. An instruction that no
# decoder references is not implemented -> reclaimable encoding space.
RTL_ROOT = "hw"
DECODER_FILES = [
    "snitch.sv",               # scalar core: offload + custom scalar decode
    "spatz_decoder.sv",        # vector unit decoder
    "spatz_fpu_sequencer.sv",  # FP offload sequencer
    "axi_dma_tc_snitch_fe.sv", # DMA front-end decoder
]

# OP-V funct3 -> category column.
FUNCT3_CAT = {
    0b000: "OPIVV", 0b100: "OPIVX", 0b011: "OPIVI",
    0b010: "OPMVV", 0b110: "OPMVX",
    0b001: "OPFVV", 0b101: "OPFVF",
    0b111: "OPCFG",  # vset*
}
CATEGORY_ORDER = ["OPIVV", "OPIVX", "OPIVI", "OPMVV", "OPMVX", "OPFVV", "OPFVF"]

OPV = 0x57
LOADFP = 0x07
STOREFP = 0x27
AMO = 0x2f
# Human-readable names for standard major opcodes that custom extensions overload.
MAJOR_NAMES = {
    0x03: "LOAD", 0x07: "LOAD-FP", 0x0f: "MISC-MEM", 0x13: "OP-IMM",
    0x17: "AUIPC", 0x1b: "OP-IMM-32", 0x23: "STORE", 0x27: "STORE-FP",
    0x2f: "AMO", 0x33: "OP", 0x37: "LUI", 0x3b: "OP-32",
    0x43: "MADD", 0x47: "MSUB", 0x4b: "NMSUB", 0x4f: "NMADD",
    0x53: "OP-FP", 0x63: "BRANCH", 0x67: "JALR", 0x6f: "JAL",
    0x73: "SYSTEM", 0x77: "OP-P / vector-crypto",
}
CUSTOM_MAJORS = OrderedDict([
    (0x0b, "CUSTOM-0"), (0x2b, "CUSTOM-1"),
    (0x5b, "CUSTOM-2"), (0x7b, "CUSTOM-3"),
])


# ---------------------------------------------------------------------------
# Encoding-line parser
# ---------------------------------------------------------------------------

class Enc:
    """A single decoded instruction: fixed-bit mask + match value (32-bit)."""
    __slots__ = ("name", "raw", "mask", "match", "origin", "kind")

    def __init__(self, name, mask, match, origin, kind, raw=""):
        self.name = name            # normalized: VADD_VV
        self.mask = mask            # 1 where the bit is fixed
        self.match = match          # value on the fixed bits
        self.origin = origin        # 'spec:rv_v' or 'opcodes-vfx_CUSTOM' etc.
        self.kind = kind            # 'spec' | 'ext'
        self.raw = raw

    def field(self, hi, lo):
        """Return integer value of bits [hi:lo] if all fixed, else None."""
        width = hi - lo + 1
        m = ((1 << width) - 1) << lo
        if (self.mask & m) != m:
            return None
        return (self.match & m) >> lo

    @property
    def opcode(self):
        return self.field(6, 0)


def normalize_name(name):
    """vadd.vv -> VADD_VV  (matches riscv-opcodes' SystemVerilog generator)."""
    return name.strip().upper().replace(".", "_")


_RANGE_RE = re.compile(r"^(\d+)\.\.(\d+)=(.+)$")
_BIT_RE = re.compile(r"^(\d+)=(.+)$")


def _int(v):
    return int(v, 16) if v.lower().startswith("0x") else int(v, 0)


def parse_opcode_line(line):
    """Parse a `name <tokens>` opcode-file line into (name, mask, match).

    Only fixed bit-ranges/bits contribute to mask/match; bare argument names
    (vd, rs1, vm, nf, ...) stay variable. Returns None for non-instruction
    lines ($import, $pseudo, comments, blanks).
    """
    line = line.split("#", 1)[0].strip()
    if not line or line.startswith("$") or line.startswith("@"):
        return None
    toks = line.split()
    if len(toks) < 2:
        return None
    name = toks[0]
    mask = 0
    match = 0
    saw_fixed = False
    for tok in toks[1:]:
        m = _RANGE_RE.match(tok)
        if m:
            hi, lo, val = int(m.group(1)), int(m.group(2)), _int(m.group(3))
            width = hi - lo + 1
            fm = ((1 << width) - 1) << lo
            mask |= fm
            match = (match & ~fm) | ((val << lo) & fm)
            saw_fixed = True
            continue
        m = _BIT_RE.match(tok)
        if m:
            b, val = int(m.group(1)), _int(m.group(2))
            mask |= (1 << b)
            match = (match & ~(1 << b)) | ((val & 1) << b)
            saw_fixed = True
            continue
        # bare arg name (variable) or arg=val (rare; not present in our files)
        if "=" in tok:
            # unknown named-field assignment -> ignore (leave variable)
            pass
    if not saw_fixed:
        return None
    return normalize_name(name), mask, match


def parse_opcode_file(path, origin, kind):
    out = []
    with open(path) as fh:
        for line in fh:
            r = parse_opcode_line(line)
            if r:
                nm, mask, match = r
                out.append(Enc(nm, mask, match, origin, kind, raw=line.strip()))
    return out


# riscv_instr.sv line: localparam logic [31:0] VADD_VV = 32'b000000???...1010111;
_SV_RE = re.compile(r"^\s*localparam\s+logic\s+\[31:0\]\s+(\w+)\s*=\s*32'b([01?]{32})\s*;")


def parse_riscv_instr_sv(path):
    """Return {NAME: (mask, match)} from the generated SystemVerilog."""
    out = {}
    with open(path) as fh:
        for line in fh:
            m = _SV_RE.match(line)
            if not m:
                continue
            name, bits = m.group(1), m.group(2)
            mask = match = 0
            for i, ch in enumerate(bits):        # bits[0] is MSB (bit 31)
                bitpos = 31 - i
                if ch == "?":
                    continue
                mask |= (1 << bitpos)
                if ch == "1":
                    match |= (1 << bitpos)
            out[name] = (mask, match)
    return out


# The generated package that *defines* the encodings — never counts as a "use".
DEFINITION_FILE = "riscv_instr.sv"
_TOKEN_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def parse_handled(repo, candidates, rtl_root=RTL_ROOT):
    """Return (handled, where) for the instruction names in `candidates`.

    An instruction is "implemented" if its name appears in one of the
    DECODER_FILES either qualified (`riscv_instr::NAME`) or unqualified
    (`NAME`, when the file does `import riscv_instr::*`). This is a decode
    check only — the logic behind the decoder is not verified. We match
    whole-word tokens against the known candidate set, which makes unqualified
    matching safe (no partial/substring hits), and we only scan files that
    reference the riscv_instr package at all, so an unrelated signal can't
    accidentally mark an instruction as live.
    """
    handled = set()
    where = defaultdict(set)
    root = os.path.join(repo, rtl_root)
    for dirpath, _, filenames in os.walk(root):
        for fn in filenames:
            if fn not in DECODER_FILES:
                continue
            p = os.path.join(dirpath, fn)
            try:
                with open(p, errors="ignore") as fh:
                    txt = fh.read()
            except OSError:
                continue
            if "riscv_instr" not in txt:          # file doesn't use the ISA pkg
                continue
            for t in set(_TOKEN_RE.findall(txt)):
                if t in candidates:
                    handled.add(t)
                    where[t].add(fn)
    return handled, {k: sorted(v) for k, v in where.items()}


# ---------------------------------------------------------------------------
# Conflict detection
# ---------------------------------------------------------------------------

def encodings_overlap(a, b):
    """True if some 32-bit word matches both a and b (compatible fixed bits)."""
    common = a.mask & b.mask
    return (a.match & common) == (b.match & common)


def find_conflicts(encs):
    """Pairwise overlaps between encodings from *different* origins.

    Bucketed by major opcode first to keep it tractable.
    """
    by_major = defaultdict(list)
    for e in encs:
        op = e.opcode
        by_major[op].append(e)
    conflicts = []
    for op, group in by_major.items():
        n = len(group)
        for i in range(n):
            for j in range(i + 1, n):
                a, b = group[i], group[j]
                if a.origin == b.origin:
                    continue
                if encodings_overlap(a, b):
                    conflicts.append((a, b))
    return conflicts


# ---------------------------------------------------------------------------
# Repo / spec discovery
# ---------------------------------------------------------------------------

def find_repo(start):
    d = os.path.abspath(start)
    while d != "/":
        if os.path.isdir(os.path.join(d, "hw", "ip", "snitch", "src")):
            return d
        d = os.path.dirname(d)
    raise SystemExit("Could not locate spatz repo root (looked for hw/ip/snitch/src)")


def ensure_upstream(cache_dir, commit):
    repo = os.path.join(cache_dir, "riscv-opcodes-upstream")
    if not os.path.isdir(os.path.join(repo, "extensions")):
        os.makedirs(cache_dir, exist_ok=True)
        print(f"  cloning upstream riscv-opcodes into {repo} ...")
        subprocess.check_call(["git", "clone", "--quiet", UPSTREAM_URL, repo])
    try:
        subprocess.check_call(["git", "-C", repo, "checkout", "--quiet", commit])
    except subprocess.CalledProcessError:
        print("  WARN: could not checkout pinned commit; using current HEAD",
              file=sys.stderr)
    return repo


# ---------------------------------------------------------------------------
# Classification
# ---------------------------------------------------------------------------

def classify(kind, name, handled):
    """Return the visual state for an instruction.

    spec + handled   -> 'impl'    (blue)
    spec + unhandled -> 'unimpl'  (gray)
    ext  + handled   -> 'impl'    (extension color)
    ext  + unhandled -> 'dead'    (black: in build, not decoded/executed)
    """
    is_handled = name in handled
    if kind == "spec":
        return "impl" if is_handled else "unimpl"
    return "impl" if is_handled else "dead"


# ---------------------------------------------------------------------------
# HTML rendering
# ---------------------------------------------------------------------------

def esc(s):
    return (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


def bits_str(mask, match):
    out = []
    for b in range(31, -1, -1):
        if not (mask >> b) & 1:
            out.append("?")
        else:
            out.append("1" if (match >> b) & 1 else "0")
    s = "".join(out)
    return s[0:1] + " " + s[1:8] + " " + s[8:13] + " " + s[13:18] + \
        " " + s[18:20] + " " + s[20:25] + " " + s[25:32]


def cell_color(entry):
    if entry is None:
        return COLOR_FREE
    if entry["conflict"]:
        return None  # rendered with hatch
    if entry["kind"] == "spec":
        return COLOR_IMPL if entry["state"] == "impl" else COLOR_UNIMPL
    # custom extension
    if entry["state"] == "dead":
        return COLOR_DEAD
    return EXT_COLORS.get(entry["origin"], "#888")


def render_html(ctx):
    P = []
    a = P.append
    a("<style>")
    a("""
    :root{--fg:#1b1f24;--bg:#ffffff;--line:#d0d5dd;--muted:#667085;--free:#f4f6f9;--panel:#fbfcfe;}
    @media (prefers-color-scheme: dark){:root{--fg:#e6e9ee;--bg:#14171c;--line:#333a44;--muted:#9aa4b2;--free:#1b1f26;--panel:#191d24;}}
    :root[data-theme=dark]{--fg:#e6e9ee;--bg:#14171c;--line:#333a44;--muted:#9aa4b2;--free:#1b1f26;--panel:#191d24;}
    :root[data-theme=light]{--fg:#1b1f24;--bg:#ffffff;--line:#d0d5dd;--muted:#667085;--free:#f4f6f9;--panel:#fbfcfe;}
    body{font-family:-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif;color:var(--fg);background:var(--bg);margin:0;padding:24px;}
    h1{font-size:20px;margin:0 0 4px;} h2{font-size:15px;margin:28px 0 8px;border-bottom:1px solid var(--line);padding-bottom:4px;}
    .sub{color:var(--muted);font-size:12px;margin-bottom:16px;}
    .legend{display:flex;flex-wrap:wrap;gap:10px 18px;font-size:12px;align-items:center;margin:10px 0 4px;}
    .legend span{display:inline-flex;align-items:center;gap:6px;}
    .sw{width:14px;height:14px;border-radius:3px;border:1px solid rgba(0,0,0,.2);display:inline-block;}
    .stats{display:flex;flex-wrap:wrap;gap:18px;font-size:12px;color:var(--muted);margin:8px 0 4px;}
    .stats b{color:var(--fg);}
    table.grid{border-collapse:collapse;font-size:10.5px;overflow-x:auto;display:block;max-width:100%;}
    table.grid th,table.grid td{border:1px solid var(--line);padding:0;text-align:center;}
    table.grid th{background:var(--panel);font-weight:600;padding:3px 5px;white-space:nowrap;position:sticky;top:0;z-index:2;}
    table.grid th.rowh{font-family:ui-monospace,Menlo,Consolas,monospace;text-align:right;padding:2px 6px;color:var(--muted);position:sticky;left:0;z-index:1;}
    td.cell{width:82px;height:26px;max-width:82px;overflow:hidden;cursor:default;}
    td.cell .nm{display:block;font-family:ui-monospace,Menlo,Consolas,monospace;font-size:9.5px;line-height:26px;color:#fff;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;padding:0 3px;}
    td.cell.free .nm{color:var(--muted);}
    td.cell.free{background:var(--free);}
    td.cell.conflict{background:repeating-linear-gradient(45deg,#e03131,#e03131 5px,#ff8787 5px,#ff8787 10px);}
    td.cell.conflict .nm{color:#fff;text-shadow:0 0 2px #000;}
    .tt{position:fixed;z-index:50;pointer-events:none;background:#000;color:#fff;font-size:11px;padding:7px 9px;border-radius:6px;max-width:340px;line-height:1.4;box-shadow:0 4px 16px rgba(0,0,0,.4);display:none;font-family:ui-monospace,Menlo,Consolas,monospace;}
    .conflicts{font-size:12px;} .conflicts .row{padding:5px 8px;border-left:3px solid #e03131;margin:5px 0;background:rgba(224,49,49,.08);border-radius:3px;font-family:ui-monospace,Menlo,Consolas,monospace;}
    .toolbar{margin:6px 0 2px;font-size:12px;color:var(--muted);}
    details{margin:6px 0;} summary{cursor:pointer;font-size:13px;font-weight:600;}
    .lst{font-size:11px;font-family:ui-monospace,Menlo,Consolas,monospace;display:flex;flex-wrap:wrap;gap:4px;margin:6px 0;}
    .chip{padding:1px 6px;border-radius:4px;color:#fff;}
    td.cell.dead .nm,.chip.dead{color:#e6e9ee;}
    td.cell.dead{outline:1px solid #5b6472;outline-offset:-1px;}
    table.bd{border-collapse:collapse;font-size:11.5px;margin:4px 0 2px;}
    table.bd th,table.bd td{border:1px solid var(--line);padding:3px 9px;text-align:right;font-variant-numeric:tabular-nums;}
    table.bd th{background:var(--panel);text-align:left;}
    table.bd td.exn{text-align:left;font-family:ui-monospace,Menlo,Consolas,monospace;}
    table.bd .dot{display:inline-block;width:10px;height:10px;border-radius:2px;margin-right:6px;vertical-align:middle;}
    table.bd tr.alldead td{color:var(--muted);}
    .conflicts .row.latent{border-left-color:#8a8f98;background:rgba(138,143,152,.10);}
    .gridscroll{max-height:440px;overflow:auto;border:1px solid var(--line);border-radius:4px;margin:4px 0;}
    td.cell.free.colclaim{background:repeating-linear-gradient(45deg,transparent,transparent 4px,rgba(224,49,49,.18) 4px,rgba(224,49,49,.18) 8px);}
    details>summary{padding:5px 2px;}
    details[open]>summary{margin-bottom:4px;}
    """)
    a("</style>")

    a(f"<h1>{esc(ctx['title'])}</h1>")
    canvas = "rv_v (RVV v1.0)" + (" + rv_zv* crypto" if ctx["crypto"] else "")
    a(f"<div class='sub'>Ground truth: upstream riscv/riscv-opcodes <b>{canvas}</b> "
      f"@ {ctx['spec_commit'][:10]} &nbsp;·&nbsp; "
      f"<b>implemented = decoded</b> in {', '.join(DECODER_FILES)} "
      f"(decode check only; the logic behind the decoder is not verified) "
      f"&nbsp;·&nbsp; generated read-only, no repo files modified</div>")
    a("<div class='sub'>This map answers four questions: "
      "<b>1)</b> which RVV v1.0 spec instructions the hardware decodes "
      "(<span style='color:" + COLOR_IMPL + "'>blue</span>) vs not (gray) · "
      "<b>2)</b> which custom instructions are hardware-backed (one hue per "
      "extension) · <b>3)</b> which are in the toolchain (riscv_instr.sv) but "
      "not decoded (black; see the dedicated section) · "
      "<b>4)</b> where the implementation deviates from the spec "
      "(mismatch / overlap panels).</div>")

    # legend
    a("<div class='legend'>")
    a(f"<span><i class='sw' style='background:{COLOR_IMPL}'></i>spec — implemented</span>")
    a(f"<span><i class='sw' style='background:{COLOR_UNIMPL}'></i>spec — not implemented (gray)</span>")
    a(f"<span><i class='sw' style='background:{COLOR_DEAD};border-color:#5b6472'></i>custom — in toolchain, not decoded (black)</span>")
    a("<span style='width:100%;height:0'></span>")
    a("<span style='color:var(--muted)'>custom, implemented:</span>")
    for ext in ctx["ext_files"] + [ORIGIN_RVV_LOCAL]:
        col = EXT_COLORS.get(ext, "#888")
        a(f"<span><i class='sw' style='background:{col}'></i>{esc(ext.replace('opcodes-',''))}</span>")
    a("<span style='width:100%;height:0'></span>")
    a(f"<span><i class='sw' style='background:linear-gradient(135deg,{COLOR_IMPL} 0 50%,{COLOR_UNIMPL} 50% 100%)'></i>spec slot — partially implemented</span>")
    a("<span><i class='sw' style='background:repeating-linear-gradient(45deg,#e03131,#e03131 3px,#ff8787 3px,#ff8787 6px)'></i>genuine conflict (bits overlap)</span>")
    a(f"<span><i class='sw' style='background:{COLOR_FREE};border:1px solid #999'></i>free</span>")
    a("</div>")

    # stats
    s = ctx["stats"]
    a("<div class='stats'>")
    a(f"<span>spec instrs: <b>{s['spec_total']}</b> "
      f"(<b style='color:{COLOR_IMPL}'>{s['spec_impl']}</b> impl / "
      f"<b>{s['spec_unimpl']}</b> not)</span>")
    a(f"<span>custom-ext instrs: <b>{s['ext_total']}</b> "
      f"(<b>{s['ext_live']}</b> live / <b style='color:#c1121f'>{s['ext_dead']}</b> dead)</span>")
    a(f"<span>active conflicts: <b style='color:#e03131'>{s['conflicts']}</b></span>")
    a(f"<span>latent (paper) conflicts: <b>{s['latent']}</b></span>")
    a(f"<span>spec deviations: <b style='color:#e8590c'>{s['rvv_mismatch']}</b> mismatches, "
      f"<b style='color:#e8590c'>{s['spec_overlap']}</b> custom-on-spec overlaps</span>")
    a(f"<span>OP-V free slots: <b>{s['opv_free']}/448</b></span>")
    a("</div>")

    # per-extension breakdown
    a("<h2>Extension status — which shipped extensions are live in this hardware</h2>")
    a("<table class='bd'><tr><th>extension</th><th>defined</th><th>implemented</th><th>dead (reclaimable)</th></tr>")
    for b in ctx["ext_breakdown"]:
        cls = " class='alldead'" if b["live"] == 0 and b["total"] else ""
        dead_txt = (f"<b style='color:#c1121f'>{b['dead']}</b>"
                    if b["dead"] else "0")
        a(f"<tr{cls}><td class='exn'><span class='dot' style='background:{b['color']}'></span>{esc(b['name'])}</td>"
          f"<td>{b['total']}</td><td>{b['live']}</td><td>{dead_txt}</td></tr>")
    a("</table>")
    a("<div class='sub'>“dead” = defined in the Makefile OPCODES build (present in riscv_instr.sv) "
      "but referenced by no decoder — encoding space reserved on paper, free to reclaim.</div>")

    # toolchain staleness (opcode files vs generated riscv_instr.sv)
    if ctx["tc_missing"] or ctx["tc_enc_diff"]:
        a("<div class='conflicts'><div class='row'>⚠ riscv_instr.sv is STALE vs the "
          "opcode files — run <code>make update_opcodes</code>. Missing: "
          f"{esc(', '.join(ctx['tc_missing']) or '—')} · encoding drift: "
          f"{esc(', '.join(ctx['tc_enc_diff']) or '—')}</div></div>")

    # point 3: in toolchain, not decoded — the full list, per extension
    n_dead = sum(len(d) for _, _, d in ctx["dead_groups"])
    a(f"<h2>⬛ In toolchain but not decoded by hardware "
      f"<span style='font-weight:400;color:var(--muted);font-size:12px'>— "
      f"{n_dead} instructions have a riscv_instr.sv localparam but appear in no "
      f"decoder; their encoding space is reclaimable</span></h2>")
    if not ctx["dead_groups"]:
        a("<div class='sub'>None — every toolchain-defined custom instruction is decoded. 🎉</div>")
    for origin, col, dead in ctx["dead_groups"]:
        a(f"<details><summary><span class='dot' style='display:inline-block;width:10px;"
          f"height:10px;border-radius:2px;background:{col};margin-right:6px'></span>"
          f"{esc(origin.replace('opcodes-', ''))} — {len(dead)} dead definitions</summary>")
        a("<div class='lst'>")
        for entry in sorted(dead, key=lambda e: e["match"] & 0xffffffff):
            a(f"<span class='chip dead' style='background:{COLOR_DEAD}' "
              f"data-tip=\"{tip_for(entry)}\">{esc(entry['name'])}</span>")
        a("</div></details>")

    # conflicts panels
    a("<h2>Active encoding conflicts <span style='font-weight:400;color:var(--muted);font-size:12px'>— both sides live in hardware</span></h2>")
    if ctx["conflicts"]:
        a("<div class='conflicts'>")
        for a1, b1 in ctx["conflicts"]:
            a(f"<div class='row'>{esc(a1.name)} <span style='color:#aaa'>[{esc(a1.origin)}]</span>"
              f" ⨯ {esc(b1.name)} <span style='color:#aaa'>[{esc(b1.origin)}]</span><br>"
              f"&nbsp;&nbsp;{bits_str(a1.mask,a1.match)}<br>&nbsp;&nbsp;{bits_str(b1.mask,b1.match)}</div>")
        a("</div>")
    else:
        a("<div class='sub'>None. No two hardware-implemented instructions collide. 🎉</div>")

    if ctx["latent"]:
        a("<details><summary>Latent conflicts (" + str(len(ctx["latent"])) +
          ") — collide only in the opcode files; ≥1 side is a dead definition</summary>")
        a("<div class='conflicts'>")
        for a1, b1 in ctx["latent"]:
            a(f"<div class='row latent'>{esc(a1.name)} <span style='color:#aaa'>[{esc(a1.origin)}]</span>"
              f" ⨯ {esc(b1.name)} <span style='color:#aaa'>[{esc(b1.origin)}]</span></div>")
        a("</div></details>")

    # point 4: deviations from the RVV spec
    a("<h2>⚠ Deviations from the RVV v1.0 spec "
      "<span style='font-weight:400;color:var(--muted);font-size:12px'>— where our "
      "implementation and the spec disagree about an encoding</span></h2>")
    if not ctx["rvv_mismatch"] and not ctx["spec_overlap"]:
        a("<div class='sub'>None. The implemented encodings agree with the spec. 🎉</div>")

    if ctx["rvv_mismatch"]:
        a("<div class='sub'><b>Encoding mismatches</b> — same instruction name, "
          "different fixed bits between local opcodes-rvv and the spec; the "
          "generated decoder masks deviate from the spec:</div>")
        a("<div class='conflicts'>")
        for loc1, up1 in ctx["rvv_mismatch"]:
            live = (" <span style='color:#e8590c'>(decoded in HW)</span>"
                    if loc1.name in ctx["handled"] else "")
            a(f"<div class='row' style='border-left-color:#e8590c;background:rgba(232,89,12,.08)'>"
              f"{esc(loc1.name)}{live}<br>"
              f"&nbsp;&nbsp;local: {bits_str(loc1.mask, loc1.match)}<br>"
              f"&nbsp;&nbsp;spec : {bits_str(up1.mask, up1.match)}</div>")
        a("</div>")

    if ctx["spec_overlap"]:
        a("<div class='sub'><b>Custom instructions on spec-claimed encodings</b> — "
          "a custom instruction's fixed bits overlap an RVV spec instruction:</div>")
        a("<div class='conflicts'>")
        for a1, b1, active in ctx["spec_overlap"]:
            sp, cu = (a1, b1) if a1.kind == "spec" else (b1, a1)
            tag = ("<span style='color:#e03131'>(both decoded — active collision)</span>"
                   if active else "<span style='color:#8a8f98'>(latent)</span>")
            a(f"<div class='row{'' if active else ' latent'}'>"
              f"{esc(cu.name)} <span style='color:#aaa'>[{esc(cu.origin)}]</span>"
              f" occupies {esc(sp.name)} <span style='color:#aaa'>[{esc(sp.origin)}]</span> {tag}<br>"
              f"&nbsp;&nbsp;custom: {bits_str(cu.mask, cu.match)}<br>"
              f"&nbsp;&nbsp;spec  : {bits_str(sp.mask, sp.match)}</div>")
        a("</div>")

    # OP-V grid
    a("<h2>OP-V space (major 0x57) — funct6 × category</h2>")
    a(f"<div class='sub'>Rows = funct6 (bits 31..26). Columns = category (funct3). "
      f"Empty = reusable slot. A “<code>name +N</code>” label means {s['opv_multi']} slot(s) "
      "hold several legal sub-encodings distinguished by vm/vs1/vs2/rs1 (e.g. vfmerge.vfm & "
      "vfmv.v.f) — hover to see each and its status. Red is reserved for genuine bit-overlaps.</div>")
    a(render_opv_grid(ctx))

    # custom major-opcode grids (funct7 × funct3)
    a("<h2>Custom major opcodes — funct7 × funct3 grids</h2>")
    a("<div class='sub'>CUSTOM-0/1/2/3 (0x0b/2b/5b/7b): DMA, post-increment, "
      "ipu, vfx, frep. Rows = funct7 (bits 31..25), cols = funct3. "
      "Red-hatched free cells = column claimed by an I-type immediate op. "
      "Click a header to expand/collapse.</div>")
    for op, name, ents in ctx["custom_grids"]:
        a(render_custom_grid(op, name, ents))

    # per-section extra tables
    for sec in ctx["sections"]:
        a(f"<h2>{esc(sec['title'])}</h2>")
        if sec.get("note"):
            a(f"<div class='sub'>{esc(sec['note'])}</div>")
        a(render_list_section(sec))

    # tooltip + theme JS
    a("""
    <div class='tt' id='tt'></div>
    <script>
    const tt=document.getElementById('tt');
    document.addEventListener('mouseover',e=>{const c=e.target.closest('[data-tip]');
      if(!c){tt.style.display='none';return;} tt.innerHTML=c.getAttribute('data-tip');
      tt.style.display='block';});
    document.addEventListener('mousemove',e=>{if(tt.style.display==='block'){
      let x=e.clientX+14,y=e.clientY+14; if(x+350>innerWidth)x=e.clientX-354;
      if(y+120>innerHeight)y=e.clientY-120; tt.style.left=x+'px';tt.style.top=y+'px';}});
    </script>
    """)
    return "<!-- generated by gen_encoding_viz.py -->\n" + "\n".join(P)


def tip_for(entry):
    if entry is None:
        return "free encoding slot"
    lines = [f"<b>{esc(entry['name'])}</b>",
             f"{esc(entry['origin'])}",
             f"{bits_str(entry['mask'], entry['match'])}"]
    if entry["state"] == "impl":
        w = entry.get("where") or []
        lines.append("decoded by" + (": " + esc(", ".join(w)) if w else " HW"))
    elif entry["kind"] == "spec":
        lines.append("spec — NOT implemented (no decoder reference)")
    else:
        lines.append("custom — in toolchain (riscv_instr.sv), NOT decoded (reclaimable)")
    if entry["conflict"]:
        lines.append("<span style='color:#ff8787'>⚠ CONFLICT: " +
                     esc(", ".join(entry["conflict_with"])) + "</span>")
    return "<br>".join(lines)


def cell_visual(entries, slot=None):
    """Aggregate a list of entries sharing one encoding slot into
    (css_class, inline_style, label, tooltip)."""
    n = len(entries)
    label = entries[0]["name"] + (f" +{n - 1}" if n > 1 else "")

    # genuine (active) overlap takes precedence -> red
    if any(e["conflict"] for e in entries):
        return "cell conflict", "", label, multi_tip(entries, slot)

    specs = [e for e in entries if e["kind"] == "spec"]
    exts = [e for e in entries if e["kind"] == "ext"]
    cls, style = "cell", ""
    if exts:
        live = [e for e in exts if e["state"] == "impl"]
        if live:
            style = f"background:{EXT_COLORS.get(live[0]['origin'], '#888')}"
        else:
            cls += " dead"
            style = f"background:{COLOR_DEAD}"
    else:
        impl = sum(1 for e in specs if e["state"] == "impl")
        if impl == len(specs):
            style = f"background:{COLOR_IMPL}"
        elif impl == 0:
            style = f"background:{COLOR_UNIMPL}"
        else:  # partially implemented slot -> two-tone (not red)
            style = (f"background:linear-gradient(135deg,{COLOR_IMPL} 0 50%,"
                     f"{COLOR_UNIMPL} 50% 100%)")
    return cls, style, label, multi_tip(entries, slot)


def multi_tip(entries, slot=None):
    if len(entries) == 1:
        return (esc(slot) + "<br>" if slot else "") + tip_for(entries[0])
    head = [f"<b>{len(entries)} sub-encodings</b>" +
            (f" in {esc(slot)}" if slot else "") + " (differ by other fields):"]
    for e in entries:
        if e["state"] == "impl":
            glyph = "<span style='color:#8fd18f'>✓ impl</span>"
        elif e["kind"] == "spec":
            glyph = "<span style='color:#c7ccd4'>✗ not impl</span>"
        else:
            glyph = "<span style='color:#ff8787'>✗ dead</span>"
        head.append(f"&nbsp;{esc(e['name'])} [{esc(e['origin'].replace('opcodes-',''))}] — {glyph}")
    return "<br>".join(head)


def render_opv_grid(ctx):
    grid = ctx["opv_grid"]   # {(funct6,cat): [entry, ...]}
    P = ["<table class='grid'>", "<tr><th class='rowh'>funct6</th>"]
    for cat in CATEGORY_ORDER:
        P.append(f"<th>{cat}</th>")
    P.append("</tr>")
    for f6 in range(64):
        P.append(f"<tr><th class='rowh'>{f6:06b}<br>0x{f6:02x}</th>")
        for cat in CATEGORY_ORDER:
            entries = grid.get((f6, cat))
            if not entries:
                P.append("<td class='cell free' data-tip=\"free encoding slot\">"
                         "<span class='nm'></span></td>")
                continue
            cls, style, label, tip = cell_visual(entries, f"funct6=0x{f6:02x} {cat}")
            P.append(f"<td class='{cls}' style='{style}' data-tip=\"{tip}\">"
                     f"<span class='nm'>{esc(label)}</span></td>")
        P.append("</tr>")
    P.append("</table>")
    return "".join(P)


def build_custom_grid(entries):
    """Slot custom-major entries into a funct7(31..25) × funct3(14..12) grid.

    Returns (cells, itype, other) where:
      cells[(funct7, funct3)] = [entry, ...]  (R-type, funct7 fixed;
          funct6-only ops occupy both funct7 rows 2*f6 and 2*f6+1)
      itype[funct3]           = [entry, ...]  (no funct7/funct6 -> whole column)
      other                   = [entry, ...]  (funct3 not fixed -> can't place)
    """
    cells = defaultdict(list)
    itype = defaultdict(list)
    other = []
    for ent in entries:
        m, v = ent["mask"], ent["match"]
        if (m >> 12) & 0x7 != 0x7:
            other.append(ent)
            continue
        f3 = (v >> 12) & 0x7
        if (m >> 25) & 0x7f == 0x7f:               # funct7 fully fixed
            cells[((v >> 25) & 0x7f, f3)].append(ent)
        elif (m >> 26) & 0x3f == 0x3f:             # funct6 fixed, bit25 free
            f6 = (v >> 26) & 0x3f
            cells[(f6 * 2, f3)].append(ent)
            cells[(f6 * 2 + 1, f3)].append(ent)
        else:                                       # I-type imm -> whole column
            itype[f3].append(ent)
    return cells, itype, other


def render_custom_grid(op, name, entries):
    cells, itype, other = build_custom_grid(entries)
    claimed_cols = set(itype)
    used = len(cells)
    free = sum(1 for f7 in range(128) for f3 in range(8)
               if (f7, f3) not in cells and f3 not in claimed_cols)
    live = sum(1 for e in entries if e["state"] == "impl")
    exts = sorted({e["origin"].replace("opcodes-", "") for e in entries})

    P = []
    P.append(f"<details><summary>{name} "
             f"(0x{op:02x}) — {len(entries)} instrs "
             f"[{esc(', '.join(exts))}] · {live} live · "
             f"{used} funct7×funct3 slots used · {free} free</summary>")
    if other:
        P.append("<div class='sub'>note: " + str(len(other)) +
                 " instr(s) here don't fix funct3 and can't be gridded — "
                 + esc(", ".join(e["name"] for e in other)) + "</div>")
    # I-type banner
    if itype:
        P.append("<div class='sub'>funct3 columns claimed by an I-type "
                 "(immediate, spans all funct7):</div><div class='lst'>")
        for f3 in sorted(itype):
            for ent in itype[f3]:
                cls, style, label, tip = cell_visual(
                    [ent], f"funct3={f3} (whole column, I-type)")
                chip = "chip" + (" dead" if "dead" in cls else "")
                sty = style or ("background:repeating-linear-gradient(45deg,"
                                "#e03131,#e03131 3px,#ff8787 3px,#ff8787 6px)")
                P.append(f"<span class='{chip}' style='{sty}' data-tip=\"{tip}\">"
                         f"f3={f3}: {esc(ent['name'])}</span>")
        P.append("</div>")
    # funct7 × funct3 grid
    P.append("<div class='gridscroll'><table class='grid'>")
    P.append("<tr><th class='rowh'>funct7</th>")
    for f3 in range(8):
        P.append(f"<th>f3={f3}</th>")
    P.append("</tr>")
    for f7 in range(128):
        P.append(f"<tr><th class='rowh'>{f7:07b}<br>0x{f7:02x}</th>")
        for f3 in range(8):
            ents = cells.get((f7, f3))
            if not ents:
                col_claimed = f3 in claimed_cols
                tip = (f"free — but funct3={f3} column is claimed by an I-type"
                       if col_claimed else "free encoding slot")
                fc = " colclaim" if col_claimed else ""
                P.append(f"<td class='cell free{fc}' data-tip=\"{tip}\">"
                         "<span class='nm'></span></td>")
                continue
            cls, style, label, tip = cell_visual(
                ents, f"funct7=0x{f7:02x} funct3={f3}")
            P.append(f"<td class='{cls}' style='{style}' data-tip=\"{tip}\">"
                     f"<span class='nm'>{esc(label)}</span></td>")
        P.append("</tr>")
    P.append("</table></div></details>")
    return "".join(P)


def render_list_section(sec):
    """Render a classified chip list for non-OP-V spaces (LS / config / custom)."""
    P = ["<div class='lst'>"]
    for entry in sorted(sec["entries"], key=lambda e: (e["match"] & 0xffffffff)):
        col = cell_color(entry)
        chip_cls = "chip"
        if entry["conflict"]:
            style = "background:repeating-linear-gradient(45deg,#e03131,#e03131 3px,#ff8787 3px,#ff8787 6px)"
        else:
            style = f"background:{col}"
            if entry["state"] == "dead":
                chip_cls += " dead"
        P.append(f"<span class='{chip_cls}' style='{style}' data-tip=\"{tip_for(entry)}\">{esc(entry['name'])}</span>")
    if not sec["entries"]:
        P.append("<span class='sub'>(none)</span>")
    P.append("</div>")
    return "".join(P)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def make_entry(e, handled, where):
    state = classify(e.kind, e.name, handled)
    return {
        "name": e.name, "origin": e.origin, "kind": e.kind,
        "mask": e.mask, "match": e.match,
        "state": state,
        "where": where.get(e.name, []),
        "conflict": False, "conflict_with": [],
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo", help="spatz repo root (auto-detected by default)")
    ap.add_argument("--out", default=None, help="output HTML path")
    ap.add_argument("--spec-commit", default=UPSTREAM_COMMIT)
    ap.add_argument("--crypto", action="store_true",
                    help="also draw the ratified vector-crypto exts (rv_zv*) on "
                         "the spec canvas (default: base rv_v v1.0 only)")
    args = ap.parse_args()

    here = os.path.dirname(os.path.abspath(__file__))
    repo = args.repo or find_repo(here)
    out = args.out or os.path.join(here, "encoding_map.html")
    local_opc = os.path.join(repo, "sw", "toolchain", "riscv-opcodes")
    sv_path = os.path.join(repo, "hw", "ip", "snitch", "src", "riscv_instr.sv")

    print("Encoding-space visualizer")
    print(f"  repo: {repo}")

    # 1. spec canvas (ground truth: rv_v v1.0; rv_zv* only on request)
    up = ensure_upstream(os.path.join(here, ".cache"), args.spec_commit)
    spec_files = list(SPEC_FILES)
    if args.crypto:
        extdir = os.path.join(up, "extensions")
        spec_files += sorted(f for f in os.listdir(extdir)
                             if f.startswith("rv_zv"))
    spec = []
    for f in spec_files:
        p = os.path.join(up, "extensions", f)
        if os.path.isfile(p):
            spec += parse_opcode_file(p, f"spec:{f}", "spec")
    print(f"  spec instrs parsed: {len(spec)} from {spec_files}")

    # 2. custom extensions (local) — the set currently enabled in the
    #    top-level Makefile OPCODES variable.
    ext_files = parse_makefile_opcodes(repo)
    if ext_files is None:
        print("  WARN: could not parse OPCODES from Makefile; using fallback list",
              file=sys.stderr)
        ext_files = list(EXT_FILES_FALLBACK)
    print(f"  enabled custom exts (Makefile OPCODES): "
          f"{', '.join(f.replace('opcodes-', '') for f in ext_files)}")
    exts = []
    for f in ext_files:
        p = os.path.join(local_opc, f)
        if os.path.isfile(p):
            exts += parse_opcode_file(p, f, "ext")
        else:
            print(f"  WARN: missing local ext file {f}", file=sys.stderr)
    print(f"  custom-ext instrs parsed: {len(exts)}")

    # 2b. local opcodes-rvv vs upstream spec canvas.
    #     * name absent upstream  -> local addition riding in the RVV space
    #       (vlx*, vfwdotp, legacy v0.9 leftovers) -> pseudo-extension, so its
    #       occupied encoding space is drawn instead of reading as free.
    #     * same name, different fixed bits -> encoding mismatch panel.
    rvv_mismatch = []
    rvv_path = os.path.join(local_opc, RVV_LOCAL_FILE)
    if os.path.isfile(rvv_path):
        up_by_name = {e.name: e for e in spec}
        n_local_only = 0
        for e in parse_opcode_file(rvv_path, ORIGIN_RVV_LOCAL, "ext"):
            u = up_by_name.get(e.name)
            if u is None:
                exts.append(e)
                n_local_only += 1
            elif (u.mask, u.match) != (e.mask, e.match):
                rvv_mismatch.append((e, u))
        print(f"  local {RVV_LOCAL_FILE}: {n_local_only} instrs not in spec v1.0, "
              f"{len(rvv_mismatch)} encoding mismatches vs upstream spec")
    else:
        print(f"  WARN: missing local {RVV_LOCAL_FILE}", file=sys.stderr)

    # 3. toolchain truth: riscv_instr.sv (generated from the OPCODES files).
    #    "in toolchain" = the name has a localparam there. Also verify the
    #    opcode files and the generated file have not drifted apart.
    sv_encs = parse_riscv_instr_sv(sv_path)
    tc_missing = sorted(e.name for e in exts if e.name not in sv_encs)
    tc_enc_diff = sorted(e.name for e in exts
                         if e.name in sv_encs
                         and sv_encs[e.name] != (e.mask, e.match))
    print(f"  toolchain (riscv_instr.sv): {len(sv_encs)} localparams; "
          f"opcode-file entries missing there: {len(tc_missing)}, "
          f"encoding drift: {len(tc_enc_diff)}")
    if tc_missing or tc_enc_diff:
        print("  WARN: riscv_instr.sv is stale vs the opcode files — "
              "run `make update_opcodes`", file=sys.stderr)

    # 4. implemented-in-hardware set: name referenced in one of the DECODER
    #    files (decode check only). Candidate names = everything we classify.
    candidates = {e.name for e in spec} | {e.name for e in exts}
    handled, where = parse_handled(repo, candidates)
    print(f"  implemented (decoded in {', '.join(DECODER_FILES)}): "
          f"{len(handled & candidates)}")

    # 5. conflicts (across all sources), split into active vs latent.
    #    active = both sides are actually decoded by HW (real silicon collision)
    #    latent = at least one side is defined-but-dead (paper collision only)
    all_encs = spec + exts
    raw_conflicts = find_conflicts(all_encs)
    conflicts, latent = [], []
    for a1, b1 in raw_conflicts:
        if a1.name in handled and b1.name in handled:
            conflicts.append((a1, b1))
        else:
            latent.append((a1, b1))

    # custom instructions sitting on encodings the RVV spec has claimed —
    # these are spec deviations, surfaced in their own panel.
    spec_overlap = [(a1, b1, a1.name in handled and b1.name in handled)
                    for a1, b1 in raw_conflicts
                    if {a1.kind, b1.kind} == {"spec", "ext"}]

    # dedup spec by name (aliases): keep first
    seen = set()
    spec_u = []
    for e in spec:
        if e.name in seen:
            continue
        seen.add(e.name)
        spec_u.append(e)

    # build entries and mark conflicts (only active conflicts colour a cell red)
    entries = {}
    for e in spec_u + exts:
        entries[id(e)] = make_entry(e, handled, where)
    for a1, b1 in conflicts:
        for x, y in ((a1, b1), (b1, a1)):
            ent = entries.get(id(x))
            if ent:
                ent["conflict"] = True
                ent["conflict_with"].append(f"{y.name}[{y.origin}]")

    # 5. place into OP-V grid + custom grids + list sections (drop NOTHING)
    opv_grid = {}   # (funct6, category) -> [entry, ...]
    ls_entries, cfg_entries = [], []
    custom_sections = OrderedDict((op, []) for op in CUSTOM_MAJORS)
    other_by_major = defaultdict(list)   # standard opcodes overloaded by exts

    for e in spec_u + exts:
        ent = entries[id(e)]
        op = e.opcode
        if op == OPV:
            f3 = e.field(14, 12)
            f6 = e.field(31, 26)
            cat = FUNCT3_CAT.get(f3) if f3 is not None else None
            if cat == "OPCFG" or f6 is None or cat is None:
                cfg_entries.append(ent)
            else:
                # A (funct6, category) cell can legitimately hold several
                # sub-encodings distinguished by vm / vs1 / vs2 / rs1 (e.g.
                # vfmerge.vfm vs vfmv.v.f, or the whole VFCVT family). Keep all;
                # red is reserved for GENUINE bit-overlaps (see active conflicts).
                opv_grid.setdefault((f6, cat), []).append(ent)
        elif op in (LOADFP, STOREFP):
            ls_entries.append(ent)
        elif op in CUSTOM_MAJORS:
            custom_sections[op].append(ent)
        else:
            # everything else (AMO, and standard opcodes overloaded by custom
            # extensions such as rv32b in OP/OP-IMM and smallfloat in OP-FP/FMA)
            other_by_major[op].append(ent)

    # custom-major grids (funct7 × funct3)
    custom_grids = [(op, CUSTOM_MAJORS[op], custom_sections[op])
                    for op in CUSTOM_MAJORS if custom_sections[op]]

    sections = []
    sections.append({"title": "Vector load/store (major 0x07 / 0x27)",
                     "note": "Unit-stride, strided, indexed, segment, whole-register, mask, fault-only-first.",
                     "entries": ls_entries})
    sections.append({"title": "Vector config (vset*)",
                     "note": "OP-V funct3=0x7; encoded via bits 31/30 rather than a plain funct6.",
                     "entries": cfg_entries})
    # standard opcodes overloaded by custom extensions (previously dropped)
    for op in sorted(other_by_major):
        ents = other_by_major[op]
        exts_here = sorted({e["origin"].replace("opcodes-", "")
                            for e in ents if e["kind"] == "ext"})
        label = MAJOR_NAMES.get(op, f"major 0x{op:02x}")
        note = ("custom ext(s) " + ", ".join(exts_here) +
                " overloading the standard " + label + " encoding") if exts_here \
               else "spec instructions in " + label
        sections.append({"title": f"{label} (0x{op:02x})",
                         "note": note, "entries": ents})

    dropped = sum(len(v) for v in other_by_major.values())
    print(f"  routed to standard/other majors (not dropped): {dropped}")

    # per-extension live/dead breakdown (incl. the not-in-spec rvv pseudo-ext)
    ext_breakdown = []
    for f in ext_files + [ORIGIN_RVV_LOCAL]:
        names = [e.name for e in exts if e.origin == f]
        live = sum(1 for n in names if n in handled)
        ext_breakdown.append({
            "name": f.replace("opcodes-", ""), "origin": f,
            "total": len(names), "live": live, "dead": len(names) - live,
            "color": EXT_COLORS.get(f, "#888"),
        })

    # "in toolchain, not in hardware": every custom-side instruction that has
    # a riscv_instr.sv localparam but no decoder reference, grouped by origin.
    dead_groups = []
    for f in ext_files + [ORIGIN_RVV_LOCAL]:
        dead = [entries[id(e)] for e in exts
                if e.origin == f and e.name not in handled
                and e.name in sv_encs]
        if dead:
            dead_groups.append((f, EXT_COLORS.get(f, "#888"), dead))

    stats = {
        "spec_total": len(spec_u),
        "spec_impl": sum(1 for e in spec_u if e.name in handled),
        "spec_unimpl": sum(1 for e in spec_u if e.name not in handled),
        "ext_total": len(exts),
        "ext_live": sum(1 for e in exts if e.name in handled),
        "ext_dead": sum(1 for e in exts if e.name not in handled),
        "conflicts": len(conflicts),
        "latent": len(latent),
        "rvv_mismatch": len(rvv_mismatch),
        "spec_overlap": len(spec_overlap),
        "opv_free": 448 - len(opv_grid),
        "opv_multi": sum(1 for v in opv_grid.values() if len(v) > 1),
    }

    ctx = {
        "title": "RVV & custom encoding-space map — Spatz",
        "spec_commit": args.spec_commit,
        "crypto": args.crypto,
        "ext_files": ext_files,
        "opv_grid": opv_grid,
        "custom_grids": custom_grids,
        "sections": sections,
        "conflicts": conflicts,
        "latent": latent,
        "rvv_mismatch": rvv_mismatch,
        "spec_overlap": spec_overlap,
        "dead_groups": dead_groups,
        "tc_missing": tc_missing,
        "tc_enc_diff": tc_enc_diff,
        "handled": handled,
        "ext_breakdown": ext_breakdown,
        "stats": stats,
    }
    html = render_html(ctx)
    with open(out, "w") as fh:
        fh.write(html)
    print(f"\n  wrote {out}")
    print(f"  spec {stats['spec_total']} (impl {stats['spec_impl']}, "
          f"unimpl {stats['spec_unimpl']}), ext {stats['ext_total']} "
          f"(live {stats['ext_live']}, dead {stats['ext_dead']}), "
          f"active-conflicts {stats['conflicts']}, latent {stats['latent']}, "
          f"OP-V free {stats['opv_free']}/448")


if __name__ == "__main__":
    main()
