// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE_HW for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Danilo Cammarata <dcammarata@iis.ee.ethz.ch>
// Author: Gianna Paulin <pauling@iis.ee.ethz.ch>
// Author: Luca Bertaccini <lbertaccini@iis.ee.ethz.ch>
// Author: Stefan Mach <smach@iis.ee.ethz.ch>

// This unit can be used to compute the following operations:
// - EXSDOTP: expanding dot product with accumulation
//             (op_a * op_b) + (op_c * op_d) + op_e
//             where op_e and the result are expressed with twice as many bits as op_a, op_b, op_c, op_d
// - EXVSUM: expanding vector inner sum
//             (op_a + op_c + op_e)
//             where op_e and the result are expressed with twice as many bits as op_a, op_c
//             EXVSUM is computed setting op_b and op_d to 1
// - VSUM:   non-expanding vector inner sum
//             (op_a + op_c + op_e)
//             where op_e and the result are expressed with as many bits as op_a, op_c
//             The bit-width can be as large as the maximum allowed destination width
//             VSUM is computed by-passing the two multiplications, thus neglecting op_b and op_d

// All the supported operations require a three-term addend (X + Y + Z). The unit first computes
// W = X + Y and then result = W + Z, where X is the maximum addend, Y is the intermediate addend
// and Z is the minimum addend.

// The unit requires two one-hot config strings to select the allowed input and output formats.
// The maximum output format should be twice as large as the maximum input format (for non-expanding
// VSUM the maximum input format is set by the maximum output format (op_a and op_c are as large
// as the accumulator and the result), then the input format is selected at run-time by the signal
// src_fmt_i.

`include "common_cells/registers.svh"

module opope_sdotp #(
  // One-hot config string: | FP32 | FP64 | FP16 | FP8 | FP16ALT | FP8ALT |
  parameter fpnew_pkg::fmt_logic_t   SrcDotpFpFmtConfig = '1, // FP32 and wider formats are not allowed
                                                              // Supported source formats (FP8, FP8ALT, FP16, FP16ALT)
  parameter fpnew_pkg::fmt_logic_t   DstDotpFpFmtConfig = '1, // FP8 and FP8alt are not supported
                                                              // Supported destination formats (FP16, FP16ALTt, FP32)
  parameter int unsigned             NumPipeRegs = 0,
  parameter fpnew_pkg::pipe_config_t PipeConfig  = fpnew_pkg::BEFORE,
  parameter fpnew_pkg::rsr_impl_t    StochasticRndImplementation = fpnew_pkg::DEFAULT_NO_RSR,
  parameter logic                    Stallable = 1'b0,
// Do not change
  localparam int unsigned           SRC_WIDTH = fpnew_pkg::max_fp_width(SrcDotpFpFmtConfig),
  localparam int unsigned           DST_WIDTH = fpnew_pkg::max_fp_width(DstDotpFpFmtConfig), // must be 2*SRC_WIDTH (expanding SDOTP)
  localparam fpnew_pkg::fp_format_e SRC_FMT   = SrcDotpFpFmtConfig == 6'h08 ? fpnew_pkg::FP16    :
                                                SrcDotpFpFmtConfig == 6'h02 ? fpnew_pkg::FP16ALT :
                                                SrcDotpFpFmtConfig == 6'h01 ? fpnew_pkg::FP8ALT  : fpnew_pkg::FP8,
  localparam fpnew_pkg::fp_format_e DST_FMT   = DstDotpFpFmtConfig == 6'h20 ? fpnew_pkg::FP32    :
                                                DstDotpFpFmtConfig == 6'h02 ? fpnew_pkg::FP16ALT : fpnew_pkg::FP16,
  localparam int unsigned NUM_FORMATS = fpnew_pkg::NUM_FP_FORMATS
) (
  input  logic                        clk_i,
  input  logic                        rst_ni,
  // Input signals
  input  logic [DST_WIDTH-1:0]        operand_a_i,
  input  logic [SRC_WIDTH-1:0]        operand_b_i,
  input  logic [DST_WIDTH-1:0]        operand_c_i,
  input  logic [SRC_WIDTH-1:0]        operand_d_i,
  input  logic [DST_WIDTH-1:0]        dst_operands_i, // accumulator
  input  logic                        reg_enable_i,
  // Output signals
  output logic [DST_WIDTH-1:0]        result_o
);

  // Select the optimized datapath only for validated configurations.
  // Retain the original implementation for other configurations and stochastic rounding.
  localparam bit SUPPORTED_FORMAT =
      ((SrcDotpFpFmtConfig == 6'h08 || SrcDotpFpFmtConfig == 6'h02)
          && DstDotpFpFmtConfig == 6'h20)
      || ((SrcDotpFpFmtConfig == 6'h04 || SrcDotpFpFmtConfig == 6'h01)
          && (DstDotpFpFmtConfig == 6'h08 || DstDotpFpFmtConfig == 6'h02))
      || (SrcDotpFpFmtConfig == 6'h04 && DstDotpFpFmtConfig == 6'h0c)
      || (SrcDotpFpFmtConfig == 6'h01 && DstDotpFpFmtConfig == 6'h09);
  // Upstream holds every register when Stallable=0. Preserve that
  // behavior through the legacy path; retimed reset transients can differ.
  localparam bit USE_OPTIMIZED = SUPPORTED_FORMAT && !StochasticRndImplementation.EnableRSR
      && NumPipeRegs <= 5 && (Stallable || NumPipeRegs == 0)
      && (PipeConfig == fpnew_pkg::BEFORE || PipeConfig == fpnew_pkg::AFTER
          || PipeConfig == fpnew_pkg::INSIDE || PipeConfig == fpnew_pkg::DISTRIBUTED);

  if (USE_OPTIMIZED) begin : gen_optimized
    opope_sdotp_multi #(
      .SrcDotpFpFmtConfig(SrcDotpFpFmtConfig),
      .DstDotpFpFmtConfig(DstDotpFpFmtConfig),
      .NumPipeRegs(NumPipeRegs),
      .PipeConfig(PipeConfig),
      .StochasticRndImplementation(StochasticRndImplementation)
    ) i_optimized (
      .clk_i, .rst_ni,
      .reg_enable_i(reg_enable_i),
      .operand_a_i, .operand_b_i, .operand_c_i, .operand_d_i, .dst_operands_i,
      .sdotp_hart_id_i('0), .is_boxed_i('1), .rnd_mode_i(fpnew_pkg::RNE),
      .op_i(fpnew_pkg::SDOTP), .op_mod_i(1'b0),
      .src_fmt_i(SRC_FMT), .dst_fmt_i(DST_FMT),
      .tag_i(1'b0), .mask_i(1'b1), .aux_i(1'b0),
      .in_valid_i(1'b1), .in_ready_o(), .flush_i(1'b0),
      .result_o, .status_o(), .extension_bit_o(), .tag_o(), .mask_o(), .aux_o(),
      .out_valid_o(), .out_ready_i(1'b1), .busy_o()
    );
  end else begin : gen_legacy
    opope_sdotp_legacy #(
      .SrcDotpFpFmtConfig(SrcDotpFpFmtConfig),
      .DstDotpFpFmtConfig(DstDotpFpFmtConfig),
      .NumPipeRegs(NumPipeRegs), .PipeConfig(PipeConfig),
      .StochasticRndImplementation(StochasticRndImplementation), .Stallable(Stallable)
    ) i_legacy (
      .clk_i, .rst_ni,
      .operand_a_i, .operand_b_i, .operand_c_i, .operand_d_i,
      .dst_operands_i, .reg_enable_i, .result_o,
      .valid_i(1'b1), .ready_o(), .result_valid_o(), .result_ready_i(1'b1)
    );
  end
endmodule
