#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
"""Find triplicated (TMR) and duplicated (DMR) flip-flops in the Spatz design.

The design is elaborated with Verilator (--xml-only) using the Bender file list.
The elaborated hierarchy/XML is analysed for the redundancy styles used in the
design, and a Tcl spec is written that `redundant_regs.tcl` consumes on the
synthesised netlist. Nothing is hardcoded to a particular module, so new
redundant blocks are picked up when the sources change.

Detected styles (see `report` output for what matched):
  var-letter   TMRG style: flop vars `<x>A/<x>B/<x>C` in one module.
  var-array    flop var that is an unpacked array of 3 (`foo_q_rep[2:0]`,
               `all_seq_q[2:0]`) in a module that contains TMR voters.
  inst-index   3 generate copies `gen_x[0..2]` of a scope, with TMR voters in
               the scope or its parent (e.g. `gen_tmr_part[i].i_tmr_part`).
  inst-letter  sibling instances `<x>A/<x>B/<x>C` (TMRG sub-blocks).
  dmr-inst     sibling instance pair `<x>_a/<x>_b` inside a *dmr* module.

The result is cached: it is regenerated only when Bender.lock (i.e. a
dependency revision), a local source file or this script changes.
"""
import argparse
import collections
import hashlib
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

VERSION = "1"
BRA = re.compile(r"^(.*)__BRA__(\d+)__KET__$")
VOTER_RE = re.compile(r"(TMR_voter|majorityVoter|bitwise_TMR)")
DMR_MOD_RE = re.compile(r"dmr", re.I)
TMR_MOD_RE = re.compile(r"tmr", re.I)


def sh(cmd, **kw):
    return subprocess.run(cmd, shell=True, check=True, universal_newlines=True, **kw)


def read_flist(path):
    return [l.strip() for l in open(path) if l.strip() and not l.startswith(("+", "-"))]


def cache_key(args, flist):
    h = hashlib.sha256()
    h.update(VERSION.encode())
    h.update(open(__file__, "rb").read())
    h.update(open(args.lock, "rb").read())
    h.update(" ".join(args.defines + args.targets + [args.top]).encode())
    root = os.path.realpath(args.root)
    for f in flist:  # local files only: dependencies are covered by the lock
        rf = os.path.realpath(f)
        if rf.startswith(root) and "/.bender/" not in rf and os.path.exists(rf):
            h.update(rf.encode())
            h.update(open(rf, "rb").read())
    return h.hexdigest()


def elaborate(args, workdir):
    os.makedirs(workdir, exist_ok=True)
    flist = os.path.join(workdir, "flist.txt")
    tgts = " ".join("-t " + t for t in args.targets)
    dfn = " ".join(args.defines)
    sh("cd %s && %s script flist-plus %s %s > %s" % (shlex.quote(args.root), args.bender, tgts, dfn, flist))
    xml = os.path.join(workdir, "design.xml")
    cmd = ("%s --xml-only --timing -Wno-fatal -Wno-lint -Wno-style --bbox-unsup "
           "--top-module %s %s -f %s --Mdir %s --xml-output %s"
           % (args.verilator, args.top, " ".join(args.defines + args.vl_defines), flist,
              os.path.join(workdir, "vl"), xml))
    r = subprocess.run(cmd, shell=True, universal_newlines=True, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT)
    if r.returncode != 0 or not os.path.exists(xml):
        sys.exit("verilator failed:\n" + r.stdout[-3000:])
    return flist, xml


def const_val(s):
    return int(s.split("h")[-1], 16)


def parse_xml(xml):
    """Return (dtypes, modules, cells)."""
    dt, mods, cells = {}, {}, []
    cur = None
    for ev, el in ET.iterparse(xml, events=("start", "end")):
        t = el.tag
        if ev == "start":
            if t == "module":
                cur = dict(orig=el.get("origName"), vars={}, flops=set(), insts=[])
                mods[el.get("name")] = cur
            continue
        if t == "unpackarraydtype":
            n = None
            r = el.find("range")
            if r is not None:
                cs = r.findall("const")
                if len(cs) == 2:
                    n = abs(const_val(cs[0].get("name")) - const_val(cs[1].get("name"))) + 1
            dt[el.get("id")] = n
        elif t == "var" and cur is not None:
            cur["vars"][el.get("name")] = el.get("dtype_id")
        elif t == "instance" and cur is not None:
            cur["insts"].append((el.get("name"), el.get("defName")))
        elif t == "always" and cur is not None:
            st = el.find("sentree")
            if st is not None and any(s.get("edgeType") in ("POS", "NEG") for s in st.iter("senitem")):
                for ad in el.iter("assigndly"):
                    kids = list(ad)
                    if kids:
                        for vr in kids[-1].iter("varref"):
                            cur["flops"].add(vr.get("name"))
        elif t == "cell":
            cells.append((el.get("hier"), el.get("submodname")))
        elif t == "module":
            cur = None
        if t in ("always", "instance", "var", "module", "cell"):
            el.clear()
    return dt, mods, cells


FOR_RE = re.compile(r"for\s*\(\s*(?:genvar\s+)?(\w+)\s*=\s*0\s*;\s*\1\s*<\s*([\w$.()]+)\s*;[^)]*\)\s*begin\s*:\s*(\w+)")
REPL_BOUND_RE = re.compile(r"^(3|(Num)?(Replicas|Copies|TmrCopies|NumTmr))$", re.I)


def replica_labels(files):
    """Generate-loop labels whose bound is the literal 3 (or a *Replicas/*Copies name).

    A loop over `FifoDepth`/`DataWidth`/... that merely happens to elaborate to 3
    iterations is not a replica, so only the source text can tell them apart.
    """
    ok, other = set(), set()
    for f in files:
        try:
            txt = open(f, errors="replace").read()
        except OSError:
            continue
        for m in FOR_RE.finditer(txt):
            (ok if REPL_BOUND_RE.match(m.group(2)) else other).add(m.group(3))
    return ok


def tok_re(tok):
    """Regex for one hierarchy token (netlist generate index may be [n] or _n_)."""
    m = BRA.match(tok)
    if m:
        return re.escape(m.group(1)) + r"(?:\[[0-9]+\]|_[0-9]+_)"
    return re.escape(tok)


def path_re(toks):
    return "[./]".join(tok_re(t) for t in toks)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    here = os.path.dirname(os.path.realpath(__file__))
    ap.add_argument("--root", default=os.path.realpath(os.path.join(here, "../../../../..")),
                    help="Spatz repo root (contains Bender.lock)")
    ap.add_argument("--top", default="spatz_cluster_wrapper")
    ap.add_argument("--out", default=os.path.join(here, "redundant_regs_spec.tcl"))
    ap.add_argument("--workdir", default=os.environ.get("REDUNDANCY_WORKDIR", ""),
                    help="scratch dir for the XML (default: a temp dir, removed afterwards)")
    ap.add_argument("--bender", default=os.environ.get("BENDER", "bender"))
    ap.add_argument("--verilator", default=os.environ.get("VERILATOR", "verilator-5.020 verilator"))
    ap.add_argument("-t", dest="targets", action="append",
                    default=None, help="bender targets (default: rtl spatz quadrilatero synthesis)")
    ap.add_argument("-D", dest="defines", action="append", default=None,
                    help="defines (default: -DDOUBLE_BW -DBUF_FPU -DTARGET_QUADRILATERO -DQUADRILATERO=1)")
    ap.add_argument("--force", action="store_true", help="ignore the cache")
    args = ap.parse_args()
    args.targets = args.targets or ["rtl", "spatz", "quadrilatero", "synthesis"]
    args.defines = args.defines or ["-DDOUBLE_BW", "-DBUF_FPU", "-DTARGET_QUADRILATERO", "-DQUADRILATERO=1"]
    args.vl_defines = ["-DSYNTHESIS", "-DTARGET_ZOIX"]  # skip simulation-only trace code
    args.lock = os.path.join(args.root, "Bender.lock")
    tmp = None if args.workdir else tempfile.mkdtemp(prefix="redundancy_")
    workdir = args.workdir or tmp

    # ---- cache: needs the file list, which bender produces quickly ----
    os.makedirs(workdir, exist_ok=True)
    pre = pre_flist = os.path.join(workdir, "flist.txt")
    tgts = " ".join("-t " + t for t in args.targets)
    sh("cd %s && %s script flist-plus %s %s > %s" % (shlex.quote(args.root), args.bender, tgts,
                                                     " ".join(args.defines), pre))
    key = cache_key(args, read_flist(pre))
    keyfile = args.out + ".key"
    if (not args.force and os.path.exists(args.out) and os.path.exists(keyfile)
            and open(keyfile).read().strip() == key):
        print("[redundancy] up to date (Bender.lock/sources unchanged): %s" % args.out)
        if tmp:
            shutil.rmtree(tmp, ignore_errors=True)
        return

    print("[redundancy] elaborating %s with verilator ..." % args.top)
    _, xml = elaborate(args, workdir)
    print("[redundancy] parsing XML ...")
    dt, mods, cells = parse_xml(xml)

    # ---- module / instance tables -------------------------------------------------
    modof = {}                          # hier path -> elaborated module name
    for h, s in cells:
        modof[h] = s
    inst_by_mod = collections.defaultdict(list)
    for h, s in cells:
        inst_by_mod[s].append(h.split("."))
    top_tok = args.top

    def rel(toks):                      # drop the top-level instance name
        return toks[1:] if toks and toks[0] == top_tok else toks

    def has_voter(mn):
        return any(VOTER_RE.search(d) for _, d in mods[mn]["insts"])

    voter_scopes, voter_parents = set(), set()
    for h, s in cells:
        if VOTER_RE.search(s):
            toks = h.split(".")
            voter_parents.add(".".join(toks[:-1]))
            for k, t in enumerate(toks):
                if BRA.match(t):
                    voter_scopes.add(".".join(toks[:k + 1]))

    specs = collections.OrderedDict()   # (kind, ...) -> payload ; keeps order stable
    report = collections.Counter()

    # ---- (1) flop vars inside a module: letters + arrays ------------------------------
    for mn, m in mods.items():
        flops = m["flops"]
        if not flops or mn not in inst_by_mod:
            continue
        groups = []                     # (kind, template, copies)
        for v in sorted(flops):
            if v[-1] in "ABC" and v[-1] == "A" and all(v[:-1] + c in flops for c in "BC"):
                groups.append(("tmr", "var-letter", v[:-1] + "@", "A B C"))
            n = dt.get(m["vars"].get(v))
            if n == 3 and has_voter(mn):
                groups.append(("tmr", "var-array", v, "0 1 2"))
        if not groups:
            continue
        scopes = sorted({path_re(rel(t)) for t in inst_by_mod[mn]})
        for sc in scopes:
            for kind, style, tmpl, copies in groups:
                specs[(kind, style, sc, tmpl, copies, tmpl.replace("@", ""))] = None
        for g in groups:
            report[(g[1], mn.split("__")[0])] += 1

    # ---- (2) replicated instance scopes -----------------------------------------------
    # 2a: generate index groups of exactly three with voter evidence
    scope_idx = collections.defaultdict(set)      # (prefix tokens, normalised token) -> {indices}
    for h, _ in cells:
        toks = h.split(".")
        for k, t in enumerate(toks):
            m = BRA.match(t)
            if m:
                scope_idx[(tuple(toks[:k]), m.group(1))].add(int(m.group(2)))
    repl = replica_labels(read_flist(pre_flist))
    skipped = collections.Counter()
    for (pre, name), idxs in sorted(scope_idx.items()):
        if len(idxs) != 3 or idxs != {0, 1, 2}:
            continue
        if name not in repl:
            skipped[name] += 1          # 3 iterations, but not a literal-3 replica loop
            continue
        sc = ".".join(pre + ("%s__BRA__0__KET__" % name,))
        parent = ".".join(pre)
        if sc not in voter_scopes and parent not in voter_parents:
            continue
        pre_re = path_re(rel(list(pre))) if rel(list(pre)) else ""
        sep = "[./]" if pre_re else ""
        pat_pre = pre_re + sep + re.escape(name)
        specs[("tmr", "inst-index", pat_pre, "", "0 1 2", name)] = None
        report[("inst-index", name)] += 1
    # 2b: letter suffix siblings (TMRG sub-blocks) and DMR pairs
    sib = collections.defaultdict(dict)
    for h, s in cells:
        toks = h.split(".")
        if len(toks) < 2:
            continue
        n = toks[-1]
        if VOTER_RE.search(s) or s.startswith("fanout"):
            continue                    # voters/fan-outs hold no state
        sib[(tuple(toks[:-1]), s)][n] = h
    for (pre, s), names in sorted(sib.items()):
        by_base = collections.defaultdict(dict)
        for n in names:
            if len(n) > 1 and n[-1] in "ABC":
                by_base[("L", n[:-1])][n[-1]] = n
            m = re.match(r"^(.*)_([ab])$", n)
            if m:
                by_base[("d", m.group(1))][m.group(2)] = n
        pmod = modof.get(".".join(pre), "")
        for (kind, base), cp in by_base.items():
            pre_re = path_re(rel(list(pre)))
            sep = "[./]" if pre_re else ""
            if kind == "L" and set(cp) == set("ABC") and (TMR_MOD_RE.search(pmod) or has_voter(pmod) if pmod else False):
                specs[("tmr", "inst-letter", pre_re + sep + re.escape(base), "", "A B C", base)] = None
                report[("inst-letter", base)] += 1
            elif kind == "d" and set(cp) == set("ab") and DMR_MOD_RE.search(mods.get(pmod, {}).get("orig", "") or ""):
                specs[("dmr", "dmr-inst", pre_re + sep + re.escape(base + "_"), "", "a b", base + "_")] = None
                report[("dmr-inst", base)] += 1

    # ---- write spec --------------------------------------------------------------------
    n_tmr = sum(1 for k in specs if k[0] == "tmr")
    n_dmr = sum(1 for k in specs if k[0] == "dmr")
    with open(args.out, "w") as f:
        f.write("# AUTOMATICALLY GENERATED by find_redundant_regs.py -- do not edit.\n")
        f.write("# Lock hash: %s\n" % key)
        f.write("# Entries are consumed by redundant_regs.tcl (get_tmr_regs / get_dmr_regs).\n")
        f.write("# var-*   : {kind style scope_regex var_template copies literal}\n")
        f.write("# inst-*  : {kind style scope_prefix_regex {} copies literal}  (copy token follows the prefix)\n")
        f.write("set ::redundancy::specs {\n")
        for (kind, style, sc, tmpl, copies, lit) in specs:
            f.write("  {%s %s {%s} {%s} {%s} {%s}}\n" % (kind, style, sc, tmpl, copies, lit))
        f.write("}\n")
    open(keyfile, "w").write(key + "\n")

    if tmp:
        shutil.rmtree(tmp, ignore_errors=True)
    print("[redundancy] wrote %s (%d TMR specs, %d DMR specs)" % (args.out, n_tmr, n_dmr))
    if skipped:
        print("[redundancy] 3-iteration generate scopes NOT treated as replicas (loop bound is not literal 3):")
        print("   " + ", ".join("%s x%d" % kv for kv in sorted(skipped.items())))
    for (style, name), n in sorted(report.items()):
        print("   %-12s %-40s x%d" % (style, name, n))


if __name__ == "__main__":
    main()
