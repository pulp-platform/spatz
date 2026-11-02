// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
#pragma once

// Adapted from baseline_spatz/sw/opopeBenchmarks/include/opope_transformer.h.
// Shared OPE application vector math. No divider, sqrt, vfrec7 or vfrsqrt7.
// Instruction-string macros compose inside ONE asm volatile block, allowing
// the caller to interleave loads and compute without function calls.
// Register arguments are string literals for disjoint aligned LMUL groups.
// The caller must set e32/VL/LMUL and list every physical register clobbered.
// Operand bundles below provide the required named scalar input operands.
#define OPE_EXP_INPUTS \
    [ope_lo] "f"(-16.0f), [ope_hi] "f"(20.0f), \
    [ope_scale] "f"(0.015625f), [ope_c5] "f"(0.0083333333f), \
    [ope_c4] "f"(0.0416666667f), [ope_c3] "f"(0.1666666667f), \
    [ope_c2] "f"(0.5f), [ope_one] "f"(1.0f)
#define OPE_RECIP_INPUTS \
    [ope_seed] "r"(0x7ef311c7u), [ope_two] "f"(2.0f)
#define OPE_RSQRT_INPUTS \
    [ope_rsqrt_seed] "r"(0x5f3759dfu), \
    [ope_half] "f"(0.5f), [ope_three_halves] "f"(1.5f)
#define OPE_RELU_INPUTS [ope_zero] "f"(0.0f)

#define OPE_EXP_ASM(x, t) \
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

#define OPE_EXP_PAIR_ASM(x0, x1, t0, t1) \
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

#define OPE_NR_RECIP_STEP_ASM(x, r, t) \
    "vfmul.vv " t ", " x ", " r "\n" \
    "vfrsub.vf " t ", " t ", %[ope_two]\n" \
    "vfmul.vv " r ", " r ", " t "\n"

#define OPE_RECIP_ASM(x, r, t) \
    "vrsub.vx " r ", " x ", %[ope_seed]\n" \
    OPE_NR_RECIP_STEP_ASM(x, r, t) \
    OPE_NR_RECIP_STEP_ASM(x, r, t) \
    OPE_NR_RECIP_STEP_ASM(x, r, t)

#define OPE_NR_RSQRT_STEP_ASM(r, t, half_x) \
    "vfmul.vv " t ", " r ", " r "\n" \
    "vfmul.vv " t ", " t ", " half_x "\n" \
    "vfrsub.vf " t ", " t ", %[ope_three_halves]\n" \
    "vfmul.vv " r ", " r ", " t "\n"

#define OPE_RSQRT_ASM(x, r, t, half_x) \
    "vsrl.vi " r ", " x ", 1\n" \
    "vrsub.vx " r ", " r ", %[ope_rsqrt_seed]\n" \
    "vfmul.vf " half_x ", " x ", %[ope_half]\n" \
    OPE_NR_RSQRT_STEP_ASM(r, t, half_x) \
    OPE_NR_RSQRT_STEP_ASM(r, t, half_x) \
    OPE_NR_RSQRT_STEP_ASM(r, t, half_x)

#define OPE_RELU_ASM(x) "vfmax.vf " x ", " x ", %[ope_zero]\n"
#define OPE_NORMALIZE_ASM(x, inv) "vfmul.vv " x ", " x ", " inv "\n"
#define OPE_LOAD_ADVANCE_ASM(v, p, bytes) \
    "vle32.v " v ", (" p ")\n" "addi " p ", " p ", " bytes "\n"
#define OPE_STORE_ADVANCE_ASM(v, p, bytes) \
    "vse32.v " v ", (" p ")\n" "addi " p ", " p ", " bytes "\n"

// O-POPE tanh-form GELU, using the same clamped exp and 3-step reciprocal.
// All five vector register arguments must be disjoint.
#define OPE_GELU_INPUTS OPE_EXP_INPUTS, OPE_RECIP_INPUTS, \
    [ope_gelu_c] "f"(0.044715f), [ope_gelu_scale] "f"(-1.5957691216f)
#define OPE_GELU_ASM(x, e, t, r, q) \
    "vfmul.vv " e ", " x ", " x "\n" \
    "vfmul.vf " e ", " e ", %[ope_gelu_c]\n" \
    "vfadd.vf " e ", " e ", %[ope_one]\n" \
    "vfmul.vv " e ", " e ", " x "\n" \
    "vfmul.vf " e ", " e ", %[ope_gelu_scale]\n" \
    OPE_EXP_ASM(e, t) \
    "vfadd.vf " e ", " e ", %[ope_one]\n" \
    OPE_RECIP_ASM(e, r, q) \
    "vfmul.vv " x ", " x ", " r "\n"
