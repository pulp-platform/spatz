#!/usr/bin/env python3
# Parse CRC vsim logs (crc_{poly}_{nbytes}.log, with QuestaSim's leading "# ")
# and build a summary table: poly name, nbytes, cycles, OK/FAIL.
#
# usage: parse_crc_logs.py <log> [<log> ...]
#   e.g. parse_crc_logs.py logs/crc_*.log
import re
import sys
import csv
import pathlib

# decimal polynom -> human name
POLY_NAME = {
    25578747: "CRC24A",   # 0x1864CFB
    25165923: "CRC24B",   # 0x1800063
    28487959: "CRC24C",   # 0x1B2B117
    69665:    "CRC16",    # 0x11021
    3617:     "CRC11",    # 0xE21
    97:       "CRC6",     # 0x61
}


def parse(path):
    txt = pathlib.Path(path).read_text()
    cyc = re.search(r'The execution took (\d+) cycles', txt)
    res = re.search(r'CRC 0x[0-9A-Fa-f]+ (OK)', txt)
    fail = re.search(r'CRC got=0x[0-9A-Fa-f]+ exp=0x[0-9A-Fa-f]+ (FAIL)', txt)
    # got/exp for failing cases
    ge = re.search(r'CRC got=0x([0-9A-Fa-f]+) exp=0x([0-9A-Fa-f]+) FAIL', txt)
    okval = re.search(r'CRC 0x([0-9A-Fa-f]+) OK', txt)

    status = "OK" if res else ("FAIL" if fail else "?")
    got = exp = ""
    if okval:
        got = exp = "0x" + okval.group(1)
    elif ge:
        got = "0x" + ge.group(1)
        exp = "0x" + ge.group(2)

    return {
        "cycles": int(cyc.group(1)) if cyc else 0,
        "status": status,
        "got": got,
        "exp": exp,
    }


def keys_from_name(path):
    m = re.search(r'crc_(\d+)_(\d+)', pathlib.Path(path).name)
    if not m:
        return (-1, -1)
    return (int(m.group(1)), int(m.group(2)))


def main():
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} <log> [<log> ...]", file=sys.stderr)
        sys.exit(1)

    rows = []
    for path in sys.argv[1:]:
        poly, nbytes = keys_from_name(path)
        d = parse(path)
        name = POLY_NAME.get(poly, f"0x{poly:X}")
        rows.append((name, poly, nbytes, d["cycles"], d["status"], d["got"], d["exp"]))

    # sort by poly name then size
    rows.sort(key=lambda r: (r[0], r[2]))

    hdr = ("CRC", "poly", "nbytes", "cycles", "status", "got", "exp")
    print(f"{hdr[0]:>7} {hdr[2]:>7} {hdr[3]:>8} {hdr[4]:>7} {hdr[5]:>8} {hdr[6]:>8}")
    n_fail = 0
    for name, poly, nbytes, cyc, st, got, exp in rows:
        if st != "OK":
            n_fail += 1
        print(f"{name:>7} {nbytes:>7} {cyc:>8} {st:>7} {got:>8} {exp:>8}")

    print(f"\n{len(rows)} cases, {n_fail} not OK")

    out = "crc_summary.csv"
    with open(out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(hdr)
        w.writerows(rows)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()