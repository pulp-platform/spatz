#!/usr/bin/env python3
# Parse decoder vsim logs (cycles_ls{N}.log, with QuestaSim's leading "# ")
# and build a summary table across lifting sizes.
#
# Per log it extracts: load_soft_bits, per-layer (v2c/c2v/soft) summed over all
# layers, hard_decision, decode result, and end-to-end total.
#
# usage: parse_decoder_logs.py <log> [<log> ...]
#   e.g. parse_decoder_logs.py logs/cycles_ls*.log
import re
import sys
import csv
import pathlib


def parse(path):
    txt = pathlib.Path(path).read_text()

    load = re.search(r'load_soft_bits took (\d+) cycles', txt)
    hard = re.search(r'hard_decision took (\d+) cycles', txt)
    res  = re.search(r'DECODE (OK|FAIL)', txt)

    v2c = [int(x) for x in re.findall(r'Update_variable_to_check took (\d+) cycles', txt)]
    c2v = [int(x) for x in re.findall(r'Update_check_to_variable_messages took (\d+) cycles', txt)]
    sft = [int(x) for x in re.findall(r'Update_soft_bits took (\d+) cycles', txt)]

    return {
        "load":   int(load.group(1)) if load else 0,
        "hard":   int(hard.group(1)) if hard else 0,
        "decode": res.group(1) if res else "?",
        "nlayers": min(len(v2c), len(c2v), len(sft)),
        "v2c": sum(v2c),
        "c2v": sum(c2v),
        "soft": sum(sft),
    }


def ls_from_name(path):
    m = re.search(r'ls(\d+)', pathlib.Path(path).name)
    return int(m.group(1)) if m else -1


def main():
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} <log> [<log> ...]", file=sys.stderr)
        sys.exit(1)

    rows = []
    for path in sys.argv[1:]:
        d = parse(path)
        ls = ls_from_name(path)
        total = d["load"] + d["v2c"] + d["c2v"] + d["soft"] + d["hard"]
        rows.append((ls, d["nlayers"], d["load"], d["v2c"], d["c2v"],
                     d["soft"], d["hard"], total, d["decode"]))

    rows.sort(key=lambda r: r[0])

    hdr = ("LS", "layers", "load", "v2c", "c2v", "soft", "hard", "end2end", "decode")
    print(f"{hdr[0]:>5} {hdr[1]:>6} {hdr[2]:>7} {hdr[3]:>8} {hdr[4]:>9} "
          f"{hdr[5]:>8} {hdr[6]:>6} {hdr[7]:>9} {hdr[8]:>6}")
    for r in rows:
        print(f"{r[0]:>5} {r[1]:>6} {r[2]:>7} {r[3]:>8} {r[4]:>9} "
              f"{r[5]:>8} {r[6]:>6} {r[7]:>9} {r[8]:>6}")

    out = "decoder_cycles_summary.csv"
    with open(out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(hdr)
        w.writerows(rows)
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()