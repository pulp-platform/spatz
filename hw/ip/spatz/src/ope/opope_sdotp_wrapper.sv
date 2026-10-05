// Copyright 2019-2021 ETH Zurich and University of Bologna.
//
// Copyright and related rights are licensed under the Solderpad Hardware
// License, Version 0.51 (the "License"); you may not use this file except in
// compliance with the License. You may obtain a copy of the License at
// http://solderpad.org/licenses/SHL-0.51. Unless required by applicable law
// or agreed to in writing, software, hardware and materials distributed under
// this License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
//
// SPDX-License-Identifier: SHL-0.51

// Author: Gianna Paulin    <pauling@iis.ee.ethz.ch>
// Author: Luca Bertaccini  <lbertaccini@iis.ee.ethz.ch>
// Author: Stefan Mach      <smach@iis.ee.ethz.ch>

`include "common_cells/registers.svh"

module opope_sdotp_wrapper #(
  parameter int unsigned             LaneWidth   = 64,
  parameter fpnew_pkg::fmt_logic_t   FpFmtConfig = '1,
  parameter int unsigned             NumPipeRegs = 0,
  parameter fpnew_pkg::pipe_config_t PipeConfig  = fpnew_pkg::BEFORE,
  parameter fpnew_pkg::rsr_impl_t    StochasticRndImplementation = fpnew_pkg::DEFAULT_NO_RSR,
  parameter logic                    Stallable = 1'b0,
  // Do not change
  localparam fpnew_pkg::fmt_logic_t FpSrcFmtConfig = FpFmtConfig[0] ? (FpFmtConfig & 6'b001111) : (FpFmtConfig & 6'b000101),
  localparam fpnew_pkg::fmt_logic_t FpDstFmtConfig = fpnew_pkg::get_dotp_dst_fmts(FpFmtConfig, FpSrcFmtConfig),
  localparam int                    SRC_WIDTH      = fpnew_pkg::maximum(fpnew_pkg::max_fp_width(FpSrcFmtConfig), 1),
  localparam int                    DST_WIDTH      = fpnew_pkg::maximum(2*fpnew_pkg::max_fp_width(FpSrcFmtConfig), 1), // do not change, current assumption of sdotpex_multi
  localparam fpnew_pkg::fp_format_e SRC_FMT        = FpSrcFmtConfig == 6'h08 ? fpnew_pkg::FP16    :
                                                     FpSrcFmtConfig == 6'h02 ? fpnew_pkg::FP16ALT :
                                                     FpSrcFmtConfig == 6'h01 ? fpnew_pkg::FP8ALT  : fpnew_pkg::FP8,
  localparam fpnew_pkg::fp_format_e DST_FMT        = FpDstFmtConfig == 6'h20 ? fpnew_pkg::FP32    :
                                                     FpDstFmtConfig == 6'h02 ? fpnew_pkg::FP16ALT : fpnew_pkg::FP16,
  localparam int                    OPERAND_WIDTH  = LaneWidth,
  localparam int unsigned           NUM_FORMATS    = fpnew_pkg::NUM_FP_FORMATS
) (
  input logic                          clk_i,
  input logic                          rst_ni,
  // Input signals
  input logic [2:0][OPERAND_WIDTH-1:0] operands_i, // 3 operands
  input  logic                         valid_i,
  output logic                         ready_o,
  input  logic                         reg_enable_i,
  // Output signals
  output logic                         result_valid_o,
  input  logic                         result_ready_i,
  output logic [OPERAND_WIDTH-1:0]     result_o
);

  // ----------
  // Constants
  // ----------
  localparam int unsigned N_SRC_FMT_OPERANDS = 4;
  localparam int unsigned N_DST_FMT_OPERANDS = 1;
  localparam int unsigned PIPE_LATENCY = NumPipeRegs;

  logic pipe_enable;

  if (PIPE_LATENCY == 0) begin : gen_zero_latency_valid
    assign ready_o        = result_ready_i;
    assign result_valid_o = valid_i;
    assign pipe_enable    = 1'b1;
  end else begin : gen_pipelined_valid
    logic [PIPE_LATENCY-1:0] valid_pipe_d;
    logic [PIPE_LATENCY-1:0] valid_pipe_q;

    assign ready_o        = Stallable ? (result_ready_i || !valid_pipe_q[PIPE_LATENCY-1]) : 1'b1;
    assign pipe_enable    = Stallable ? reg_enable_i : 1'b1;
    assign result_valid_o = valid_pipe_q[PIPE_LATENCY-1];

    always_comb begin
      valid_pipe_d = valid_pipe_q;
      if (pipe_enable) begin
        if (PIPE_LATENCY == 1)
          valid_pipe_d = valid_i & ready_o;
        else
          valid_pipe_d = {valid_pipe_q[PIPE_LATENCY-2:0], valid_i & ready_o};
      end
    end

    always_ff @(posedge clk_i or negedge rst_ni) begin
      if (!rst_ni)
        valid_pipe_q <= '0;
      else
        valid_pipe_q <= valid_pipe_d;
    end
  end

  // -----------------
  // Input processing
  // -----------------
  logic                             [NUM_FORMATS-1:0][DST_WIDTH-1:0] local_src_fmt_operand_a;  // lane-local operands
  logic                             [NUM_FORMATS-1:0][SRC_WIDTH-1:0] local_src_fmt_operand_b;  // lane-local operands
  logic                             [NUM_FORMATS-1:0][DST_WIDTH-1:0] local_src_fmt_operand_c;  // lane-local operands
  logic                             [NUM_FORMATS-1:0][SRC_WIDTH-1:0] local_src_fmt_operand_d;  // lane-local operands
  logic                                              [DST_WIDTH-1:0] local_dst_fmt_operands;  // lane-local operands
  logic                                          [OPERAND_WIDTH-1:0] local_result;  // lane-local operands


  // ----------------------------------
  // assign operands with dst format
  // ----------------------------------
  assign local_dst_fmt_operands = operands_i[2][DST_WIDTH-1:0];


  // ----------------------------------
  // assign operands with src format
  // ----------------------------------
  // NaN-boxing check
  for (genvar fmt = 0; fmt < int'(NUM_FORMATS); fmt++) begin : gen_nanbox

    localparam int unsigned FP_WIDTH         = fpnew_pkg::fp_width(fpnew_pkg::fp_format_e'(fmt));
    localparam int unsigned FP_WIDTH_MIN     = fpnew_pkg::minimum(SRC_WIDTH, FP_WIDTH);
    localparam int unsigned FP_WIDTH_DST_MIN = fpnew_pkg::minimum(DST_WIDTH, FP_WIDTH);

    logic [N_SRC_FMT_OPERANDS-1:0][FP_WIDTH_DST_MIN-1:0] tmp_operands;     // lane-local operands

    always_comb begin : nanbox
      // shift operands to correct position
      tmp_operands[0] = operands_i[0] >> 0*FP_WIDTH;
      tmp_operands[1] = operands_i[1] >> 0*FP_WIDTH;
      tmp_operands[2] = operands_i[0] >> 1*FP_WIDTH;
      tmp_operands[3] = operands_i[1] >> 1*FP_WIDTH;
      // nan-box if needed
      local_src_fmt_operand_a[fmt] = '1;
      local_src_fmt_operand_b[fmt] = '1;
      local_src_fmt_operand_c[fmt] = '1;
      local_src_fmt_operand_d[fmt] = '1;

      local_src_fmt_operand_a[fmt][FP_WIDTH_MIN-1:0] = tmp_operands[0][FP_WIDTH_MIN-1:0];
      local_src_fmt_operand_b[fmt][FP_WIDTH_MIN-1:0] = tmp_operands[1][FP_WIDTH_MIN-1:0];
      local_src_fmt_operand_c[fmt][FP_WIDTH_MIN-1:0] = tmp_operands[2][FP_WIDTH_MIN-1:0];
      local_src_fmt_operand_d[fmt][FP_WIDTH_MIN-1:0] = tmp_operands[3][FP_WIDTH_MIN-1:0];
      
    end
  end

  opope_sdotp #(
    .SrcDotpFpFmtConfig ( FpSrcFmtConfig ), // FP8, FP8ALT, FP16, FP16ALT
    .DstDotpFpFmtConfig ( FpDstFmtConfig ), // FP32, FP16, FP16ALT
    .NumPipeRegs        ( NumPipeRegs    ),
    .PipeConfig         ( PipeConfig     ),
    .StochasticRndImplementation ( StochasticRndImplementation ),
    .Stallable          ( Stallable      )
  ) i_opopope_sdotp (
    .clk_i,
    .rst_ni,
    .operand_a_i     ( local_src_fmt_operand_a[SRC_FMT] ),
    .operand_b_i     ( local_src_fmt_operand_b[SRC_FMT] ),
    .operand_c_i     ( local_src_fmt_operand_c[SRC_FMT] ),
    .operand_d_i     ( local_src_fmt_operand_d[SRC_FMT] ),
    .dst_operands_i  ( local_dst_fmt_operands           ), // 1 operand
    .reg_enable_i ( pipe_enable                          ),
    .result_o        ( local_result[DST_WIDTH-1:0] )
  );

  if(OPERAND_WIDTH > DST_WIDTH) begin
   assign local_result[OPERAND_WIDTH-1:DST_WIDTH]  = '1;
  end
  assign result_o                              = local_result;

endmodule
