"""Generate the input image, parameter tables and golden output for the bilinear resize task.

Writes two C headers:
  <root>/data/data.h     image[] and params[] (inputs)
  <root>/Golden/gold.h   golden[] (expected output of resize_bilinear)

Usage: python gen_data.py [data.h path] [gold.h path]
"""
import sys
from pathlib import Path

import numpy as np

IN_H, IN_W, CH = 720, 1280, 3
OUT_H, OUT_W = 360, 640

# Packed params layout: name, element count, dtype. Little-endian, no padding.
PARAMS_LAYOUT = (("x0", OUT_W, "<u2"), ("x1", OUT_W, "<u2"), ("wx", OUT_W, "u1"),
                 ("y0", OUT_H, "<u2"), ("y1", OUT_H, "<u2"), ("wy", OUT_H, "u1"))


def axis_tables(in_size, out_size):
    d = np.arange(out_size, dtype=np.int64)
    # Half-pixel centers in Q8, integer arithmetic only.
    s = np.maximum(((2 * d + 1) * in_size * 256) // (2 * out_size) - 128, 0)
    i0 = np.minimum(s >> 8, in_size - 1)
    i1 = np.minimum(i0 + 1, in_size - 1)
    return i0, i1, s & 255


def pack_params(tables):
    """tables: dict name -> array. Returns (packed bytes, list of (name, offset))."""
    raw, offsets, pos = b"", [], 0
    for name, n, dt in PARAMS_LAYOUT:
        chunk = tables[name].astype(dt).tobytes()
        assert len(chunk) == n * np.dtype(dt).itemsize
        offsets.append((name, pos))
        raw += chunk
        pos += len(chunk)
    return raw, offsets


def unpack_params(raw):
    """Decode the packed bytes exactly as the C code will see them."""
    expected = sum(n * np.dtype(dt).itemsize for _, n, dt in PARAMS_LAYOUT)
    if len(raw) != expected:
        raise ValueError(f"params: expected {expected} bytes, got {len(raw)}")
    params, offset = {}, 0
    for name, n, dt in PARAMS_LAYOUT:
        params[name] = np.frombuffer(raw, dt, n, offset)
        offset += params[name].nbytes
    return params


def resize_bilinear(src, p):
    """src: uint8 (IN_H, IN_W, CH). Returns uint8 (OUT_H, OUT_W, CH). Same uint32 math as the C."""
    s = src.astype(np.uint32)
    wx = p["wx"].astype(np.uint32)[None, :, None]  # broadcast over rows and channels
    wy = p["wy"].astype(np.uint32)[:, None, None]  # broadcast over columns and channels
    x0, x1 = p["x0"].astype(np.intp), p["x1"].astype(np.intp)

    row0 = s[p["y0"].astype(np.intp)]  # (OUT_H, IN_W, CH)
    row1 = s[p["y1"].astype(np.intp)]
    top = row0[:, x0] * (256 - wx) + row0[:, x1] * wx  # (OUT_H, OUT_W, CH), Q8
    bottom = row1[:, x0] * (256 - wx) + row1[:, x1] * wx
    out = (top * (256 - wy) + bottom * wy + (1 << 15)) >> 16
    return out.astype(np.uint8)


def c_array(name, raw, align, per_line=16):
    vals = np.frombuffer(raw, dtype=np.uint8).tolist()
    lines = [f"static const uint8_t {name}[{len(vals)}] __attribute__((aligned({align}))) = {{"]
    for i in range(0, len(vals), per_line):
        lines.append("    " + ", ".join(f"0x{v:02x}" for v in vals[i:i + per_line]) + ",")
    lines.append("};")
    return "\n".join(lines)


def write_data_header(path, image, params, offsets):
    with open(path, "w") as f:
        f.write("// Auto-generated, do not edit.\n")
        f.write("#ifndef DATA_H\n#define DATA_H\n\n#include <stdint.h>\n\n")
        f.write(f"#define IN_H {IN_H}\n#define IN_W {IN_W}\n#define CH {CH}\n")
        f.write(f"#define OUT_H {OUT_H}\n#define OUT_W {OUT_W}\n\n")
        f.write("// Byte offsets into params (x0, x1, y0, y1 are uint16_t, wx and wy are uint8_t)\n")
        for name, off in offsets:
            f.write(f"#define PARAMS_{name.upper()}_OFF {off}\n")
        f.write(f"#define PARAMS_SIZE {len(params)}\n\n")
        f.write(c_array("image", image.tobytes(), 16) + "\n\n")
        f.write(c_array("params", params, 4) + "\n\n")
        f.write("#endif // DATA_H\n")


def write_gold_header(path, golden):
    with open(path, "w") as f:
        f.write("// Auto-generated, do not edit.\n")
        f.write("#ifndef GOLD_H\n#define GOLD_H\n\n#include <stdint.h>\n\n")
        f.write(f"#define GOLDEN_SIZE {golden.nbytes}\n\n")
        f.write(c_array("golden", golden.tobytes(), 16) + "\n\n")
        f.write("#endif // GOLD_H\n")


def main(data_path, gold_path):
    data_path, gold_path = Path(data_path), Path(gold_path)
    data_path.parent.mkdir(parents=True, exist_ok=True)
    gold_path.parent.mkdir(parents=True, exist_ok=True)

    rng = np.random.default_rng(0)
    image = rng.integers(0, 256, (IN_H, IN_W, CH), dtype=np.uint8)

    x0, x1, wx = axis_tables(IN_W, OUT_W)
    y0, y1, wy = axis_tables(IN_H, OUT_H)
    params, offsets = pack_params(dict(x0=x0, x1=x1, wx=wx, y0=y0, y1=y1, wy=wy))

    # Golden model runs on the packed bytes, so it sees exactly what the C code sees.
    golden = resize_bilinear(image, unpack_params(params))

    write_data_header(data_path, image, params, offsets)
    write_gold_header(gold_path, golden)
    print(f"wrote {data_path} (image {image.nbytes} B, params {len(params)} B)")
    print(f"wrote {gold_path} (golden {golden.nbytes} B, {OUT_H}x{OUT_W}x{CH})")


if __name__ == "__main__":
    root = Path(__file__).resolve().parent.parent
    data_out = sys.argv[1] if len(sys.argv) > 1 else root / "data" / "data.h"
    gold_out = sys.argv[2] if len(sys.argv) > 2 else root / "Golden" / "gold.h"
    main(data_out, gold_out)