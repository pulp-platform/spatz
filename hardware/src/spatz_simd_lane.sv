// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// The SIMD lane calculates all simd operations given for a give distinct
// element width.

module spatz_simd_lane
  import spatz_pkg::*;
#(
  parameter int unsigned Width = 8
) (
  input  logic clk_i,
  input  logic rst_ni,
  // Operation Signals
  input  op_e              operation_i,
  input  logic [Width-1:0] op_s1_i,
  input  logic [Width-1:0] op_s2_i,
  input  logic [Width-1:0] op_d_i,
  input  logic             is_signed_i,
  input  logic             carry_i,
  input  rvv_pkg::vew_e    sew_i,
  // Result Output
  output logic [Width-1:0] result_o
);

  ////////////////
  // Multiplier //
  ////////////////

  logic [2*Width-1:0] mult_result;
  logic [Width-1:0]   mult_op1;
  logic [Width-1:0]   mult_op2;

  // Multiplier
  assign mult_result = $signed({mult_op1[Width-1] & is_signed_i & ~(operation_i == VMULHSU), mult_op1}) *
                       $signed({mult_op2[Width-1] & is_signed_i, mult_op2});

  // Select multiplier operands
  always_comb begin : mult_operands
    mult_op1 = op_s1_i;
    mult_op2 = op_s2_i;
    if ((operation_i == VMADD) || (operation_i == VNMSUB)) begin
      mult_op1 = op_s1_i;
      mult_op2 = op_d_i;
    end
  end // mult_operands

  ////////////////////////
  // Adder / Subtractor //
  ////////////////////////

  logic [Width:0]   adder_result;
  logic [Width:0]   subtractor_result;
  logic [Width-1:0] simd_result;
  logic [Width-1:0] arith_op1; // Subtrahend
  logic [Width-1:0] arith_op2; // Minuend

  // Select arithmetic operands
  always_comb begin : arith_operands
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
      default: begin
        arith_op1 = op_s1_i;
        arith_op2 = op_s2_i;
      end
    endcase // operation_i
  end // arith_operands

  assign adder_result      = $signed(arith_op2) + $signed(arith_op1) + carry_i;
  assign subtractor_result = $signed(arith_op2) - $signed(arith_op1) - carry_i;

  /////////////
  // Shifter //
  /////////////

  logic [$clog2(Width)-1:0] shift_amount;
  logic [Width-1:0] shift_operand;
  if (Width >= 32) begin
    always_comb begin : shift_operands
      unique case (sew_i)
        rvv_pkg::EW_16: begin
          shift_amount = op_s1_i[3:0];
          if (operation_i == VSRA) shift_operand = $signed(op_s2_i[15:0]);
          else shift_operand = $unsigned(op_s2_i[15:0]);
        end
        rvv_pkg::EW_32: begin
          shift_amount = op_s1_i[4:0];
          shift_operand = op_s2_i[31:0];
        end
        default: begin
          shift_amount = op_s1_i[2:0];
          if (operation_i == VSRA) shift_operand = $signed(op_s2_i[7:0]);
          else shift_operand = $unsigned(op_s2_i[7:0]);
        end
      endcase
    end // shift_operands
  end else if (Width >= 16) begin
    always_comb begin : shift_operands
      unique case (sew_i)
        rvv_pkg::EW_16: begin
          shift_amount = op_s1_i[3:0];
          shift_operand = op_s2_i[15:0];
        end
        default: begin
          shift_amount = op_s1_i[2:0];
          if (operation_i == VSRA) shift_operand = $signed(op_s2_i[7:0]);
          else shift_operand = $unsigned(op_s2_i[7:0]);
        end
      endcase
    end // shift_operands
  end else begin
    always_comb begin
      shift_amount = op_s1_i[2:0];
      shift_operand = op_s2_i[7:0];
    end
  end

  ////////////
  // Result //
  ////////////

  // Calculate arithmetic and logics and select correct result
  always_comb begin : simd
    simd_result = '0;
    unique case (operation_i)
      VADD, VMACC, VMADD, VADC: simd_result = adder_result[Width-1:0];
      VSUB, VRSUB, VNMSAC, VNMSUB, VSBC: simd_result = subtractor_result[Width-1:0];
      VMIN, VMINU: simd_result = $signed({op_s1_i[Width-1] & is_signed_i, op_s1_i}) <=
                                 $signed({op_s2_i[Width-1] & is_signed_i, op_s2_i}) ?
                                 op_s1_i : op_s2_i;
      VMAX, VMAXU: simd_result = $signed({op_s1_i[Width-1] & is_signed_i, op_s1_i}) >
                                 $signed({op_s2_i[Width-1] & is_signed_i, op_s2_i}) ?
                                 op_s1_i : op_s2_i;
      VAND: simd_result = op_s1_i & op_s2_i;
      VOR:  simd_result = op_s1_i | op_s2_i;
      VXOR: simd_result = op_s1_i ^ op_s2_i;
      VSLL: simd_result = shift_operand << shift_amount;
      VSRL: simd_result = shift_operand >> shift_amount;
      VSRA: simd_result = $signed(shift_operand) >>> shift_amount;
      // TODO: Change selection when SEW does not equal Width
      VMUL: simd_result = mult_result[Width-1:0];
      VMULH: simd_result = mult_result[2*Width-1:Width];
      VMULHU: simd_result = mult_result[2*Width-1:Width];
      VMULHSU: simd_result = mult_result[2*Width-1:Width];
      VMADC: simd_result = Width'(adder_result[Width]);
      VMSBC: simd_result = Width'(subtractor_result[Width]);
      default simd_result = '0;
    endcase // operation_i
  end // simd

  assign result_o = simd_result;

endmodule : spatz_simd_lane
