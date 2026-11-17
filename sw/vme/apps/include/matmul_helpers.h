// Copyright 2026 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
#pragma once

// Middle K groups for a 1x4 FP16->FP32 outer-product panel.
// Caller supplies a0p, b0p..b3p, mtype/vtype, VL and byte strides.
#define MIDDLE_K_1x4_MATMUL(a0p, a_middle_end) do { \
  asm volatile("vtfmm.tvv mt0, v0, v16" : : : "memory"); \
  asm volatile("vle32.v v2, (%0)\naddi %0, %0, %1" : "+r"(a0p) : "i"(operand_vector_bytes) : "v2", "memory"); \
  asm volatile("vtfmm.tvv mt0, v1, v17" : : : "memory"); \
  asm volatile("vle32.v v18, (%0)\naddi %0, %0, %1" : "+r"(b0p) : "i"(operand_vector_bytes) : "v18", "memory"); \
  asm volatile("vle32.v v3, (%0)\naddi %0, %0, %1" : "+r"(a0p) : "i"(operand_vector_bytes) : "v3", "memory"); \
  asm volatile("vtfmm.tvv mt0, v2, v18" : : : "memory"); \
  asm volatile("vle32.v v19, (%0)\naddi %0, %0, %1" : "+r"(b0p) : "i"(operand_vector_bytes) : "v19", "memory"); \
  asm volatile("vtfmm.tvv mt0, v3, v19" : : : "memory"); \
  asm volatile("vle32.v v24, (%0)\naddi %0, %0, %1" : "+r"(b1p) : "i"(operand_vector_bytes) : "v24", "memory"); \
  asm volatile("vle32.v v25, (%0)\naddi %0, %0, %1" : "+r"(b1p) : "i"(operand_vector_bytes) : "v25", "memory"); \
  asm volatile("vtfmm.tvv mt4, v0, v24" : : : "memory"); \
  asm volatile("vle32.v v26, (%0)\naddi %0, %0, %1" : "+r"(b1p) : "i"(operand_vector_bytes) : "v26", "memory"); \
  asm volatile("vtfmm.tvv mt4, v1, v25" : : : "memory"); \
  asm volatile("vle32.v v27, (%0)\naddi %0, %0, %1" : "+r"(b1p) : "i"(operand_vector_bytes) : "v27", "memory"); \
  asm volatile("vtfmm.tvv mt4, v2, v26" : : : "memory"); \
  asm volatile("vtfmm.tvv mt4, v3, v27" : : : "memory"); \
  asm volatile("vsetvli x0, %0, e32, m4, ta, ma" : : "r"(group_vl) : "memory"); \
  asm volatile("vle32.v v16, (%0)" : : "r"(b2p) : "v16", "v17", "v18", "v19", "memory"); b2p += operand_group_bytes; \
  asm volatile("vle32.v v24, (%0)" : : "r"(b3p) : "v24", "v25", "v26", "v27", "memory"); b3p += operand_group_bytes; \
  asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory"); \
  asm volatile("vtfmm.tvv mt8, v0, v16" : : : "memory"); \
  asm volatile("vtfmm.tvv mt8, v1, v17" : : : "memory"); \
  asm volatile("vtfmm.tvv mt8, v2, v18" : : : "memory"); \
  asm volatile("vle32.v v16, (%0)\naddi %0, %0, %1" : "+r"(b0p) : "i"(operand_vector_bytes) : "v16", "memory"); \
  asm volatile("vtfmm.tvv mt8, v3, v19" : : : "memory"); \
  asm volatile("vle32.v v17, (%0)\naddi %0, %0, %1" : "+r"(b0p) : "i"(operand_vector_bytes) : "v17", "memory"); \
  asm volatile("vtfmm.tvv mt12, v0, v24" : : : "memory"); \
  asm volatile("vtfmm.tvv mt12, v1, v25" : : : "memory"); \
  asm volatile("vle32.v v0, (%0)\naddi %0, %0, %1" : "+r"(a0p) : "i"(operand_vector_bytes) : "v0", "memory"); \
  asm volatile("vtfmm.tvv mt12, v2, v26" : : : "memory"); \
  asm volatile("vle32.v v1, (%0)\naddi %0, %0, %1" : "+r"(a0p) : "i"(operand_vector_bytes) : "v1", "memory"); \
  asm volatile("vtfmm.tvv mt12, v3, v27" : : : "memory"); \
} while (0)
