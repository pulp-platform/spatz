#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Data generator for the sp-mdgather benchmark: molecular-dynamics
# cluster-pair gather (GROMACS-style cluster-pair algorithm, Pall et al. 2020).
#
# Atoms are grouped in clusters of 4. Coordinates are stored cluster-blocked
# in the padded "xyzq" X4 layout: per cluster c, 64 bytes at coords + c*64:
#   x[4] fp32 | y[4] fp32 | z[4] fp32 | q[4] fp32
# A pair list gives, for each i-cluster, LIST j-cluster indices (uint16).
#
# The golden model computes, for every i-atom against every atom of every
# listed j-cluster, the SIMPLIFIED FMA-only pair force (no div/sqrt):
#   dx = xi - xj; dy = yi - yj; dz = zi - zj
#   r2 = dx*dx + dy*dy + dz*dz
#   w  = max(CUT2 - r2, 0)
#   s  = qj * w * w
#   fx_i += s*dx; fy_i += s*dy; fz_i += s*dz
# accumulated in numpy float64 and cast to float32 at the end.
#
# Besides the raw pair list, an EXPANDED per-element index array is emitted
# for the plain-RVV (vluxei32) baseline: one uint16 entry per gathered
# ELEMENT (LIST*4 per i-cluster) with value
#   pairlist[c][j]*16 + lane   (lane = 0..3)
# i.e. the 16-byte-field-granular element index. The baseline kernel loads it
# with vle16 INSIDE the timed region (its memory traffic is charged fairly),
# widens it to e32 and shifts by 2 to obtain byte offsets
#   cluster*64 + lane*4
# without needing vrgather/vid (unsupported on Spatz).

import argparse
import ast
import pathlib
import re

import numpy as np

try:
    import hjson
except ModuleNotFoundError:
    hjson = None

np.random.seed(42)
global verbose


def load_hjson_like(text):
    if hjson is not None:
        return hjson.loads(text)

    text = re.sub(r"//.*", "", text)
    text = re.sub(r"([,{]\s*)([A-Za-z_][A-Za-z0-9_]*)\s*:", r'\1"\2":', text)
    text = text.replace("false", "False").replace("true", "True")
    return ast.literal_eval(text)


def array_to_cstr(a, fmt=float):
    """Emit a C initializer list (float literals or uint16 hex)."""
    out = "{"
    if fmt == float:
        for el in np.asarray(a).flat:
            # repr of np.float32 round-trips the fp32 value exactly
            out += "{}, ".format(el)
    elif fmt == "u16":
        for el in np.asarray(a, dtype=np.uint16).flat:
            out += "0x{:04x}, ".format(int(el))
    else:
        raise ValueError(f"Unsupported fmt={fmt}")
    out = out[:-2] + "}"
    return out


def golden_forces(coords, pairlist, cut2):
    """Reference forces in float64, cast to float32 at the very end.

    coords: [NC, 4, 4] fp32 (xyzq blocked: [:,0,:]=x, [:,1,:]=y, [:,2,:]=z,
    [:,3,:]=q), pairlist: [NC, LIST] int. Returns [NC, 4, 3] fp32 with the
    (fx, fy, fz) per i-atom.
    """
    c64 = coords.astype(np.float64)
    x, y, z, q = c64[:, 0, :], c64[:, 1, :], c64[:, 2, :], c64[:, 3, :]

    # j-data gathered per i-cluster: [NC, LIST, 4]
    xj, yj, zj, qj = x[pairlist], y[pairlist], z[pairlist], q[pairlist]

    # [NC, 4(i-atom), LIST, 4(j-atom)]
    dx = x[:, :, None, None] - xj[:, None, :, :]
    dy = y[:, :, None, None] - yj[:, None, :, :]
    dz = z[:, :, None, None] - zj[:, None, :, :]
    r2 = dx * dx + dy * dy + dz * dz
    w = np.maximum(np.float64(cut2) - r2, 0.0)
    s = qj[:, None, :, :] * w * w

    f = np.stack(
        [(s * dx).sum(axis=(2, 3)),
         (s * dy).sum(axis=(2, 3)),
         (s * dz).sum(axis=(2, 3))],
        axis=-1,
    )  # [NC, 4, 3]
    return f.astype(np.float32)


def emit_header_file(**kwargs):
    file_path = pathlib.Path(__file__).parent.parent / "data"
    file_path.mkdir(parents=True, exist_ok=True)

    nc = kwargs["NC"]
    list_len = kwargs["LIST"]
    file = file_path / f"data_mdgather_{nc}_{list_len}.h"

    emit_str = (
        "// Copyright 2026 ETH Zurich and University of Bologna.\n"
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        "// SPDX-License-Identifier: Apache-2.0\n\n"
        "// This file was generated automatically.\n\n"
    )
    emit_str += emit_mdgather_layer(**kwargs)

    with file.open("w") as f:
        f.write(emit_str)

    if verbose:
        print(f"Wrote: {file}")


def emit_mdgather_layer(**kwargs):
    nc = kwargs["NC"]
    list_len = kwargs["LIST"]
    cut2 = kwargs["CUT2"]
    coords = kwargs["coords"]          # [NC, 4, 4] fp32
    pairlist = kwargs["pairlist"]      # [NC, LIST] uint16
    pairlist_exp = kwargs["pairlist_exp"]  # [NC, LIST*4] uint16
    golden = kwargs["golden"]          # [NC, 4, 3] fp32

    layer_str = ""
    layer_str += "#include <stdint.h>\n"
    layer_str += '#include "layer.h"\n\n'

    layer_str += "const md_layer mdgather_l = {\n"
    layer_str += f"\t.NC = {nc},\n"
    layer_str += f"\t.LIST = {list_len},\n"
    layer_str += f"\t.CUT2 = {float(np.float32(cut2))!r}f\n"
    layer_str += "};\n\n\n"

    # Cluster-blocked xyzq coordinates: 16 fp32 (64 bytes) per cluster.
    layer_str += (
        f'static float mdgather_coords_dram[{nc} * 16] '
        f'__attribute__((section(".data"), aligned(64))) = '
        + array_to_cstr(coords)
        + ";\n\n\n"
    )

    # Raw pair list: LIST uint16 j-cluster indices per i-cluster (VLXBLK
    # variant gathers directly from these).
    layer_str += (
        f'static uint16_t mdgather_pairlist_dram[{nc} * {list_len}] '
        f'__attribute__((section(".data"))) = '
        + array_to_cstr(pairlist, fmt="u16")
        + ";\n\n\n"
    )

    # Expanded per-element index array for the plain-RVV baseline:
    # entry e of i-cluster c is pairlist[c][e/4]*16 + (e%4).
    layer_str += (
        f'static uint16_t mdgather_pairlist_exp_dram[{nc} * {list_len} * 4] '
        f'__attribute__((section(".data"))) = '
        + array_to_cstr(pairlist_exp, fmt="u16")
        + ";\n\n\n"
    )

    # Golden forces: (fx, fy, fz) per i-atom, [NC][4][3] row-major.
    layer_str += (
        f"static const float mdgather_golden[{nc} * 4 * 3] = "
        + array_to_cstr(golden)
        + ";\n\n\n"
    )

    return layer_str


def main():
    parser = argparse.ArgumentParser(
        description="Generate data for the sp-mdgather benchmark")
    parser.add_argument("-c", "--cfg", type=pathlib.Path, required=True,
                        help="Select param config file (HJSON/JSON)")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Set verbose")
    args = parser.parse_args()

    global verbose
    verbose = args.verbose

    with args.cfg.open() as f:
        param = load_hjson_like(f.read())

    if param["kernel"] != "MD-GATHER":
        raise ValueError('Unsupported kernel in cfg. Use kernel: "MD-GATHER".')

    nc = int(param["NC"])
    list_len = int(param["LIST"])
    cut2 = float(param["CUT2"])
    box = float(param.get("BOX", 3.0))

    # The expanded element indices (cluster*16 + lane) must fit in uint16.
    assert nc * 16 <= 65536, "NC too large for uint16 element indices"
    assert nc >= 2, "Need at least two clusters"

    # Coordinates in a box, charges in [0.5, 1.5], fp32, xyzq layout.
    coords = np.empty((nc, 4, 4), dtype=np.float32)
    coords[:, 0:3, :] = np.random.uniform(
        0.0, box, size=(nc, 3, 4)).astype(np.float32)
    coords[:, 3, :] = np.random.uniform(
        0.5, 1.5, size=(nc, 4)).astype(np.float32)

    # Pair list: LIST random j-cluster ids != self per i-cluster.
    r = np.random.randint(0, nc - 1, size=(nc, list_len))
    self_ids = np.arange(nc)[:, None]
    pairlist = (r + (r >= self_ids)).astype(np.uint16)
    assert np.all(pairlist != self_ids)

    # Expanded per-element indices for the RVV baseline (see header comment).
    pairlist_exp = (
        (pairlist.astype(np.uint32) * 16)[:, :, None]
        + np.arange(4, dtype=np.uint32)[None, None, :]
    ).reshape(nc, list_len * 4).astype(np.uint16)

    golden = golden_forces(coords, pairlist.astype(np.int64), cut2)

    emit_header_file(
        NC=nc, LIST=list_len, CUT2=cut2,
        coords=coords, pairlist=pairlist,
        pairlist_exp=pairlist_exp, golden=golden,
    )

    if verbose:
        print(f"Generated NC={nc} LIST={list_len} CUT2={cut2} BOX={box}, "
              f"golden |f| max = {np.abs(golden).max():.4f}")


if __name__ == "__main__":
    main()
