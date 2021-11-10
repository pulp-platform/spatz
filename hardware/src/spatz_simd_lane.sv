// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

module spatz_simd import spatz_pkg::*; #(
  parameter int unsigned Width = 8
) (
  input  logic clk_i,
  input  logic rst_ni,

  // Operation Signals
  input  op_e               operation_i,
  input  logic [Width-1:0] op_s1_i,
  input  logic [Width-1:0] op_s2_i,
  input  logic [Width-1:0] op_d_i,
  input  logic             carry_i,
  input  rvv_pkg::vew_e    sew_i,
  // Result Output
  output logic [Width-1:0] result_o
);

  logic is_unsigned = (operation_i == VMINU) | (operation_i == VMAXU) | (operation_i == VMULHU) | (operation_i == VMULHSU);

  ////////////////
  // Multiplier //
  ////////////////

  logic [2*Width-1:0] mult_result;
  logic [Width-1:0]   mult_op1;
  logic [Width-1:0]   mult_op2;

  // Multiplier
  assign mult_result = $signed({mult_op1[Width-1] & ~is_unsigned & ~(operation_i == VMULHSU), mult_op1}) *
                       $signed({mult_op2[Width-1] & ~is_unsigned, mult_op2});

  // Select multiplier operands
  always_comb begin : proc_mult_operands
    unique case (operation_i)
      VMUL,
      VMULH,
      VMULHU,
      VMULHSU,
      VMACC,
      VNMSAC: begin
        mult_op1 = op_s1_i;
        mult_op2 = op_s2_i;
      end
      VMADD,
      VNMSUB: begin
        mult_op1 = op_s1_i;
        mult_op2 = op_d_i;
      end
    endcase
  end // proc_mult_operands

  ////////////
  // Result //
  ////////////

  logic [Width:0]   adder_result;
  logic [Width:0]   subtractor_result;
  logic [Width-1:0] simd_result;
  logic [Width-1:0] arith_op1; // Subtrahend
  logic [Width-1:0] arith_op2; // Minuend

  // Select arithmetic operands
  always_comb begin : proc_arith_op
    arith_op1 = op_s1_i;
    arith_op2 = op_s2_i;

    unique case (operation_i)
      VMACC,
      VNMSAC: begin
        arith_op1 = mult_result[Width-1:0];
        arith_op2 = op_d_i;
      end
      VMADD,
      VNMSUB: begin
        arith_op1 = mult_result[Width-1:0];
        arith_op2 = op_s2_i;
      end
      VRSUB: begin
        arith_op1 = op_s2_i;
        arith_op2 = op_s1_i;
      end
    endcase // operation_i
  end // proc_arith_op

  assign adder_result      = $signed(arith_op2) + $signed(arith_op1) + carry_i;
  assign subtractor_result = $signed(arith_op2) - $signed(arith_op1) - carry_i;

  // Calculate arithmetic and logics and select correct result
  always_comb begin : proc_simd
    simd_result = '0;
    unique case (operation_i)
      VADD, VMACC, VMADD, VADC: simd_result = adder_result[Width-1:0];
      VSUB, VRSUB, VNMSAC, VNMSUB, VSBC: simd_result = adder_result[Width-1:0];
      VMIN, VMINU: simd_result = $signed({op_s1_i[Width-1] & ~is_unsigned, op_s1_i[Width-1]}) <=
                                 $signed({op_s2_i[Width-1] & ~is_unsigned, op_s2_i[Width-1]}) ?
                                 op_s1_i : op_s2_i;
      VMAX, VMAXU: simd_result = $signed({op_s1_i[Width-1] & ~is_unsigned, op_s1_i[Width-1]}) >
                                 $signed({op_s2_i[Width-1] & ~is_unsigned, op_s2_i[Width-1]}) ?
                                 op_s1_i : op_s2_i;
      VAND: simd_result = op_s1_i & op_s2_i;
      VOR:   simd_result = op_s1_i | op_s2_i;
      VXOR: simd_result = op_s1_i ^ op_s2_i;
      VSLL: simd_result = $unsigned(op_s1_i) << op_s2_i[$clog2(Width)-1:0];
      VSRL: simd_result = $unsigned(op_s1_i) >> op_s2_i[$clog2(Width)-1:0];
      VSRA: simd_result = $signed(op1) >>> op_s2_i[$clog2(Width)-1:0];
      // TODO: Change selection when SEW does not equal Width
      VMUL: simd_result = mult_result[Width-1:0];
      VMULH: simd_result = mult_result[2*Width-1:Width];
      VMULHU: simd_result = mult_result[2*Width-1:Width];
      VMULHSU: simd_result = mult_result[2*Width-1:Width];
      VMADC: simd_result = Width'(adder_result[Width]);
      VMSBC: simd_result = Width'(subtractor_result[Width]);
     endcase // operation_i
  end // proc_simd

  assign result_o = simd_result;

endmodule : spatz_simd
