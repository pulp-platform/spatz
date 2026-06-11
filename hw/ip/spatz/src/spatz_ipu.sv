// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich
//
// The IPU distributes the operands to the four SIMD lanes and afterwards
// collects the results.

module spatz_ipu import spatz_pkg::*; import rvv_pkg::vew_e; #(
    parameter type tag_t    = logic,
    parameter bit  Pipeline = 1
  ) (
    input  logic   clk_i,
    input  logic   rst_ni,
    // Operation Signals
    input  op_e    operation_i,
    input  logic   operation_valid_i,
    output logic   operation_ready_o,
    input  elen_t  op_s1_i,
    input  elen_t  op_s2_i,
    input  elen_t  op_d_i,
    input  tag_t   tag_i,
    input  elenb_t carry_i,
    input  vew_e   sew_i,
    // Result Output
    output elenb_t be_o,
    output elen_t  result_o,
    output elenb_t result_valid_o,
    input  logic   result_ready_i,
    output tag_t   tag_o,
    output logic   busy_o
  );

// Include FF
`include "common_cells/registers.svh"

  /////////////
  // Signals //
  /////////////

  op_e    operation;
  logic   operation_valid;
  elen_t  op_s1, op_s2, op_d;
  elenb_t carry;
  vew_e   sew;

  // Is the operation signed?
  logic is_signed;

  // Is the operation signed and is this not a VMULHSU?
  logic is_signed_and_not_vmulhsu;

  logic [1:0] op_count_q, op_count_d;
  `FF(op_count_q, op_count_d, '0)
  always_comb begin: proc_busy
    // Maintain state
    op_count_d = op_count_q;

    if (operation_valid_i && operation_ready_o)
      op_count_d += 1;
    if (result_valid_o[0] && result_ready_i)
      op_count_d -= 1;

    busy_o = (op_count_q != '0);
  end: proc_busy

  // Forward the tag
  if (Pipeline) begin: gen_pipeline
    assign operation_ready_o = !operation_valid || operation_valid && result_ready_i;

    `FFL(operation, operation_i, operation_valid_i && operation_ready_o, VMIN)
    `FFL(operation_valid, operation_valid_i, operation_ready_o, 1'b0)
    `FFL(op_s1, op_s1_i, operation_valid_i && operation_ready_o, '0)
    `FFL(op_s2, op_s2_i, operation_valid_i && operation_ready_o, '0)
    `FFL(op_d, op_d_i, operation_valid_i && operation_ready_o, '0)
    `FFL(carry, carry_i, operation_valid_i && operation_ready_o, '0)
    `FFL(sew, sew_i, operation_valid_i && operation_ready_o, rvv_pkg::EW_32)
    `FFL(tag_o, tag_i, operation_valid_i && operation_ready_o, '0)

    // Is the operation signed?
    logic is_signed_d;
    assign is_signed_d = operation_i inside {VMIN, VMAX, VMULH, VMULHSU, VDIV, VREM};
    `FFL(is_signed, is_signed_d, operation_valid_i && operation_ready_o, 1'b0)

    // Is the operation signed and is this a VMULHSU?
    logic is_signed_and_not_vmulhsu_d;
    assign is_signed_and_not_vmulhsu_d = is_signed_d && (operation_i != VMULHSU);
    `FFL(is_signed_and_not_vmulhsu, is_signed_and_not_vmulhsu_d, operation_valid_i && operation_ready_o, 1'b0)
  end: gen_pipeline else begin: gen_pipeline
    assign operation         = operation_i;
    assign operation_valid   = operation_valid_i;
    assign operation_ready_o = '1;

    assign tag_o = tag_i;
    assign op_s1 = op_s1_i;
    assign op_s2 = op_s2_i;
    assign op_d  = op_d_i;
    assign carry = carry_i;
    assign sew   = sew_i;

    // Is the operation signed?
    assign is_signed = operation inside {VMIN, VMAX, VMULH, VMULHSU, VDIV, VREM};

    // Is the operation signed and is this a VMULHSU?
    assign is_signed_and_not_vmulhsu = is_signed && (operation != VMULHSU);
  end: gen_pipeline

  if (MAXEW == rvv_pkg::EW_32) begin: gen_32b_ipu

    typedef struct packed {
      logic [1:0][7:0] ew8;
      logic [15:0]     ew16;
      logic [31:0]     ew32;
    } input_ops_t;

    typedef struct packed {
      // Input operands
      input_ops_t [2:0] ops;
      // Input carry
      logic [1:0] ew8_carry;
      logic ew16_carry;
      logic ew32_carry;
      // Valid lanes
      logic [1:0] ew8_valid;
      logic ew16_valid;
      logic ew32_valid;
    } lane_signal_inp_t;

    lane_signal_inp_t lane_signal_inp;

    // SIMD output signals
    typedef struct packed {
      // Results
      logic [1:0][7:0] ew8_res;
      logic [15:0] ew16_res;
      logic [31:0] ew32_res;
    } lane_signal_res_t;
    lane_signal_res_t lane_signal_res;

    typedef struct packed {
      logic [1:0] ew8_valid;
      logic ew16_valid;
      logic ew32_valid;
    } lane_signal_res_valid_t;
    lane_signal_res_valid_t lane_signal_res_valid;

    /////////////////
    // Distributor //
    /////////////////

    // Distribute operands to the SIMD lanes
    always_comb begin : distributor
      lane_signal_inp = '0;

      unique case (sew)
        rvv_pkg::EW_8: begin
          lane_signal_inp.ops[0].ew32   = is_signed_and_not_vmulhsu ? 32'($signed(op_s1[31:24])) : 32'(op_s1[31:24]);
          lane_signal_inp.ops[0].ew16   = is_signed_and_not_vmulhsu ? 16'($signed(op_s1[23:16])) : 16'(op_s1[23:16]);
          lane_signal_inp.ops[0].ew8[1] = op_s1[15:8];
          lane_signal_inp.ops[0].ew8[0] = op_s1[7:0];

          lane_signal_inp.ops[1].ew32   = is_signed ? 32'($signed(op_s2[31:24])) : 32'(op_s2[31:24]);
          lane_signal_inp.ops[1].ew16   = is_signed ? 16'($signed(op_s2[23:16])) : 16'(op_s2[23:16]);
          lane_signal_inp.ops[1].ew8[1] = op_s2[15:8];
          lane_signal_inp.ops[1].ew8[0] = op_s2[7:0];

          lane_signal_inp.ops[2].ew32   = is_signed ? 32'($signed(op_d[31:24])) : 32'(op_d[31:24]);
          lane_signal_inp.ops[2].ew16   = is_signed ? 16'($signed(op_d[23:16])) : 16'(op_d[23:16]);
          lane_signal_inp.ops[2].ew8[1] = op_d[15:8];
          lane_signal_inp.ops[2].ew8[0] = op_d[7:0];

          lane_signal_inp.ew8_carry[0] = carry[0];
          lane_signal_inp.ew8_carry[1] = carry[1];
          lane_signal_inp.ew16_carry   = carry[2];
          lane_signal_inp.ew32_carry   = carry[3];

          // Activate all lanes
          lane_signal_inp.ew32_valid = operation_valid;
          lane_signal_inp.ew16_valid = operation_valid;
          lane_signal_inp.ew8_valid  = {2{operation_valid}};
        end

        rvv_pkg::EW_16: begin
          lane_signal_inp.ops[0].ew32 = is_signed_and_not_vmulhsu ? 32'($signed(op_s1[31:16])) : 32'(op_s1[31:16]);
          lane_signal_inp.ops[0].ew16 = op_s1[15:0];

          lane_signal_inp.ops[1].ew32 = is_signed ? 32'($signed(op_s2[31:16])) : 32'(op_s2[31:16]);
          lane_signal_inp.ops[1].ew16 = op_s2[15:0];

          lane_signal_inp.ops[2].ew32 = is_signed ? 32'($signed(op_d[31:16])) : 32'(op_d[31:16]);
          lane_signal_inp.ops[2].ew16 = op_d[15:0];

          lane_signal_inp.ew16_carry = carry[0];
          lane_signal_inp.ew32_carry = carry[1];

          // Activate the 32 and 16b lanes
          lane_signal_inp.ew32_valid = operation_valid;
          lane_signal_inp.ew16_valid = operation_valid;
          lane_signal_inp.ew8_valid  = 2'b00;
        end

        default: begin
          lane_signal_inp.ops[0].ew32 = op_s1;
          lane_signal_inp.ops[1].ew32 = op_s2;
          lane_signal_inp.ops[2].ew32 = op_d;
          lane_signal_inp.ew32_carry  = carry[0];

          // Activate the 32b lane
          lane_signal_inp.ew32_valid = operation_valid;
          lane_signal_inp.ew16_valid = 1'b0;
          lane_signal_inp.ew8_valid  = 2'b00;
        end
      endcase
    end

    ///////////////
    // Collector //
    ///////////////

    // Collect results from the SIMD lanes
    always_comb begin : collector
      unique case (sew)
        rvv_pkg::EW_8 : begin
          result_o       = {lane_signal_res.ew32_res[7:0], lane_signal_res.ew16_res[7:0], lane_signal_res.ew8_res[1], lane_signal_res.ew8_res[0]};
          result_valid_o = {lane_signal_res_valid.ew32_valid, lane_signal_res_valid.ew16_valid, lane_signal_res_valid.ew8_valid};
        end
        rvv_pkg::EW_16: begin
          result_o       = {lane_signal_res.ew32_res[15:0], lane_signal_res.ew16_res};
          result_valid_o = {{2{lane_signal_res_valid.ew32_valid}}, {2{lane_signal_res_valid.ew16_valid}}};
        end
        default: begin
          result_o       = lane_signal_res.ew32_res;
          result_valid_o = {4{lane_signal_res_valid.ew32_valid}};
        end
      endcase
    end

    // TODO: CHANGE
    assign be_o = '1;

    ////////////////
    // SIMD Lanes //
    ////////////////

    spatz_simd_lane #(
      .Width(8)
    ) i_simd_lane_8b_0 (
      .clk_i            (clk_i                             ),
      .rst_ni           (rst_ni                            ),
      .operation_i      (operation                         ),
      .operation_valid_i(lane_signal_inp.ew8_valid[0]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew8[0]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew8[0]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew8[0]     ),
      .is_signed_i      (is_signed                         ),
      .carry_i          (lane_signal_inp.ew8_carry[0]      ),
      .sew_i            (sew                               ),
      .result_o         (lane_signal_res.ew8_res[0]        ),
      .result_valid_o   (lane_signal_res_valid.ew8_valid[0]),
      .result_ready_i   (result_ready_i                    )
    );

    spatz_simd_lane #(
      .Width(8)
    ) i_simd_lane_8b_1 (
      .clk_i            (clk_i                             ),
      .rst_ni           (rst_ni                            ),
      .operation_i      (operation                         ),
      .operation_valid_i(lane_signal_inp.ew8_valid[1]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew8[1]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew8[1]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew8[1]     ),
      .is_signed_i      (is_signed                         ),
      .carry_i          (lane_signal_inp.ew8_carry[1]      ),
      .sew_i            (sew                               ),
      .result_o         (lane_signal_res.ew8_res[1]        ),
      .result_valid_o   (lane_signal_res_valid.ew8_valid[1]),
      .result_ready_i   (result_ready_i                    )
    );

    spatz_simd_lane #(
      .Width(16)
    ) i_simd_lane_16b_0 (
      .clk_i            (clk_i                           ),
      .rst_ni           (rst_ni                          ),
      .operation_i      (operation                       ),
      .operation_valid_i(lane_signal_inp.ew16_valid      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew16     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew16     ),
      .op_d_i           (lane_signal_inp.ops[2].ew16     ),
      .is_signed_i      (is_signed                       ),
      .carry_i          (lane_signal_inp.ew16_carry      ),
      .sew_i            (sew                             ),
      .result_o         (lane_signal_res.ew16_res        ),
      .result_valid_o   (lane_signal_res_valid.ew16_valid),
      .result_ready_i   (result_ready_i                  )
    );

    spatz_simd_lane #(
      .Width(32)
    ) i_simd_lane_32b_0 (
      .clk_i            (clk_i                           ),
      .rst_ni           (rst_ni                          ),
      .operation_i      (operation                       ),
      .operation_valid_i(lane_signal_inp.ew32_valid      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew32     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew32     ),
      .op_d_i           (lane_signal_inp.ops[2].ew32     ),
      .is_signed_i      (is_signed                       ),
      .carry_i          (lane_signal_inp.ew32_carry      ),
      .sew_i            (sew                             ),
      .result_o         (lane_signal_res.ew32_res        ),
      .result_valid_o   (lane_signal_res_valid.ew32_valid),
      .result_ready_i   (result_ready_i                  )
    );

  end: gen_32b_ipu else if (MAXEW == rvv_pkg::EW_64) begin: gen_64b_ipu
    typedef struct packed {
      logic [3:0][7:0]  ew8;
      logic [1:0][15:0] ew16;
      logic [31:0]      ew32;
      logic [63:0]      ew64;
    } input_ops_t;

    typedef struct packed {
      // Input operands
      input_ops_t [2:0] ops;
      // Input carry
      logic [3:0] ew8_carry;
      logic [1:0] ew16_carry;
      logic ew32_carry;
      logic ew64_carry;
      // Valid lanes
      logic [3:0] ew8_valid;
      logic [1:0] ew16_valid;
      logic ew32_valid;
      logic ew64_valid;
    } lane_signal_inp_t;
    lane_signal_inp_t lane_signal_inp;

    // SIMD output signals
    typedef struct packed {
      // Results
      logic [3:0][7:0] ew8_res;
      logic [1:0][15:0] ew16_res;
      logic [31:0] ew32_res;
      logic [63:0] ew64_res;
    } lane_signal_res_t;
    lane_signal_res_t lane_signal_res;

    typedef struct packed {
      logic [3:0] ew8_valid;
      logic [1:0] ew16_valid;
      logic ew32_valid;
      logic ew64_valid;
    } lane_signal_res_valid_t;
    lane_signal_res_valid_t lane_signal_res_valid;

    /////////////////
    // Distributor //
    /////////////////

    // Distribute operands to the SIMD lanes
    always_comb begin : distributor
      lane_signal_inp = '0;

      unique case (sew)
        rvv_pkg::EW_8: begin
          lane_signal_inp.ops[0].ew64    = is_signed_and_not_vmulhsu ? 64'($signed(op_s1[63:56])) : 64'(op_s1[63:56]);
          lane_signal_inp.ops[0].ew32    = is_signed_and_not_vmulhsu ? 32'($signed(op_s1[55:48])) : 32'(op_s1[55:48]);
          lane_signal_inp.ops[0].ew16[1] = is_signed_and_not_vmulhsu ? 16'($signed(op_s1[47:40])) : 16'(op_s1[47:40]);
          lane_signal_inp.ops[0].ew16[0] = is_signed_and_not_vmulhsu ? 16'($signed(op_s1[39:32])) : 16'(op_s1[39:32]);
          lane_signal_inp.ops[0].ew8[3]  = op_s1[31:24];
          lane_signal_inp.ops[0].ew8[2]  = op_s1[23:16];
          lane_signal_inp.ops[0].ew8[1]  = op_s1[15:8];
          lane_signal_inp.ops[0].ew8[0]  = op_s1[7:0];

          lane_signal_inp.ops[1].ew64    = is_signed ? 64'($signed(op_s2[63:56])) : 64'(op_s2[63:56]);
          lane_signal_inp.ops[1].ew32    = is_signed ? 32'($signed(op_s2[55:48])) : 32'(op_s2[55:48]);
          lane_signal_inp.ops[1].ew16[1] = is_signed ? 16'($signed(op_s2[47:40])) : 16'(op_s2[47:40]);
          lane_signal_inp.ops[1].ew16[0] = is_signed ? 16'($signed(op_s2[39:32])) : 16'(op_s2[39:32]);
          lane_signal_inp.ops[1].ew8[3]  = op_s2[31:24];
          lane_signal_inp.ops[1].ew8[2]  = op_s2[23:16];
          lane_signal_inp.ops[1].ew8[1]  = op_s2[15:8];
          lane_signal_inp.ops[1].ew8[0]  = op_s2[7:0];

          lane_signal_inp.ops[2].ew64    = is_signed ? 64'($signed(op_d[63:56])) : 64'(op_d[63:56]);
          lane_signal_inp.ops[2].ew32    = is_signed ? 32'($signed(op_d[55:48])) : 32'(op_d[55:48]);
          lane_signal_inp.ops[2].ew16[1] = is_signed ? 16'($signed(op_d[47:40])) : 16'(op_d[47:40]);
          lane_signal_inp.ops[2].ew16[0] = is_signed ? 16'($signed(op_d[39:32])) : 16'(op_d[39:32]);
          lane_signal_inp.ops[2].ew8[3]  = op_d[31:24];
          lane_signal_inp.ops[2].ew8[2]  = op_d[23:16];
          lane_signal_inp.ops[2].ew8[1]  = op_d[15:8];
          lane_signal_inp.ops[2].ew8[0]  = op_d[7:0];

          lane_signal_inp.ew8_carry[0]  = carry[0];
          lane_signal_inp.ew8_carry[1]  = carry[1];
          lane_signal_inp.ew8_carry[2]  = carry[2];
          lane_signal_inp.ew8_carry[3]  = carry[3];
          lane_signal_inp.ew16_carry[0] = carry[4];
          lane_signal_inp.ew16_carry[1] = carry[5];
          lane_signal_inp.ew32_carry    = carry[6];
          lane_signal_inp.ew64_carry    = carry[7];

          // Activate all lanes
          lane_signal_inp.ew64_valid = operation_valid;
          lane_signal_inp.ew32_valid = operation_valid;
          lane_signal_inp.ew16_valid = {2{operation_valid}};
          lane_signal_inp.ew8_valid  = {4{operation_valid}};
        end

        rvv_pkg::EW_16: begin
          lane_signal_inp.ops[0].ew64    = is_signed_and_not_vmulhsu ? 64'($signed(op_s1[63:48])) : 64'(op_s1[63:48]);
          lane_signal_inp.ops[0].ew32    = is_signed_and_not_vmulhsu ? 32'($signed(op_s1[47:32])) : 32'(op_s1[47:32]);
          lane_signal_inp.ops[0].ew16[1] = op_s1[31:16];
          lane_signal_inp.ops[0].ew16[0] = op_s1[15:0];

          lane_signal_inp.ops[1].ew64    = is_signed ? 64'($signed(op_s2[63:48])) : 64'(op_s2[63:48]);
          lane_signal_inp.ops[1].ew32    = is_signed ? 32'($signed(op_s2[47:32])) : 32'(op_s2[47:32]);
          lane_signal_inp.ops[1].ew16[1] = op_s2[31:16];
          lane_signal_inp.ops[1].ew16[0] = op_s2[15:0];

          lane_signal_inp.ops[2].ew64    = is_signed ? 64'($signed(op_d[63:48])) : 64'(op_d[63:48]);
          lane_signal_inp.ops[2].ew32    = is_signed ? 32'($signed(op_d[47:32])) : 32'(op_d[47:32]);
          lane_signal_inp.ops[2].ew16[1] = op_d[31:16];
          lane_signal_inp.ops[2].ew16[0] = op_d[15:0];

          lane_signal_inp.ew16_carry[0] = carry[0];
          lane_signal_inp.ew16_carry[1] = carry[2];
          lane_signal_inp.ew32_carry    = carry[2];
          lane_signal_inp.ew64_carry    = carry[3];

          // Activate the 64, 32, 16b lanes
          lane_signal_inp.ew64_valid = operation_valid;
          lane_signal_inp.ew32_valid = operation_valid;
          lane_signal_inp.ew16_valid = {2{operation_valid}};
          lane_signal_inp.ew8_valid  = '0;
        end

        rvv_pkg::EW_32: begin
          lane_signal_inp.ops[0].ew64 = is_signed_and_not_vmulhsu ? 64'($signed(op_s1[63:32])) : 64'(op_s1[63:32]);
          lane_signal_inp.ops[0].ew32 = op_s1[31:0];

          lane_signal_inp.ops[1].ew64 = is_signed ? 64'($signed(op_s2[63:32])) : 64'(op_s2[63:32]);
          lane_signal_inp.ops[1].ew32 = op_s2[31:0];

          lane_signal_inp.ops[2].ew64 = is_signed ? 64'($signed(op_d[63:32])) : 64'(op_d[63:32]);
          lane_signal_inp.ops[2].ew32 = op_d[31:0];

          lane_signal_inp.ew32_carry = carry[0];
          lane_signal_inp.ew64_carry = carry[1];

          // Activate the 64 and 32b lanes
          lane_signal_inp.ew64_valid = operation_valid;
          lane_signal_inp.ew32_valid = operation_valid;
          lane_signal_inp.ew16_valid = '0;
          lane_signal_inp.ew8_valid  = '0;
        end

        default: begin
          lane_signal_inp.ops[0].ew64 = op_s1;
          lane_signal_inp.ops[1].ew64 = op_s2;
          lane_signal_inp.ops[2].ew64 = op_d;
          lane_signal_inp.ew64_carry  = carry[0];

          // Activate the 32b lane
          lane_signal_inp.ew64_valid = operation_valid;
          lane_signal_inp.ew32_valid = '0;
          lane_signal_inp.ew16_valid = '0;
          lane_signal_inp.ew8_valid  = '0;
        end
      endcase
    end

    ///////////////
    // Collector //
    ///////////////

    // Collect results from the SIMD lanes
    always_comb begin : collector
      unique case (sew)
        rvv_pkg::EW_8 : begin
          result_o = {lane_signal_res.ew64_res[7:0], lane_signal_res.ew32_res[7:0], lane_signal_res.ew16_res[1][7:0], lane_signal_res.ew16_res[0][7:0],
            lane_signal_res.ew8_res[3], lane_signal_res.ew8_res[2], lane_signal_res.ew8_res[1], lane_signal_res.ew8_res[0]};
          result_valid_o = {lane_signal_res_valid.ew64_valid, lane_signal_res_valid.ew32_valid, lane_signal_res_valid.ew16_valid[1], lane_signal_res_valid.ew16_valid[0],
            lane_signal_res_valid.ew8_valid[3], lane_signal_res_valid.ew8_valid[2], lane_signal_res_valid.ew8_valid[1], lane_signal_res_valid.ew8_valid[0]};
        end
        rvv_pkg::EW_16: begin
          result_o       = {lane_signal_res.ew64_res[15:0], lane_signal_res.ew32_res[15:0], lane_signal_res.ew16_res[1], lane_signal_res.ew16_res[0]};
          result_valid_o = {{2{lane_signal_res_valid.ew64_valid}}, {2{lane_signal_res_valid.ew32_valid}}, {2{lane_signal_res_valid.ew16_valid[1]}}, {2{lane_signal_res_valid.ew16_valid[0]}} };
        end
        rvv_pkg::EW_32: begin
          result_o       = {lane_signal_res.ew64_res[31:0], lane_signal_res.ew32_res};
          result_valid_o = {{4{lane_signal_res_valid.ew64_valid}}, {4{lane_signal_res_valid.ew32_valid}}};
        end
        default: begin
          result_o       = lane_signal_res.ew64_res;
          result_valid_o = {8{lane_signal_res_valid.ew64_valid}};
        end
      endcase
    end

    // TODO: CHANGE
    assign be_o = '1;

    ////////////////
    // SIMD Lanes //
    ////////////////

    spatz_simd_lane #(
      .Width(8)
    ) i_simd_lane_8b_0 (
      .clk_i            (clk_i                             ),
      .rst_ni           (rst_ni                            ),
      .operation_i      (operation                         ),
      .operation_valid_i(lane_signal_inp.ew8_valid[0]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew8[0]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew8[0]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew8[0]     ),
      .is_signed_i      (is_signed                         ),
      .carry_i          (lane_signal_inp.ew8_carry[0]      ),
      .sew_i            (sew                               ),
      .result_o         (lane_signal_res.ew8_res[0]        ),
      .result_valid_o   (lane_signal_res_valid.ew8_valid[0]),
      .result_ready_i   (result_ready_i                    )
    );

    spatz_simd_lane #(
      .Width(8)
    ) i_simd_lane_8b_1 (
      .clk_i            (clk_i                             ),
      .rst_ni           (rst_ni                            ),
      .operation_i      (operation                         ),
      .operation_valid_i(lane_signal_inp.ew8_valid[1]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew8[1]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew8[1]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew8[1]     ),
      .is_signed_i      (is_signed                         ),
      .carry_i          (lane_signal_inp.ew8_carry[1]      ),
      .sew_i            (sew                               ),
      .result_o         (lane_signal_res.ew8_res[1]        ),
      .result_valid_o   (lane_signal_res_valid.ew8_valid[1]),
      .result_ready_i   (result_ready_i                    )
    );

    spatz_simd_lane #(
      .Width(8)
    ) i_simd_lane_8b_2 (
      .clk_i            (clk_i                             ),
      .rst_ni           (rst_ni                            ),
      .operation_i      (operation                         ),
      .operation_valid_i(lane_signal_inp.ew8_valid[2]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew8[2]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew8[2]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew8[2]     ),
      .is_signed_i      (is_signed                         ),
      .carry_i          (lane_signal_inp.ew8_carry[2]      ),
      .sew_i            (sew                               ),
      .result_o         (lane_signal_res.ew8_res[2]        ),
      .result_valid_o   (lane_signal_res_valid.ew8_valid[2]),
      .result_ready_i   (result_ready_i                    )
    );

    spatz_simd_lane #(
      .Width(8)
    ) i_simd_lane_8b_3 (
      .clk_i            (clk_i                             ),
      .rst_ni           (rst_ni                            ),
      .operation_i      (operation                         ),
      .operation_valid_i(lane_signal_inp.ew8_valid[3]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew8[3]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew8[3]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew8[3]     ),
      .is_signed_i      (is_signed                         ),
      .carry_i          (lane_signal_inp.ew8_carry[3]      ),
      .sew_i            (sew                               ),
      .result_o         (lane_signal_res.ew8_res[3]        ),
      .result_valid_o   (lane_signal_res_valid.ew8_valid[3]),
      .result_ready_i   (result_ready_i                    )
    );

    spatz_simd_lane #(
      .Width(16)
    ) i_simd_lane_16b_0 (
      .clk_i            (clk_i                              ),
      .rst_ni           (rst_ni                             ),
      .operation_i      (operation                          ),
      .operation_valid_i(lane_signal_inp.ew16_valid[0]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew16[0]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew16[0]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew16[0]     ),
      .is_signed_i      (is_signed                          ),
      .carry_i          (lane_signal_inp.ew16_carry[0]      ),
      .sew_i            (sew                                ),
      .result_o         (lane_signal_res.ew16_res[0]        ),
      .result_valid_o   (lane_signal_res_valid.ew16_valid[0]),
      .result_ready_i   (result_ready_i                     )
    );

    spatz_simd_lane #(
      .Width(16)
    ) i_simd_lane_16b_1 (
      .clk_i            (clk_i                              ),
      .rst_ni           (rst_ni                             ),
      .operation_i      (operation                          ),
      .operation_valid_i(lane_signal_inp.ew16_valid[1]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew16[1]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew16[1]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew16[1]     ),
      .is_signed_i      (is_signed                          ),
      .carry_i          (lane_signal_inp.ew16_carry[1]      ),
      .sew_i            (sew                                ),
      .result_o         (lane_signal_res.ew16_res[1]        ),
      .result_valid_o   (lane_signal_res_valid.ew16_valid[1]),
      .result_ready_i   (result_ready_i                     )
    );

    spatz_simd_lane #(
      .Width(32)
    ) i_simd_lane_32b_0 (
      .clk_i            (clk_i                           ),
      .rst_ni           (rst_ni                          ),
      .operation_i      (operation                       ),
      .operation_valid_i(lane_signal_inp.ew32_valid      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew32     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew32     ),
      .op_d_i           (lane_signal_inp.ops[2].ew32     ),
      .is_signed_i      (is_signed                       ),
      .carry_i          (lane_signal_inp.ew32_carry      ),
      .sew_i            (sew                             ),
      .result_o         (lane_signal_res.ew32_res        ),
      .result_valid_o   (lane_signal_res_valid.ew32_valid),
      .result_ready_i   (result_ready_i                  )
    );

    spatz_simd_lane #(
      .Width(64)
    ) i_simd_lane_64b_0 (
      .clk_i            (clk_i                           ),
      .rst_ni           (rst_ni                          ),
      .operation_i      (operation                       ),
      .operation_valid_i(lane_signal_inp.ew64_valid      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew64     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew64     ),
      .op_d_i           (lane_signal_inp.ops[2].ew64     ),
      .is_signed_i      (is_signed                       ),
      .carry_i          (lane_signal_inp.ew64_carry      ),
      .sew_i            (sew                             ),
      .result_o         (lane_signal_res.ew64_res        ),
      .result_valid_o   (lane_signal_res_valid.ew64_valid),
      .result_ready_i   (result_ready_i                  )
    );

  end: gen_64b_ipu else begin: gen_error
    $error("[spatz_ipu] Spatz' IPU only supports 32b and 64b modes.");
  end: gen_error

endmodule : spatz_ipu
