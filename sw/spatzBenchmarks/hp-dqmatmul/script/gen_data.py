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
import hjson

np.random.seed(42)
torch.manual_seed(42)

device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")
global verbose


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
        file = file_path / (
            "data_dq_" + str(kwargs["M"]) + "_" + str(kwargs["N"]) + "_" + str(kwargs["K"]) + ".h"
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


def make_vq_B_from_codebooks(cb0, cb1, idx0, idx1, D=8):
    """
    Reconstruct dense B from two codebooks:

      B[k, j*D:(j+1)*D] = cb0[idx0[k,j]] + cb1[idx1[k,j]]

    cb0: [CB0_N, D] fp16
    cb1: [CB1_N, D] fp16
    idx0/idx1: [K, N/D] uint8 (on CPU is fine)
    returns B: [K, N] fp16 (on device of cb0/cb1)
    """
    K, blocks = idx0.shape
    N = blocks * D
    B = torch.empty((K, N), dtype=cb0.dtype, device=cb0.device)

    idx0_np = idx0.detach().cpu().numpy()
    idx1_np = idx1.detach().cpu().numpy()

    for k in range(K):
        for j in range(blocks):
            i0 = int(idx0_np[k, j])
            i1 = int(idx1_np[k, j])
            v = cb0[i0] + cb1[i1]
            B[k, j * D : (j + 1) * D] = v

    return B


def emit_VQ_GEMM_layer(name="gemm", **kwargs):
    """
    Emits data for VQ-GEMM (two-codebook additive dequant, EI8 indices, CB_D=8).
    Does NOT emit gemm_checksum. Emits element-wise golden output as gemm_golden.
    """
    A = kwargs["A"]
    C = kwargs["C"]
    B_cb0 = kwargs["B_cb0"]
    B_cb1 = kwargs["B_cb1"]
    idx0 = kwargs["idx0"]      # [K, N/8] uint8
    idx1 = kwargs["idx1"]      # [K, N/8] uint8
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

    # Kernel assumptions (your current RVV kernel)
    assert CB0_D == 8 and CB1_D == 8, "This kernel expects CB_D=8"
    assert (N % 8) == 0, "For your kernel test setup, N should be multiple of 8"
    assert CB0_IDX_WIDTH == 1 and CB1_IDX_WIDTH == 1, "Kernel uses EI8 indices (1 byte)"
    assert not kwargs["ta"] and not kwargs["tb"], "Generator assumes no transpose (matches kernel)"

    blocks0 = N // CB0_D
    blocks1 = N // CB1_D
    assert blocks0 == blocks1, "Expected same D for both codebooks"

    size_idx0 = K * blocks0
    size_idx1 = K * blocks1

    layer_str = ""
    layer_str += '#include <stdint.h>\n'
    layer_str += '#include "layer.h"\n\n'

    layer_str += "const dq_gemm_layer dq_gemm_l = {\n"
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
        param = hjson.loads(f.read())

    if param["kernel"] == "VQ-GEMM":
        # This generator targets your FP16 VQ kernel
        assert int(param["prec"]) == 16, "This generator targets prec=16 for your RVV kernel"

        M, N, K = int(param["M"]), int(param["N"]), int(param["K"])
        CB0_N, CB0_D = int(param["CB0_N"]), int(param["CB0_D"])
        CB1_N, CB1_D = int(param["CB1_N"]), int(param["CB1_D"])
        CB0_IDX_WIDTH = int(param.get("CB0_IDX_WIDTH", 1))
        CB1_IDX_WIDTH = int(param.get("CB1_IDX_WIDTH", 1))

        ta = bool(param.get("transpose_A", False))
        tb = bool(param.get("transpose_B", False))
        if ta or tb:
            raise ValueError("VQ-GEMM generator currently assumes no transpose (matches your kernel)")

        alpha = int(param.get("alpha", 0))
        if alpha != 0 and verbose:
            print("Note: alpha is nonzero in cfg, but golden currently computes A@B (no alpha*C).")

        # Generate A and initial C (FP16)
        A = torch.randn((M, K), dtype=torch.float16, device=device)
        C = torch.randn((M, N), dtype=torch.float16, device=device)

        # Codebooks: [CB*_N, 8] (FP16)
        cb0 = torch.randn((CB0_N, CB0_D), dtype=torch.float16, device=device)
        cb1 = torch.randn((CB1_N, CB1_D), dtype=torch.float16, device=device)

        # Indices per row and per 8-wide block (EI8)
        assert (N % CB0_D) == 0 and (N % CB1_D) == 0, "N must be divisible by CB*_D"
        blocks = N // CB0_D
        idx0 = torch.randint(0, CB0_N, (K, blocks), dtype=torch.uint8, device="cpu")
        idx1 = torch.randint(0, CB1_N, (K, blocks), dtype=torch.uint8, device="cpu")

        # Reconstruct B and compute element-wise golden output
        B = make_vq_B_from_codebooks(cb0, cb1, idx0, idx1, D=CB0_D)

        # Golden output: if your kernel computes C = A@B (and ignores input C), use this:
        golden = torch.matmul(A, B)

        # If your kernel instead should do: C_out = A@B + alpha*C_in, use this instead:
        # golden = torch.matmul(A, B) + (alpha * C)

        kwargs = {
            "A": A,
            "C": C,
            "B_cb0": cb0,
            "B_cb1": cb1,
            "idx0": idx0,
            "idx1": idx1,
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
        }

        emit_header_file("VQ-GEMM", **kwargs)

    else:
        raise ValueError('Unsupported kernel in cfg. Use kernel: "VQ-GEMM".')


if __name__ == "__main__":
    main()
