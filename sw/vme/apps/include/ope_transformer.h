// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
#pragma once

// Shared VME application vector math. No divider, sqrt, vfrec7 or vfrsqrt7.
// Instruction-string macros compose inside ONE asm volatile block, allowing
// the caller to interleave loads and compute without function calls.
// Register arguments are string literals for disjoint aligned LMUL groups.
// The caller must set e32/VL/LMUL and list every physical register clobbered.
// Operand bundles below provide the required named scalar input operands.
#if defined(OPE_EXP_SCALE32) && OPE_EXP_SCALE32
#define OPE_EXP_SCALE_VALUE 0.03125f
#else
#define OPE_EXP_SCALE_VALUE 0.015625f
#endif
#define EXP_INPUTS \
    [ope_lo] "f"(-16.0f), [ope_hi] "f"(20.0f), \
    [ope_scale] "f"(OPE_EXP_SCALE_VALUE), [ope_c5] "f"(0.0083333333f), \
    [ope_c4] "f"(0.0416666667f), [ope_c3] "f"(0.1666666667f), \
    [ope_c2] "f"(0.5f), [ope_one] "f"(1.0f)
#define RECIP_INPUTS \
    [ope_seed] "r"(0x7ef311c7u), [ope_two] "f"(2.0f)
#define RSQRT_INPUTS \
    [ope_rsqrt_seed] "r"(0x5f3759dfu), \
    [ope_half] "f"(0.5f), [ope_three_halves] "f"(1.5f)
#define RELU_INPUTS [ope_zero] "f"(0.0f)

#define EXP_ASM(x, t) \
    "vfmax.vf " x ", " x ", %[ope_lo]\n" \
    "vfmin.vf " x ", " x ", %[ope_hi]\n" \
    "vfmul.vf " x ", " x ", %[ope_scale]\n" \
    "vfmv.v.f " t ", %[ope_c5]\n" \
    "vfmul.vv " t ", " t ", " x "\n" \
    "vfadd.vf " t ", " t ", %[ope_c4]\n" \
    "vfmul.vv " t ", " t ", " x "\n" \
    "vfadd.vf " t ", " t ", %[ope_c3]\n" \
    "vfmul.vv " t ", " t ", " x "\n" \
    "vfadd.vf " t ", " t ", %[ope_c2]\n" \
    "vfmul.vv " t ", " t ", " x "\n" \
    "vfadd.vf " t ", " t ", %[ope_one]\n" \
    "vfmul.vv " t ", " t ", " x "\n" \
    "vfadd.vf " t ", " t ", %[ope_one]\n" \
    "vfmul.vv " x ", " t ", " t "\n" \
    "vfmul.vv " x ", " x ", " x "\n" \
    "vfmul.vv " x ", " x ", " x "\n" \
    "vfmul.vv " x ", " x ", " x "\n" \
    "vfmul.vv " x ", " x ", " x "\n" \
    "vfmul.vv " x ", " x ", " x "\n"

#define EXP_PAIR_ASM(x0, x1, t0, t1) \
    "vfmax.vf " x0 ", " x0 ", %[ope_lo]\n" \
    "vfmax.vf " x1 ", " x1 ", %[ope_lo]\n" \
    "vfmin.vf " x0 ", " x0 ", %[ope_hi]\n" \
    "vfmin.vf " x1 ", " x1 ", %[ope_hi]\n" \
    "vfmul.vf " x0 ", " x0 ", %[ope_scale]\n" \
    "vfmul.vf " x1 ", " x1 ", %[ope_scale]\n" \
    "vfmv.v.f " t0 ", %[ope_c5]\n" \
    "vfmv.v.f " t1 ", %[ope_c5]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[ope_c4]\n" \
    "vfadd.vf " t1 ", " t1 ", %[ope_c4]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[ope_c3]\n" \
    "vfadd.vf " t1 ", " t1 ", %[ope_c3]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[ope_c2]\n" \
    "vfadd.vf " t1 ", " t1 ", %[ope_c2]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[ope_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[ope_one]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[ope_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[ope_one]\n" \
    "vfmul.vv " x0 ", " t0 ", " t0 "\n" \
    "vfmul.vv " x1 ", " t1 ", " t1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n"

#define NR_RECIP_STEP_ASM(x, r, t) \
    "vfmul.vv " t ", " x ", " r "\n" \
    "vfrsub.vf " t ", " t ", %[ope_two]\n" \
    "vfmul.vv " r ", " r ", " t "\n"

#define RECIP_ASM(x, r, t) \
    "vrsub.vx " r ", " x ", %[ope_seed]\n" \
    NR_RECIP_STEP_ASM(x, r, t) \
    NR_RECIP_STEP_ASM(x, r, t) \
    NR_RECIP_STEP_ASM(x, r, t)

// Two Newton steps match the optimized O-POPE GELU path. The measured
// approximation error is below the application tolerance while removing one
// dependent multiply/subtract/multiply chain per vector.
#define RECIP2_ASM(x, r, t) \
    "vrsub.vx " r ", " x ", %[ope_seed]\n" \
    NR_RECIP_STEP_ASM(x, r, t) \
    NR_RECIP_STEP_ASM(x, r, t)


#define NR_RSQRT_STEP_ASM(r, t, half_x) \
    "vfmul.vv " t ", " r ", " r "\n" \
    "vfmul.vv " t ", " t ", " half_x "\n" \
    "vfrsub.vf " t ", " t ", %[ope_three_halves]\n" \
    "vfmul.vv " r ", " r ", " t "\n"

#define RSQRT_ASM(x, r, t, half_x) \
    "vsrl.vi " r ", " x ", 1\n" \
    "vrsub.vx " r ", " r ", %[ope_rsqrt_seed]\n" \
    "vfmul.vf " half_x ", " x ", %[ope_half]\n" \
    NR_RSQRT_STEP_ASM(r, t, half_x) \
    NR_RSQRT_STEP_ASM(r, t, half_x) \
    NR_RSQRT_STEP_ASM(r, t, half_x)

#define RELU_ASM(x) "vfmax.vf " x ", " x ", %[ope_zero]\n"
#define NORMALIZE_ASM(x, inv) "vfmul.vv " x ", " x ", " inv "\n"
#define LOAD_ADVANCE_ASM(v, p, bytes) \
    "vle32.v " v ", (" p ")\n" "addi " p ", " p ", " bytes "\n"
#define STORE_ADVANCE_ASM(v, p, bytes) \
    "vse32.v " v ", (" p ")\n" "addi " p ", " p ", " bytes "\n"

// Tanh-form GELU using clamped exp and the baseline two-step reciprocal.
// All five vector register arguments must be disjoint.
#define GELU_INPUTS EXP_INPUTS, RECIP_INPUTS, \
    [ope_gelu_c] "f"(0.044715f), [ope_gelu_scale] "f"(-1.5957691216f)
#define GELU_ASM(x, e, t, r, q) \
    "vfmul.vv " e ", " x ", " x "\n" \
    "vfmul.vf " e ", " e ", %[ope_gelu_c]\n" \
    "vfadd.vf " e ", " e ", %[ope_one]\n" \
    "vfmul.vv " e ", " e ", " x "\n" \
    "vfmul.vf " e ", " e ", %[ope_gelu_scale]\n" \
    EXP_ASM(e, t) \
    "vfadd.vf " e ", " e ", %[ope_one]\n" \
    RECIP2_ASM(e, r, q) \
    "vfmul.vv " x ", " x ", " r "\n"

// Apply tanh-form GELU to two independent LMUL groups. The caller selects
// e32,m4 before issuing this string and may restore another LMUL afterward.
#define GELU_PAIR_ASM(x0, x1, e0, e1, r, q) \
    "vfmul.vv " e0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " e1 ", " x1 ", " x1 "\n" \
    "vfmul.vf " e0 ", " e0 ", %[ope_gelu_c]\n" \
    "vfmul.vf " e1 ", " e1 ", %[ope_gelu_c]\n" \
    "vfadd.vf " e0 ", " e0 ", %[ope_one]\n" \
    "vfadd.vf " e1 ", " e1 ", %[ope_one]\n" \
    "vfmul.vv " e0 ", " e0 ", " x0 "\n" \
    "vfmul.vv " e1 ", " e1 ", " x1 "\n" \
    "vfmul.vf " e0 ", " e0 ", %[ope_gelu_scale]\n" \
    "vfmul.vf " e1 ", " e1 ", %[ope_gelu_scale]\n" \
    EXP_PAIR_ASM(e0, e1, r, q) \
    "vfadd.vf " e0 ", " e0 ", %[ope_one]\n" \
    "vfadd.vf " e1 ", " e1 ", %[ope_one]\n" \
    RECIP2_ASM(e0, r, q) \
    "vfmul.vv " x0 ", " x0 ", " r "\n" \
    RECIP2_ASM(e1, q, e0) \
    "vfmul.vv " x1 ", " x1 ", " q "\n"

