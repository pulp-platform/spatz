#!/usr/bin/env python3
"""
Generate shuffled FP16 sin/cos test data with separate outC[] and outS[] golden arrays.
Also runs a brute-force sweep of the 2-term polynomial constants (C2, S3) and
the range-reduction constants (INV_PIO2, PIO2_HI) to verify they are optimal for FP16.

HW algorithm (per-element):
  y  = x * INV_PIO2
  k  = round(y)              # integer quadrant
  kf = float(k)
  r  = x - kf * PIO2_HI     # reduced argument
  z  = r * r
  cos_poly = 1 + C2 * z
  sin_poly = r * (1 + S3 * z)
  quadrant remap via k & 3
"""

import numpy as np
import struct, random, sys

# =======================================================================
# FP16 utility
# =======================================================================
def f16_hex(val):
    """Return uint16 hex of an FP16 value."""
    return struct.unpack('<H', np.float16(val).tobytes())[0]

def hex_to_f16(h):
    """Convert uint16 hex to np.float16."""
    return np.frombuffer(struct.pack('<H', h), dtype=np.float16)[0]

def f16(val):
    """Cast to FP16."""
    return np.float16(val)

# =======================================================================
# Generate structured test data (same coverage as original, then shuffle)
# =======================================================================
def generate_test_data(N=4096, seed=42):
    """Create N FP16 test inputs with good coverage, shuffled."""
    vals = set()

    # --- Edge cases ---
    edges = [
        0.0, -0.0,
        np.float16(np.finfo(np.float16).tiny),     # smallest normal
        -np.float16(np.finfo(np.float16).tiny),
        np.float16(2.0**-24),                      # smallest subnormal FP16
        -np.float16(2.0**-24),
        np.float16(500.0), np.float16(-500.0), # clamped range boundary
        np.float16(0.001), np.float16(-0.001),
        np.float16(0.01), np.float16(-0.01),
        np.float16(0.1), np.float16(-0.1),
        np.float16(0.5), np.float16(-0.5),
        np.float16(1.0), np.float16(-1.0),
        np.float16(2.0), np.float16(-2.0),
        np.float16(10.0), np.float16(-10.0),
        np.float16(100.0), np.float16(-100.0),
        np.float16(500.0), np.float16(-500.0),
    ]

    # Trig-specific edge cases: multiples of pi/6, pi/4, pi/3, pi/2, pi, 2*pi
    import math
    trig_points = []
    for mult in [1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 15, 20, 100]:
        for frac in [math.pi/6, math.pi/4, math.pi/3, math.pi/2, math.pi, 2*math.pi]:
            v = mult * frac
            if abs(v) <= 500:
                trig_points.append(np.float16(v))
                trig_points.append(np.float16(-v))

    for v in edges + trig_points:
        vals.add(float(np.float16(v)))

    # --- Logarithmic sweep of positive & negative ---
    for exp in np.linspace(-14, 15, 200):
        v = np.float16(2.0 ** exp)
        if abs(float(v)) <= 500:
            vals.add(float(v))
            vals.add(float(-v))

    # --- Uniform random across FP16 range ---
    rng = np.random.RandomState(seed)
    # Random in [-500, 500]
    while len(vals) < N:
        # Mix: 70% uniform over moderate range, 30% wide range
        if rng.rand() < 0.7:
            x = rng.uniform(-20*math.pi, 20*math.pi)
        else:
            x = rng.uniform(-500, 500)
        v = float(np.float16(x))
        if not np.isnan(v) and not np.isinf(v) and abs(v) <= 500:
            vals.add(v)

    # Trim to exactly N, then shuffle
    data = sorted(vals)
    if len(data) > N:
        data = data[:N]

    rng2 = random.Random(seed)
    rng2.shuffle(data)

    return np.array(data, dtype=np.float16)

# =======================================================================
# Simulate HW sin/cos (FP16 arithmetic)
# =======================================================================
def hw_sincos_f16(x_arr, inv_pio2_h, pio2_hi_h, cos_c2_h, sin_s3_h):
    """
    Simulate the HW 2-term polynomial in FP16 arithmetic.
    Returns (sin_out, cos_out) as float64 arrays.
    Skips values where range reduction overflows (sets to NaN).
    """
    import warnings
    warnings.filterwarnings('ignore', category=RuntimeWarning)

    inv_pio2 = hex_to_f16(inv_pio2_h)
    pio2_hi  = hex_to_f16(pio2_hi_h)
    cos_c2   = hex_to_f16(cos_c2_h)
    sin_s3   = hex_to_f16(sin_s3_h)

    N = len(x_arr)
    sin_out = np.full(N, np.nan, dtype=np.float64)
    cos_out = np.full(N, np.nan, dtype=np.float64)

    for i in range(N):
        x = np.float16(x_arr[i])

        # Range reduction
        y  = np.float16(np.float16(x) * inv_pio2)
        if np.isinf(y) or np.isnan(y):
            continue
        k  = int(np.round(float(y)))
        kf = np.float16(k)
        r  = np.float16(np.float16(x) - np.float16(kf * pio2_hi))
        if np.isinf(r) or np.isnan(r):
            continue
        z  = np.float16(np.float16(r) * np.float16(r))
        if np.isinf(z) or np.isnan(z):
            continue

        # Polynomials
        cp = np.float16(np.float16(1.0) + np.float16(cos_c2 * z))
        sp_inner = np.float16(np.float16(1.0) + np.float16(sin_s3 * z))
        sp = np.float16(np.float16(r) * sp_inner)

        if np.isinf(cp) or np.isnan(cp) or np.isinf(sp) or np.isnan(sp):
            continue

        # Quadrant
        q = k & 3
        b0 = q & 1
        b1 = (q >> 1) & 1

        if b0 == 0:
            sin_mag = float(sp)
            cos_mag = float(cp)
        else:
            sin_mag = float(cp)
            cos_mag = float(sp)

        sin_sign = 1 - 2 * b1
        cos_sign = 1 - 2 * (b1 ^ b0)

        sin_out[i] = sin_sign * sin_mag
        cos_out[i] = cos_sign * cos_mag

    return sin_out, cos_out

# =======================================================================
# Golden reference (float64)
# =======================================================================
def golden_sincos_f64(x_arr):
    """Compute sin/cos in float64 then cast to FP16."""
    sin_ref = np.array([np.float16(np.sin(float(x))) for x in x_arr], dtype=np.float16)
    cos_ref = np.array([np.float16(np.cos(float(x))) for x in x_arr], dtype=np.float16)
    return sin_ref, cos_ref

# =======================================================================
# Constant optimization
# =======================================================================
def optimize_constants(data):
    """
    Brute-force search around the current FP16 constants to find the best C2, S3.
    Also verify INV_PIO2 and PIO2_HI.
    
    FP16 range reduction is only accurate when |k| ≤ 1024 (10-bit mantissa),
    so we restrict optimization to |x| ≤ 1024 * π/2 ≈ 1608.
    For polynomial accuracy, we also check the near range |x| ≤ 4π.
    """
    # Use only values where range reduction has a chance of working
    MAX_RANGE_FULL = 1600.0   # ~1024 quadrants
    MAX_RANGE_POLY = 4 * np.pi  # polynomial accuracy focus

    mask_full = np.array([abs(float(x)) <= MAX_RANGE_FULL for x in data])
    mask_poly = np.array([abs(float(x)) <= MAX_RANGE_POLY for x in data])
    data_full = data[mask_full]
    data_poly = data[mask_poly]

    sin_ref_full = np.array([np.sin(float(x)) for x in data_full])
    cos_ref_full = np.array([np.cos(float(x)) for x in data_full])
    sin_ref_poly = np.array([np.sin(float(x)) for x in data_poly])
    cos_ref_poly = np.array([np.cos(float(x)) for x in data_poly])

    # Current constants
    curr = {
        'INV_PIO2': 0x3917,
        'PIO2_HI':  0x3E48,
        'COS_C2':   0xB7F2,
        'SIN_S3':   0xB150,
    }

    print("\n" + "="*72)
    print("CONSTANT OPTIMIZATION ANALYSIS")
    print("="*72)
    print(f"  Full-range test set: {len(data_full)} values (|x| ≤ {MAX_RANGE_FULL})")
    print(f"  Poly-range test set: {len(data_poly)} values (|x| ≤ {MAX_RANGE_POLY:.1f})")

    # Decode current constants
    for name, h in curr.items():
        v = float(hex_to_f16(h))
        print(f"  {name:12s} = 0x{h:04X} = {v:.10f}")

    import math
    print(f"\n  Exact 2/π       = {2/math.pi:.10f}")
    print(f"  Exact π/2       = {math.pi/2:.10f}")
    print(f"  Exact -1/6      = {-1/6:.10f}")
    print(f"  Exact -1/2      = {-0.5:.10f}")

    def safe_mae(hw, ref):
        """MAE ignoring NaN entries."""
        mask = ~np.isnan(hw) & ~np.isinf(hw)
        if mask.sum() == 0:
            return float('inf')
        return np.mean(np.abs(hw[mask] - ref[mask]))

    def safe_max(hw, ref):
        """MAX error ignoring NaN entries."""
        mask = ~np.isnan(hw) & ~np.isinf(hw)
        if mask.sum() == 0:
            return float('inf')
        return np.max(np.abs(hw[mask] - ref[mask]))

    # ---- Sweep COS_C2 (near range, polynomial accuracy) ----
    print("\n--- COS_C2 sweep (±8 ULP, |x| ≤ 4π) ---")
    best_c2, best_c2_mae = curr['COS_C2'], 1e30
    for offset in range(-8, 9):
        h = curr['COS_C2'] + offset
        if h < 0 or h > 0xFFFF:
            continue
        sin_hw, cos_hw = hw_sincos_f16(data_poly, curr['INV_PIO2'], curr['PIO2_HI'], h, curr['SIN_S3'])
        cos_mae = safe_mae(cos_hw, cos_ref_poly)
        cos_max = safe_max(cos_hw, cos_ref_poly)
        tag = " <-- current" if offset == 0 else ""
        print(f"  0x{h:04X} ({float(hex_to_f16(h)):+.6f}): cos MAE={cos_mae:.6f}  MAX={cos_max:.6f}{tag}")
        if cos_mae < best_c2_mae:
            best_c2_mae = cos_mae
            best_c2 = h

    print(f"  BEST COS_C2 = 0x{best_c2:04X} (MAE={best_c2_mae:.6f})")

    # ---- Sweep SIN_S3 ----
    print("\n--- SIN_S3 sweep (±8 ULP, |x| ≤ 4π) ---")
    best_s3, best_s3_mae = curr['SIN_S3'], 1e30
    for offset in range(-8, 9):
        h = curr['SIN_S3'] + offset
        if h < 0 or h > 0xFFFF:
            continue
        sin_hw, cos_hw = hw_sincos_f16(data_poly, curr['INV_PIO2'], curr['PIO2_HI'], curr['COS_C2'], h)
        sin_mae = safe_mae(sin_hw, sin_ref_poly)
        sin_max = safe_max(sin_hw, sin_ref_poly)
        tag = " <-- current" if offset == 0 else ""
        print(f"  0x{h:04X} ({float(hex_to_f16(h)):+.6f}): sin MAE={sin_mae:.6f}  MAX={sin_max:.6f}{tag}")
        if sin_mae < best_s3_mae:
            best_s3_mae = sin_mae
            best_s3 = h

    print(f"  BEST SIN_S3 = 0x{best_s3:04X} (MAE={best_s3_mae:.6f})")

    # ---- Sweep INV_PIO2 (full range) ----
    print("\n--- INV_PIO2 sweep (±4 ULP, |x| ≤ 1600) ---")
    best_ip, best_ip_mae = curr['INV_PIO2'], 1e30
    for offset in range(-4, 5):
        h = curr['INV_PIO2'] + offset
        if h < 0 or h > 0xFFFF:
            continue
        sin_hw, cos_hw = hw_sincos_f16(data_full, h, curr['PIO2_HI'], curr['COS_C2'], curr['SIN_S3'])
        total_mae = safe_mae(cos_hw, cos_ref_full) + safe_mae(sin_hw, sin_ref_full)
        tag = " <-- current" if offset == 0 else ""
        print(f"  0x{h:04X} ({float(hex_to_f16(h)):+.10f}): combined MAE={total_mae:.6f}{tag}")
        if total_mae < best_ip_mae:
            best_ip_mae = total_mae
            best_ip = h

    print(f"  BEST INV_PIO2 = 0x{best_ip:04X} (combined MAE={best_ip_mae:.6f})")

    # ---- Sweep PIO2_HI (full range) ----
    print("\n--- PIO2_HI sweep (±4 ULP, |x| ≤ 1600) ---")
    best_ph, best_ph_mae = curr['PIO2_HI'], 1e30
    for offset in range(-4, 5):
        h = curr['PIO2_HI'] + offset
        if h < 0 or h > 0xFFFF:
            continue
        sin_hw, cos_hw = hw_sincos_f16(data_full, curr['INV_PIO2'], h, curr['COS_C2'], curr['SIN_S3'])
        total_mae = safe_mae(cos_hw, cos_ref_full) + safe_mae(sin_hw, sin_ref_full)
        tag = " <-- current" if offset == 0 else ""
        print(f"  0x{h:04X} ({float(hex_to_f16(h)):+.10f}): combined MAE={total_mae:.6f}{tag}")
        if total_mae < best_ph_mae:
            best_ph_mae = total_mae
            best_ph = h

    print(f"  BEST PIO2_HI = 0x{best_ph:04X} (combined MAE={best_ph_mae:.6f})")

    # ---- Joint sweep COS_C2 × SIN_S3 (poly range) ----
    print("\n--- Joint COS_C2 × SIN_S3 sweep (±4 ULP, |x| ≤ 4π) ---")
    best_pair = (curr['COS_C2'], curr['SIN_S3'])
    best_pair_mae = 1e30
    for c2_off in range(-4, 5):
        for s3_off in range(-4, 5):
            c2h = curr['COS_C2'] + c2_off
            s3h = curr['SIN_S3'] + s3_off
            if c2h < 0 or c2h > 0xFFFF or s3h < 0 or s3h > 0xFFFF:
                continue
            sin_hw, cos_hw = hw_sincos_f16(data_poly, curr['INV_PIO2'], curr['PIO2_HI'], c2h, s3h)
            total = safe_mae(cos_hw, cos_ref_poly) + safe_mae(sin_hw, sin_ref_poly)
            if total < best_pair_mae:
                best_pair_mae = total
                best_pair = (c2h, s3h)

    print(f"  BEST PAIR: COS_C2=0x{best_pair[0]:04X}, SIN_S3=0x{best_pair[1]:04X}")
    print(f"  Combined MAE = {best_pair_mae:.6f}")
    sin_hw, cos_hw = hw_sincos_f16(data_poly, curr['INV_PIO2'], curr['PIO2_HI'], curr['COS_C2'], curr['SIN_S3'])
    curr_total = safe_mae(cos_hw, cos_ref_poly) + safe_mae(sin_hw, sin_ref_poly)
    print(f"  Current combined MAE = {curr_total:.6f}")
    if best_pair == (curr['COS_C2'], curr['SIN_S3']):
        print("  ==> Current constants ARE optimal.")
    else:
        improvement = (curr_total - best_pair_mae) / curr_total * 100
        print(f"  ==> Improvement: {improvement:.3f}%")
        print(f"      COS_C2: 0x{curr['COS_C2']:04X} → 0x{best_pair[0]:04X}")
        print(f"      SIN_S3: 0x{curr['SIN_S3']:04X} → 0x{best_pair[1]:04X}")

    return best_pair

# =======================================================================
# Write data.h
# =======================================================================
def write_data_h(filepath, data):
    N = len(data)
    with open(filepath, 'w') as f:
        f.write(f"#define B_Size 1\n")
        f.write(f"#define T_Size 1\n")
        f.write(f"#define C_Size {N}\n\n")
        f.write('__attribute__((section(".data")))\n')
        f.write("__fp16 data1_dram[] = {\n")
        for i in range(0, N, 10):
            chunk = data[i:i+10]
            vals = [f"{float(v):.6f}" for v in chunk]
            line = ", ".join(vals)
            if i + 10 < N:
                line += ","
            f.write(f"  {line}\n")
        f.write("};\n")
    print(f"  Wrote {filepath} ({N} values, shuffled)")

# =======================================================================
# Write gold.h with separate outC[] and outS[]
# =======================================================================
def write_gold_h(filepath, sin_ref, cos_ref):
    N = len(sin_ref)
    with open(filepath, 'w') as f:
        f.write("// Copyright 2020 ETH Zurich and University of Bologna.\n")
        f.write("// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n")
        f.write("// SPDX-License-Identifier: Apache-2.0\n\n")
        f.write("#pragma once\n\n")

        # outS (sine)
        f.write('__attribute__((section(".data")))\n')
        f.write("__fp16 outS[] = {\n")
        for i in range(0, N, 10):
            chunk = sin_ref[i:i+10]
            vals = [f"{float(v):.6f}" for v in chunk]
            line = ", ".join(vals)
            if i + 10 < N:
                line += ","
            f.write(f"  {line}\n")
        f.write("};\n\n")

        # outC (cosine)
        f.write('__attribute__((section(".data")))\n')
        f.write("__fp16 outC[] = {\n")
        for i in range(0, N, 10):
            chunk = cos_ref[i:i+10]
            vals = [f"{float(v):.6f}" for v in chunk]
            line = ", ".join(vals)
            if i + 10 < N:
                line += ","
            f.write(f"  {line}\n")
        f.write("};\n")

    print(f"  Wrote {filepath} (outS[{N}] + outC[{N}])")

# =======================================================================
# Main
# =======================================================================
if __name__ == "__main__":
    N = 4096
    print(f"Generating {N} shuffled FP16 sin/cos test inputs...")
    data = generate_test_data(N, seed=42)
    print(f"  Generated {len(data)} unique FP16 values")
    print(f"  Range: [{float(data.min()):.4f}, {float(data.max()):.4f}]")

    # Compute golden reference
    print("\nComputing golden sin/cos (float64 → FP16)...")
    sin_ref, cos_ref = golden_sincos_f64(data)

    # Write files
    import os
    base = os.path.dirname(os.path.abspath(__file__))
    write_data_h(os.path.join(base, "data", "data.h"), data)
    write_gold_h(os.path.join(base, "golden", "gold.h"), sin_ref, cos_ref)

    # Optimize constants
    best_pair = optimize_constants(data)

    # Summary error analysis with CURRENT constants
    print("\n" + "="*72)
    print("ERROR ANALYSIS WITH CURRENT HW CONSTANTS")
    print("="*72)

    # Restrict to safe range for meaningful analysis
    for label, maxr in [("Near (|x| ≤ 4π)", 4*np.pi), ("Medium (|x| ≤ 100)", 100), ("Wide (|x| ≤ 1600)", 1600)]:
        mask = np.array([abs(float(x)) <= maxr for x in data])
        data_r = data[mask]
        sin_ref_r = np.array([np.sin(float(x)) for x in data_r])
        cos_ref_r = np.array([np.cos(float(x)) for x in data_r])
        sin_hw, cos_hw = hw_sincos_f16(data_r, 0x3917, 0x3E48, 0xB7F2, 0xB150)

        valid = ~np.isnan(cos_hw) & ~np.isinf(cos_hw)
        if valid.sum() > 0:
            cos_err = np.abs(cos_hw[valid] - cos_ref_r[valid])
            sin_err = np.abs(sin_hw[valid] - sin_ref_r[valid])
            print(f"\n  {label} ({valid.sum()}/{len(data_r)} valid):")
            print(f"    Cosine MAE: {np.mean(cos_err):.6f}   MAX: {np.max(cos_err):.6f}")
            print(f"    Sine   MAE: {np.mean(sin_err):.6f}   MAX: {np.max(sin_err):.6f}")

            # Worst cases
            worst_idx = np.argsort(cos_err)[-3:][::-1]
            print(f"    Top-3 worst cos: ", end="")
            for idx in worst_idx:
                orig_idx = np.where(valid)[0][idx]
                print(f"x={float(data_r[orig_idx]):.3f}(err={cos_err[idx]:.4f}) ", end="")
            print()
        else:
            print(f"\n  {label}: no valid results")

    print("\nDone.")
