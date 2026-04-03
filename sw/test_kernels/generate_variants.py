#!/usr/bin/env python3
"""generate_variants.py — Generate test kernel variants for Spatz benchmarks.

Creates:
  - Data headers for all (precision × element_count) combinations
  - FP16 kernel source files (adapted from FP32 originals)
  - BF16 kernel source files (adapted from FP32 originals)
  - Updated CMakeLists.txt

Usage:
    cd test_kernels
    python3 generate_variants.py
"""

import os
import re
import struct
import random
import math
import textwrap

# ─── paths ────────────────────────────────────────────────────────────────────
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR   = os.path.join(SCRIPT_DIR, "data")

SIZES      = [64, 128, 256, 512]
PRECISIONS = ["fp32", "fp16", "bf16"]

KERNEL_DIRS = [
    "01_RoPE", "02_Mish", "03_Stable_Softmax",
    "04_RMSNorm", "05_GeLU_Tanh", "06_LogCosh_Loss",
]
VARIANTS = ["baseline_function", "custom_function"]

# ─── reproducible random data ────────────────────────────────────────────────
random.seed(42)
MAX_SIZE    = max(SIZES)
BASE_VALUES = [random.uniform(-3.0, 3.0) for _ in range(MAX_SIZE)]

# ═══════════════════════════════════════════════════════════════════════════════
#  Precision helpers
# ═══════════════════════════════════════════════════════════════════════════════

def float_to_fp16_hex(val):
    """Convert Python float → IEEE 754 binary16 hex."""
    return struct.unpack('<H', struct.pack('<e', val))[0]

def float_to_bf16_hex(val):
    """Convert Python float → bfloat16 hex (round to nearest even)."""
    i32 = struct.unpack('<I', struct.pack('<f', val))[0]
    upper = (i32 >> 16) & 0xFFFF
    lower = i32 & 0xFFFF
    if lower > 0x8000 or (lower == 0x8000 and (upper & 1)):
        upper += 1
    return upper

# ═══════════════════════════════════════════════════════════════════════════════
#  1. Data-header generation
# ═══════════════════════════════════════════════════════════════════════════════

def _gen_fp32_data(size):
    vals = BASE_VALUES[:size]
    lines = [
        f"// Auto-generated data — {size} FP32 values in [-3, 3]",
        "#pragma once", "",
        "#define B_Size 1",
        "#define T_Size 1",
        f"#define C_Size {size}", "",
        'float __attribute__((section(".data"))) data_signed[] = {',
    ]
    for i in range(0, size, 4):
        chunk = vals[i:i+4]
        fmtd  = ", ".join(f"{v:>15.8e}f" for v in chunk)
        comma = "," if i + 4 < size else ""
        lines.append(f"  {fmtd}{comma}")
    lines += ["};", ""]
    return "\n".join(lines)


def _gen_fp16_data(size):
    vals = BASE_VALUES[:size]
    lines = [
        f"// Auto-generated data — {size} FP16 values in [-3, 3] (uint16_t hex)",
        "#pragma once", "",
        "#include <stdint.h>", "",
        "#define B_Size 1",
        "#define T_Size 1",
        f"#define C_Size {size}", "",
        'uint16_t __attribute__((section(".data"))) data_signed[] = {',
    ]
    for i in range(0, size, 8):
        chunk = vals[i:min(i+8, size)]
        fmtd  = ", ".join(f"0x{float_to_fp16_hex(v):04X}u" for v in chunk)
        comma = "," if i + 8 < size else ""
        lines.append(f"  {fmtd}{comma}")
    lines += ["};", ""]
    return "\n".join(lines)


def _gen_bf16_data(size):
    vals = BASE_VALUES[:size]
    lines = [
        f"// Auto-generated data — {size} BF16 values in [-3, 3] (uint16_t hex)",
        "#pragma once", "",
        "#include <stdint.h>", "",
        "#define B_Size 1",
        "#define T_Size 1",
        f"#define C_Size {size}", "",
        'uint16_t __attribute__((section(".data"))) data_signed[] = {',
    ]
    for i in range(0, size, 8):
        chunk = vals[i:min(i+8, size)]
        fmtd  = ", ".join(f"0x{float_to_bf16_hex(v):04X}u" for v in chunk)
        comma = "," if i + 8 < size else ""
        lines.append(f"  {fmtd}{comma}")
    lines += ["};", ""]
    return "\n".join(lines)


def generate_data_headers():
    """Write all 12 data headers into data/."""
    os.makedirs(DATA_DIR, exist_ok=True)
    gen_map = {"fp32": _gen_fp32_data, "fp16": _gen_fp16_data, "bf16": _gen_bf16_data}
    for prec in PRECISIONS:
        for sz in SIZES:
            fname = f"data_{prec}_{sz}.h"
            path  = os.path.join(DATA_DIR, fname)
            with open(path, "w") as f:
                f.write(gen_map[prec](sz))
            print(f"  [data] {fname}")

# ═══════════════════════════════════════════════════════════════════════════════
#  2. Modify existing FP32 kernels for configurable data include
# ═══════════════════════════════════════════════════════════════════════════════
CONFIGURABLE_INCLUDE = textwrap.dedent("""\
    /* -- data include (overridable via -DDATA_HEADER_PATH=...) -- */
    #if defined(DATA_HEADER_PATH)
    #define _DATA_STR2(x) #x
    #define _DATA_STR(x)  _DATA_STR2(x)
    #include _DATA_STR(DATA_HEADER_PATH)
    #else
    #include "../data/data.h"
    #endif
""")

def patch_fp32_kernels():
    """Replace the fixed data include in each FP32 kernel with a configurable one."""
    old_include = '#include "../data/data.h"'
    for kdir in KERNEL_DIRS:
        for variant in VARIANTS:
            path = os.path.join(SCRIPT_DIR, kdir, f"{variant}.c")
            if not os.path.isfile(path):
                print(f"  [skip] {kdir}/{variant}.c not found")
                continue
            with open(path, "r") as f:
                src = f.read()
            if "DATA_HEADER_PATH" in src:
                print(f"  [skip] {kdir}/{variant}.c already patched")
                continue
            if old_include not in src:
                print(f"  [warn] {kdir}/{variant}.c – include not found")
                continue
            src = src.replace(old_include, CONFIGURABLE_INCLUDE.rstrip(), 1)
            with open(path, "w") as f:
                f.write(src)
            print(f"  [patch] {kdir}/{variant}.c")

# ═══════════════════════════════════════════════════════════════════════════════
#  3. FP16 / BF16 kernel generation  (transform FP32 source)
# ═══════════════════════════════════════════════════════════════════════════════

# ── hex constants ─────────────────────────────────────────────────────────────
FP16_CONSTS = {
    # Schraudolph exp
    "SCH_C":        f"0x{float_to_fp16_hex(1477.3):04X}u",    # 2^10/ln2
    "SCH_B":        f"0x{float_to_fp16_hex(15360.0):04X}u",   # 15 × 2^10
    # Fast ln
    "LN_BIAS":      "0x3C00u",                                 # int bias for FP16
    "LN_SCALE":     f"0x{float_to_fp16_hex(math.log(2)/1024.0):04X}u",
    # Tanh polynomial
    "TANH_A":       f"0x{float_to_fp16_hex(0.02655122):04X}u",
    "TANH_B":       f"0x{float_to_fp16_hex(-0.22830456):04X}u",
    "TANH_C":       f"0x{float_to_fp16_hex(0.97858769):04X}u",
    # Cody-Waite sin/cos
    "CW_C2":        f"0x{float_to_fp16_hex(-0.49670):04X}u",
    "CW_S3":        f"0x{float_to_fp16_hex(-0.16605):04X}u",
    "CW_INVPIO2":   f"0x{float_to_fp16_hex(0.63661977):04X}u",
    "CW_PIO2":      f"0x{float_to_fp16_hex(1.57079632679):04X}u",
    # GeLU
    "GELU_COEFF":   f"0x{float_to_fp16_hex(0.044715):04X}u",
    "SQRT_2_PI":    f"0x{float_to_fp16_hex(0.7978845608):04X}u",
    # Misc
    "RSQRT_MAGIC":  "0x59BAu",
    "REC_MAGIC":    "0x7BFFu",
    "EPS":          f"0x{float_to_fp16_hex(1e-4):04X}u",
    # Common scalars
    "ONE":          "0x3C00u",
    "HALF":         "0x3800u",
    "NEG_ONE":      "0xBC00u",
    "TWO":          "0x4000u",
    "THREE_HALVES": "0x3E00u",
    "ZERO":         "0x0000u",
    "NEG_INF":      "0xFC00u",
    "ROPE_BASE":    f"0x{float_to_fp16_hex(10000.0):04X}u",
    "PT5":          "0x3800u",   # 0.5
}

BF16_CONSTS = {
    # Schraudolph exp
    "SCH_C":        "0x4339u",     # ~185.0  (2^7/ln2)
    "SCH_B":        "0x467Eu",     # 16256.0 (127 × 2^7)
    # Fast ln
    "LN_BIAS":      "0x3F80u",    # int bias for BF16
    "LN_SCALE":     f"0x{float_to_bf16_hex(math.log(2)/128.0):04X}u",
    # Tanh polynomial
    "TANH_A":       f"0x{float_to_bf16_hex(0.02655122):04X}u",
    "TANH_B":       f"0x{float_to_bf16_hex(-0.22830456):04X}u",
    "TANH_C":       f"0x{float_to_bf16_hex(0.97858769):04X}u",
    # Cody-Waite sin/cos
    "CW_C2":        f"0x{float_to_bf16_hex(-0.49670):04X}u",
    "CW_S3":        f"0x{float_to_bf16_hex(-0.16605):04X}u",
    "CW_INVPIO2":   f"0x{float_to_bf16_hex(0.63661977):04X}u",
    "CW_PIO2":      f"0x{float_to_bf16_hex(1.57079632679):04X}u",
    # GeLU
    "GELU_COEFF":   f"0x{float_to_bf16_hex(0.044715):04X}u",
    "SQRT_2_PI":    f"0x{float_to_bf16_hex(0.7978845608):04X}u",
    # Misc
    "RSQRT_MAGIC":  "0x5F37u",
    "REC_MAGIC":    "0x7EFFu",
    "EPS":          f"0x{float_to_bf16_hex(1e-6):04X}u",
    # Common scalars
    "ONE":          "0x3F80u",
    "HALF":         "0x3F00u",
    "NEG_ONE":      "0xBF80u",
    "TWO":          "0x4000u",
    "THREE_HALVES": "0x3FC0u",
    "ZERO":         "0x0000u",
    "NEG_INF":      "0xFF80u",
    "ROPE_BASE":    f"0x{float_to_bf16_hex(10000.0):04X}u",
    "PT5":          "0x3F00u",
}

# ── NaN-boxing helper (shared by all FP16/BF16 kernels) ──────────────────────
NANBOX_HELPER = textwrap.dedent("""\
    /* NaN-box a 16-bit FP constant into bits[15:0] of a float register.
     * Works for both FP16 and BF16; the Spatz VFU reads rs2[15:0] for e16. */
    static inline float h_scalar(uint32_t hex16) {
        float f;
        uint32_t nanboxed = 0xFFFF0000u | hex16;
        asm volatile("fmv.w.x %0, %1" : "=f"(f) : "r"(nanboxed));
        return f;
    }
""")

# ── transformation engine ─────────────────────────────────────────────────────

def _transform_kernel(src, prec, kdir):
    """
    Transform an FP32 kernel source string into an FP16 or BF16 variant.
    Returns the transformed source string.
    """
    consts = FP16_CONSTS if prec == "fp16" else BF16_CONSTS
    precision_tag = "FP16" if prec == "fp16" else "BF16"

    out = src

    # ── 0. Replace top-of-file precision comment ──────────────────────────
    out = re.sub(r'/\*.*?\*/', lambda m: m.group(0).replace("(RV64GCV)", f"({precision_tag})").replace("FP32", precision_tag), out, count=1, flags=re.DOTALL)

    # ── 1. Data include → configurable ────────────────────────────────────
    # If the source is already patched (has DATA_HEADER_PATH), just replace
    # the default fallback path. Otherwise, wrap with the configurable block.
    if "DATA_HEADER_PATH" in src:
        # Already patched: replace ALL remaining #include "../data/data*.h"
        # within the #else fallback with the new precision-specific default
        out = re.sub(
            r'#include\s+"../data/data[^"]*\.h"',
            f'#include "../data/data_{prec}_128.h"',
            out
        )
    else:
        old_inc = '#include "../data/data.h"'
        new_inc = CONFIGURABLE_INCLUDE.rstrip().replace(
            '#include "../data/data.h"',
            f'#include "../data/data_{prec}_128.h"'
        )
        out = out.replace(old_inc, new_inc, 1)

    # ── 2. Add stdint.h if missing ────────────────────────────────────────
    if "#include <stdint.h>" not in out:
        out = out.replace("#include <math.h>", "#include <math.h>\n#include <stdint.h>", 1)

    # ── 3. Insert NaN-boxing helper after the last #include line ──────────
    # Find last #include
    includes = list(re.finditer(r'^#include\s+.*$', out, re.MULTILINE))
    if includes:
        insert_pos = includes[-1].end()
        out = out[:insert_pos] + "\n\n" + NANBOX_HELPER + out[insert_pos:]

    # ── 4. Assembly: e32 → e16, vle32 → vle16, vse32 → vse16 ────────────
    out = out.replace("e32, m4", "e16, m4")
    out = out.replace("e32, m1", "e16, m1")
    out = out.replace("vle32.v", "vle16.v")
    out = out.replace("vse32.v", "vse16.v")

    # ── 5. Data pointer types: float * → uint16_t * ──────────────────────
    #    Function parameters and global declarations
    #    Match all const float* and non-const float* followed by known data var names
    out = re.sub(r'\bconst\s+float\s*\*', 'const uint16_t *', out)
    _data_ptr_names = (
        r'g_\w+|out\w*|inp\w*|dst\w*|src\w*|pred\w*|truth\w*|'
        r'theta\w*|x_even\w*|x_odd\w*|exp_buf\w*'
    )
    out = re.sub(rf'\bfloat\s*\*\s*(?=(?:{_data_ptr_names})\b)', 'uint16_t *', out)

    # ── 6. Allocation casts: (float *) → (uint16_t *) ────────────────────
    out = out.replace("(float *)snrt_l1alloc", "(uint16_t *)snrt_l1alloc")

    # ── 7. sizeof(float) → sizeof(uint16_t) ──────────────────────────────
    out = out.replace("sizeof(float)", "sizeof(uint16_t)")

    # ── 8. Scalar constant replacements ──────────────────────────────────
    # Replace static const float declarations with h_scalar() calls
    _scalar_map = {
        # Schraudolph
        ("SCH_C",  r'12102203\.0f'):               consts["SCH_C"],
        ("SCH_B",  r'1064866805\.0f'):              consts["SCH_B"],
        # Cody-Waite
        ("CW_C2",  r'-0\.49670f'):                  consts["CW_C2"],
        ("CW_S3",  r'-0\.16605f'):                  consts["CW_S3"],
        ("CW_INVPIO2", r'0\.63661977f'):            consts["CW_INVPIO2"],
        ("CW_PIO2",r'1\.57079632679489661923f'):    consts["CW_PIO2"],
        # Tanh
        ("TANH_A", r'0\.02655122f'):                consts["TANH_A"],
        ("TANH_B", r'-0\.22830456f'):               consts["TANH_B"],
        ("TANH_C", r'0\.97858769f'):                consts["TANH_C"],
        # GeLU
        ("GELU_COEFF", r'0\.044715f'):              consts["GELU_COEFF"],
        ("SQRT_2_PI",  r'0\.7978845608f'):          consts["SQRT_2_PI"],
        # RoPE
        ("ROPE_BASE",  r'10000\.0f'):               consts["ROPE_BASE"],
    }
    for name, pattern, hexval in [(n, p, h) for (n, p), h in _scalar_map.items()]:
        # Replace 'static const float NAME = VALUE;' with '#define + h_scalar'
        decl_re = rf'static\s+const\s+float\s+{name}\s*=\s*{pattern}\s*;'
        if re.search(decl_re, out):
            out = re.sub(decl_re, f"#define H_{name} {hexval}", out)
            # Replace usage in asm constraints: %[xxx]"f"(NAME)
            # The NAME is now a macro → wrap in h_scalar()
            out = re.sub(rf'\b{name}\b(?!\s*=)(?!.*#define)', f"h_scalar(H_{name})", out)

    # ── 9. Replace remaining float scalar locals used as asm operands ─────
    # For 'float one = 1.0f;' etc.
    _local_scalar_map = [
        (r'float\s+pos_one\s*=\s*1\.0f\s*;',        f'float pos_one = h_scalar({consts["ONE"]});'),
        (r'float\s+neg_one\s*=\s*-1\.0f\s*;',        f'float neg_one = h_scalar({consts["NEG_ONE"]});'),
        (r'float\s+one\s*=\s*1\.0f\s*;',              f'float one  = h_scalar({consts["ONE"]});'),
        (r'float\s+half\s*=\s*0\.5f\s*;',             f'float half = h_scalar({consts["HALF"]});'),
        (r'float\s+zero_f\s*=\s*0\.0f\s*;',           f'float zero_f = h_scalar({consts["ZERO"]});'),
        (r'float\s+neg_inf\s*=\s*-__builtin_inff\(\);', f'float neg_inf = h_scalar({consts["NEG_INF"]});'),
    ]
    for pat, repl in _local_scalar_map:
        out = re.sub(pat, repl, out)

    # ── 9b. Replace bare float literals in asm constraints ────────────────
    # e.g. [ONE]"f"(1.0f) → [ONE]"f"(h_scalar(0x3C00u))
    _asm_literal_map = [
        (r'"f"\(1\.0f\)',   f'"f"(h_scalar({consts["ONE"]}))'),
        (r'"f"\(0\.5f\)',   f'"f"(h_scalar({consts["HALF"]}))'),
        (r'"f"\(0\.0f\)',   f'"f"(h_scalar({consts["ZERO"]}))'),
        (r'"f"\(-1\.0f\)',  f'"f"(h_scalar({consts["NEG_ONE"]}))'),
        (r'"f"\(2\.0f\)',   f'"f"(h_scalar({consts["TWO"]}))'),
        (r'"f"\(1\.5f\)',   f'"f"(h_scalar({consts["THREE_HALVES"]}))'),
    ]
    for pat, repl in _asm_literal_map:
        out = re.sub(pat, repl, out)

    # ── 10. Integer constants for bit tricks ──────────────────────────────
    # LN_BIAS, RSQRT_MAGIC, REC_MAGIC
    out = re.sub(r'static\s+const\s+unsigned\s+int\s+LN_BIAS\s*=\s*0x3F800000u\s*;',
                 f'#define H_LN_BIAS {consts["LN_BIAS"]}', out)
    out = re.sub(r'\bLN_BIAS\b(?!.*#define)', f'((unsigned int)H_LN_BIAS)', out)

    out = re.sub(r'static\s+const\s+float\s+LN_SCALE\s*=\s*8\.26295829e-8f\s*;',
                 f'#define H_LN_SCALE {consts["LN_SCALE"]}', out)
    out = re.sub(r'\bLN_SCALE\b(?!.*#define)', f'h_scalar(H_LN_SCALE)', out)

    out = re.sub(r'static\s+const\s+unsigned\s+int\s+RSQRT_MAGIC\s*=\s*0x5F3759DFu\s*;',
                 f'#define H_RSQRT_MAGIC {consts["RSQRT_MAGIC"]}', out)
    out = re.sub(r'\bRSQRT_MAGIC\b(?!.*#define)', 'H_RSQRT_MAGIC', out)

    out = re.sub(r'static\s+const\s+unsigned\s+int\s+REC_MAGIC\s*=\s*0x7EFFFFFFu\s*;',
                 f'#define H_REC_MAGIC {consts["REC_MAGIC"]}', out)
    out = re.sub(r'\bREC_MAGIC\b(?!.*#define)', 'H_REC_MAGIC', out)

    out = re.sub(r'static\s+const\s+float\s+EPS\s*=\s*1e-6f\s*;',
                 f'#define H_EPS {consts["EPS"]}', out)
    out = re.sub(r'\bEPS\b(?!.*#define)', 'h_scalar(H_EPS)', out)

    # ── 11. printf adaptations ────────────────────────────────────────────
    # For check_result and printf with float values: change to note uint16_t
    # We'll just leave printf as-is; the validation will print raw values or
    # we remove the content. Let's simplify: replace float args with (int) casts.
    # Actually, just change the check_result to print hex or skip detailed output.
    out = re.sub(
        r'printf\("  out\[%d\] = %f\\n", i, out\[i\]\);',
        r'printf("  out[%d] = 0x%04X\\n", i, (unsigned)out[i]);',
        out
    )
    out = re.sub(
        r'printf\("  x\[%d\]=%f  mish=%f\\n", i, input\[i\], x\[i\]\);',
        r'printf("  x[%d]=0x%04X  mish=0x%04X\\n", i, (unsigned)input[i], (unsigned)x[i]);',
        out
    )
    out = re.sub(
        r'printf\("  x\[%d\]=%f  sm=%f\\n", i, input\[i\], x\[i\]\);',
        r'printf("  x[%d]=0x%04X  sm=0x%04X\\n", i, (unsigned)input[i], (unsigned)x[i]);',
        out
    )
    out = re.sub(
        r'printf\("  x\[%d\]=%f  norm=%f\\n", i, input\[i\], x\[i\]\);',
        r'printf("  x[%d]=0x%04X  norm=0x%04X\\n", i, (unsigned)input[i], (unsigned)x[i]);',
        out
    )
    out = re.sub(
        r'printf\("  x\[%d\]=%f  gelu=%f\\n", i, input\[i\], x\[i\]\);',
        r'printf("  x[%d]=0x%04X  gelu=0x%04X\\n", i, (unsigned)input[i], (unsigned)x[i]);',
        out
    )
    out = re.sub(
        r'printf\("  loss\[%d\] = %f\\n", i, x\[i\]\);',
        r'printf("  loss[%d] = 0x%04X\\n", i, (unsigned)x[i]);',
        out
    )
    # Sum check in softmax/logcosh: float sum → remove or simplify
    out = re.sub(
        r'float sum = 0\.0f;\s*\n\s*for \(int i = 0; i < r; i\+\+\) sum \+= x\[i\];',
        '/* sum check skipped for 16-bit mode */',
        out
    )
    out = re.sub(
        r'printf\("  sum\(softmax\) = %f  \(should be ≈1\.0\)\\n", sum\);',
        r'printf("  [16-bit mode]\\n");',
        out
    )
    out = re.sub(
        r'printf\("  mean_loss = %f\\n", sum / \(float\)r\);',
        r'printf("  [16-bit mode]\\n");',
        out
    )

    # ── 12. data_signed usage for non-DMA init ─────────────────────────────
    # e.g. g_theta[i] = data_signed[i] * 0.5f;  →  vector multiply approach
    # For simplicity in 16-bit, replace manual init loops with DMA copies
    # and constant manipulation via vector ops.
    # We handle RoPE's manual init and LogCosh's true-data init by keeping
    # the loops but operating on uint16_t (the multiply by 0.5 becomes a
    # vector element-wise op — but in the init loop, we'll use a simple
    # vector approach or keep the scalar approach with h_scalar).

    # For RoPE init: data_signed[i] * 0.5f
    # Replace with a vector-based initialization
    # Actually, for simplicity, let's just init via DMA and skip the scaling
    # (the scaling doesn't affect the benchmark, just the input values)

    # Replace manual float arithmetic on data elements with uint16_t assignments
    # RoPE: g_theta[i] = data_signed[i] * 0.5f → g_theta[i] = data_signed[i];
    out = re.sub(r'g_theta\[i\]\s*=\s*data_signed\[i\]\s*\*\s*0\.5f;',
                 'g_theta[i] = data_signed[i];', out)
    # RoPE: g_x_even[i] = data_signed[i]; → stays same (uint16_t = uint16_t)
    # RoPE: g_x_odd[i] = data_signed[(i + half) % N]; → stays same

    # LogCosh: g_true[i] = data_signed[(i + N / 4) % N] * 0.8f;
    # → just copy shifted values without scaling
    out = re.sub(r'g_true\[i\]\s*=\s*data_signed\[\(i \+ N / 4\) % N\]\s*\*\s*0\.8f;',
                 'g_true[i] = data_signed[(i + N / 4) % N];', out)

    # ── 12b. Fix scalar extraction → partial array store for reduction kernels ─
    # Pattern: float local_VAR; asm volatile( ... vfmv.f.s %[xx], vNN ... ); g_partial_XX[cid] = local_VAR;
    # Replace with direct vector store from the reduction register.
    _extract_store_patterns = [
        # Softmax pass1: local_max from v8
        (r'float local_max;\s*\n'
         r'\s*asm volatile\(\s*\n'
         r'\s*"vsetivli\s+zero,\s*1,\s*e16,\s*m1,\s*ta,\s*ma\s*\\n\\t"\s*\n'
         r'\s*"vfmv\.f\.s\s+%\[mx\],\s*v8\s*\\n\\t"\s*\n'
         r'\s*:\s*\[mx\]"=f"\(local_max\).*?\n'
         r'\s*\);\s*\n'
         r'\s*g_partial_max\[cid\]\s*=\s*local_max;',
         '/* store 16-bit max directly from vector register */\n'
         '    asm volatile(\n'
         '        "vsetivli  zero, 1, e16, m1, ta, ma  \\n\\t"\n'
         '        "vse16.v   v8, (%[dst])               \\n\\t"\n'
         '        : : [dst]"r"(&g_partial_max[cid]) : "memory"\n'
         '    );'),
        # Softmax pass2: local_sum from v12
        (r'float local_sum;\s*\n'
         r'\s*asm volatile\(\s*\n'
         r'\s*"vsetivli\s+zero,\s*1,\s*e16,\s*m1,\s*ta,\s*ma\s*\\n\\t"\s*\n'
         r'\s*"vfmv\.f\.s\s+%\[s\],\s*v12\s*\\n\\t"\s*\n'
         r'\s*:\s*\[s\]"=f"\(local_sum\).*?\n'
         r'\s*\);\s*\n'
         r'\s*g_partial_sum\[cid\]\s*=\s*local_sum;',
         '/* store 16-bit sum directly from vector register */\n'
         '    asm volatile(\n'
         '        "vsetivli  zero, 1, e16, m1, ta, ma  \\n\\t"\n'
         '        "vse16.v   v12, (%[dst])              \\n\\t"\n'
         '        : : [dst]"r"(&g_partial_sum[cid]) : "memory"\n'
         '    );'),
        # RMSNorm pass1: local_sq from v8
        (r'float local_sq;\s*\n'
         r'\s*asm volatile\(\s*\n'
         r'\s*"vsetivli\s+zero,\s*1,\s*e16,\s*m1,\s*ta,\s*ma\s*\\n\\t"\s*\n'
         r'\s*"vfmv\.f\.s\s+%\[sq\],\s*v8\s*\\n\\t"\s*\n'
         r'\s*:\s*\[sq\]"=f"\(local_sq\).*?\n'
         r'\s*\);\s*\n'
         r'\s*g_partial_sq\[cid\]\s*=\s*local_sq;',
         '/* store 16-bit sq-sum directly from vector register */\n'
         '    asm volatile(\n'
         '        "vsetivli  zero, 1, e16, m1, ta, ma  \\n\\t"\n'
         '        "vse16.v   v8, (%[dst])               \\n\\t"\n'
         '        : : [dst]"r"(&g_partial_sq[cid]) : "memory"\n'
         '    );'),
    ]
    for pat, repl in _extract_store_patterns:
        out = re.sub(pat, repl, out, flags=re.DOTALL)

    # ── 13. Softmax/RMSNorm: adapt Newton-Raphson scalar ops ─────────────
    # The NR steps do scalar float arithmetic: y = y * (2.0f - total_sum * y)
    # For 16-bit, we need to do this via h_scalar values and vector single-elem ops
    # Replace the scalar NR block with a vectorized single-element version

    # 13a. Softmax BASELINE NR block (union-based magic-seed + 2 NR)
    # Use [^\n]*\n to match NR lines with optional trailing comments
    out = re.sub(
        r'union \{ float f; uint32_t u; \} seed, tmp;\s*\n'
        r'\s*tmp\.f = total_sum;\s*\n'
        r'\s*seed\.u = H?_?REC_MAGIC - tmp\.u;\s*\n'
        r'\s*float y = seed\.f;\s*\n'
        r'\s*y = y \* \(2\.0f - total_sum \* y\);[^\n]*\n'
        r'\s*y = y \* \(2\.0f - total_sum \* y\);[^\n]*\n'
        r'\s*\*g_inv_sum = y;',
        f'/* HW reciprocal + NR via single-element vector ops */\n'
        f'        float inv_y;\n'
        f'        asm volatile(\n'
        f'            "vsetivli zero, 1, e16, m1, ta, ma \\n\\t"\n'
        f'            "vfmv.v.f v0, %[s]                 \\n\\t"\n'
        f'            "vfrec7.v v0, v0                    \\n\\t"\n'
        f'            "vfmv.f.s %[r], v0                  \\n\\t"\n'
        f'            : [r]"=f"(inv_y) : [s]"f"(total_sum) : "v0"\n'
        f'        );\n'
        f'        /* 1 NR step in 16-bit via vector ops */\n'
        f'        float two_h = h_scalar({consts["TWO"]});\n'
        f'        asm volatile(\n'
        f'            "vsetivli zero, 1, e16, m1, ta, ma \\n\\t"\n'
        f'            "vfmv.v.f v0, %[x]                 \\n\\t"\n'
        f'            "vfmv.v.f v1, %[y]                 \\n\\t"\n'
        f'            "vfmul.vv v2, v0, v1               \\n\\t"\n'
        f'            "vfrsub.vf v2, v2, %[two]          \\n\\t"\n'
        f'            "vfmul.vv v1, v1, v2               \\n\\t"\n'
        f'            "vfmv.f.s %[r], v1                  \\n\\t"\n'
        f'            : [r]"=f"(inv_y)\n'
        f'            : [x]"f"(total_sum), [y]"f"(inv_y), [two]"f"(two_h)\n'
        f'            : "v0", "v1", "v2"\n'
        f'        );\n'
        f'        *g_inv_sum = inv_y;',
        out, flags=re.DOTALL
    )

    # 13b. Softmax CUSTOM NR (after vfrec7.v HW seed, 2 C-level NR steps)
    # Pattern: y = y * (2.0f - total_sum * y); ... *g_inv_sum = y;
    out = re.sub(
        r'y = y \* \(2\.0f - total_sum \* y\);[^\n]*\n'
        r'\s*y = y \* \(2\.0f - total_sum \* y\);[^\n]*\n'
        r'\s*\*g_inv_sum = y;',
        f'/* 1 NR step in 16-bit via vector ops (replaces 2 C-level steps) */\n'
        f'        {{ float nr_two = h_scalar({consts["TWO"]});\n'
        f'        asm volatile(\n'
        f'            "vsetivli zero, 1, e16, m1, ta, ma \\n\\t"\n'
        f'            "vfmv.v.f v0, %[x]                 \\n\\t"\n'
        f'            "vfmv.v.f v1, %[y]                 \\n\\t"\n'
        f'            "vfmul.vv v2, v0, v1               \\n\\t"\n'
        f'            "vfrsub.vf v2, v2, %[two]          \\n\\t"\n'
        f'            "vfmul.vv v1, v1, v2               \\n\\t"\n'
        f'            "vfmv.f.s %[r], v1                  \\n\\t"\n'
        f'            : [r]"=f"(y)\n'
        f'            : [x]"f"(total_sum), [y]"f"(y), [two]"f"(nr_two)\n'
        f'            : "v0", "v1", "v2"\n'
        f'        ); }}\n'
        f'        *g_inv_sum = y;',
        out
    )

    # 13c. RMSNorm BASELINE: Quake III rsqrt + 2 NR
    out = re.sub(
        r'union \{ float f; uint32_t u; \} conv;\s*\n'
        r'\s*conv\.f = mean_sq;\s*\n'
        r'\s*conv\.u = H?_?RSQRT_MAGIC - \(conv\.u >> 1\);\s*\n'
        r'\s*float y = conv\.f;\s*\n'
        r'\s*float half_x = 0\.5f \* mean_sq;\s*\n'
        r'\s*y = y \* \(1\.5f - half_x \* y \* y\);[^\n]*\n'
        r'\s*y = y \* \(1\.5f - half_x \* y \* y\);[^\n]*\n'
        r'\s*\*g_inv_rms = y;',
        f'/* HW rsqrt + 1 NR step via single-element vector ops */\n'
        f'        float y;\n'
        f'        asm volatile(\n'
        f'            "vsetivli zero, 1, e16, m1, ta, ma \\n\\t"\n'
        f'            "vfmv.v.f v0, %[ms]                \\n\\t"\n'
        f'            "vfrsqrt7.v v0, v0                  \\n\\t"\n'
        f'            "vfmv.f.s %[y], v0                  \\n\\t"\n'
        f'            : [y]"=f"(y) : [ms]"f"(mean_sq) : "v0"\n'
        f'        );\n'
        f'        float threehalves = h_scalar({consts["THREE_HALVES"]});\n'
        f'        float half_h = h_scalar({consts["HALF"]});\n'
        f'        asm volatile(\n'
        f'            "vsetivli zero, 1, e16, m1, ta, ma \\n\\t"\n'
        f'            "vfmv.v.f v0, %[ms]                \\n\\t"\n'
        f'            "vfmul.vf v0, v0, %[hx]            \\n\\t"\n'
        f'            "vfmv.v.f v1, %[y]                 \\n\\t"\n'
        f'            "vfmul.vv v2, v1, v1               \\n\\t"\n'
        f'            "vfmul.vv v2, v0, v2               \\n\\t"\n'
        f'            "vfrsub.vf v2, v2, %[th]           \\n\\t"\n'
        f'            "vfmul.vv v1, v1, v2               \\n\\t"\n'
        f'            "vfmv.f.s %[r], v1                  \\n\\t"\n'
        f'            : [r]"=f"(y)\n'
        f'            : [ms]"f"(mean_sq), [y]"f"(y), [hx]"f"(half_h), [th]"f"(threehalves)\n'
        f'            : "v0", "v1", "v2"\n'
        f'        );\n'
        f'        *g_inv_rms = y;',
        out, flags=re.DOTALL
    )

    # 13d. RMSNorm CUSTOM NR (after vfrsqrt7.v HW seed, 1 C-level NR step)
    # Pattern: float half_x = 0.5f * mean_sq; y = y * (1.5f - half_x * y * y); *g_inv_rms = y;
    out = re.sub(
        r'float half_x = 0\.5f \* mean_sq;\s*\n'
        r'\s*y = y \* \(1\.5f - half_x \* y \* y\);[^\n]*\n'
        r'\s*\*g_inv_rms = y;',
        f'/* 1 NR step in 16-bit via vector ops */\n'
        f'        {{ float nr_threehalves = h_scalar({consts["THREE_HALVES"]});\n'
        f'        float nr_half = h_scalar({consts["HALF"]});\n'
        f'        asm volatile(\n'
        f'            "vsetivli zero, 1, e16, m1, ta, ma \\n\\t"\n'
        f'            "vfmv.v.f v0, %[ms]                \\n\\t"\n'
        f'            "vfmul.vf v0, v0, %[hx]            \\n\\t"\n'
        f'            "vfmv.v.f v1, %[y]                 \\n\\t"\n'
        f'            "vfmul.vv v2, v1, v1               \\n\\t"\n'
        f'            "vfmul.vv v2, v0, v2               \\n\\t"\n'
        f'            "vfrsub.vf v2, v2, %[th]           \\n\\t"\n'
        f'            "vfmul.vv v1, v1, v2               \\n\\t"\n'
        f'            "vfmv.f.s %[r], v1                  \\n\\t"\n'
        f'            : [r]"=f"(y)\n'
        f'            : [ms]"f"(mean_sq), [y]"f"(y), [hx]"f"(nr_half), [th]"f"(nr_threehalves)\n'
        f'            : "v0", "v1", "v2"\n'
        f'        ); }}\n'
        f'        *g_inv_rms = y;',
        out
    )

    # 13e. RMSNorm mean_sq computation for 16-bit
    # 'float mean_sq = sq_sum / (float)N + h_scalar(H_EPS);'
    # Compute via 16-bit vector ops: mean_sq = sq_sum * (1/N) + eps
    out = re.sub(
        r'float mean_sq = sq_sum / \(float\)N \+ h_scalar\(H_EPS\);',
        f'/* compute mean_sq in 16-bit via vector ops */\n'
        f'        float mean_sq;\n'
        f'        {{\n'
        f'            float eps_h = h_scalar(H_EPS);\n'
        f'            float n_recip;\n'
        f'            asm volatile(\n'
        f'                "vsetivli zero, 1, e16, m1, ta, ma \\n\\t"\n'
        f'                "vmv.v.x  v0, %[n]                 \\n\\t"\n'
        f'                "vfcvt.f.xu.v v0, v0               \\n\\t"\n'
        f'                "vfrec7.v v0, v0                    \\n\\t"\n'
        f'                "vfmv.f.s %[r], v0                  \\n\\t"\n'
        f'                : [r]"=f"(n_recip) : [n]"r"(N) : "v0"\n'
        f'            );\n'
        f'            asm volatile(\n'
        f'                "vsetivli zero, 1, e16, m1, ta, ma \\n\\t"\n'
        f'                "vfmv.v.f v0, %[sq]                \\n\\t"\n'
        f'                "vfmul.vf v0, v0, %[inv]           \\n\\t"\n'
        f'                "vfadd.vf v0, v0, %[eps]           \\n\\t"\n'
        f'                "vfmv.f.s %[r], v0                  \\n\\t"\n'
        f'                : [r]"=f"(mean_sq)\n'
        f'                : [sq]"f"(sq_sum), [inv]"f"(n_recip), [eps]"f"(eps_h)\n'
        f'                : "v0"\n'
        f'            );\n'
        f'        }}',
        out
    )

    # ── 14. Softmax/RMSNorm partial sums: cross-core reduction ───────────
    # The partial arrays (g_partial_max, g_partial_sum, g_partial_sq) stay as float
    # because the NaN-boxed 16-bit values are stored/loaded as float register values.
    # The scalar comparison 'if (g_partial_max[c] > global_max)' won't work
    # because NaN-boxed values look like NaN in FP32.
    # Replace with vector-based reduction for the cross-core max.

    # Replace the cross-core max reduction loop
    out = re.sub(
        r'global_max = g_partial_max\[0\];\s*\n'
        r'\s*for \(int c = 1; c < SM_CORES; c\+\+\)\s*\n'
        r'\s*if \(g_partial_max\[c\] > global_max\)\s*\n'
        r'\s*global_max = g_partial_max\[c\];',
        f'/* reduce partial max values via vector instruction */\n'
        f'        float gm_neg_inf = h_scalar({consts["NEG_INF"]});\n'
        f'        asm volatile(\n'
        f'            "vsetivli zero, 1, e16, m1, ta, ma   \\n\\t"\n'
        f'            "vfmv.s.f v4, %[ni]                  \\n\\t"\n'
        f'            "vsetvli zero, %[n], e16, m1, ta, ma  \\n\\t"\n'
        f'            "vle16.v  v0, (%[src])               \\n\\t"\n'
        f'            "vfredmax.vs v4, v0, v4              \\n\\t"\n'
        f'            "vfmv.f.s %[mx], v4                  \\n\\t"\n'
        f'            : [mx]"=f"(global_max)\n'
        f'            : [src]"r"(g_partial_max), [n]"r"(SM_CORES), [ni]"f"(gm_neg_inf)\n'
        f'            : "v0", "v4"\n'
        f'        );',
        out
    )

    # Replace the cross-core sum reduction
    out = re.sub(
        r'float total_sum = 0\.0f;\s*\n'
        r'\s*for \(int c = 0; c < SM_CORES; c\+\+\)\s*\n'
        r'\s*total_sum \+= g_partial_sum\[c\];',
        f'/* reduce partial sums via vector instruction */\n'
        f'        float ts_zero = h_scalar({consts["ZERO"]});\n'
        f'        float total_sum;\n'
        f'        asm volatile(\n'
        f'            "vsetivli zero, 1, e16, m1, ta, ma   \\n\\t"\n'
        f'            "vfmv.s.f v4, %[z]                   \\n\\t"\n'
        f'            "vsetvli zero, %[n], e16, m1, ta, ma  \\n\\t"\n'
        f'            "vle16.v  v0, (%[src])               \\n\\t"\n'
        f'            "vfredusum.vs v4, v0, v4             \\n\\t"\n'
        f'            "vfmv.f.s %[s], v4                   \\n\\t"\n'
        f'            : [s]"=f"(total_sum)\n'
        f'            : [src]"r"(g_partial_sum), [n]"r"(SM_CORES), [z]"f"(ts_zero)\n'
        f'            : "v0", "v4"\n'
        f'        );',
        out
    )

    # Replace the sq_sum reduction (RMSNorm)
    out = re.sub(
        r'float sq_sum = 0\.0f;\s*\n'
        r'\s*for \(int c = 0; c < SM_CORES; c\+\+\)\s*\n'
        r'\s*sq_sum \+= g_partial_sq\[c\];',
        f'/* reduce partial sq-sums via vector instruction */\n'
        f'        float sq_zero = h_scalar({consts["ZERO"]});\n'
        f'        float sq_sum;\n'
        f'        asm volatile(\n'
        f'            "vsetivli zero, 1, e16, m1, ta, ma   \\n\\t"\n'
        f'            "vfmv.s.f v4, %[z]                   \\n\\t"\n'
        f'            "vsetvli zero, %[n], e16, m1, ta, ma  \\n\\t"\n'
        f'            "vle16.v  v0, (%[src])               \\n\\t"\n'
        f'            "vfredusum.vs v4, v0, v4             \\n\\t"\n'
        f'            "vfmv.f.s %[s], v4                   \\n\\t"\n'
        f'            : [s]"=f"(sq_sum)\n'
        f'            : [src]"r"(g_partial_sq), [n]"r"(SM_CORES), [z]"f"(sq_zero)\n'
        f'            : "v0", "v4"\n'
        f'        );',
        out
    )

    # ── 15. Broadcast slots: use uint16_t via vse16/vle16 ────────────────
    # g_partial_max[0] = global_max; → store via vector
    out = re.sub(
        r'g_partial_max\[0\] = global_max;\s*(/\*.*?\*/)?',
        f'/* broadcast via vector store */\n'
        f'        asm volatile(\n'
        f'            "vsetivli zero, 1, e16, m1, ta, ma \\n\\t"\n'
        f'            "vfmv.v.f v0, %[mx]               \\n\\t"\n'
        f'            "vse16.v  v0, (%[dst])             \\n\\t"\n'
        f'            : : [mx]"f"(global_max), [dst]"r"(g_partial_max) : "v0", "memory"\n'
        f'        );',
        out
    )

    # global_max = g_partial_max[0]; (all cores read)
    out = re.sub(
        r'global_max = g_partial_max\[0\];\s*(/\*.*?\*/)?',
        f'/* load broadcast value via vector */\n'
        f'    asm volatile(\n'
        f'        "vsetivli zero, 1, e16, m1, ta, ma \\n\\t"\n'
        f'        "vle16.v  v0, (%[src])             \\n\\t"\n'
        f'        "vfmv.f.s %[mx], v0               \\n\\t"\n'
        f'        : [mx]"=f"(global_max) : [src]"r"(g_partial_max) : "v0"\n'
        f'    );',
        out
    )

    # inv_sum broadcast: *g_inv_sum = VAR; → store via vector
    out = re.sub(
        r'\*g_inv_sum = (result|inv_y|y);',
        lambda m: (
            f'asm volatile(\n'
            f'            "vsetivli zero, 1, e16, m1, ta, ma \\n\\t"\n'
            f'            "vfmv.v.f v0, %[v]               \\n\\t"\n'
            f'            "vse16.v  v0, (%[dst])            \\n\\t"\n'
            f'            : : [v]"f"({m.group(1)}), [dst]"r"(g_inv_sum) : "v0", "memory"\n'
            f'        );'
        ),
        out
    )

    # float inv = *g_inv_sum; → load via vector
    out = re.sub(
        r'float inv = \*g_inv_sum;\s*(/\*.*?\*/)?',
        f'float inv;\n'
        f'    asm volatile(\n'
        f'        "vsetivli zero, 1, e16, m1, ta, ma \\n\\t"\n'
        f'        "vle16.v  v0, (%[src])             \\n\\t"\n'
        f'        "vfmv.f.s %[v], v0                 \\n\\t"\n'
        f'        : [v]"=f"(inv) : [src]"r"(g_inv_sum) : "v0"\n'
        f'    );',
        out
    )

    # *g_inv_rms = y;
    out = re.sub(
        r'\*g_inv_rms = y;',
        f'asm volatile(\n'
        f'            "vsetivli zero, 1, e16, m1, ta, ma \\n\\t"\n'
        f'            "vfmv.v.f v0, %[v]               \\n\\t"\n'
        f'            "vse16.v  v0, (%[dst])            \\n\\t"\n'
        f'            : : [v]"f"(y), [dst]"r"(g_inv_rms) : "v0", "memory"\n'
        f'        );',
        out
    )

    # float inv = *g_inv_rms;
    out = re.sub(
        r'float inv = \*g_inv_rms;\s*(/\*.*?\*/)?',
        f'float inv;\n'
        f'    asm volatile(\n'
        f'        "vsetivli zero, 1, e16, m1, ta, ma \\n\\t"\n'
        f'        "vle16.v  v0, (%[src])             \\n\\t"\n'
        f'        "vfmv.f.s %[v], v0                 \\n\\t"\n'
        f'        : [v]"=f"(inv) : [src]"r"(g_inv_rms) : "v0"\n'
        f'    );',
        out
    )

    # ── 16. Partial array allocation: use sizeof(uint16_t) ────────────────
    # Already handled by the generic sizeof(float) → sizeof(uint16_t) replacement
    # But the single inv_sum/inv_rms floats need 2 bytes too
    # snrt_l1alloc(1 * sizeof(uint16_t)) is fine for uint16_t storage

    # ── 17. Update printf tag with precision ──────────────────────────────
    out = out.replace("BASELINE]", f"BASELINE {precision_tag}]")
    out = out.replace("CUSTOM]",  f"CUSTOM {precision_tag}]")
    out = out.replace("2-CORE]",  f"2-CORE {precision_tag}]")

    return out


def generate_precision_kernels(prec):
    """Generate FP16 or BF16 kernel files by transforming FP32 originals."""
    for kdir in KERNEL_DIRS:
        out_dir = os.path.join(SCRIPT_DIR, f"{kdir}_{prec}")
        os.makedirs(out_dir, exist_ok=True)
        for variant in VARIANTS:
            src_path = os.path.join(SCRIPT_DIR, kdir, f"{variant}.c")
            dst_path = os.path.join(out_dir, f"{variant}.c")
            if not os.path.isfile(src_path):
                print(f"  [skip] {kdir}/{variant}.c not found")
                continue
            with open(src_path, "r") as f:
                src = f.read()
            transformed = _transform_kernel(src, prec, kdir)
            with open(dst_path, "w") as f:
                f.write(transformed)
            print(f"  [{prec}] {kdir}_{prec}/{variant}.c")


# ═══════════════════════════════════════════════════════════════════════════════
#  4. CMakeLists.txt generation
# ═══════════════════════════════════════════════════════════════════════════════

def generate_cmake():
    cmake = textwrap.dedent("""\
    # Copyright 2020 ETH Zurich and University of Bologna.
    # Licensed under the Apache License, Version 2.0, see LICENSE for details.
    # SPDX-License-Identifier: Apache-2.0
    #
    # ML Kernel Test Framework
    # Generates test binaries for 6 kernels × 2 variants × 3 precisions × 4 sizes
    #
    # Precisions:  fp32, fp16, bf16
    # Sizes:       64, 128, 256, 512
    #
    # Kernels:
    #   01_RoPE          – Rotary Positional Embedding (sin, cos)
    #   02_Mish          – Mish activation (exp, ln, tanh)
    #   03_Stable_Softmax – Numerically stable softmax (exp, rec)
    #   04_RMSNorm       – Root Mean Square Normalization (rsqrt)
    #   05_GeLU_Tanh     – GeLU Tanh approximation (tanh)
    #   06_LogCosh_Loss  – Log-Cosh loss function (cosh, ln)

    cmake_minimum_required(VERSION 3.13)

    if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
        set(CMAKE_TOOLCHAIN_FILE toolchain-gcc CACHE STRING "Toolchain to use")
        project(TestKernels LANGUAGES C ASM)
        include(SnitchUtilities)
        add_subdirectory(../snRuntime snRuntime)
    endif()

    include_directories(${SNRUNTIME_INCLUDE_DIRS})
    add_compile_options(-O3 -g -ffunction-sections)

    enable_testing()
    set(SNITCH_TEST_PREFIX test-kernel-)

    # ---------- kernel list ----------
    set(KERNEL_DIRS
        01_RoPE
        02_Mish
        03_Stable_Softmax
        04_RMSNorm
        05_GeLU_Tanh
        06_LogCosh_Loss
    )

    set(KERNEL_VARIANTS
        baseline_function
        custom_function
    )

    set(ELEMENT_COUNTS 64 128 256 512)
    set(PRECISIONS fp32 fp16 bf16)

    # ---------- generate targets: kernel × variant × precision × size ----------
    foreach(PREC ${PRECISIONS})
        foreach(NELEM ${ELEMENT_COUNTS})
            foreach(KDIR ${KERNEL_DIRS})
                # Source directory: fp32 uses original dir, fp16/bf16 use suffixed dirs
                if (PREC STREQUAL "fp32")
                    set(SRC_DIR ${KDIR})
                else()
                    set(SRC_DIR ${KDIR}_${PREC})
                endif()

                foreach(VARIANT ${KERNEL_VARIANTS})
                    set(TARGET_NAME ${KDIR}_${VARIANT}_${PREC}_n${NELEM})
                    set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}/${VARIANT}.c)

                    if (EXISTS ${SRC_FILE})
                        add_snitch_test(${TARGET_NAME} ${SRC_FILE})
                        # Tell the kernel which data header to include
                        target_compile_definitions(test-${SNITCH_TEST_PREFIX}${TARGET_NAME} PRIVATE
                            DATA_HEADER_PATH=../data/data_${PREC}_${NELEM}.h)
                    endif()
                endforeach()
            endforeach()
        endforeach()
    endforeach()
    """)
    path = os.path.join(SCRIPT_DIR, "CMakeLists.txt")
    with open(path, "w") as f:
        f.write(cmake)
    print(f"  [cmake] CMakeLists.txt")


# ═══════════════════════════════════════════════════════════════════════════════
#  Main
# ═══════════════════════════════════════════════════════════════════════════════

def main():
    print("=== Generating data headers ===")
    generate_data_headers()

    print("\n=== Patching FP32 kernels for configurable data include ===")
    patch_fp32_kernels()

    print("\n=== Generating FP16 kernels ===")
    generate_precision_kernels("fp16")

    print("\n=== Generating BF16 kernels ===")
    generate_precision_kernels("bf16")

    print("\n=== Generating CMakeLists.txt ===")
    generate_cmake()

    print("\nDone! Generated:")
    print(f"  - {len(PRECISIONS) * len(SIZES)} data headers")
    print(f"  - {len(KERNEL_DIRS)} FP16 kernel directories")
    print(f"  - {len(KERNEL_DIRS)} BF16 kernel directories")
    print(f"  - Updated CMakeLists.txt")
    print(f"\nTotal targets: {len(KERNEL_DIRS) * len(VARIANTS) * len(PRECISIONS) * len(SIZES)} = "
          f"{len(KERNEL_DIRS)}×{len(VARIANTS)}×{len(PRECISIONS)}×{len(SIZES)}")


if __name__ == "__main__":
    main()
