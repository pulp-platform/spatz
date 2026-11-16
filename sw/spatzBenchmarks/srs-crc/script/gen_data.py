#!/usr/bin/env python3
# Reads srsRAN CRC dumps (crc_{polynom}_{nbytes}.bin) and the manifest
# (crc_manifest.txt: "polynom nbytes order gold_checksum" per line) and emits
# one header per test-case into ../data/, in the Snitch runner's layout.
#
# Manifest line format (as written by the srsRAN dump patch):
#   <polynom_dec> <nbytes> <order> <gold_hex_or_dec>
#
# usage: gen_crc_data.py <bin_dir>
import sys
import pathlib


def parse_manifest(path):
    entries = []
    for line in pathlib.Path(path).read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        poly   = int(parts[0], 0)
        nbytes = int(parts[1], 0)
        order  = int(parts[2], 0)
        gold   = int(parts[3], 0)
        entries.append((poly, nbytes, order, gold))
    return entries


def emit(poly, nbytes, order, gold, base, out_dir):
    bin_path = base / f"crc_{poly}_{nbytes}.bin"
    data = list(bin_path.read_bytes())
    if len(data) != nbytes:
        print(f"WARN {bin_path}: {len(data)} bytes, manifest says {nbytes}",
              file=sys.stderr)

    out = []
    out.append("// AUTO-GENERATED from srsRAN CRC dump.")
    out.append("#ifndef DATA_CRC_H\n#define DATA_CRC_H\n#include <stdint.h>\n")
    out.append(f"#define CRC_MSG_LEN {len(data)}")
    out.append(f"#define CRC_POLY    0x{poly:X}ULL")
    out.append(f"#define CRC_ORDER   {order}")
    out.append(f"#define CRC_GOLD    0x{gold:X}ULL\n")
    out.append(f"static const uint8_t crc_msg_dram[CRC_MSG_LEN] "
               f"__attribute__((section(\".data\"))) = {{")
    for i in range(0, len(data), 24):
        out.append("  " + ",".join(str(x) for x in data[i:i + 24]) + ",")
    out.append("};\n")
    out.append("#endif")

    out_path = out_dir / f"data_{poly}_{nbytes}.h"
    out_path.write_text("\n".join(out))
    print(f"poly=0x{poly:X} nbytes={nbytes} order={order} gold=0x{gold:X} -> {out_path}")


def main():
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} <bin_dir>", file=sys.stderr)
        sys.exit(1)
    base = pathlib.Path(sys.argv[1])
    out_dir = pathlib.Path(__file__).parent.parent / "data"
    out_dir.mkdir(parents=True, exist_ok=True)

    for poly, nbytes, order, gold in parse_manifest(base / "crc_manifest.txt"):
        emit(poly, nbytes, order, gold, base, out_dir)


if __name__ == "__main__":
    main()