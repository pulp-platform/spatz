// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// The IPU distributes the operands to the four SIMD lanes and afterwards
// collects the results.

module spatz_ipu import spatz_pkg::*; import rvv_pkg::vew_e; #(
    parameter type tag_t         = logic,
    parameter bit  HasMultiplier = 1
  ) (
    input  logic   clk_i,
    input  logic   rst_ni,
    // Operation Signals
    input  op_e    operation_i,
    input  logic   operation_valid_i,
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
    output tag_t   tag_o
  );

  /////////////
  // Signals //
  /////////////

  // Is the operation signed
  logic is_signed;
  assign is_signed = operation_i inside {VMIN, VMAX, VMULH, VMULHSU, VDIV, VREM};

  // Forward the tag
  assign tag_o = tag_i;

  if (MAXEW == rvv_pkg::EW_32) begin: gen_32b_ipu

    struct packed {
      // Input operands
      struct packed {
        logic [1:0][7:0] ew8;
        logic [15:0] ew16;
        logic [31:0] ew32;
      } [2:0] ops;
      // Input carry
      logic [1:0] ew8_carry;
      logic ew16_carry;
      logic ew32_carry;
      // Valid lanes
      logic [1:0] ew8_valid;
      logic ew16_valid;
      logic ew32_valid;
    } lane_signal_inp;

    // SIMD output signals
    struct packed {
      // Results
      logic [1:0][7:0] ew8_res;
      logic [15:0] ew16_res;
      logic [31:0] ew32_res;
    } lane_signal_res;

    struct packed {
      logic [1:0] ew8_valid;
      logic ew16_valid;
      logic ew32_valid;
    } lane_signal_res_valid;

    /////////////////
    // Distributor //
    /////////////////

    // Distribute operands to the SIMD lanes
    always_comb begin : distributor
      lane_signal_inp = 'x;

      unique case (sew_i)
        rvv_pkg::EW_8: begin
          lane_signal_inp.ops[0].ew32   = is_signed && operation_i != VMULHSU ? 32'($signed(op_s1_i[31:24])) : 32'(op_s1_i[31:24]);
          lane_signal_inp.ops[0].ew16   = is_signed && operation_i != VMULHSU ? 16'($signed(op_s1_i[23:16])) : 16'(op_s1_i[23:16]);
          lane_signal_inp.ops[0].ew8[1] = op_s1_i[15:8];
          lane_signal_inp.ops[0].ew8[0] = op_s1_i[7:0];

          lane_signal_inp.ops[1].ew32   = is_signed ? 32'($signed(op_s2_i[31:24])) : 32'(op_s2_i[31:24]);
          lane_signal_inp.ops[1].ew16   = is_signed ? 16'($signed(op_s2_i[23:16])) : 16'(op_s2_i[23:16]);
          lane_signal_inp.ops[1].ew8[1] = op_s2_i[15:8];
          lane_signal_inp.ops[1].ew8[0] = op_s2_i[7:0];

          lane_signal_inp.ops[2].ew32   = is_signed ? 32'($signed(op_d_i[31:24])) : 32'(op_d_i[31:24]);
          lane_signal_inp.ops[2].ew16   = is_signed ? 16'($signed(op_d_i[23:16])) : 16'(op_d_i[23:16]);
          lane_signal_inp.ops[2].ew8[1] = op_d_i[15:8];
          lane_signal_inp.ops[2].ew8[0] = op_d_i[7:0];

          lane_signal_inp.ew8_carry[0] = carry_i[0];
          lane_signal_inp.ew8_carry[1] = carry_i[1];
          lane_signal_inp.ew16_carry   = carry_i[2];
          lane_signal_inp.ew32_carry   = carry_i[3];

          // Activate all lanes
          lane_signal_inp.ew32_valid = operation_valid_i;
          lane_signal_inp.ew16_valid = operation_valid_i;
          lane_signal_inp.ew8_valid  = {2{operation_valid_i}};
        end

        rvv_pkg::EW_16: begin
          lane_signal_inp.ops[0].ew32 = is_signed && operation_i != VMULHSU ? 32'($signed(op_s1_i[31:16])) : 32'(op_s1_i[31:16]);
          lane_signal_inp.ops[0].ew16 = op_s1_i[15:0];

          lane_signal_inp.ops[1].ew32 = is_signed ? 32'($signed(op_s2_i[31:16])) : 32'(op_s2_i[31:16]);
          lane_signal_inp.ops[1].ew16 = op_s2_i[15:0];

          lane_signal_inp.ops[2].ew32 = is_signed ? 32'($signed(op_d_i[31:16])) : 32'(op_d_i[31:16]);
          lane_signal_inp.ops[2].ew16 = op_d_i[15:0];

          lane_signal_inp.ew16_carry = carry_i[0];
          lane_signal_inp.ew32_carry = carry_i[1];

          // Activate the 32 and 16b lanes
          lane_signal_inp.ew32_valid = operation_valid_i;
          lane_signal_inp.ew16_valid = operation_valid_i;
          lane_signal_inp.ew8_valid  = 2'b00;
        end

        default: begin
          lane_signal_inp.ops[0].ew32 = op_s1_i;
          lane_signal_inp.ops[1].ew32 = op_s2_i;
          lane_signal_inp.ops[2].ew32 = op_d_i;
          lane_signal_inp.ew32_carry  = carry_i[0];

          // Activate the 32b lane
          lane_signal_inp.ew32_valid = operation_valid_i;
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
      unique case (sew_i)
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
      .Width        (8            ),
      .HasMultiplier(HasMultiplier)
    ) i_simd_lane_8b_0 (
      .clk_i            (clk_i                             ),
      .rst_ni           (rst_ni                            ),
      .operation_i      (operation_i                       ),
      .operation_valid_i(lane_signal_inp.ew8_valid[0]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew8[0]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew8[0]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew8[0]     ),
      .is_signed_i      (is_signed                         ),
      .carry_i          (lane_signal_inp.ew8_carry[0]      ),
      .sew_i            (sew_i                             ),
      .result_o         (lane_signal_res.ew8_res[0]        ),
      .result_valid_o   (lane_signal_res_valid.ew8_valid[0]),
      .result_ready_i   (result_ready_i                    )
    );

    spatz_simd_lane #(
      .Width        (8            ),
      .HasMultiplier(HasMultiplier)
    ) i_simd_lane_8b_1 (
      .clk_i            (clk_i                             ),
      .rst_ni           (rst_ni                            ),
      .operation_i      (operation_i                       ),
      .operation_valid_i(lane_signal_inp.ew8_valid[1]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew8[1]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew8[1]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew8[1]     ),
      .is_signed_i      (is_signed                         ),
      .carry_i          (lane_signal_inp.ew8_carry[1]      ),
      .sew_i            (sew_i                             ),
      .result_o         (lane_signal_res.ew8_res[1]        ),
      .result_valid_o   (lane_signal_res_valid.ew8_valid[1]),
      .result_ready_i   (result_ready_i                    )
    );

    spatz_simd_lane #(
      .Width        (16           ),
      .HasMultiplier(HasMultiplier)
    ) i_simd_lane_16b_0 (
      .clk_i            (clk_i                           ),
      .rst_ni           (rst_ni                          ),
      .operation_i      (operation_i                     ),
      .operation_valid_i(lane_signal_inp.ew16_valid      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew16     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew16     ),
      .op_d_i           (lane_signal_inp.ops[2].ew16     ),
      .is_signed_i      (is_signed                       ),
      .carry_i          (lane_signal_inp.ew16_carry      ),
      .sew_i            (sew_i                           ),
      .result_o         (lane_signal_res.ew16_res        ),
      .result_valid_o   (lane_signal_res_valid.ew16_valid),
      .result_ready_i   (result_ready_i                  )
    );

    spatz_simd_lane #(
      .Width        (32           ),
      .HasMultiplier(HasMultiplier)
    ) i_simd_lane_32b_0 (
      .clk_i            (clk_i                           ),
      .rst_ni           (rst_ni                          ),
      .operation_i      (operation_i                     ),
      .operation_valid_i(lane_signal_inp.ew32_valid      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew32     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew32     ),
      .op_d_i           (lane_signal_inp.ops[2].ew32     ),
      .is_signed_i      (is_signed                       ),
      .carry_i          (lane_signal_inp.ew32_carry      ),
      .sew_i            (sew_i                           ),
      .result_o         (lane_signal_res.ew32_res        ),
      .result_valid_o   (lane_signal_res_valid.ew32_valid),
      .result_ready_i   (result_ready_i                  )
    );

  end: gen_32b_ipu else if (MAXEW == rvv_pkg::EW_64) begin: gen_64b_ipu

    struct packed {
      // Input operands
      struct packed {
        logic [3:0][7:0] ew8;
        logic [1:0][15:0] ew16;
        logic [31:0] ew32;
        logic [63:0] ew64;
      } [2:0] ops;
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
    } lane_signal_inp;

    // SIMD output signals
    struct packed {
      // Results
      logic [3:0][7:0] ew8_res;
      logic [1:0][15:0] ew16_res;
      logic [31:0] ew32_res;
      logic [63:0] ew64_res;
    } lane_signal_res;

    struct packed {
      logic [3:0] ew8_valid;
      logic [1:0] ew16_valid;
      logic ew32_valid;
      logic ew64_valid;
    } lane_signal_res_valid;

    /////////////////
    // Distributor //
    /////////////////

    // Distribute operands to the SIMD lanes
    always_comb begin : distributor
      lane_signal_inp = 'x;

      unique case (sew_i)
        rvv_pkg::EW_8: begin
          lane_signal_inp.ops[0].ew64    = is_signed && operation_i != VMULHSU ? 64'($signed(op_s1_i[63:56])) : 64'(op_s1_i[63:56]);
          lane_signal_inp.ops[0].ew32    = is_signed && operation_i != VMULHSU ? 32'($signed(op_s1_i[55:48])) : 32'(op_s1_i[55:48]);
          lane_signal_inp.ops[0].ew16[1] = is_signed && operation_i != VMULHSU ? 16'($signed(op_s1_i[47:40])) : 16'(op_s1_i[47:40]);
          lane_signal_inp.ops[0].ew16[0] = is_signed && operation_i != VMULHSU ? 16'($signed(op_s1_i[39:32])) : 16'(op_s1_i[39:32]);
          lane_signal_inp.ops[0].ew8[3]  = op_s1_i[31:24];
          lane_signal_inp.ops[0].ew8[2]  = op_s1_i[23:16];
          lane_signal_inp.ops[0].ew8[1]  = op_s1_i[15:8];
          lane_signal_inp.ops[0].ew8[0]  = op_s1_i[7:0];

          lane_signal_inp.ops[1].ew64    = is_signed && operation_i != VMULHSU ? 64'($signed(op_s2_i[63:56])) : 64'(op_s2_i[63:56]);
          lane_signal_inp.ops[1].ew32    = is_signed && operation_i != VMULHSU ? 32'($signed(op_s2_i[55:48])) : 32'(op_s2_i[55:48]);
          lane_signal_inp.ops[1].ew16[1] = is_signed && operation_i != VMULHSU ? 16'($signed(op_s2_i[47:40])) : 16'(op_s2_i[47:40]);
          lane_signal_inp.ops[1].ew16[0] = is_signed && operation_i != VMULHSU ? 16'($signed(op_s2_i[39:32])) : 16'(op_s2_i[39:32]);
          lane_signal_inp.ops[1].ew8[3]  = op_s2_i[31:24];
          lane_signal_inp.ops[1].ew8[2]  = op_s2_i[23:16];
          lane_signal_inp.ops[1].ew8[1]  = op_s2_i[15:8];
          lane_signal_inp.ops[1].ew8[0]  = op_s2_i[7:0];

          lane_signal_inp.ops[2].ew64    = is_signed && operation_i != VMULHSU ? 64'($signed(op_d_i[63:56])) : 64'(op_d_i[63:56]);
          lane_signal_inp.ops[2].ew32    = is_signed && operation_i != VMULHSU ? 32'($signed(op_d_i[55:48])) : 32'(op_d_i[55:48]);
          lane_signal_inp.ops[2].ew16[1] = is_signed && operation_i != VMULHSU ? 16'($signed(op_d_i[47:40])) : 16'(op_d_i[47:40]);
          lane_signal_inp.ops[2].ew16[0] = is_signed && operation_i != VMULHSU ? 16'($signed(op_d_i[39:32])) : 16'(op_d_i[39:32]);
          lane_signal_inp.ops[2].ew8[3]  = op_d_i[31:24];
          lane_signal_inp.ops[2].ew8[2]  = op_d_i[23:16];
          lane_signal_inp.ops[2].ew8[1]  = op_d_i[15:8];
          lane_signal_inp.ops[2].ew8[0]  = op_d_i[7:0];

          lane_signal_inp.ew8_carry[0]  = carry_i[0];
          lane_signal_inp.ew8_carry[1]  = carry_i[1];
          lane_signal_inp.ew8_carry[2]  = carry_i[2];
          lane_signal_inp.ew8_carry[3]  = carry_i[3];
          lane_signal_inp.ew16_carry[0] = carry_i[4];
          lane_signal_inp.ew16_carry[1] = carry_i[5];
          lane_signal_inp.ew32_carry    = carry_i[6];
          lane_signal_inp.ew64_carry    = carry_i[7];

          // Activate all lanes
          lane_signal_inp.ew64_valid = operation_valid_i;
          lane_signal_inp.ew32_valid = operation_valid_i;
          lane_signal_inp.ew16_valid = {2{operation_valid_i}};
          lane_signal_inp.ew8_valid  = {4{operation_valid_i}};
        end

        rvv_pkg::EW_16: begin
          lane_signal_inp.ops[0].ew64    = is_signed && operation_i != VMULHSU ? 64'($signed(op_s1_i[63:48])) : 64'(op_s1_i[63:48]);
          lane_signal_inp.ops[0].ew32    = is_signed && operation_i != VMULHSU ? 32'($signed(op_s1_i[47:32])) : 32'(op_s1_i[47:32]);
          lane_signal_inp.ops[0].ew16[1] = op_s1_i[31:16];
          lane_signal_inp.ops[0].ew16[0] = op_s1_i[15:0];

          lane_signal_inp.ops[1].ew64    = is_signed && operation_i != VMULHSU ? 64'($signed(op_s2_i[63:48])) : 64'(op_s2_i[63:48]);
          lane_signal_inp.ops[1].ew32    = is_signed && operation_i != VMULHSU ? 32'($signed(op_s2_i[47:32])) : 32'(op_s2_i[47:32]);
          lane_signal_inp.ops[1].ew16[1] = op_s2_i[31:16];
          lane_signal_inp.ops[1].ew16[0] = op_s2_i[15:0];

          lane_signal_inp.ops[2].ew64    = is_signed && operation_i != VMULHSU ? 64'($signed(op_d_i[63:48])) : 64'(op_d_i[63:48]);
          lane_signal_inp.ops[2].ew32    = is_signed && operation_i != VMULHSU ? 32'($signed(op_d_i[47:32])) : 32'(op_d_i[47:32]);
          lane_signal_inp.ops[2].ew16[1] = op_d_i[31:16];
          lane_signal_inp.ops[2].ew16[0] = op_d_i[15:0];

          lane_signal_inp.ew16_carry[0] = carry_i[0];
          lane_signal_inp.ew16_carry[1] = carry_i[2];
          lane_signal_inp.ew32_carry    = carry_i[2];
          lane_signal_inp.ew64_carry    = carry_i[3];

          // Activate the 64, 32, 16b lanes
          lane_signal_inp.ew64_valid = operation_valid_i;
          lane_signal_inp.ew32_valid = operation_valid_i;
          lane_signal_inp.ew16_valid = {2{operation_valid_i}};
          lane_signal_inp.ew8_valid  = '0;
        end

        rvv_pkg::EW_32: begin
          lane_signal_inp.ops[0].ew64 = is_signed && operation_i != VMULHSU ? 64'($signed(op_s1_i[63:32])) : 64'(op_s1_i[63:32]);
          lane_signal_inp.ops[0].ew32 = op_s1_i[31:0];

          lane_signal_inp.ops[1].ew64 = is_signed && operation_i != VMULHSU ? 64'($signed(op_s2_i[63:32])) : 64'(op_s2_i[63:32]);
          lane_signal_inp.ops[1].ew32 = op_s2_i[31:0];

          lane_signal_inp.ops[2].ew64 = is_signed && operation_i != VMULHSU ? 64'($signed(op_d_i[63:32])) : 64'(op_d_i[63:32]);
          lane_signal_inp.ops[2].ew32 = op_d_i[31:0];

          lane_signal_inp.ew32_carry = carry_i[0];
          lane_signal_inp.ew64_carry = carry_i[1];

          // Activate the 64 and 32b lanes
          lane_signal_inp.ew64_valid = operation_valid_i;
          lane_signal_inp.ew32_valid = operation_valid_i;
          lane_signal_inp.ew16_valid = '0;
          lane_signal_inp.ew8_valid  = '0;
        end

        default: begin
          lane_signal_inp.ops[0].ew32 = op_s1_i;
          lane_signal_inp.ops[1].ew32 = op_s2_i;
          lane_signal_inp.ops[2].ew32 = op_d_i;
          lane_signal_inp.ew32_carry  = carry_i[0];

          // Activate the 32b lane
          lane_signal_inp.ew64_valid = operation_valid_i;
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
      unique case (sew_i)
        rvv_pkg::EW_8 : begin
          result_o = {lane_signal_res.ew64_res[7:0], lane_signal_res.ew32_res[7:0], lane_signal_res.ew16_res[1], lane_signal_res.ew16_res[0],
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
      .Width        (8            ),
      .HasMultiplier(HasMultiplier)
    ) i_simd_lane_8b_0 (
      .clk_i            (clk_i                             ),
      .rst_ni           (rst_ni                            ),
      .operation_i      (operation_i                       ),
      .operation_valid_i(lane_signal_inp.ew8_valid[0]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew8[0]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew8[0]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew8[0]     ),
      .is_signed_i      (is_signed                         ),
      .carry_i          (lane_signal_inp.ew8_carry[0]      ),
      .sew_i            (sew_i                             ),
      .result_o         (lane_signal_res.ew8_res[0]        ),
      .result_valid_o   (lane_signal_res_valid.ew8_valid[0]),
      .result_ready_i   (result_ready_i                    )
    );

    spatz_simd_lane #(
      .Width        (8            ),
      .HasMultiplier(HasMultiplier)
    ) i_simd_lane_8b_1 (
      .clk_i            (clk_i                             ),
      .rst_ni           (rst_ni                            ),
      .operation_i      (operation_i                       ),
      .operation_valid_i(lane_signal_inp.ew8_valid[1]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew8[1]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew8[1]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew8[1]     ),
      .is_signed_i      (is_signed                         ),
      .carry_i          (lane_signal_inp.ew8_carry[1]      ),
      .sew_i            (sew_i                             ),
      .result_o         (lane_signal_res.ew8_res[1]        ),
      .result_valid_o   (lane_signal_res_valid.ew8_valid[1]),
      .result_ready_i   (result_ready_i                    )
    );

    spatz_simd_lane #(
      .Width        (8            ),
      .HasMultiplier(HasMultiplier)
    ) i_simd_lane_8b_2 (
      .clk_i            (clk_i                             ),
      .rst_ni           (rst_ni                            ),
      .operation_i      (operation_i                       ),
      .operation_valid_i(lane_signal_inp.ew8_valid[2]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew8[2]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew8[2]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew8[2]     ),
      .is_signed_i      (is_signed                         ),
      .carry_i          (lane_signal_inp.ew8_carry[2]      ),
      .sew_i            (sew_i                             ),
      .result_o         (lane_signal_res.ew8_res[2]        ),
      .result_valid_o   (lane_signal_res_valid.ew8_valid[2]),
      .result_ready_i   (result_ready_i                    )
    );

    spatz_simd_lane #(
      .Width        (8            ),
      .HasMultiplier(HasMultiplier)
    ) i_simd_lane_8b_3 (
      .clk_i            (clk_i                             ),
      .rst_ni           (rst_ni                            ),
      .operation_i      (operation_i                       ),
      .operation_valid_i(lane_signal_inp.ew8_valid[3]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew8[3]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew8[3]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew8[3]     ),
      .is_signed_i      (is_signed                         ),
      .carry_i          (lane_signal_inp.ew8_carry[3]      ),
      .sew_i            (sew_i                             ),
      .result_o         (lane_signal_res.ew8_res[3]        ),
      .result_valid_o   (lane_signal_res_valid.ew8_valid[3]),
      .result_ready_i   (result_ready_i                    )
    );

    spatz_simd_lane #(
      .Width        (16           ),
      .HasMultiplier(HasMultiplier)
    ) i_simd_lane_16b_0 (
      .clk_i            (clk_i                              ),
      .rst_ni           (rst_ni                             ),
      .operation_i      (operation_i                        ),
      .operation_valid_i(lane_signal_inp.ew16_valid[0]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew16[0]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew16[0]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew16[0]     ),
      .is_signed_i      (is_signed                          ),
      .carry_i          (lane_signal_inp.ew16_carry[0]      ),
      .sew_i            (sew_i                              ),
      .result_o         (lane_signal_res.ew16_res[0]        ),
      .result_valid_o   (lane_signal_res_valid.ew16_valid[0]),
      .result_ready_i   (result_ready_i                     )
    );

    spatz_simd_lane #(
      .Width        (16           ),
      .HasMultiplier(HasMultiplier)
    ) i_simd_lane_16b_1 (
      .clk_i            (clk_i                              ),
      .rst_ni           (rst_ni                             ),
      .operation_i      (operation_i                        ),
      .operation_valid_i(lane_signal_inp.ew16_valid[1]      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew16[1]     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew16[1]     ),
      .op_d_i           (lane_signal_inp.ops[2].ew16[1]     ),
      .is_signed_i      (is_signed                          ),
      .carry_i          (lane_signal_inp.ew16_carry[1]      ),
      .sew_i            (sew_i                              ),
      .result_o         (lane_signal_res.ew16_res[1]        ),
      .result_valid_o   (lane_signal_res_valid.ew16_valid[1]),
      .result_ready_i   (result_ready_i                     )
    );

    spatz_simd_lane #(
      .Width        (32           ),
      .HasMultiplier(HasMultiplier)
    ) i_simd_lane_32b_0 (
      .clk_i            (clk_i                           ),
      .rst_ni           (rst_ni                          ),
      .operation_i      (operation_i                     ),
      .operation_valid_i(lane_signal_inp.ew32_valid      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew32     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew32     ),
      .op_d_i           (lane_signal_inp.ops[2].ew32     ),
      .is_signed_i      (is_signed                       ),
      .carry_i          (lane_signal_inp.ew32_carry      ),
      .sew_i            (sew_i                           ),
      .result_o         (lane_signal_res.ew32_res        ),
      .result_valid_o   (lane_signal_res_valid.ew32_valid),
      .result_ready_i   (result_ready_i                  )
    );

    spatz_simd_lane #(
      .Width        (64           ),
      .HasMultiplier(HasMultiplier)
    ) i_simd_lane_64b_0 (
      .clk_i            (clk_i                           ),
      .rst_ni           (rst_ni                          ),
      .operation_i      (operation_i                     ),
      .operation_valid_i(lane_signal_inp.ew64_valid      ),
      .op_s1_i          (lane_signal_inp.ops[0].ew64     ),
      .op_s2_i          (lane_signal_inp.ops[1].ew64     ),
      .op_d_i           (lane_signal_inp.ops[2].ew64     ),
      .is_signed_i      (is_signed                       ),
      .carry_i          (lane_signal_inp.ew64_carry      ),
      .sew_i            (sew_i                           ),
      .result_o         (lane_signal_res.ew64_res        ),
      .result_valid_o   (lane_signal_res_valid.ew64_valid),
      .result_ready_i   (result_ready_i                  )
    );

  end: gen_64b_ipu else begin: gen_error
    $error("[spatz_ipu] Spatz' IPU only supports 32b and 64b modes.");
  end: gen_error

endmodule : spatz_ipu
