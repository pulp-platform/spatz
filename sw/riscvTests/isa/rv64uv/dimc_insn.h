// SPDX-License-Identifier: Apache-2.0
#ifndef DIMC_INSN_H
#define DIMC_INSN_H
// Encode sf.vqmmacc without requiring a custom compiler mnemonic.
#define DIMC_STRINGIFY_(x) #x
#define DIMC_STRINGIFY(x) DIMC_STRINGIFY_(x)
#define DIMC_ENCODING(vd, group, vs1, ci, imm) \
    ((0x2e << 26) | ((imm) << 25) | ((group) << 23) | \
     ((ci) << 20) | ((vs1) << 15) | ((vd) << 7) | 0x77)
#define DIMC_ASM(vd, group, vs1, ci, imm) \
    ".word " DIMC_STRINGIFY(DIMC_ENCODING(vd, group, vs1, ci, imm))
#endif
