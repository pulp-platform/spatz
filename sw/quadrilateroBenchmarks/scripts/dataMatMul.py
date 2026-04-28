# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
"""
=============================================================================
MatMul Golden Model Generator (Hexadecimal Output, Matrix-Shaped Formatting)
=============================================================================
This script generates a mathematical Golden Model for a Matrix Multiplication
(MatMul) hardware accelerator. It computes C = A * B and exports the matrices
into a single C header file (e.g., data_8_8_8.h) with a configuration struct.
Arrays are printed in HEXADECIMAL format and aligned to their memory columns.

Standard GEMM Dimensions:
  - Matrix A: M rows x K columns
  - Matrix B: K rows x N columns
  - Matrix C: M rows x N columns

Command-Line Arguments:
  --M        (int) : Number of rows for matrix A and resulting matrix C. (Default: 8)
  --K        (int) : Number of columns for matrix A and rows for matrix B. (Inner dim) (Default: 8)
  --N        (int) : Number of columns for matrix B and resulting matrix C. (Default: 8)
  --dtype    (str) : Data type for A and B. Choices: 'float', 'bfloat', 'int', 'uint'. (Default: 'float')
  --bits     (int) : Bit-width for A and B. Choices: 32, 16, or 8. (Default: 32)
  --fp8_fmt  (str) : FP8 format if dtype=float and bits=8. Choices: 'e4m3' or 'e5m2'.
  --transA   (int) : Set to 1 to transpose matrix A in memory. (Default: 1)
  --transB   (int) : Set to 1 to transpose matrix B in memory. (Default: 0)
  --inc_dir  (str) : Output directory for the generated .h file. (Default: './include')
=============================================================================
"""

import numpy as np
import torch 
import torch.nn.functional as F
import argparse
import os

# Precision settings for console printing
torch.set_printoptions(precision=10, sci_mode=False)
torch.random.manual_seed(1337)

parser = argparse.ArgumentParser(description="MatMul Golden Model Generator")
parser.add_argument('--M', type=int, default=8, help="Rows of A and C")
parser.add_argument('--K', type=int, default=8, help="Columns of A, Rows of B (Inner dimension)")
parser.add_argument('--N', type=int, default=8, help="Columns of B and C")
parser.add_argument('--dtype', type=str, default='float', choices=['float', 'bfloat', 'int', 'uint'], help="Data type of A and B")
parser.add_argument('--bits', type=int, default=32, choices=[32, 16, 8], help="Bit resolution for A and B")
parser.add_argument('--fp8_fmt', type=str, default='e4m3', choices=['e4m3', 'e5m2'], help="FP8 format")
parser.add_argument('--transA', type=int, default=1, help="1 if A is transposed in memory")
parser.add_argument('--transB', type=int, default=0, help="1 if B is transposed in memory")
parser.add_argument('--inc_dir', type=str, default='./include', help="Directory for .h files")
args = parser.parse_args()

# Create output directory if it doesn't exist
os.makedirs(args.inc_dir, exist_ok=True)

# Validate bfloat resolution
if args.dtype == 'bfloat' and args.bits != 16:
    print("Warning: bfloat must be 16 bits. Forcing --bits 16.")
    args.bits = 16

# ---------------------------------------------------------
# 1. DATA GENERATION
# ---------------------------------------------------------
A_math, B_math = None, None 
A_mem, B_mem = None, None   

print(f"\nGenerating MatMul: A({args.M}x{args.K}) * B({args.K}x{args.N}) -> C({args.M}x{args.N})")
if args.dtype == 'float':
    print(f"Format: FLOAT {args.bits}-bit" + (f" ({args.fp8_fmt.upper()})" if args.bits == 8 else ""))
elif args.dtype == 'bfloat':
    print("Format: BFLOAT 16-bit")
elif args.dtype == 'uint':
    print(f"Format: UINT {args.bits}-bit")
else:
    print(f"Format: INT {args.bits}-bit")

# --- FLOAT TYPES ---
if args.dtype == 'float':
    A_base = (torch.randn(args.M, args.K, dtype=torch.float32))
    B_base = (torch.randn(args.K, args.N, dtype=torch.float32))
    
    if args.bits == 32:
        A_math, B_math = A_base, B_base
        A_mem, B_mem = A_base.view(torch.int32), B_base.view(torch.int32)
    elif args.bits == 16:
        A_fp16, B_fp16 = A_base.to(torch.float16), B_base.to(torch.float16)
        A_math, B_math = A_fp16.to(torch.float32), B_fp16.to(torch.float32)
        A_mem, B_mem = A_fp16.view(torch.int16), B_fp16.view(torch.int16)
    elif args.bits == 8:
        pt_fp8_type = torch.float8_e4m3fn if args.fp8_fmt == 'e4m3' else torch.float8_e5m2
        A_fp8, B_fp8 = A_base.to(pt_fp8_type), B_base.to(pt_fp8_type)
        A_math, B_math = A_fp8.to(torch.float32), B_fp8.to(torch.float32)
        A_mem, B_mem = A_fp8.view(torch.int8), B_fp8.view(torch.int8)

# --- BFLOAT16 TYPE ---
elif args.dtype == 'bfloat':
    A_base = (torch.randn(args.M, args.K, dtype=torch.float32))
    B_base = (torch.randn(args.K, args.N, dtype=torch.float32))
    A_bf16, B_bf16 = A_base.to(torch.bfloat16), B_base.to(torch.bfloat16)
    A_math, B_math = A_bf16.to(torch.float32), B_bf16.to(torch.float32)
    A_mem, B_mem = A_bf16.view(torch.int16), B_bf16.view(torch.int16)

# --- SIGNED INT TYPES ---
elif args.dtype == 'int':
    if args.bits == 32:
        A_mem = torch.randint(-100, 100, (args.M, args.K), dtype=torch.int32)
        B_mem = torch.randint(-100, 100, (args.K, args.N), dtype=torch.int32)
    elif args.bits == 16:
        A_mem = torch.randint(-50, 50, (args.M, args.K), dtype=torch.int16)
        B_mem = torch.randint(-50, 50, (args.K, args.N), dtype=torch.int16)
    elif args.bits == 8:
        A_mem = torch.randint(-10, 10, (args.M, args.K), dtype=torch.int8)
        B_mem = torch.randint(-10, 10, (args.K, args.N), dtype=torch.int8)
    A_math, B_math = A_mem.to(torch.float32), B_mem.to(torch.float32)

# --- UNSIGNED INT TYPES ---
elif args.dtype == 'uint':
    # Using positive ranges to represent unsigned values correctly in memory
    if args.bits == 32:
        A_mem = torch.randint(0, 100, (args.M, args.K), dtype=torch.int32)
        B_mem = torch.randint(0, 100, (args.K, args.N), dtype=torch.int32)
    elif args.bits == 16:
        A_mem = torch.randint(0, 50, (args.M, args.K), dtype=torch.int16)
        B_mem = torch.randint(0, 50, (args.K, args.N), dtype=torch.int16)
    elif args.bits == 8:
        A_mem = torch.randint(0, 10, (args.M, args.K), dtype=torch.uint8)
        B_mem = torch.randint(0, 10, (args.K, args.N), dtype=torch.uint8)
    A_math, B_math = A_mem.to(torch.float32), B_mem.to(torch.float32)

# ---------------------------------------------------------
# 2. GOLDEN MODEL COMPUTATION & CHECKSUM
# ---------------------------------------------------------
# C will automatically have shape (M, N)
C_math = torch.matmul(A_math, B_math)
if args.dtype in ['int', 'uint']:
    C_math = C_math.to(torch.int32)

# Calculate row-wise sum for the checksum (Size will be M)
C_checksum = C_math.sum(dim=1).to(torch.float32)

# ---------------------------------------------------------
# 3. MEMORY TRANSPOSITION & PACKING
# ---------------------------------------------------------
A_mem = A_mem.T if args.transA == 1 else A_mem
B_mem = B_mem.T if args.transB == 1 else B_mem

def pack_vertical(tensor, bits):
    """
    Packs elements vertically (along columns).
    """
    if bits == 32:
        return tensor.contiguous().view(torch.int32)
    
    rows, cols = tensor.shape
    pack_factor = 32 // bits
    
    pad_rows = (pack_factor - (rows % pack_factor)) % pack_factor
    if pad_rows > 0:
        tensor = F.pad(tensor, (0, 0, 0, pad_rows), "constant", 0)
    
    new_rows = tensor.shape[0] // pack_factor
    
    if bits == 16:
        t_int = tensor.contiguous().view(torch.int16).to(torch.int32)
        t_grouped = t_int.view(new_rows, 2, cols)
        lower = t_grouped[:, 0, :] & 0xFFFF
        upper = t_grouped[:, 1, :] & 0xFFFF
        return (lower | (upper << 16)).to(torch.int32)
    elif bits == 8:
        # Cast to int8 for correct 1-byte masking logic
        t_int = tensor.contiguous().view(torch.int8).to(torch.int32)
        t_grouped = t_int.view(new_rows, 4, cols)
        b0 = t_grouped[:, 0, :] & 0xFF
        b1 = t_grouped[:, 1, :] & 0xFF
        b2 = t_grouped[:, 2, :] & 0xFF
        b3 = t_grouped[:, 3, :] & 0xFF
        return (b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)).to(torch.int32)

A_packed = pack_vertical(A_mem, args.bits)
B_packed = pack_vertical(B_mem, args.bits)

# Prepare C and always treat it as 32-bit uint for memory layout consistency
C_packed = C_math.contiguous()
if C_math.dtype == torch.float32:
    C_packed = C_packed.view(torch.int32)

# ---------------------------------------------------------
# 4. EXPORT TO SINGLE C HEADER FILE
# ---------------------------------------------------------
# Determine DTYPE macro for the struct based on user request
if args.dtype == 'float':
    if args.bits == 32: macro_dtype = "FP32"
    elif args.bits == 16: macro_dtype = "FP16"
    else: macro_dtype = "FP8" if args.fp8_fmt == 'e5m2' else "FP8E4M3"
elif args.dtype == 'bfloat':
    macro_dtype = "BF16"
elif args.dtype == 'uint':
    macro_dtype = f"UINT{args.bits}"  # Generates UINT32, UINT16, UINT8
else:
    macro_dtype = f"INT{args.bits}"   # Generates INT32, INT16, INT8

filename = f"data_{args.M}_{args.N}_{args.K}.h"
filepath = os.path.join(args.inc_dir, filename)

def array_to_hex_string(tensor):
    """ Converts tensor to a string of base-16 (hex) integers, respecting column boundaries. """
    flat = tensor.flatten().detach().cpu().numpy().astype(np.uint32)
    # Determine the correct number of columns from the tensor shape
    cols = tensor.shape[-1] if len(tensor.shape) > 1 else len(flat)
    
    lines = []
    for i in range(0, len(flat), cols):
        chunk = flat[i:i+cols]
        lines.append("    " + ", ".join([f"0x{val:08x}" for val in chunk]))
    return ",\n".join(lines)

def array_to_float_string(tensor):
    """ Converts tensor to a string of floats, respecting column boundaries. """
    flat = tensor.flatten().detach().cpu().numpy()
    cols = tensor.shape[-1] if len(tensor.shape) > 1 else len(flat)
    
    lines = []
    for i in range(0, len(flat), cols):
        chunk = flat[i:i+cols]
        lines.append("    " + ", ".join([f"{val:.7g}" for val in chunk]))
    return ",\n".join(lines)

with open(filepath, "w") as f:
    f.write('#include "layer.h"\n')
    f.write('#include <stdint.h>\n\n')
    
    # Write Struct
    f.write(f"const gemm_layer gemm_l = {{\n")
    f.write(f"    .M = {args.M},\n")
    f.write(f"    .N = {args.N},\n")
    f.write(f"    .K = {args.K},\n")
    f.write(f"    .TA = {args.transA},\n")
    f.write(f"    .TB = {args.transB},\n")
    f.write(f"    .ALPHA = 1.0,\n")
    f.write(f"    .dtype = {macro_dtype},\n")
    f.write(f"    .expand = 0\n")
    f.write(f"}};\n\n")

    # Write A, B, and C as uint32_t using HEX formatting and matching columns
    f.write(f"static uint32_t gemm_A_dram [{A_packed.numel()}] __attribute__((section(\".data\"))) = {{\n")
    f.write(array_to_hex_string(A_packed) + "\n};\n\n")

    f.write(f"static uint32_t gemm_B_dram [{B_packed.numel()}] __attribute__((section(\".data\"))) = {{\n")
    f.write(array_to_hex_string(B_packed) + "\n};\n\n")

    f.write(f"static uint32_t gemm_C_dram [{C_packed.numel()}] __attribute__((section(\".data\"))) = {{\n")
    f.write(array_to_hex_string(C_packed) + "\n};\n\n")

    # Write Checksum as float
    f.write(f"static const float gemm_checksum[{args.M}] = {{\n")
    f.write(array_to_float_string(C_checksum) + "\n};\n")

print(f"\nFile successfully generated: {filepath}")