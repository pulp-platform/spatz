// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// The IPU distributes the operands to the four SIMD lanes and afterwards
// collects the results.

module spatz_ipu
  import spatz_pkg::*;
(
  input  logic clk_i,
  input  logic rst_ni,
  // Operation Signals
  input  op_e              operation_i,
  input  logic [ELEN-1:0]  op_s1_i,
  input  logic [ELEN-1:0]  op_s2_i,
  input  logic [ELEN-1:0]  op_d_i,
  input  logic [ELENB-1:0] carry_i,
  input  rvv_pkg::vew_e    sew_i,
  // Result Output
  output logic [ELENB-1:0] be_o,
  output logic [ELEN-1:0]  result_o
);

  /////////////
  // Signals //
  /////////////

  // Is the operation signed
  logic is_signed;
  assign is_signed = operation_i inside {VMIN, VMAX, VMULH, VMULHSU};

  // SIMD input signals
  typedef struct packed {
    // Operands in
    logic [1:0][2:0][7:0] ew8_op;
    logic [2:0][15:0]     ew16_op;
    logic [2:0][31:0]     ew32_op;
    // Carry in
    logic [1:0] ew8_carry;
    logic       ew16_carry;
    logic       ew32_carry;
  } simd_lanes_inp_t;

  // SIMD output signals
  typedef struct packed {
    // Results
    logic [1:0][7:0] ew8_res;
    logic [15:0]     ew16_res;
    logic [31:0]     ew32_res;
  } simd_lanes_res_t;

  simd_lanes_inp_t lane_signal_inp;
  simd_lanes_res_t lane_signal_res;

  /////////////////
  // Distributor //
  /////////////////

  // Distribute operands to the SIMD lanes
  always_comb begin : distributor
    lane_signal_inp = '0;

    if (sew_i == rvv_pkg::EW_8) begin
      if (is_signed) begin
        lane_signal_inp.ew32_op[1] = 32'($signed(op_s2_i[31:24]));
        lane_signal_inp.ew32_op[2] = 32'($signed(op_d_i[31:24]));
        lane_signal_inp.ew16_op[1] = 16'($signed(op_s2_i[23:16]));
        lane_signal_inp.ew16_op[2] = 16'($signed(op_d_i[23:16]));
      end else begin
        lane_signal_inp.ew32_op[0] = 32'(op_s1_i[31:24]);
        lane_signal_inp.ew32_op[1] = 32'(op_s2_i[31:24]);
        lane_signal_inp.ew32_op[2] = 32'(op_d_i[31:24]);
        lane_signal_inp.ew16_op[0] = 16'(op_s1_i[23:16]);
        lane_signal_inp.ew16_op[1] = 16'(op_s2_i[23:16]);
        lane_signal_inp.ew16_op[2] = 16'(op_d_i[23:16]);
      end
      lane_signal_inp.ew32_op[0] = is_signed & operation_i != VMULHSU ? 32'($signed(op_s1_i[31:24])) : 32'(op_s1_i[31:24]);
      lane_signal_inp.ew16_op[0] = is_signed & operation_i != VMULHSU ? 16'($signed(op_s1_i[23:16])) : 16'(op_s1_i[23:16]);
      lane_signal_inp.ew8_op[1][0] = op_s1_i[15:8];
      lane_signal_inp.ew8_op[1][1] = op_s2_i[15:8];
      lane_signal_inp.ew8_op[1][2] = op_d_i[15:8];
      lane_signal_inp.ew8_op[0][0] = op_s1_i[7:0];
      lane_signal_inp.ew8_op[0][1] = op_s2_i[7:0];
      lane_signal_inp.ew8_op[0][2] = op_d_i[7:0];

      lane_signal_inp.ew8_carry[0] = carry_i[0];
      lane_signal_inp.ew8_carry[1] = carry_i[1];
      lane_signal_inp.ew16_carry   = carry_i[2];
      lane_signal_inp.ew32_carry   = carry_i[3];
    end else if (sew_i == rvv_pkg::EW_16) begin
      if (is_signed) begin
        lane_signal_inp.ew32_op[1] = 32'($signed(op_s2_i[31:16]));
        lane_signal_inp.ew32_op[2] = 32'($signed(op_d_i[31:16]));
      end else begin
        lane_signal_inp.ew32_op[0] = 32'((op_s1_i[31:16]));
        lane_signal_inp.ew32_op[1] = 32'((op_s2_i[31:16]));
        lane_signal_inp.ew32_op[2] = 32'((op_d_i[31:16]));
      end
      lane_signal_inp.ew32_op[0] = is_signed & operation_i != VMULHSU ? 32'($signed(op_s1_i[31:16])) : 32'((op_s1_i[31:16]));
      lane_signal_inp.ew16_op[0] = op_s1_i[15:0];
      lane_signal_inp.ew16_op[1] = op_s2_i[15:0];
      lane_signal_inp.ew16_op[2] = op_d_i[15:0];

      lane_signal_inp.ew16_carry = carry_i[0];
      lane_signal_inp.ew32_carry = carry_i[1];
    end else begin
      lane_signal_inp.ew32_op[0] = op_s1_i;
      lane_signal_inp.ew32_op[1] = op_s2_i;
      lane_signal_inp.ew32_op[2] = op_d_i;
      lane_signal_inp.ew32_carry = carry_i[0];
    end
  end

  ///////////////
  // Collector //
  ///////////////

  // Collect results from the SIMD lanes
  always_comb begin : collector
    if (sew_i == rvv_pkg::EW_8) begin
      result_o = {lane_signal_res.ew32_res[7:0], lane_signal_res.ew16_res[7:0], lane_signal_res.ew8_res[1], lane_signal_res.ew8_res[0]};
    end else if (sew_i == rvv_pkg::EW_16) begin
      result_o = {lane_signal_res.ew32_res[15:0], lane_signal_res.ew16_res};
    end else begin
      result_o = lane_signal_res.ew32_res;
    end
  end

  // TODO: CHANGE
  assign be_o = '1;

  ////////////////
  // SIMD Lanes //
  ////////////////

  spatz_simd_lane #(
    .Width (8)
  ) i_simd_lane_8b_0 (
    .clk_i      (clk_i),
    .rst_ni     (rst_ni),
    .operation_i(operation_i),
    .op_s1_i    (lane_signal_inp.ew8_op[0][0]),
    .op_s2_i    (lane_signal_inp.ew8_op[0][1]),
    .op_d_i     (lane_signal_inp.ew8_op[0][2]),
    .is_signed_i(is_signed),
    .carry_i    (lane_signal_inp.ew8_carry[0]),
    .sew_i      (sew_i),
    .result_o   (lane_signal_res.ew8_res[0])
  );

  spatz_simd_lane #(
    .Width (8)
  ) i_simd_lane_8b_1 (
    .clk_i      (clk_i),
    .rst_ni     (rst_ni),
    .operation_i(operation_i),
    .op_s1_i    (lane_signal_inp.ew8_op[1][0]),
    .op_s2_i    (lane_signal_inp.ew8_op[1][1]),
    .op_d_i     (lane_signal_inp.ew8_op[1][2]),
    .is_signed_i(is_signed),
    .carry_i    (lane_signal_inp.ew8_carry[1]),
    .sew_i      (sew_i),
    .result_o   (lane_signal_res.ew8_res[1])
  );

  spatz_simd_lane #(
    .Width (16)
  ) i_simd_lane_16b_0 (
    .clk_i      (clk_i),
    .rst_ni     (rst_ni),
    .operation_i(operation_i),
    .op_s1_i    (lane_signal_inp.ew16_op[0]),
    .op_s2_i    (lane_signal_inp.ew16_op[1]),
    .op_d_i     (lane_signal_inp.ew16_op[2]),
    .is_signed_i(is_signed),
    .carry_i    (lane_signal_inp.ew16_carry),
    .sew_i      (sew_i),
    .result_o   (lane_signal_res.ew16_res)
  );

  spatz_simd_lane #(
    .Width (32)
  ) i_simd_lane_32b_0 (
    .clk_i      (clk_i),
    .rst_ni     (rst_ni),
    .operation_i(operation_i),
    .op_s1_i    (lane_signal_inp.ew32_op[0]),
    .op_s2_i    (lane_signal_inp.ew32_op[1]),
    .op_d_i     (lane_signal_inp.ew32_op[2]),
    .is_signed_i(is_signed),
    .carry_i    (lane_signal_inp.ew32_carry),
    .sew_i      (sew_i),
    .result_o   (lane_signal_res.ew32_res)
  );

endmodule : spatz_ipu
