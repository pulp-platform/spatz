#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51
#
# Visualize DRAM channel traffic recorded by DRAMSys (the *.tdb SQLite trace)
# and align it against software benchmark phases printed by the testbench.
#
# The testbench emits, on every benchmark_mark() write:
#     [PHASE] <t> ns mark=<value>
# Pass the run transcript with --transcript to draw those boundaries.
#
# Bandwidth is computed from the *bus occupancy* of each burst (the RD/MWR
# data-strobe window), so a curve can never exceed the physical peak; counting
# whole bursts at their start time instead would overshoot in narrow bins.
#
# Examples
#   dram_traffic_plot.py run.tdb --transcript run.log --elf run.elf -o out.png
#   dram_traffic_plot.py base.tdb sparse.tdb -o compare.png       # overlay

import argparse
import re
import sqlite3
import sys

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# DDR4-2400 x8 rank: 64-bit bus at 2400 MT/s = 19.21 GB/s. Overridable.
DEFAULT_PEAK_GBPS = 19.21
BURST_BYTES = 64                 # BL8 x 64-bit bus
NUM_BANKS = 16                   # DDR4: 4 bank groups x 4 banks
DEFAULT_DRAM_BASE = 0x8000_0000  # DRAMSys Address is relative to this


def load_traffic(tdb_path):
    """Per-burst arrays from a .tdb.

    Returns t0_ns, t1_ns (data-strobe begin/end, the bus-occupancy window of
    each burst), is_write, bank, addr, act_ns. `Bank` is the absolute bank
    index (0..15). Addresses are joined from Transactions via Phases.Transact.
    """
    con = sqlite3.connect(tdb_path)
    rows = con.execute(
        "SELECT p.PhaseName, p.DataStrobeBegin, p.DataStrobeEnd, p.Bank, t.Address "
        "FROM Phases p LEFT JOIN Transactions t ON p.Transact = t.ID "
        "WHERE p.PhaseName IN ('RD','MWR') AND p.DataStrobeBegin > 0 "
        "ORDER BY p.DataStrobeBegin"
    ).fetchall()
    acts = con.execute(
        "SELECT PhaseBegin FROM Phases WHERE PhaseName='ACT' ORDER BY PhaseBegin"
    ).fetchall()
    con.close()
    if not rows:
        raise RuntimeError(f"{tdb_path}: no RD/MWR data phases found")

    t0 = np.array([r[1] for r in rows], dtype=float) / 1000.0   # ps -> ns
    t1 = np.array([r[2] for r in rows], dtype=float) / 1000.0
    is_wr = np.array([r[0] == "MWR" for r in rows], dtype=bool)
    bank = np.array([r[3] for r in rows], dtype=int)
    addr = np.array([r[4] if r[4] is not None else -1 for r in rows], dtype=np.int64)
    act_ns = np.array([a[0] for a in acts], dtype=float) / 1000.0
    return t0, t1, is_wr, bank, addr, act_ns


def parse_phases(transcript_path):
    """Return [(time_ns, mark_value)] from [PHASE] transcript lines."""
    pat = re.compile(r"\[PHASE\]\s+(\d+)\s*ns\s+mark=(\d+)")
    out = []
    with open(transcript_path) as f:
        for line in f:
            m = pat.search(line)
            if m:
                out.append((float(m.group(1)), int(m.group(2))))
    return out


def exec_ranges(elf_path, dram_base):
    """DRAM-relative (lo, hi) ranges of executable ELF sections (code fetched
    by the icache)."""
    from elftools.elf.elffile import ELFFile
    from elftools.elf.constants import SH_FLAGS
    ranges = []
    with open(elf_path, "rb") as f:
        for sec in ELFFile(f).iter_sections():
            if (sec["sh_flags"] & SH_FLAGS.SHF_EXECINSTR) and sec["sh_size"] > 0:
                lo = sec["sh_addr"] - dram_base
                ranges.append((lo, lo + sec["sh_size"]))
    return ranges


def in_ranges(addr, ranges):
    m = np.zeros(len(addr), dtype=bool)
    for lo, hi in ranges:
        m |= (addr >= lo) & (addr < hi)
    return m


def occupancy_bw(t0, t1, mask, edges, peak):
    """GB/s per window from bus-occupancy: fraction of each window covered by a
    burst's data-strobe interval, times the peak rate. Cannot exceed peak
    because strobe intervals never overlap (single shared bus)."""
    win_ns = np.diff(edges)
    busy = np.zeros(len(edges) - 1)
    b, e = t0[mask], t1[mask]
    # keep only bursts overlapping the plotted window, clipped to its edges
    keep = (e > edges[0]) & (b < edges[-1])
    b = np.clip(b[keep], edges[0], edges[-1])
    e = np.clip(e[keep], edges[0], edges[-1])
    if len(b) == 0:
        return busy
    kb = np.clip(np.searchsorted(edges, b, "right") - 1, 0, len(busy) - 1)
    ke = np.clip(np.searchsorted(edges, e, "right") - 1, 0, len(busy) - 1)
    same = kb == ke
    np.add.at(busy, kb[same], (e - b)[same])
    # bursts straddling a window boundary (rare): clip per window
    for i in np.where(~same)[0]:
        for k in range(kb[i], ke[i] + 1):
            lo, hi = edges[k], edges[k + 1]
            busy[k] += max(0.0, min(e[i], hi) - max(b[i], lo))
    return busy / win_ns * peak


def draw_curve(ax, centers, series, peak, ylabel, avg, avg_active):
    """A stacked bandwidth subplot. `series` = list of (bw, color, label).
    avg / avg_active are fixed reference values (computed over the full run,
    not the plotted window) so they stay put when zooming with --tstart/--tend."""
    base = np.zeros_like(centers)
    for bw, color, label in series:
        ax.fill_between(centers, base, base + bw, step="mid", color=color,
                        alpha=0.85, label=label)
        base = base + bw
    ax.axhline(peak, color="k", ls="--", lw=1, label=f"peak {peak:.1f}")
    ax.axhline(avg, color="tab:green", ls=":", lw=1.3, label=f"avg {avg:.2f}")
    ax.axhline(avg_active, color="tab:red", ls="-.", lw=1.3,
               label=f"active avg {avg_active:.2f}")
    ax.set_ylabel(ylabel)
    ax.set_ylim(0, peak * 1.08)
    ax.legend(loc="upper left", fontsize=7, ncol=3)


def ref_stats(t0, t1, layers, full_edges, peak):
    """Fixed reference avg / active-avg for a subplot, computed over the whole
    run (full_edges) so they do not change with the plotted time window."""
    ref = np.zeros(len(full_edges) - 1)
    for mask, _, _ in layers:
        ref += occupancy_bw(t0, t1, mask, full_edges, peak)
    active = ref > 0
    return ref.mean(), (ref[active].mean() if active.any() else 0.0)


def plot_run(axes, tdb_path, phases, args, code_ranges, dma_wr_ranges):
    t0, t1, is_wr, bank, addr, act_ns = load_traffic(tdb_path)
    # Default view is the full first->last-burst span; --tstart/--tend zoom in.
    lo = args.tstart if args.tstart is not None else t0.min()
    hi = args.tend if args.tend is not None else t1.max()
    span = max(hi - lo, 1.0)
    win = args.window_ns if args.window_ns else max(span / 400.0, 1.0)
    edges = np.arange(lo, hi + win, win)
    centers = (edges[:-1] + edges[1:]) / 2.0
    # Fixed reference binning over the full run (independent of the zoom window).
    fspan = max(t1.max() - t0.min(), 1.0)
    fwin = args.window_ns if args.window_ns else max(fspan / 400.0, 1.0)
    full_edges = np.arange(t0.min(), t1.max() + fwin, fwin)
    ax_r, ax_w, ax_act, ax_bank = axes
    peak = args.peak_gbps

    # --- reads: data (blue) vs icache/code (gray) ---
    rd = ~is_wr
    if code_ranges:
        code = in_ranges(addr, code_ranges)
        read_layers = [(rd & ~code, "tab:blue", "data (DMA)"),
                       (rd & code, "0.55", "icache refill")]
    else:
        read_layers = [(rd, "tab:blue", "read")]

    # --- writes: DMA (orange) vs core/other e.g. spills (gray) ---
    if dma_wr_ranges:
        dma = in_ranges(addr, dma_wr_ranges)
        write_layers = [(is_wr & dma, "tab:orange", "DMA write"),
                        (is_wr & ~dma, "0.55", "core/other write")]
    else:
        # No DMA-write ranges declared: cannot attribute writes to the DMA
        # engine from the trace (see --dma-write-range), so mark them neutral.
        write_layers = [(is_wr, "0.55", "write (core/other)")]

    for ax, layers, ylabel in ((ax_r, read_layers, "read\nGB/s"),
                               (ax_w, write_layers, "write\nGB/s")):
        avg, avg_active = ref_stats(t0, t1, layers, full_edges, peak)
        series = [(occupancy_bw(t0, t1, m, edges, peak), c, l)
                  for m, c, l in layers]
        draw_curve(ax, centers, series, peak, ylabel, avg, avg_active)

    # --- row-activation rate ---
    if len(act_ns):
        rate, _ = np.histogram(act_ns, bins=edges)
        ax_act.fill_between(centers, 0, rate / (win / 1000.0), step="mid",
                            color="tab:red", alpha=0.7)
    ax_act.set_ylabel("ACT / us")

    # --- bank occupancy strip: accesses per (bank, window) ---
    heat, _, _ = np.histogram2d(bank, (t0 + t1) / 2.0,
                                bins=[np.arange(NUM_BANKS + 1) - 0.5, edges])
    im = ax_bank.imshow(heat, aspect="auto", origin="lower", cmap="magma",
                        extent=[lo, hi, -0.5, NUM_BANKS - 0.5],
                        interpolation="nearest")
    ax_bank.set_ylabel("bank")
    ax_bank.set_xlabel("time (ns)")
    ax_bank.set_yticks(range(0, NUM_BANKS, 4))
    cbar = ax_bank.figure.colorbar(im, ax=ax_bank, pad=0.01,
                                   fraction=0.03, aspect=10)
    cbar.set_label("accesses / window", fontsize=7)

    # --- phase boundaries ---
    if phases:
        for ax in axes:
            for (pt, _) in phases:
                if lo <= pt <= hi:
                    ax.axvline(pt, color="tab:green", ls="-", lw=0.9, alpha=0.6)
        ymax = ax_r.get_ylim()[1]
        for (pt, pv) in phases:
            if lo <= pt <= hi:
                ax_r.text(pt, ymax * 0.96, f" mark={pv}", color="tab:green",
                          fontsize=8, va="top", ha="left", rotation=90)

    mid = (t0 + t1) / 2.0
    in_win = (mid >= lo) & (mid <= hi)
    return dict(kib=np.count_nonzero(in_win) * BURST_BYTES / 1024.0,
                span_ns=span, n_act=int(((act_ns >= lo) & (act_ns <= hi)).sum()))


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("tdb", nargs="+", help="one or two DRAMSys .tdb files")
    ap.add_argument("--transcript", help="run log with [PHASE] lines")
    ap.add_argument("--elf", help="test ELF; reads to executable sections are "
                    "classified as icache refills (drawn gray)")
    ap.add_argument("--dram-base", type=lambda x: int(x, 0),
                    default=DEFAULT_DRAM_BASE,
                    help="base subtracted from ELF vaddrs (default 0x80000000)")
    ap.add_argument("--dma-write-range", action="append", default=[],
                    metavar="LO:HI",
                    help="DRAM-relative addr range whose writes are DMA "
                    "(orange); others are core/other (gray). Repeatable.")
    ap.add_argument("--tstart", type=float, default=None,
                    help="start of the plotted time window in ns (default: first burst)")
    ap.add_argument("--tend", type=float, default=None,
                    help="end of the plotted time window in ns (default: last burst)")
    ap.add_argument("--peak-gbps", type=float, default=DEFAULT_PEAK_GBPS)
    ap.add_argument("--window-ns", type=float, default=None,
                    help="time-bin width (default: span/400)")
    ap.add_argument("-o", "--out", default="dram_traffic.png")
    args = ap.parse_args()

    if len(args.tdb) > 2:
        ap.error("at most two .tdb files (overlay compares two runs)")

    phases = parse_phases(args.transcript) if args.transcript else []
    code_ranges = []
    if args.elf:
        try:
            code_ranges = exec_ranges(args.elf, args.dram_base)
        except ImportError:
            print("warning: pyelftools missing; skipping icache split", file=sys.stderr)
        except Exception as e:  # noqa: BLE001
            print(f"warning: could not read {args.elf}: {e}", file=sys.stderr)

    dma_wr_ranges = []
    for r in args.dma_write_range:
        a, b = r.split(":")
        dma_wr_ranges.append((int(a, 0), int(b, 0)))

    nruns = len(args.tdb)
    fig, axall = plt.subplots(4, nruns, figsize=(9 * nruns, 9), squeeze=False,
                              gridspec_kw={"height_ratios": [2, 2, 1, 1.4]},
                              sharex="col")
    stats = []
    for col, tdb in enumerate(args.tdb):
        axes = [axall[r][col] for r in range(4)]
        s = plot_run(axes, tdb, phases, args, code_ranges, dma_wr_ranges)
        axes[0].set_title(tdb.split("/")[-1])
        stats.append((tdb, s))

    fig.tight_layout()
    fig.savefig(args.out, dpi=130)
    print(f"wrote {args.out}")
    for tdb, s in stats:
        print(f"  {tdb.split('/')[-1]}: {s['kib']:.1f} KiB over "
              f"{s['span_ns']:.0f} ns | {s['n_act']} activations")


if __name__ == "__main__":
    sys.exit(main())
