# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

"""Helpers shared by the data generators of the O-POPE benchmarks."""

import argparse
import pathlib
import sys

import hjson
import numpy as np

# TCDM is 128 kB; the stack (1 kB/hart + TLS) lives at its top and is not protected
# by the L1 allocator, so keep a margin.
TCDM_BYTES = 128 * 1024
BUDGET_BYTES = 120 * 1024
ALLOC_ALIGN = 256  # snrt_l1alloc rounds every allocation up to this


def parse_cfg():
    parser = argparse.ArgumentParser(description="Generate O-POPE benchmark data")
    parser.add_argument("-c", "--cfg", type=pathlib.Path, required=True,
                        help="Select param config file")
    args = parser.parse_args()
    with args.cfg.open() as f:
        return args.cfg.stem, hjson.loads(f.read())


def words(t):
    """FP32 array-like -> flat uint32 bit patterns."""
    return np.ascontiguousarray(np.asarray(t, dtype=np.float32)).view(np.uint32).flatten()


def halves(t):
    """FP16-quantised array-like -> flat uint16 bit patterns."""
    return np.ascontiguousarray(np.asarray(t, dtype=np.float16)).view(np.uint16).flatten()


def pack_pairs(h16):
    """uint16 [A][B] -> uint32 [A/2][B]: word (p, j) = row 2p (bits 15:0) | row 2p+1 (bits 31:16)"""
    a = np.asarray(h16).astype(np.uint32)
    return a[0::2, :] | (a[1::2, :] << 16)


def carray(name, vals, ctype="uint32_t"):
    body = ", ".join(str(int(v)) if ctype != "uint32_t" else "0x{:08x}".format(int(v))
                     for v in vals)
    return (f'static {ctype} {name}[{len(vals)}] '
            f'__attribute__((section(".data"))) = {{{body}}};\n\n')


def check_footprint(buffers):
    """buffers: dict name -> bytes of each separate L1 allocation."""
    total = sum(-(-b // ALLOC_ALIGN) * ALLOC_ALIGN for b in buffers.values())
    print(f"L1 footprint: {total} B of {BUDGET_BYTES} B budget "
          f"({', '.join(f'{k}={v}' for k, v in buffers.items())})")
    if total > BUDGET_BYTES:
        sys.exit(f"ERROR: L1 footprint {total} B exceeds the {BUDGET_BYTES} B budget")
    return total


def write_header(app_dir, tag, defines, arrays):
    out = (
        "// Copyright 2025 ETH Zurich and University of Bologna.\n"
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        "// SPDX-License-Identifier: Apache-2.0\n\n"
        "// This file was generated automatically.\n\n"
    )
    for k, v in defines.items():
        out += f"#define {k} {v}\n"
    out += "\n" + "".join(arrays)
    data_dir = pathlib.Path(app_dir) / "data"
    data_dir.mkdir(parents=True, exist_ok=True)
    (data_dir / f"data_{tag}.h").write_text(out)
