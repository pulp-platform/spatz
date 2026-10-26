# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13)

set(SNRT_NFPU_PER_CORE "0" CACHE STRING "Number of FPUs per Spatz")

include_directories(include)
include_directories(../spatzBenchmarks/include)
include_directories(${SNRUNTIME_INCLUDE_DIRS})

add_compile_options(-O3 -g -ffunction-sections)

# Macro to compile an O-POPE GEMM test: (name, file, M, N, K, fmt)
macro(add_opope_test name file M N K fmt)
  set(target_name ${name}_M${M}_N${N}_K${K}_${fmt})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} opope_benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC
    DATAHEADER="data/data_${M}_${N}_${K}_${fmt}.h"
    SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

# Macro to compile an O-POPE application: (name, file, tag); the data header is data/data_<tag>.h
# and is generated from script/<tag>.json by the application's script/gen_data.py
macro(add_opope_app name file tag)
  set(target_name ${name}_${tag})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} opope_benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC
    DATAHEADER="data/data_${tag}.h"
    SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

# Cycle counter / kernel markers (shared with spatzBenchmarks)
add_library(opope_benchmark ../spatzBenchmarks/benchmark/benchmark.c)

enable_testing()
set(SNITCH_TEST_PREFIX opopeBenchmarks-)

add_opope_test(sp-matmul sp-matmul/main.c 64 64 64 fp32)

# Applications (GeMM on O-POPE + Spatz pre/post-processing)
add_opope_app(sp-cmatmul sp-cmatmul/main.c m32_n32_k32)
# add_opope_app(sp-cmatmul sp-cmatmul/main.c m48_n40_k56)
# add_opope_app(sp-cmatmul sp-cmatmul/main.c m48_n56_k56)
add_opope_app(sp-conv sp-conv/main.c cin8_h10_w16_cout32)
add_opope_app(sp-conv sp-conv/main.c cin8_h14_w16_cout32)
add_opope_app(sp-gathermlp sp-gathermlp/main.c dlrm_r256_d32_b64)
add_opope_app(sp-gathermlp sp-gathermlp/main.c dlrm_r384_d32_b64)
add_opope_app(sp-gathermlp sp-gathermlp/main.c gcn_n64_f32_deg7)
add_opope_app(sp-vqgemm sp-vqgemm/main.c m32_n64_k64)
add_opope_app(sp-vqgemm sp-vqgemm/main.c m48_n128_k96)
add_opope_app(sp-dqmatmul sp-dqmatmul/main.c m32_n64_k64_cbd8)
# add_opope_app(sp-flashattn sp-flashattn/main.c tq64_t128_d32_bc32)
# add_opope_app(sp-flashattn sp-flashattn/main.c tq64_t128_d64_bc64)
# add_opope_app(sp-flashattn sp-flashattn/main.c tq64_t32_d32_bc32)
add_opope_app(sp-flashattn sp-flashattn/main.c tq64_t64_d32_bc32)

# Transformer pipelines (feature-major activations, O-POPE GeMMs + Spatz LN/GELU/softmax/...)
add_opope_app(sp-tf-p1-ln-gemm sp-tf-p1-ln-gemm/main.c c64_t64_o128)
add_opope_app(sp-tf-p2-gemm-gelu sp-tf-p2-gemm-gelu/main.c c64_t64_o128)
add_opope_app(sp-tf-p3-ln-gemm-gelu sp-tf-p3-ln-gemm-gelu/main.c c64_t64_o128)
add_opope_app(sp-tf-p4a-gemm-resid-fused sp-tf-p4a-gemm-resid-fused/main.c c64_t64)
add_opope_app(sp-tf-p4b-gemm-resid-sep sp-tf-p4b-gemm-resid-sep/main.c c64_t64)
add_opope_app(sp-tf-p5-gemm-relu sp-tf-p5-gemm-relu/main.c c64_t64_o128)
add_opope_app(sp-tf-p6-gemm-softmax sp-tf-p6-gemm-softmax/main.c c64_t32_v128)
add_opope_app(sp-tf-p7-gemm-softmax-ce sp-tf-p7-gemm-softmax-ce/main.c c64_t32_v128)
add_opope_app(sp-tf-p8-attention sp-tf-p8-attention/main.c d64_t64)
add_opope_app(sp-tf-p9-mlp-block sp-tf-p9-mlp-block/main.c c64_t64_h96)
add_opope_app(sp-tf-p10-embed-ln-gemm sp-tf-p10-embed-ln-gemm/main.c c64_t32_v128_o64)
add_opope_app(sp-tf-p11-block sp-tf-p11-block/main.c c32_t32_h128)
add_opope_app(widening-2x-opope-matmul widening-2x-opope-matmul/main.c m96_n128_k128)

# Widening (FP16 inputs, FP32 output) applications: O-POPE SDOTP GeMM
add_opope_app(widening-hp-conv widening-hp-conv/main.c cin16_h14_w16_cout48)
add_opope_app(widening-hp-gathermlp widening-hp-gathermlp/main.c dlrm_r144_d64_b128_o128)
add_opope_app(widening-hp-gathermlp widening-hp-gathermlp/main.c gcn_n128_f64_o128)
add_opope_app(widening-hp-vqgemm widening-hp-vqgemm/main.c m96_n128_k128)
add_opope_app(widening-hp-cmatmul widening-hp-cmatmul/main.c m64_n64_k64)
add_opope_app(widening-hp-dqmatmul widening-hp-dqmatmul/main.c m128_n64_k128_cbd8)
add_opope_app(widening-hp-flashattn widening-hp-flashattn/main.c tq64_t256_d64_bc64)

# Widening transformer pipelines (FP16 GeMM inputs, FP32 outputs)
add_opope_app(widening-hp-tf-p1-ln-gemm widening-hp-tf-p1-ln-gemm/main.c c96_t64_o176)
add_opope_app(widening-hp-tf-p2-gemm-gelu widening-hp-tf-p2-gemm-gelu/main.c c128_t64_o192)
add_opope_app(widening-hp-tf-p3-ln-gemm-gelu widening-hp-tf-p3-ln-gemm-gelu/main.c c96_t64_o176)
add_opope_app(widening-hp-tf-p5-gemm-relu widening-hp-tf-p5-gemm-relu/main.c c128_t64_o192)
add_opope_app(widening-hp-tf-p4a-gemm-resid-fused widening-hp-tf-p4a-gemm-resid-fused/main.c c128_t112)
add_opope_app(widening-hp-tf-p4b-gemm-resid-sep widening-hp-tf-p4b-gemm-resid-sep/main.c c128_t64)
add_opope_app(widening-hp-tf-p6-gemm-softmax widening-hp-tf-p6-gemm-softmax/main.c c128_t64_v192)
add_opope_app(widening-hp-tf-p7-gemm-softmax-ce widening-hp-tf-p7-gemm-softmax-ce/main.c c128_t64_v192)
add_opope_app(widening-hp-tf-p8-attention widening-hp-tf-p8-attention/main.c d64_t96)
add_opope_app(widening-hp-tf-p9-mlp-block widening-hp-tf-p9-mlp-block/main.c c64_t64_h128)
add_opope_app(widening-hp-tf-p10-embed-ln-gemm widening-hp-tf-p10-embed-ln-gemm/main.c c64_t64_v64_o64)
add_opope_app(widening-hp-tf-p11-block widening-hp-tf-p11-block/main.c c32_t32_h192)
