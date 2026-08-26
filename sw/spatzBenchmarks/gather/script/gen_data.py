#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Bowen Wang <bowwang@iis.ee.ethz.ch>
#
# Emit the data header for the `gather` benchmark: a dense DRAM matrix
# (ROWS x DIM, fp16 - like an LLM KV cache) plus an index stream of G unique,
# sorted indices selecting the rows to gather. Driven by a json/hjson config:
#
#   ./gen_data.py -c script/gather_R2048_D128_G128.json
#
# Output: data/data_gather_R<ROWS>_D<DIM>_G<G>.h

import argparse
import pathlib

import numpy as np
import hjson


def c_fp16_array(name, arr):
    """Emit a flat __fp16 array (kept in .data, 64-byte aligned so each row
    starts on a cache-line boundary)."""
    vals = arr.reshape(-1)
    out = [f'static __fp16 {name}[{vals.size}] '
           f'__attribute__((section(".data"), aligned(64))) = {{']
    line = '  '
    for i, v in enumerate(vals):
        line += repr(float(v)) + (', ' if i != vals.size - 1 else '')
        if len(line) > 100:
            out.append(line)
            line = '  '
    if line.strip():
        out.append(line)
    out.append('};\n')
    return '\n'.join(out)


def c_u32_array(name, arr):
    out = [f'static uint32_t {name}[{len(arr)}] '
           f'__attribute__((section(".data"), aligned(64))) = {{']
    line = '  '
    for i, v in enumerate(arr):
        line += f'{int(v)}u' + (', ' if i != len(arr) - 1 else '')
        if (i % 12 == 11) or i == len(arr) - 1:
            out.append(line)
            line = '  '
    out.append('};\n')
    return '\n'.join(out)


def main():
    ap = argparse.ArgumentParser(description="Generate data for the gather kernel")
    ap.add_argument("-c", "--cfg", type=pathlib.Path, required=True)
    args = ap.parse_args()

    with args.cfg.open() as f:
        p = hjson.loads(f.read())

    rows = int(p["ROWS"])
    dim = int(p["DIM"])
    g = int(p["G"])
    seed = int(p.get("seed", 42))

    assert g <= rows, "cannot pick G unique indices from ROWS < G"

    rng = np.random.default_rng(seed)
    matrix = rng.standard_normal((rows, dim)).astype(np.float16)
    # G unique indices in [0, ROWS), sorted ascending.
    index = np.sort(rng.choice(rows, size=g, replace=False)).astype(np.uint32)

    hdr = (
        "// Copyright 2026 ETH Zurich and University of Bologna.\n"
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        "// SPDX-License-Identifier: Apache-2.0\n\n"
        "// This file was generated automatically by script/gen_data.py.\n\n"
        '#include "layer.h"\n\n'
        f"const gather_layer gather_l = {{\n"
        f"\t.ROWS = {rows},\n"
        f"\t.DIM = {dim},\n"
        f"\t.G = {g},\n"
        f"\t.dtype = FP16,\n"
        "};\n\n\n"
        + c_fp16_array("gather_matrix_dram", matrix) + "\n\n"
        + c_u32_array("gather_index_dram", index) + "\n"
    )

    out_dir = pathlib.Path(__file__).parent.parent / "data"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_file = out_dir / f"data_gather_R{rows}_D{dim}_G{g}.h"
    out_file.write_text(hdr)
    print(f"wrote {out_file} ({matrix.nbytes} B matrix, {g} indices)")


if __name__ == "__main__":
    main()
