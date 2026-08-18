#!/usr/bin/env python3
# Copyright 2022 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Author: Tim Fischer <fischeti@iis.ee.ethz.ch>
# Adapted for VQ-GEMM (two-codebook additive dequant) by ChatGPT.

import numpy as np
import torch
import argparse
import pathlib
import ast
import re

try:
    import hjson
except ModuleNotFoundError:
    hjson = None

np.random.seed(42)
torch.manual_seed(42)

device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")
global verbose


def randn_fp16(shape):
    return torch.randn(shape, dtype=torch.float32, device=device).to(torch.float16)


def rand_fp16(shape):
    return torch.rand(shape, dtype=torch.float32, device=device).to(torch.float16)


def rand_scale_fp16(shape):
    return (torch.rand(shape, dtype=torch.float32, device=device) + 0.5).to(torch.float16)


def load_hjson_like(text):
    if hjson is not None:
        return hjson.loads(text)

    text = re.sub(r"//.*", "", text)
    text = re.sub(r"([,{]\s*)([A-Za-z_][A-Za-z0-9_]*)\s*:", r'\1"\2":', text)
    text = text.replace("false", "False").replace("true", "True")
    return ast.literal_eval(text)


def array_to_cstr(a, fmt=float):
    """
    Emit a C initializer list.

    fmt == float: emits numeric literals (works for __fp16/float/double)
    fmt == "u8": emits hex bytes for uint8_t arrays
    fmt else: legacy fp8 bit-pack path (kept for compatibility)
    """
    out = "{"
    if fmt == float:
        if isinstance(a, np.ndarray):
            a = a.flat
        if isinstance(a, torch.Tensor):
            a = a.detach().cpu().numpy().flat
        for el in a:
            out += "{}, ".format(el)
    elif fmt == "u8":
        if isinstance(a, torch.Tensor):
            a = a.detach().cpu().numpy()
        a = np.asarray(a, dtype=np.uint8).flat
        for el in a:
            out += "0x{:02x}, ".format(int(el))
    else:
        # legacy fp8 path from original script
        for sign, exp, mant in zip(
            a["sign"].numpy().flat,
            a["exponent"].numpy().flat,
            a["mantissa"].numpy().flat,
        ):
            value = sign * 2**7 + exp * 2**2 + mant
            out += "0x{:02x}, ".format(value)

    out = out[:-2] + "}"
    return out


def emit_header_file(layer_type: str, **kwargs):
    """
    Writes a generated header into <repo_root>/data.

    Output directory is: Path(__file__).parent.parent / "data"
    so if this script is scripts/gen_data.py, output is data/<file>.h
    """
    file_path = pathlib.Path(__file__).parent.parent / "data"
    file_path.mkdir(parents=True, exist_ok=True)

    emit_str = (
        "// Copyright 2026 ETH Zurich and University of Bologna.\n"
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        "// SPDX-License-Identifier: Apache-2.0\n\n"
        "// This file was generated automatically.\n\n"
    )

    if layer_type == "GEMM":
        file = file_path / (
            "data_" + str(kwargs["M"]) + "_" + str(kwargs["N"]) + "_" + str(kwargs["K"]) + ".h"
        )
        emit_str += emit_GEMM_layer(**kwargs)
    elif layer_type == "VQ-GEMM":
        tag = str(kwargs.get("DATA_TAG", "")).strip()
        prefix = "data_vq_" + (tag + "_" if tag else "")
        file = file_path / (
            prefix + str(kwargs["M"]) + "_" + str(kwargs["N"]) + "_" + str(kwargs["K"]) + ".h"
        )
        emit_str += emit_VQ_GEMM_layer(**kwargs)
    else:
        raise ValueError(f"Unsupported layer_type={layer_type}")

    with file.open("w") as f:
        f.write(emit_str)

    if verbose:
        print(f"Wrote: {file}")


def emit_GEMM_layer(name="gemm", **kwargs):
    """
    Kept for compatibility. Emits element-wise golden output as gemm_golden,
    and does NOT emit gemm_checksum.
    """
    mat_A = kwargs["A"]
    mat_B = kwargs["B"]
    mat_C = kwargs["C"]
    result = kwargs["result"]

    m = kwargs["M"]
    n = kwargs["N"]
    k = kwargs["K"]

    layer_str = ""
    layer_str += '#include "layer.h"\n\n'
    layer_str += f"const gemm_layer {name}_l = {{\n"
    layer_str += f"\t.M = {m},\n"
    layer_str += f"\t.N = {n},\n"
    layer_str += f"\t.K = {k},\n"
    layer_str += f'\t.TA = {int(kwargs["ta"])},\n'
    layer_str += f'\t.TB = {int(kwargs["tb"])},\n'
    layer_str += f'\t.ALPHA = {kwargs["alpha"]},\n'
    layer_str += f'\t.dtype = FP{kwargs["prec"]},\n'
    layer_str += f'\t.expand = {kwargs["expand"]}\n'
    layer_str += "};\n\n\n"

    ctypes = {"64": "double", "32": "float", "16": "__fp16", "8": "char"}
    dtype = ctypes[str(kwargs["prec"])]

    layer_str += (
        f'static {dtype} {name}_A_dram [{m}*{k}] __attribute__((section(".data"))) = '
        + array_to_cstr(mat_A)
        + ";\n\n\n"
    )
    layer_str += (
        f'static {dtype} {name}_B_dram [{k}*{n}] __attribute__((section(".data"))) = '
        + array_to_cstr(mat_B)
        + ";\n\n\n"
    )
    layer_str += (
        f'static {dtype} {name}_C_dram [{m}*{n}] __attribute__((section(".data"))) = '
        + array_to_cstr(mat_C)
        + ";\n\n\n"
    )

    # Element-wise golden (A@B). If you want ALPHA*C included, adjust result generation in main().
    layer_str += (
        f"static const {dtype} {name}_golden[{m}*{n}] = "
        + array_to_cstr(result.reshape(-1))
        + ";\n\n\n"
    )
    return layer_str


def normalize_vq_layout(layout):
    layout = str(layout).strip().lower().replace("-", "_")
    nblk_aliases = {"n", "nblk", "out", "outblk", "output", "horizontal", "row", "aqlm"}
    kblk_aliases = {"k", "kblk", "in", "inblk", "input", "vertical", "column", "col", "vptq"}
    if layout in nblk_aliases:
        return "nblk"
    if layout in kblk_aliases:
        return "kblk"
    raise ValueError(f"Unsupported VQ_LAYOUT={layout!r}; use 'nblk' or 'kblk'")


def default_scale_axis(layout):
    # Current AQLM-like data uses one scale per K row.  VPTQ-like data uses one
    # scale per N/output column.
    return "row" if layout == "nblk" else "col"


def normalize_scale_axis(scale_axis):
    scale_axis = str(scale_axis).strip().lower().replace("-", "_")
    aliases = {
        "row": "row",
        "k": "row",
        "per_row": "row",
        "col": "col",
        "column": "col",
        "n": "col",
        "per_col": "col",
    }
    if scale_axis not in aliases:
        raise ValueError(f"Unsupported VQ_SCALE_AXIS={scale_axis!r}; use 'row' or 'col'")
    return aliases[scale_axis]


def make_vq_B_nblk_from_codebooks(cb0, cb1, idx0, idx1, scales, D=8):
    """
    Reconstruct dense B from two codebooks:

      B[k, j*D:(j+1)*D] = scales[k] * (cb0[idx0[k,j]] + cb1[idx1[k,j]])

    cb0: [CB0_N, D] fp16
    cb1: [CB1_N, D] fp16
    idx0/idx1: [K, N/D] uint8 (on CPU is fine)
    scales: [K] fp16 row scales
    returns B: [K, N] fp16 (on device of cb0/cb1)
    """
    K, blocks = idx0.shape
    N = blocks * D
    B = torch.empty((K, N), dtype=cb0.dtype, device=cb0.device)

    idx0_np = idx0.detach().cpu().numpy()
    idx1_np = idx1.detach().cpu().numpy()

    for k in range(K):
        scale = scales[k]
        for j in range(blocks):
            i0 = int(idx0_np[k, j])
            i1 = int(idx1_np[k, j])
            v = scales[k].float() * (cb0[i0].float() + cb1[i1].float())
            B[k, j * D : (j + 1) * D] = v.to(B.dtype)

    return B


def make_vq_B_kblk_from_codebooks(cb0, cb1, idx0, idx1, scales, D=8):
    """
    Reconstruct dense B from K-axis / input-axis blocks:

      B[j*D:(j+1)*D, n] = scales[n] * (cb0[idx0[n,j]] + cb1[idx1[n,j]])

    cb0: [CB0_N, D] fp16
    cb1: [CB1_N, D] fp16
    idx0/idx1: [N, K/D] uint8 (on CPU is fine)
    scales: [N] fp16 column scales
    returns B: [K, N] fp16 (on device of cb0/cb1)
    """
    N, blocks = idx0.shape
    K = blocks * D
    B = torch.empty((K, N), dtype=cb0.dtype, device=cb0.device)

    idx0_np = idx0.detach().cpu().numpy()
    idx1_np = idx1.detach().cpu().numpy()

    for n in range(N):
        scale = scales[n]
        for j in range(blocks):
            i0 = int(idx0_np[n, j])
            i1 = int(idx1_np[n, j])
            v = scales[n].float() * (cb0[i0].float() + cb1[i1].float())
            B[j * D : (j + 1) * D, n] = v.to(B.dtype)

    return B


def make_vq_B_from_codebooks(cb0, cb1, idx0, idx1, scales, D=8, layout="nblk"):
    layout = normalize_vq_layout(layout)
    if layout == "nblk":
        return make_vq_B_nblk_from_codebooks(cb0, cb1, idx0, idx1, scales, D=D)
    return make_vq_B_kblk_from_codebooks(cb0, cb1, idx0, idx1, scales, D=D)


def verify_vq_B_from_codebooks(B, cb0, cb1, idx0, idx1, scales, D, layout, samples=32):
    layout = normalize_vq_layout(layout)
    idx0_np = idx0.detach().cpu().numpy()
    idx1_np = idx1.detach().cpu().numpy()

    if layout == "nblk":
        K, blocks = idx0.shape
        for s in range(min(samples, K * blocks * D)):
            k = s % K
            block = (s // K) % blocks
            d = (s // (K * blocks)) % D
            expected = (
                scales[k].float()
                * (
                    cb0[int(idx0_np[k, block]), d].float()
                    + cb1[int(idx1_np[k, block]), d].float()
                )
            ).to(B.dtype)
            actual = B[k, block * D + d]
            assert actual.item() == expected.item()
    else:
        N, blocks = idx0.shape
        for s in range(min(samples, N * blocks * D)):
            n = s % N
            block = (s // N) % blocks
            d = (s // (N * blocks)) % D
            expected = (
                scales[n].float()
                * (
                    cb0[int(idx0_np[n, block]), d].float()
                    + cb1[int(idx1_np[n, block]), d].float()
                )
            ).to(B.dtype)
            actual = B[block * D + d, n]
            assert actual.item() == expected.item()


def emit_VQ_GEMM_layer(name="gemm", **kwargs):
    """
    Emits data for VQ-GEMM (two-codebook additive dequant, EI8 indices).
    Does NOT emit gemm_checksum. Emits element-wise golden output as gemm_golden.
    """
    A = kwargs["A"]
    C = kwargs["C"]
    B_cb0 = kwargs["B_cb0"]
    B_cb1 = kwargs["B_cb1"]
    idx0 = kwargs["idx0"]      # nblk: [K, N/CB_D], kblk: [N, K/CB_D]
    idx1 = kwargs["idx1"]      # nblk: [K, N/CB_D], kblk: [N, K/CB_D]
    scales = kwargs["scales"]  # nblk: [K], kblk: [N]
    golden = kwargs["golden"]  # [M, N] fp16 golden output to compare element-wise

    M = kwargs["M"]
    N = kwargs["N"]
    K = kwargs["K"]

    CB0_N = kwargs["CB0_N"]
    CB0_D = kwargs["CB0_D"]
    CB1_N = kwargs["CB1_N"]
    CB1_D = kwargs["CB1_D"]
    CB0_IDX_WIDTH = kwargs["CB0_IDX_WIDTH"]
    CB1_IDX_WIDTH = kwargs["CB1_IDX_WIDTH"]
    layout = normalize_vq_layout(kwargs.get("VQ_LAYOUT", "nblk"))
    scale_axis = normalize_scale_axis(kwargs.get("VQ_SCALE_AXIS", default_scale_axis(layout)))

    # Kernel assumptions (current RVV kernels use EI8 indices and D in {4,8,16}).
    assert CB0_D == CB1_D, "Both codebooks must use the same block length"
    assert CB0_D in (4, 8, 16, 32), "VLXBLK benchmark kernels support CB_D in {4,8,16,32}"
    assert CB0_IDX_WIDTH == 1 and CB1_IDX_WIDTH == 1, "Kernel uses EI8 indices (1 byte)"
    assert not kwargs["ta"] and not kwargs["tb"], "Generator assumes no transpose (matches kernel)"

    if layout == "nblk":
        assert (N % CB0_D) == 0, "N must be divisible by the codebook block length for nblk"
        assert scale_axis == "row", "nblk currently expects row/K scales"
        blocks0 = N // CB0_D
        blocks1 = N // CB1_D
        idx_outer = K
        scale_len = K
    else:
        assert (K % CB0_D) == 0, "K must be divisible by the codebook block length for kblk"
        assert scale_axis == "col", "kblk currently expects column/N scales"
        blocks0 = K // CB0_D
        blocks1 = K // CB1_D
        idx_outer = N
        scale_len = N
    assert blocks0 == blocks1, "Expected same D for both codebooks"
    assert tuple(idx0.shape) == (idx_outer, blocks0), "idx0 shape does not match VQ layout"
    assert tuple(idx1.shape) == (idx_outer, blocks1), "idx1 shape does not match VQ layout"
    assert int(scales.numel()) == scale_len, "scale vector length does not match VQ layout"

    size_idx0 = idx_outer * blocks0
    size_idx1 = idx_outer * blocks1

    layer_str = ""
    layer_str += '#include <stdint.h>\n'
    layer_str += '#include "layer.h"\n\n'
    layer_str += "#define VQ_LAYOUT_NBLK 0\n"
    layer_str += "#define VQ_LAYOUT_KBLK 1\n"
    layer_str += "#define VQ_SCALE_AXIS_ROW 0\n"
    layer_str += "#define VQ_SCALE_AXIS_COL 1\n"
    layer_str += f"#define VQ_LAYOUT VQ_LAYOUT_{layout.upper()}\n"
    layer_str += f"#define VQ_SCALE_AXIS VQ_SCALE_AXIS_{scale_axis.upper()}\n"
    layer_str += f"#define VQ_SCALE_LEN {scale_len}\n"
    layer_str += f"#define VQ_BLOCK_LEN {CB0_D}\n"
    layer_str += f"#define VQ_IDX_WIDTH_BYTES {CB0_IDX_WIDTH}\n\n"

    layer_str += "const vq_gemm_layer vq_gemm_l = {\n"
    layer_str += f"\t.M = {M},\n"
    layer_str += f"\t.N = {N},\n"
    layer_str += f"\t.K = {K},\n"
    layer_str += f"\t.TA = {int(kwargs['ta'])},\n"
    layer_str += f"\t.TB = {int(kwargs['tb'])},\n"
    layer_str += f"\t.ALPHA = {kwargs['alpha']},\n"
    layer_str += f"\t.dtype = FP{kwargs['prec']},\n"
    layer_str += f"\t.expand = {kwargs['expand']},\n"
    layer_str += f"\t.CB0_N = {CB0_N},\n"
    layer_str += f"\t.CB0_D = {CB0_D},\n"
    layer_str += f"\t.CB1_N = {CB1_N},\n"
    layer_str += f"\t.CB1_D = {CB1_D},\n"
    layer_str += f"\t.CB0_IDX_WIDTH = {CB0_IDX_WIDTH},\n"
    layer_str += f"\t.CB1_IDX_WIDTH = {CB1_IDX_WIDTH}\n"
    layer_str += "};\n\n\n"

    # We always emit FP16 for this kernel
    dtype = "__fp16"

    layer_str += (
        f'static {dtype} gemm_A_dram[{M}*{K}] __attribute__((section(".data"))) = '
        + array_to_cstr(A)
        + ";\n\n\n"
    )
    layer_str += (
        f'static {dtype} gemm_B_cb0_dram[{CB0_N}*{CB0_D}] __attribute__((section(".data"))) = '
        + array_to_cstr(B_cb0)
        + ";\n\n\n"
    )
    layer_str += (
        f'static {dtype} gemm_B_cb1_dram[{CB1_N}*{CB1_D}] __attribute__((section(".data"))) = '
        + array_to_cstr(B_cb1)
        + ";\n\n\n"
    )

    # IMPORTANT: sizes are emitted as constants (no Python // leaking into C)
    layer_str += (
        f'static uint8_t gemm_B_idx0_dram[{size_idx0}] __attribute__((section(".data"))) = '
        + array_to_cstr(idx0.reshape(-1), fmt="u8")
        + ";\n\n\n"
    )
    layer_str += (
        f'static uint8_t gemm_B_idx1_dram[{size_idx1}] __attribute__((section(".data"))) = '
        + array_to_cstr(idx1.reshape(-1), fmt="u8")
        + ";\n\n\n"
    )
    layer_str += (
        f'static {dtype} gemm_B_scales_dram[{scale_len}] __attribute__((section(".data"))) = '
        + array_to_cstr(scales)
        + ";\n\n\n"
    )

    layer_str += (
        f'static {dtype} gemm_C_dram[{M}*{N}] __attribute__((section(".data"))) = '
        + array_to_cstr(C)
        + ";\n\n\n"
    )

    # Element-wise golden output (flattened, row-major)
    layer_str += (
        f"static const {dtype} gemm_golden[{M}*{N}] = "
        + array_to_cstr(golden.reshape(-1))
        + ";\n\n\n"
    )

    return layer_str


def main():
    parser = argparse.ArgumentParser(description="Generate data for kernels")
    parser.add_argument(
        "-c",
        "--cfg",
        type=pathlib.Path,
        required=True,
        help="Select param config file kernel (HJSON/JSON)",
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Set verbose")
    args = parser.parse_args()

    global verbose
    verbose = args.verbose

    with args.cfg.open() as f:
        param = load_hjson_like(f.read())

    if param["kernel"] == "VQ-GEMM":
        # This generator targets your FP16 VQ kernel
        assert int(param["prec"]) == 16, "This generator targets prec=16 for your RVV kernel"

        M, N, K = int(param["M"]), int(param["N"]), int(param["K"])
        CB0_N, CB0_D = int(param["CB0_N"]), int(param["CB0_D"])
        CB1_N, CB1_D = int(param["CB1_N"]), int(param["CB1_D"])
        CB0_IDX_WIDTH = int(param.get("CB0_IDX_WIDTH", 1))
        CB1_IDX_WIDTH = int(param.get("CB1_IDX_WIDTH", 1))
        layout = normalize_vq_layout(param.get("VQ_LAYOUT", "nblk"))
        scale_axis = normalize_scale_axis(param.get("VQ_SCALE_AXIS", default_scale_axis(layout)))

        ta = bool(param.get("transpose_A", False))
        tb = bool(param.get("transpose_B", False))
        if ta or tb:
            raise ValueError("VQ-GEMM generator currently assumes no transpose (matches your kernel)")

        alpha = int(param.get("alpha", 0))
        if alpha != 0 and verbose:
            print("Note: alpha is nonzero in cfg, but golden currently computes A@B (no alpha*C).")

        # Generate A and initial C (FP16)
        A = randn_fp16((M, K))
        C = randn_fp16((M, N))

        num_codebooks = int(param.get("VQ_NUM_CODEBOOKS", 2))
        if num_codebooks not in (1, 2):
            raise ValueError("VQ_NUM_CODEBOOKS must be 1 or 2")

        # Codebooks: [CB*_N, CB*_D] (FP16)
        cb0 = randn_fp16((CB0_N, CB0_D))
        cb1 = randn_fp16((CB1_N, CB1_D))
        if num_codebooks == 1:
            cb1.zero_()

        if layout == "nblk":
            # AQLM-like layout: indices select D-wide blocks across the N/output axis
            # for each K row.  Current RVV/VLXBLK kernels consume this layout.
            assert scale_axis == "row", "nblk currently expects row/K scales"
            assert (N % CB0_D) == 0 and (N % CB1_D) == 0, "N must be divisible by CB*_D"
            blocks = N // CB0_D
            idx0 = torch.randint(0, CB0_N, (K, blocks), dtype=torch.uint8, device="cpu")
            idx1 = torch.randint(0, CB1_N, (K, blocks), dtype=torch.uint8, device="cpu")
            if num_codebooks == 1:
                idx1.zero_()
            scales = rand_scale_fp16((K,))
        else:
            # VPTQ-like layout: indices select D-tall blocks down the K/input axis
            # for each N/output column.  This is generator-only until matching
            # kblk kernels are added.
            assert scale_axis == "col", "kblk currently expects column/N scales"
            assert (K % CB0_D) == 0 and (K % CB1_D) == 0, "K must be divisible by CB*_D"
            blocks = K // CB0_D
            idx0 = torch.randint(0, CB0_N, (N, blocks), dtype=torch.uint8, device="cpu")
            idx1 = torch.randint(0, CB1_N, (N, blocks), dtype=torch.uint8, device="cpu")
            if num_codebooks == 1:
                idx1.zero_()
            scales = rand_scale_fp16((N,))

        # Reconstruct B and compute element-wise golden output
        B = make_vq_B_from_codebooks(cb0, cb1, idx0, idx1, scales, D=CB0_D, layout=layout)
        verify_vq_B_from_codebooks(B, cb0, cb1, idx0, idx1, scales, CB0_D, layout)

        # Golden output: if your kernel computes C = A@B (and ignores input C), use this:
        golden = torch.matmul(A.float(), B.float()).to(torch.float16)

        # If your kernel instead should do: C_out = A@B + alpha*C_in, use this instead:
        # golden = torch.matmul(A, B) + (alpha * C)

        kwargs = {
            "A": A,
            "C": C,
            "B_cb0": cb0,
            "B_cb1": cb1,
            "idx0": idx0,
            "idx1": idx1,
            "scales": scales,
            "golden": golden,
            "M": M,
            "N": N,
            "K": K,
            "ta": ta,
            "tb": tb,
            "alpha": alpha,
            "prec": int(param["prec"]),
            "expand": int(param.get("expand", 0)),
            "CB0_N": CB0_N,
            "CB0_D": CB0_D,
            "CB1_N": CB1_N,
            "CB1_D": CB1_D,
            "CB0_IDX_WIDTH": CB0_IDX_WIDTH,
            "CB1_IDX_WIDTH": CB1_IDX_WIDTH,
            "DATA_TAG": str(param.get("DATA_TAG", "")).strip(),
            "VQ_LAYOUT": layout,
            "VQ_SCALE_AXIS": scale_axis,
        }

        emit_header_file("VQ-GEMM", **kwargs)

        if verbose:
            print(
                f"Verified VQ {layout} reconstruction: "
                f"idx={tuple(idx0.shape)}, scales={tuple(scales.shape)}, B={tuple(B.shape)}"
            )

    else:
        raise ValueError('Unsupported kernel in cfg. Use kernel: "VQ-GEMM".')


if __name__ == "__main__":
    main()
