# Copyright 2024 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Author: Pei-Yu Lin <peilin@ethz.ch>
#
# gen_data.py
# Generate both floating-point and integer data headers for VME GEMM benchmarks.

import argparse
import pathlib

import hjson
import numpy as np
import torch

# ---------------------------------------------------------------------------
# Common helpers
# ---------------------------------------------------------------------------

def array_to_cstr_f32(arr: np.ndarray) -> str:
    """Convert a flat float32 numpy array to a C brace-initializer string."""
    items = ", ".join(repr(float(x)) + "f" for x in arr.flat)
    return "{" + items + "}"


def array_to_cstr_u8(arr: np.ndarray) -> str:
    """Convert a flat uint8 numpy array to a C brace-initializer string."""
    items = ", ".join(str(int(x)) for x in arr.flat)
    return "{" + items + "}"


def array_to_cstr_s8(arr: np.ndarray) -> str:
    """Convert a flat int8 numpy array to a C brace-initializer string."""
    items = ", ".join(str(int(x)) for x in arr.flat)
    return "{" + items + "}"


def array_to_cstr_i32(arr: np.ndarray) -> str:
    """Convert a flat int32 numpy array to a C brace-initializer string."""
    items = ", ".join(str(int(x)) for x in arr.flat)
    return "{" + items + "}"


def emit_file_banner() -> list[str]:
    """Common generated-header banner."""
    return [
        "// Copyright 2024 ETH Zurich and University of Bologna.",
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details.",
        "// SPDX-License-Identifier: Apache-2.0",
        "//",
        "// Author: Pei-Yu Lin <peilin@ethz.ch>",
        "//",
        "// This file was generated automatically by script/gen_data.py.",
        "// Do not edit by hand – re-run the script to regenerate.",
        "",
        '#include "layer.h"',
    ]


def emit_layer_struct(M: int, N: int, K: int, TM: int, TN: int) -> list[str]:
    """Emit vme_gemm_layer gemm_l."""
    return [
        "",
        f"const vme_gemm_layer gemm_l = {{",
        f"    .M  = {M},",
        f"    .N  = {N},",
        f"    .K  = {K},",
        f"    .TM = {TM},",
        f"    .TN = {TN},",
        "};",
        "",
    ]


def parse_common_params(param: dict):
    """Extract common GEMM parameters."""
    M = int(param["M"])
    N = int(param["N"])
    K = int(param["K"])
    TM = int(param.get("TM", 16))
    TN = int(param.get("TN", 16))
    seed = int(param.get("seed", 42))

    assert M % (2 * TM) == 0, f"M={M} must be a multiple of 2*TM={2*TM}"
    assert N % (2 * TN) == 0, f"N={N} must be a multiple of 2*TN={2*TN}"

    return M, N, K, TM, TN, seed


# ---------------------------------------------------------------------------
# Floating-point data generation
# ---------------------------------------------------------------------------

def emit_fp32_header(M: int, N: int, K: int, TM: int, TN: int,
                     A: np.ndarray,
                     B: np.ndarray,
                     checksum: np.ndarray) -> str:
    """Return full C header for floating-point VME GEMM data."""
    lines = emit_file_banner()
    lines += emit_layer_struct(M, N, K, TM, TN)
    lines += [
        "",
        "// fp32 input data for floating-point VME GEMM.",
        "// Layout: row-major A[M*K], row-major B[K*N].",
        f'static float gemm_A_f32_dram[{M}*{K}] __attribute__((section(".data"))) =',
        array_to_cstr_f32(A) + ";",
        "",
        "",
        f'static float gemm_B_f32_dram[{K}*{N}] __attribute__((section(".data"))) =',
        array_to_cstr_f32(B) + ";",
        "",
        "",
        "// Compatibility aliases for older benchmark code that expects",
        "// gemm_A_dram / gemm_B_dram.",
        "#define gemm_A_dram gemm_A_f32_dram",
        "#define gemm_B_dram gemm_B_f32_dram",
        "",
        "",
        "// Row sums of fp32 reference C = A * B.",
        f"static const float gemm_checksum[{M}] =",
        array_to_cstr_f32(checksum) + ";",
        "",
    ]
    return "\n".join(lines)


def generate_fp32_header(M: int, N: int, K: int, TM: int, TN: int,
                         seed: int,
                         verbose: bool) -> str:
    """Generate fp32 A/B data and return header string."""
    torch.manual_seed(seed)
    np.random.seed(seed)

    A_t = torch.randn((M, K), dtype=torch.float32)
    B_t = torch.randn((K, N), dtype=torch.float32)

    C_t = torch.matmul(A_t, B_t)
    checksum_t = C_t.sum(dim=1)

    A_np = A_t.numpy()
    B_np = B_t.numpy()
    checksum_np = checksum_t.numpy()

    if verbose:
        print(f"[FP32] A shape: {A_np.shape}, min={A_np.min():.4f}, max={A_np.max():.4f}")
        print(f"[FP32] B shape: {B_np.shape}, min={B_np.min():.4f}, max={B_np.max():.4f}")
        print(f"[FP32] C shape: {C_t.shape}, min={C_t.min():.4f}, max={C_t.max():.4f}")

    return emit_fp32_header(M, N, K, TM, TN, A_np, B_np, checksum_np)


# ---------------------------------------------------------------------------
# Integer data generation
# ---------------------------------------------------------------------------

def matmul_i32(A: np.ndarray, B: np.ndarray) -> np.ndarray:
    """Compute int32 reference C = A @ B."""
    A_i32 = A.astype(np.int32)
    B_i32 = B.astype(np.int32)
    return A_i32 @ B_i32


def make_u8_tensor(shape, low: int, high: int) -> torch.Tensor:
    """Generate uint8-like tensor using int16 container."""
    return torch.randint(low=low, high=high + 1, size=shape, dtype=torch.int16)


def make_s8_tensor(shape, low: int, high: int) -> torch.Tensor:
    """Generate int8-like tensor using int16 container."""
    return torch.randint(low=low, high=high + 1, size=shape, dtype=torch.int16)


def emit_int_header(M: int, N: int, K: int, TM: int, TN: int,
                    A_u8: np.ndarray,
                    B_u8: np.ndarray,
                    C_vtmmu_u8u8: np.ndarray,
                    C_vtmmu_u8s8: np.ndarray,
                    checksum_vtmmu_u8u8: np.ndarray,
                    checksum_vtmmu_u8s8: np.ndarray,
                    A_s8: np.ndarray,
                    B_s8: np.ndarray,
                    C_vtmms_s8u8: np.ndarray,
                    C_vtmms_s8s8: np.ndarray,
                    checksum_vtmms_s8u8: np.ndarray,
                    checksum_vtmms_s8s8: np.ndarray) -> str:
    lines = emit_file_banner()
    lines += [
        "#include <stdint.h>",
    ]
    lines += emit_layer_struct(M, N, K, TM, TN)
    lines += [
        "",
        "// Unsigned integer input data for vtmmu.",
        "// Layout: row-major A[M*K], row-major B[K*N].",
        "// Integer kernel should load A using vlse8.v stride-load.",
        f'static uint8_t gemm_A_u8_dram[{M}*{K}] __attribute__((section(".data"))) =',
        array_to_cstr_u8(A_u8) + ";",
        "",
        "",
        f'static uint8_t gemm_B_u8_dram[{K}*{N}] __attribute__((section(".data"))) =',
        array_to_cstr_u8(B_u8) + ";",
        "",
        "",
        "// Golden reference for vtmmu altfmt=0: uint8 A x uint8 B -> int32.",
        f'static int32_t gemm_C_vtmmu_u8u8_ref[{M}*{N}] __attribute__((section(".data"))) =',
        array_to_cstr_i32(C_vtmmu_u8u8) + ";",
        "",
        "",
        f"static const int32_t gemm_checksum_vtmmu_u8u8[{M}] =",
        array_to_cstr_i32(checksum_vtmmu_u8u8) + ";",
        "",
        "",
        "// Golden reference for vtmmu altfmt=1: uint8 A x int8 B -> int32.",
        f'static int32_t gemm_C_vtmmu_u8s8_ref[{M}*{N}] __attribute__((section(".data"))) =',
        array_to_cstr_i32(C_vtmmu_u8s8) + ";",
        "",
        "",
        f"static const int32_t gemm_checksum_vtmmu_u8s8[{M}] =",
        array_to_cstr_i32(checksum_vtmmu_u8s8) + ";",
        "",
        "",
        "// Backward-compatible aliases for old vtmmu altfmt=0 code.",
        "",
        "",
        "// Signed integer input data for vtmms.",
        "// Layout: row-major A[M*K], row-major B[K*N].",
        "// Integer kernel should load A using vlse8.v stride-load.",
        f'static int8_t gemm_A_s8_dram[{M}*{K}] __attribute__((section(".data"))) =',
        array_to_cstr_s8(A_s8) + ";",
        "",
        "",
        f'static int8_t gemm_B_s8_dram[{K}*{N}] __attribute__((section(".data"))) =',
        array_to_cstr_s8(B_s8) + ";",
        "",
        "",
        "// Golden reference for vtmms altfmt=0: int8 A x uint8 B -> int32.",
        f'static int32_t gemm_C_vtmms_s8u8_ref[{M}*{N}] __attribute__((section(".data"))) =',
        array_to_cstr_i32(C_vtmms_s8u8) + ";",
        "",
        "",
        f"static const int32_t gemm_checksum_vtmms_s8u8[{M}] =",
        array_to_cstr_i32(checksum_vtmms_s8u8) + ";",
        "",
        "",
        "// Golden reference for vtmms altfmt=1: int8 A x int8 B -> int32.",
        f'static int32_t gemm_C_vtmms_s8s8_ref[{M}*{N}] __attribute__((section(".data"))) =',
        array_to_cstr_i32(C_vtmms_s8s8) + ";",
        "",
        "",
        f"static const int32_t gemm_checksum_vtmms_s8s8[{M}] =",
        array_to_cstr_i32(checksum_vtmms_s8s8) + ";",
        "",
        "",
    ]
    return "\n".join(lines)


def generate_int_header(M: int, N: int, K: int, TM: int, TN: int,
                        seed: int,
                        u8_low: int,
                        u8_high: int,
                        s8_low: int,
                        s8_high: int,
                        verbose: bool) -> str:
    assert K % 4 == 0, f"K={K} must be a multiple of TK=4 for int8 VME kernels"

    assert 0 <= u8_low <= u8_high <= 255, \
        f"Invalid uint8 range [{u8_low}, {u8_high}]"
    assert -128 <= s8_low <= s8_high <= 127, \
        f"Invalid int8 range [{s8_low}, {s8_high}]"

    # Offset seed so FP and INT data are deterministic but not accidentally
    # correlated with the fp32 random stream.
    int_seed = seed + 1000
    torch.manual_seed(int_seed)
    np.random.seed(int_seed)

    A_u8_t = make_u8_tensor((M, K), u8_low, u8_high)
    B_u8_t = make_u8_tensor((K, N), u8_low, u8_high)

    A_s8_t = make_s8_tensor((M, K), s8_low, s8_high)
    B_s8_t = make_s8_tensor((K, N), s8_low, s8_high)

    A_u8_np = A_u8_t.numpy().astype(np.uint8)
    B_u8_np = B_u8_t.numpy().astype(np.uint8)

    A_s8_np = A_s8_t.numpy().astype(np.int8)
    B_s8_np = B_s8_t.numpy().astype(np.int8)

    # ISS integer semantics:
    #   vtmmu altfmt=0: uint8 A x uint8 B
    #   vtmmu altfmt=1: uint8 A x int8  B
    #   vtmms altfmt=0: int8  A x uint8 B
    #   vtmms altfmt=1: int8  A x int8  B
    C_vtmmu_u8u8_np = matmul_i32(A_u8_np, B_u8_np).astype(np.int32)
    C_vtmmu_u8s8_np = matmul_i32(A_u8_np, B_s8_np).astype(np.int32)
    C_vtmms_s8u8_np = matmul_i32(A_s8_np, B_u8_np).astype(np.int32)
    C_vtmms_s8s8_np = matmul_i32(A_s8_np, B_s8_np).astype(np.int32)

    checksum_vtmmu_u8u8_np = C_vtmmu_u8u8_np.sum(axis=1).astype(np.int32)
    checksum_vtmmu_u8s8_np = C_vtmmu_u8s8_np.sum(axis=1).astype(np.int32)
    checksum_vtmms_s8u8_np = C_vtmms_s8u8_np.sum(axis=1).astype(np.int32)
    checksum_vtmms_s8s8_np = C_vtmms_s8s8_np.sum(axis=1).astype(np.int32)

    if verbose:
        print(f"[INT] A_u8 shape: {A_u8_np.shape}, min={A_u8_np.min()}, max={A_u8_np.max()}")
        print(f"[INT] B_u8 shape: {B_u8_np.shape}, min={B_u8_np.min()}, max={B_u8_np.max()}")
        print(f"[INT] A_s8 shape: {A_s8_np.shape}, min={A_s8_np.min()}, max={A_s8_np.max()}")
        print(f"[INT] B_s8 shape: {B_s8_np.shape}, min={B_s8_np.min()}, max={B_s8_np.max()}")
        print(f"[INT] C_vtmmu_u8u8 shape: {C_vtmmu_u8u8_np.shape}, min={C_vtmmu_u8u8_np.min()}, max={C_vtmmu_u8u8_np.max()}")
        print(f"[INT] C_vtmmu_u8s8 shape: {C_vtmmu_u8s8_np.shape}, min={C_vtmmu_u8s8_np.min()}, max={C_vtmmu_u8s8_np.max()}")
        print(f"[INT] C_vtmms_s8u8 shape: {C_vtmms_s8u8_np.shape}, min={C_vtmms_s8u8_np.min()}, max={C_vtmms_s8u8_np.max()}")
        print(f"[INT] C_vtmms_s8s8 shape: {C_vtmms_s8s8_np.shape}, min={C_vtmms_s8s8_np.min()}, max={C_vtmms_s8s8_np.max()}")

    return emit_int_header(
        M, N, K, TM, TN,
        A_u8_np, B_u8_np,
        C_vtmmu_u8u8_np,
        C_vtmmu_u8s8_np,
        checksum_vtmmu_u8u8_np,
        checksum_vtmmu_u8s8_np,
        A_s8_np, B_s8_np,
        C_vtmms_s8u8_np,
        C_vtmms_s8s8_np,
        checksum_vtmms_s8u8_np,
        checksum_vtmms_s8s8_np,
    )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Generate both FP and INT VME GEMM benchmark data headers"
    )
    parser.add_argument(
        "-c", "--cfg",
        type=pathlib.Path,
        required=True,
        help="Path to the HJSON/JSON config file, e.g. gemm_64_64_64.json",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Print extra information",
    )
    args = parser.parse_args()

    with args.cfg.open() as f:
        param = hjson.loads(f.read())

    M, N, K, TM, TN, seed = parse_common_params(param)

    # Keep kernel optional/backward-compatible.
    # The script now always generates both fp32 and integer headers.
    kernel = str(param.get("kernel", "VME_GEMM"))
    if kernel not in ("VME_GEMM", "VME_INT_GEMM", "VME_GEMM_INT", "VME_GEMM_ALL"):
        raise AssertionError(
            f"Unexpected kernel type: {kernel!r} "
            f"(allowed: 'VME_GEMM', 'VME_INT_GEMM', 'VME_GEMM_INT', 'VME_GEMM_ALL')"
        )

    # Integer ranges are inclusive. Defaults are intentionally small to keep
    # int32 golden values easy to inspect and safe from overflow.
    u8_low = int(param.get("u8_low", 0))
    u8_high = int(param.get("u8_high", 15))
    s8_low = int(param.get("s8_low", -8))
    s8_high = int(param.get("s8_high", 7))

    fp_header = generate_fp32_header(M, N, K, TM, TN, seed, args.verbose)
    int_header = generate_int_header(
        M, N, K, TM, TN,
        seed,
        u8_low, u8_high,
        s8_low, s8_high,
        args.verbose,
    )

    out_dir = pathlib.Path(__file__).parent.parent / "data"
    out_dir.mkdir(parents=True, exist_ok=True)

    fp_out_file = out_dir / f"data_{M}_{N}_{K}.h"
    int_out_file = out_dir / f"data_int_{M}_{N}_{K}.h"

    with fp_out_file.open("w") as f:
        f.write(fp_header)

    with int_out_file.open("w") as f:
        f.write(int_header)

    print(f"Written: {fp_out_file}")
    print(f"Written: {int_out_file}")


if __name__ == "__main__":
    main()