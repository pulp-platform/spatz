// Copyright 2019-2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE_HW for details.
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

// Authors: Luca Bertaccini <lbertaccini@iis.ee.ethz.ch>
//          Stefan Mach <smach@iis.ee.ethz.ch>
//          Gianna Paulin <pauling@iis.ee.ethz.ch>

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

module opope_sdotp_multi #(
  // One-hot config string: | FP32 | FP64 | FP16 | FP8 | FP16ALT | FP8ALT |
  parameter fpnew_pkg::fmt_logic_t   SrcDotpFpFmtConfig = '1, // FP32 and wider formats are not allowed
                                                              // Supported source formats (FP8, FP8ALT, FP16, FP16ALT)
  parameter fpnew_pkg::fmt_logic_t   DstDotpFpFmtConfig = '1, // FP8 and FP8alt are not supported
                                                              // Supported destination formats (FP16, FP16ALTt, FP32)
  parameter int unsigned             NumPipeRegs = 0,
  parameter fpnew_pkg::pipe_config_t PipeConfig  = fpnew_pkg::BEFORE,
  parameter type                     TagType     = logic,
  parameter type                     AuxType     = logic,
  parameter fpnew_pkg::rsr_impl_t    StochasticRndImplementation = fpnew_pkg::DEFAULT_NO_RSR,
// Do not change
  localparam int unsigned SRC_WIDTH = fpnew_pkg::max_fp_width(SrcDotpFpFmtConfig),
  localparam int unsigned DST_WIDTH = fpnew_pkg::max_fp_width(DstDotpFpFmtConfig), // must be 2*SRC_WIDTH (expanding SDOTP)
  localparam int unsigned NUM_FORMATS = fpnew_pkg::NUM_FP_FORMATS
) (
  input  logic                        clk_i,
  input  logic                        rst_ni,
  input  logic                        reg_enable_i,
  input  logic [33:0]                 sdotp_hart_id_i,
  // Input signals
  // op_a and op_c will contain useful bits in [SRC_WIDTH-1:0] for EXSDOTP, EXVSUM
  // op_a and op_c will contain useful bits in [DST_WIDTH-1:0] for VSUM (non-expanding)
  // op_b and op_d are neglected for non-expanding VSUM
  input  logic [DST_WIDTH-1:0]        operand_a_i,
  input  logic [SRC_WIDTH-1:0]        operand_b_i,
  input  logic [DST_WIDTH-1:0]        operand_c_i,
  input  logic [SRC_WIDTH-1:0]        operand_d_i,
  input  logic [DST_WIDTH-1:0]        dst_operands_i, // accumulator
  input  logic [NUM_FORMATS-1:0][4:0] is_boxed_i,     // 5 operands
  input  fpnew_pkg::roundmode_e       rnd_mode_i,
  input  fpnew_pkg::operation_e       op_i,
  input  logic                        op_mod_i,
  input  fpnew_pkg::fp_format_e       src_fmt_i, // format of op_a, op_b, op_c, op_d
  input  fpnew_pkg::fp_format_e       dst_fmt_i, // format of the accumulator (op_e) and result
  input  TagType                      tag_i,
  input  logic                        mask_i,
  input  AuxType                      aux_i,
  // Input Handshake
  input  logic                        in_valid_i,
  output logic                        in_ready_o,
  input  logic                        flush_i,
  // Output signals
  output logic [DST_WIDTH-1:0]        result_o,
  output fpnew_pkg::status_t          status_o,
  output logic                        extension_bit_o,
  output TagType                      tag_o,
  output logic                        mask_o,
  output AuxType                      aux_o,
  // Output handshake
  output logic                        out_valid_o,
  input  logic                        out_ready_i,
  // Indication of valid data in flight
  output logic                        busy_o
);

  // ----------
  // Constants
  // ----------
  // The super-format that can hold all formats
  localparam fpnew_pkg::fp_encoding_t SUPER_FORMAT = fpnew_pkg::super_format(SrcDotpFpFmtConfig);
  localparam fpnew_pkg::fp_encoding_t SUPER_DST_FORMAT = fpnew_pkg::super_format(DstDotpFpFmtConfig);

  localparam int unsigned SUPER_EXP_BITS = SUPER_FORMAT.exp_bits;
  localparam int unsigned SUPER_MAN_BITS = SUPER_FORMAT.man_bits;
  localparam int unsigned SUPER_DST_EXP_BITS = SUPER_DST_FORMAT.exp_bits;
  localparam int unsigned SUPER_DST_MAN_BITS = (fpnew_pkg::maximum(SUPER_DST_FORMAT.man_bits, 2*SUPER_MAN_BITS + 1)
      + ((SrcDotpFpFmtConfig == 6'h02 && DstDotpFpFmtConfig == 6'h20)
          ? 2*SUPER_MAN_BITS : 0));

  // Precision bits 'p' include the implicit bit
  localparam int unsigned PRECISION_BITS = SUPER_MAN_BITS + 1;
  // Destination precision bits 'p_dst' include the implicit bit
  localparam int unsigned DST_PRECISION_BITS = SUPER_DST_MAN_BITS + 1;
  localparam int unsigned ADDITIONAL_PRECISION_BITS = fpnew_pkg::maximum(DST_PRECISION_BITS - 2 * PRECISION_BITS, 0);
  // Stochastic rounding implementation
  localparam logic        ENABLE_RSR         = StochasticRndImplementation.EnableRSR;
  localparam int unsigned RSR_PRECISION_BITS = StochasticRndImplementation.RsrPrecision;
  localparam int unsigned LFSR_WIDTH         = StochasticRndImplementation.LfsrInternalPrecision;
  // The leading-zero counter operates on LZC_SUM_WIDTH bits
  localparam int unsigned LZC_SUM_WIDTH  = 2*DST_PRECISION_BITS + PRECISION_BITS + 5;
  localparam int unsigned LZC_RESULT_WIDTH = $clog2(LZC_SUM_WIDTH);

  // Internal exponent width must accomodate all meaningful exponent values in order to avoid
  // datapath leakage. This is either given by the exponent bits or the width of the LZC result.
  localparam int unsigned EXP_WIDTH = unsigned'(fpnew_pkg::maximum(SUPER_EXP_BITS + 2, LZC_RESULT_WIDTH));
  localparam int unsigned DST_EXP_WIDTH = unsigned'(fpnew_pkg::maximum(SUPER_DST_EXP_BITS + 2, LZC_RESULT_WIDTH));
  // Shift amount width: maximum internal mantissa size is 2*DST_PRECISION_BITS+3 bits
  localparam int unsigned SHIFT_AMOUNT_WIDTH = $clog2(2*DST_PRECISION_BITS+PRECISION_BITS+4);
  localparam int unsigned DST_SHIFT_AMOUNT_WIDTH = $clog2(2*DST_PRECISION_BITS+PRECISION_BITS+5);
  // Pipelines
  localparam int unsigned NUM_INP_REGS =
    (PipeConfig == fpnew_pkg::BEFORE)      ? NumPipeRegs :
    (PipeConfig == fpnew_pkg::DISTRIBUTED) ? ((NumPipeRegs + 1) / 3) : // Second to get distributed regs
    (PipeConfig == fpnew_pkg::INSIDE)      ? (NumPipeRegs > 4) :
                                            0;

  localparam int unsigned NUM_IM_EARLY_REGS =
    (PipeConfig == fpnew_pkg::INSIDE)      ? ((NumPipeRegs == 3) || (NumPipeRegs > 4)) :
                                            0;

  localparam int unsigned NUM_IM_LATE_REGS =
    (PipeConfig == fpnew_pkg::INSIDE)      ? ((NumPipeRegs == 2) || (NumPipeRegs == 4)) :
                                            0;

  localparam int unsigned NUM_MID_REGS =
    (PipeConfig == fpnew_pkg::DISTRIBUTED) ? ((NumPipeRegs + 2) / 3) : // First to get distributed regs
    (PipeConfig == fpnew_pkg::INSIDE)      ? ((NumPipeRegs > 4) ? (NumPipeRegs - 4) : ((NumPipeRegs == 1) || (NumPipeRegs == 3) || (NumPipeRegs == 4))) : // absorbs overflow beyond 4 stages
                                            0;

  localparam int unsigned NUM_MO_EARLY_REGS =
    (PipeConfig == fpnew_pkg::INSIDE)      ? ((NumPipeRegs == 2) || (NumPipeRegs == 4)) :
                                            0;

  localparam int unsigned NUM_MO_LATE_REGS =
    (PipeConfig == fpnew_pkg::INSIDE)      ? ((NumPipeRegs == 3) || (NumPipeRegs > 4)) :
                                            0;

  localparam int unsigned NUM_OUT_REGS =
    (PipeConfig == fpnew_pkg::AFTER)       ? NumPipeRegs :
    (PipeConfig == fpnew_pkg::DISTRIBUTED) ? (NumPipeRegs / 3) : // Last to get distributed regs
    (PipeConfig == fpnew_pkg::INSIDE)      ? (NumPipeRegs > 3) :
                                            0;

  // ----------------
  // Type definition
  // ----------------
  typedef struct packed {
    logic                      sign;
    logic [SUPER_EXP_BITS-1:0] exponent;
    logic [SUPER_MAN_BITS-1:0] mantissa;
  } fp_src_t;
  typedef struct packed {
    logic                          sign;
    logic [SUPER_DST_EXP_BITS-1:0] exponent;
    logic [SUPER_DST_MAN_BITS-1:0] mantissa;
  } fp_dst_t;

  // ---------------
  // Input pipeline
  // ---------------
  // Selected pipeline output signals as non-arrays
  logic [DST_WIDTH-1:0]  operand_a_q;
  logic [SRC_WIDTH-1:0]  operand_b_q;
  logic [DST_WIDTH-1:0]  operand_c_q;
  logic [SRC_WIDTH-1:0]  operand_d_q;
  logic [DST_WIDTH-1:0]  dst_operands_q;
  fpnew_pkg::fp_format_e src_fmt_q;
  fpnew_pkg::fp_format_e dst_fmt_q;

  // Input pipeline signals, index i holds signal after i register stages
  logic                  [0:NUM_INP_REGS][DST_WIDTH-1:0]        inp_pipe_operand_a_q;
  logic                  [0:NUM_INP_REGS][SRC_WIDTH-1:0]        inp_pipe_operand_b_q;
  logic                  [0:NUM_INP_REGS][DST_WIDTH-1:0]        inp_pipe_operand_c_q;
  logic                  [0:NUM_INP_REGS][SRC_WIDTH-1:0]        inp_pipe_operand_d_q;
  logic                  [0:NUM_INP_REGS][DST_WIDTH-1:0]        inp_pipe_dst_operands_q;
  logic                  [0:NUM_INP_REGS][NUM_FORMATS-1:0][4:0] inp_pipe_is_boxed_q;
  fpnew_pkg::roundmode_e [0:NUM_INP_REGS]                       inp_pipe_rnd_mode_q;
  fpnew_pkg::operation_e [0:NUM_INP_REGS]                       inp_pipe_op_q;
  logic                  [0:NUM_INP_REGS]                       inp_pipe_op_mod_q;
  fpnew_pkg::fp_format_e [0:NUM_INP_REGS]                       inp_pipe_src_fmt_q;
  fpnew_pkg::fp_format_e [0:NUM_INP_REGS]                       inp_pipe_dst_fmt_q;
  TagType                [0:NUM_INP_REGS]                       inp_pipe_tag_q;
  logic                  [0:NUM_INP_REGS]                       inp_pipe_mask_q;
  AuxType                [0:NUM_INP_REGS]                       inp_pipe_aux_q;
  logic                  [0:NUM_INP_REGS]                       inp_pipe_valid_q;
  // Ready signal is combinatorial for all stages
  logic                  [0:NUM_INP_REGS]                       inp_pipe_ready;

  // Input stage: First element of pipeline is taken from inputs
  assign inp_pipe_operand_a_q[0]    = operand_a_i;
  assign inp_pipe_operand_b_q[0]    = operand_b_i;
  assign inp_pipe_operand_c_q[0]    = operand_c_i;
  assign inp_pipe_operand_d_q[0]    = operand_d_i;
  assign inp_pipe_dst_operands_q[0] = dst_operands_i;
  assign inp_pipe_is_boxed_q[0]     = is_boxed_i;
  assign inp_pipe_rnd_mode_q[0]     = rnd_mode_i;
  assign inp_pipe_op_q[0]           = op_i;
  assign inp_pipe_op_mod_q[0]       = op_mod_i;
  assign inp_pipe_src_fmt_q[0]      = src_fmt_i;
  assign inp_pipe_dst_fmt_q[0]      = dst_fmt_i;
  assign inp_pipe_tag_q[0]          = tag_i;
  assign inp_pipe_mask_q[0]         = mask_i;
  assign inp_pipe_aux_q[0]          = aux_i;
  assign inp_pipe_valid_q[0]        = in_valid_i;
  // Input stage: Propagate pipeline ready signal to upstream circuitry
  assign in_ready_o                 = inp_pipe_ready[0];
  // Generate the register stages
  for (genvar i = 0; i < NUM_INP_REGS; i++) begin : gen_input_pipeline
    // Internal register enable for this stage
    logic reg_ena;
    // Determine the ready signal of the current stage - advance the pipeline:
    // 1. if the next stage is ready for our data
    // 2. if the next stage only holds a bubble (not valid) -> we can pop it
    assign inp_pipe_ready[i] = inp_pipe_ready[i+1] | ~inp_pipe_valid_q[i+1];
    // Valid: enabled by ready signal, synchronous clear with the flush signal
    `FFLARNC(inp_pipe_valid_q[i+1], inp_pipe_valid_q[i], reg_enable_i, flush_i, 1'b0, clk_i, rst_ni)
    // Enable register if pipleine ready and a valid data item is present
    assign reg_ena = reg_enable_i;
    // Generate the pipeline registers within the stages, use enable-registers
    `FFL(inp_pipe_operand_a_q[i+1],    inp_pipe_operand_a_q[i],    reg_ena, '0)
    `FFL(inp_pipe_operand_b_q[i+1],    inp_pipe_operand_b_q[i],    reg_ena, '0)
    `FFL(inp_pipe_operand_c_q[i+1],    inp_pipe_operand_c_q[i],    reg_ena, '0)
    `FFL(inp_pipe_operand_d_q[i+1],    inp_pipe_operand_d_q[i],    reg_ena, '0)
    `FFL(inp_pipe_dst_operands_q[i+1], inp_pipe_dst_operands_q[i], reg_ena, '0)
    `FFL(inp_pipe_is_boxed_q[i+1],     inp_pipe_is_boxed_q[i],     reg_ena, '0)
    `FFL(inp_pipe_rnd_mode_q[i+1],     inp_pipe_rnd_mode_q[i],     reg_ena, fpnew_pkg::RNE)
    `FFL(inp_pipe_op_q[i+1],           inp_pipe_op_q[i],           reg_ena, fpnew_pkg::SDOTP)
    `FFL(inp_pipe_op_mod_q[i+1],       inp_pipe_op_mod_q[i],       reg_ena, '0)
    `FFL(inp_pipe_src_fmt_q[i+1],      inp_pipe_src_fmt_q[i],      reg_ena, fpnew_pkg::FP8)
    `FFL(inp_pipe_dst_fmt_q[i+1],      inp_pipe_dst_fmt_q[i],      reg_ena, fpnew_pkg::FP16)
    `FFL(inp_pipe_tag_q[i+1],          inp_pipe_tag_q[i],          reg_ena, TagType'('0))
    `FFL(inp_pipe_mask_q[i+1],         inp_pipe_mask_q[i],         reg_ena, '0)
    `FFL(inp_pipe_aux_q[i+1],          inp_pipe_aux_q[i],          reg_ena, AuxType'('0))
  end
  // Output stage: assign selected pipe outputs to signals for later use
  assign operand_a_q    = inp_pipe_operand_a_q[NUM_INP_REGS];
  assign operand_b_q    = inp_pipe_operand_b_q[NUM_INP_REGS];
  assign operand_c_q    = inp_pipe_operand_c_q[NUM_INP_REGS];
  assign operand_d_q    = inp_pipe_operand_d_q[NUM_INP_REGS];
  assign dst_operands_q = inp_pipe_dst_operands_q[NUM_INP_REGS];
  assign src_fmt_q      = inp_pipe_src_fmt_q[NUM_INP_REGS];
  assign dst_fmt_q      = inp_pipe_dst_fmt_q[NUM_INP_REGS];

  // -----------------
  // Input processing
  // -----------------
  fp_src_t             operand_a, operand_b, operand_c, operand_d;
  fp_dst_t             operand_e;
  fp_dst_t             operand_a_vsum, operand_c_vsum;
  fpnew_pkg::fp_info_t info_a, info_b, info_c, info_d, info_e;
  logic                a_sign, c_sign;
  logic                any_operand_inf;
  logic                any_operand_nan;
  logic                signalling_nan;
  logic [2:0]          effective_subtraction;
  logic                tentative_sign;

  opope_sdotp_input_prep #(
    .SrcDotpFpFmtConfig ( SrcDotpFpFmtConfig ),
    .DstDotpFpFmtConfig ( DstDotpFpFmtConfig )
  ) i_fpnew_sdotp_input_prep (
    .operand_a_q_i           ( operand_a_q                          ),
    .operand_b_q_i           ( operand_b_q                          ),
    .operand_c_q_i           ( operand_c_q                          ),
    .operand_d_q_i           ( operand_d_q                          ),
    .dst_operands_q_i        ( dst_operands_q                       ),
    .is_boxed_i              ( inp_pipe_is_boxed_q[NUM_INP_REGS]    ),
    .src_fmt_i               ( src_fmt_q                            ),
    .dst_fmt_i               ( dst_fmt_q                            ),
    .op_i                    ( inp_pipe_op_q[NUM_INP_REGS]          ),
    .op_mod_i                ( inp_pipe_op_mod_q[NUM_INP_REGS]      ),
    .operand_a_o             ( operand_a                            ),
    .operand_b_o             ( operand_b                            ),
    .operand_c_o             ( operand_c                            ),
    .operand_d_o             ( operand_d                            ),
    .operand_e_o             ( operand_e                            ),
    .operand_a_vsum_o        ( operand_a_vsum                       ),
    .operand_c_vsum_o        ( operand_c_vsum                       ),
    .info_a_o                ( info_a                               ),
    .info_b_o                ( info_b                               ),
    .info_c_o                ( info_c                               ),
    .info_d_o                ( info_d                               ),
    .info_e_o                ( info_e                               ),
    .a_sign_o                ( a_sign                               ),
    .c_sign_o                ( c_sign                               ),
    .any_operand_inf_o       ( any_operand_inf                      ),
    .any_operand_nan_o       ( any_operand_nan                      ),
    .signalling_nan_o        ( signalling_nan                       ),
    .effective_subtraction_o ( effective_subtraction                )
  );

  // ----------------------
  // Special case handling
  // ----------------------
  logic [DST_WIDTH-1:0] special_result;
  fpnew_pkg::status_t   special_status;
  logic                 result_is_special;

  opope_sdotp_special_case #(
    .SrcDotpFpFmtConfig ( SrcDotpFpFmtConfig ),
    .DstDotpFpFmtConfig ( DstDotpFpFmtConfig )
  ) i_fpnew_sdotp_special_case (
    .info_a_i                ( info_a                ),
    .info_b_i                ( info_b                ),
    .info_c_i                ( info_c                ),
    .info_d_i                ( info_d                ),
    .info_e_i                ( info_e                ),
    .a_sign_i                ( a_sign                ),
    .c_sign_i                ( c_sign                ),
    .operand_b_i             ( operand_b             ),
    .operand_d_i             ( operand_d             ),
    .operand_e_i             ( operand_e             ),
    .any_operand_inf_i       ( any_operand_inf       ),
    .any_operand_nan_i       ( any_operand_nan       ),
    .signalling_nan_i        ( signalling_nan        ),
    .effective_subtraction_i ( effective_subtraction ),
    .dst_fmt_i               ( dst_fmt_q             ),
    .special_result_o        ( special_result        ),
    .special_status_o        ( special_status        ),
    .result_is_special_o     ( result_is_special     )
  );

  // ---------------------------
  // Initial exponent data path
  // ---------------------------
  logic signed [DST_EXP_WIDTH-1:0] exponent_int, exponent_min;
  logic signed [DST_EXP_WIDTH-1:0] tentative_exponent;
  logic [2:0]                      exponent_cmp;
  logic                            effective_subtraction_first;
  logic                            info_min_is_zero;
  logic                            info_int_is_zero;
  logic                            info_max_is_zero;
  logic                            addend_min_sign;
  logic                            addend_int_sign;
  logic                            addend_max_sign;
  logic [SHIFT_AMOUNT_WIDTH-1:0]   addend_shamt;

  opope_sdotp_exponent_prep #(
    .SrcDotpFpFmtConfig ( SrcDotpFpFmtConfig ),
    .DstDotpFpFmtConfig ( DstDotpFpFmtConfig )
  ) i_fpnew_sdotp_exponent_prep (
    .operand_a_i                   ( operand_a                      ),
    .operand_b_i                   ( operand_b                      ),
    .operand_c_i                   ( operand_c                      ),
    .operand_d_i                   ( operand_d                      ),
    .operand_e_i                   ( operand_e                      ),
    .operand_a_vsum_i              ( operand_a_vsum                 ),
    .operand_c_vsum_i              ( operand_c_vsum                 ),
    .info_a_i                      ( info_a                         ),
    .info_b_i                      ( info_b                         ),
    .info_c_i                      ( info_c                         ),
    .info_d_i                      ( info_d                         ),
    .info_e_i                      ( info_e                         ),
    .a_sign_i                      ( a_sign                         ),
    .c_sign_i                      ( c_sign                         ),
    .op_i                          ( inp_pipe_op_q[NUM_INP_REGS]    ),
    .src_fmt_i                     ( src_fmt_q                      ),
    .dst_fmt_i                     ( dst_fmt_q                      ),
    .effective_subtraction_i       ( effective_subtraction          ),
    .exponent_cmp_o                ( exponent_cmp                   ),
    .tentative_sign_o              ( tentative_sign                 ),
    .effective_subtraction_first_o ( effective_subtraction_first    ),
    .info_min_is_zero_o            ( info_min_is_zero               ),
    .info_int_is_zero_o            ( info_int_is_zero               ),
    .info_max_is_zero_o            ( info_max_is_zero               ),
    .addend_min_sign_o             ( addend_min_sign                ),
    .addend_int_sign_o             ( addend_int_sign                ),
    .addend_max_sign_o             ( addend_max_sign                ),
    .tentative_exponent_o          ( tentative_exponent             ),
    .exponent_int_o                ( exponent_int                   ),
    .exponent_min_o                ( exponent_min                   ),
    .addend_shamt_o                ( addend_shamt                   )
  );

  // --------------------------
  // INP to MID EARLY pipeline
  // --------------------------
  // Pipeline output signals as non-arrays
  fp_src_t             operand_a_q2, operand_b_q2, operand_c_q2, operand_d_q2;
  fpnew_pkg::fp_info_t info_a_q2, info_b_q2, info_c_q2, info_d_q2;

  // Internal pipeline signals, index i holds signal after i register stages
  fpnew_pkg::operation_e [0:NUM_IM_EARLY_REGS]                         im_early_pipe_op_q;
  fp_src_t               [0:NUM_IM_EARLY_REGS]                         im_early_pipe_operand_a_q;
  fp_src_t               [0:NUM_IM_EARLY_REGS]                         im_early_pipe_operand_b_q;
  fp_src_t               [0:NUM_IM_EARLY_REGS]                         im_early_pipe_operand_c_q;
  fp_src_t               [0:NUM_IM_EARLY_REGS]                         im_early_pipe_operand_d_q;
  fp_dst_t               [0:NUM_IM_EARLY_REGS]                         im_early_pipe_operand_e_q;
  fp_dst_t               [0:NUM_IM_EARLY_REGS]                         im_early_pipe_operand_a_vsum_q;
  fp_dst_t               [0:NUM_IM_EARLY_REGS]                         im_early_pipe_operand_c_vsum_q;
  fpnew_pkg::fp_info_t   [0:NUM_IM_EARLY_REGS]                         im_early_pipe_info_a_q;
  fpnew_pkg::fp_info_t   [0:NUM_IM_EARLY_REGS]                         im_early_pipe_info_b_q;
  fpnew_pkg::fp_info_t   [0:NUM_IM_EARLY_REGS]                         im_early_pipe_info_c_q;
  fpnew_pkg::fp_info_t   [0:NUM_IM_EARLY_REGS]                         im_early_pipe_info_d_q;
  fpnew_pkg::fp_info_t   [0:NUM_IM_EARLY_REGS]                         im_early_pipe_info_e_q;
  logic                  [0:NUM_IM_EARLY_REGS][SHIFT_AMOUNT_WIDTH-1:0] im_early_pipe_addend_shamt_q;
  logic                  [0:NUM_IM_EARLY_REGS]                         im_early_pipe_eff_sub_first_q;
  logic                  [0:NUM_IM_EARLY_REGS]                         im_early_pipe_tentative_sign_q;
  logic signed           [0:NUM_IM_EARLY_REGS][DST_EXP_WIDTH-1:0]      im_early_pipe_tent_exp_q;
  logic signed           [0:NUM_IM_EARLY_REGS][DST_EXP_WIDTH-1:0]      im_early_pipe_exp_int_q;
  logic signed           [0:NUM_IM_EARLY_REGS][DST_EXP_WIDTH-1:0]      im_early_pipe_exp_min_q;
  logic                  [0:NUM_IM_EARLY_REGS]                         im_early_pipe_info_min_is_zero_q;
  logic                  [0:NUM_IM_EARLY_REGS]                         im_early_pipe_info_int_is_zero_q;
  logic                  [0:NUM_IM_EARLY_REGS]                         im_early_pipe_info_max_is_zero_q;
  logic                  [0:NUM_IM_EARLY_REGS]                         im_early_pipe_addend_min_sign_q;
  logic                  [0:NUM_IM_EARLY_REGS]                         im_early_pipe_addend_int_sign_q;
  logic                  [0:NUM_IM_EARLY_REGS]                         im_early_pipe_addend_max_sign_q;
  logic                  [0:NUM_IM_EARLY_REGS][2:0]                    im_early_pipe_exp_cmp_q;
  logic                  [0:NUM_IM_EARLY_REGS]                         im_early_pipe_a_sign_q;
  logic                  [0:NUM_IM_EARLY_REGS]                         im_early_pipe_c_sign_q;
  fpnew_pkg::roundmode_e [0:NUM_IM_EARLY_REGS]                         im_early_pipe_rnd_mode_q;
  fpnew_pkg::fp_format_e [0:NUM_IM_EARLY_REGS]                         im_early_pipe_dst_fmt_q;
  logic                  [0:NUM_IM_EARLY_REGS][DST_WIDTH-1:0]          im_early_pipe_special_result_q;
  fpnew_pkg::status_t    [0:NUM_IM_EARLY_REGS]                         im_early_pipe_special_status_q;
  logic                  [0:NUM_IM_EARLY_REGS]                         im_early_pipe_res_is_spec_q;
  TagType                [0:NUM_IM_EARLY_REGS]                         im_early_pipe_tag_q;
  logic                  [0:NUM_IM_EARLY_REGS]                         im_early_pipe_mask_q;
  AuxType                [0:NUM_IM_EARLY_REGS]                         im_early_pipe_aux_q;
  logic                  [0:NUM_IM_EARLY_REGS]                         im_early_pipe_valid_q;
  // Ready signal is combinatorial for all stages
  logic                  [0:NUM_IM_EARLY_REGS]                         im_early_pipe_ready;

  // Input stage: First element of pipeline is taken from upstream logic
  assign im_early_pipe_op_q[0]               = inp_pipe_op_q[NUM_INP_REGS];
  assign im_early_pipe_operand_a_q[0]        = operand_a;
  assign im_early_pipe_operand_b_q[0]        = operand_b;
  assign im_early_pipe_operand_c_q[0]        = operand_c;
  assign im_early_pipe_operand_d_q[0]        = operand_d;
  assign im_early_pipe_operand_e_q[0]        = operand_e;
  assign im_early_pipe_operand_a_vsum_q[0]   = operand_a_vsum;
  assign im_early_pipe_operand_c_vsum_q[0]   = operand_c_vsum;
  assign im_early_pipe_info_a_q[0]           = info_a;
  assign im_early_pipe_info_b_q[0]           = info_b;
  assign im_early_pipe_info_c_q[0]           = info_c;
  assign im_early_pipe_info_d_q[0]           = info_d;
  assign im_early_pipe_info_e_q[0]           = info_e;
  assign im_early_pipe_addend_shamt_q[0]     = addend_shamt;
  assign im_early_pipe_eff_sub_first_q[0]    = effective_subtraction_first;
  assign im_early_pipe_tentative_sign_q[0]   = tentative_sign;
  assign im_early_pipe_tent_exp_q[0]         = tentative_exponent;
  assign im_early_pipe_exp_int_q[0]          = exponent_int;
  assign im_early_pipe_exp_min_q[0]          = exponent_min;
  assign im_early_pipe_info_min_is_zero_q[0] = info_min_is_zero;
  assign im_early_pipe_info_int_is_zero_q[0] = info_int_is_zero;
  assign im_early_pipe_info_max_is_zero_q[0] = info_max_is_zero;
  assign im_early_pipe_addend_min_sign_q[0]  = addend_min_sign;
  assign im_early_pipe_addend_int_sign_q[0]  = addend_int_sign;
  assign im_early_pipe_addend_max_sign_q[0]  = addend_max_sign;
  assign im_early_pipe_exp_cmp_q[0]          = exponent_cmp;
  assign im_early_pipe_a_sign_q[0]           = a_sign;
  assign im_early_pipe_c_sign_q[0]           = c_sign;
  assign im_early_pipe_rnd_mode_q[0]         = inp_pipe_rnd_mode_q[NUM_INP_REGS];
  assign im_early_pipe_dst_fmt_q[0]          = dst_fmt_q;
  assign im_early_pipe_special_result_q[0]   = special_result;
  assign im_early_pipe_special_status_q[0]   = special_status;
  assign im_early_pipe_res_is_spec_q[0]      = result_is_special;
  assign im_early_pipe_tag_q[0]              = inp_pipe_tag_q[NUM_INP_REGS];
  assign im_early_pipe_mask_q[0]             = inp_pipe_mask_q[NUM_INP_REGS];
  assign im_early_pipe_aux_q[0]              = inp_pipe_aux_q[NUM_INP_REGS];
  assign im_early_pipe_valid_q[0]            = inp_pipe_valid_q[NUM_INP_REGS];
  // Input stage: Propagate pipeline ready signal to input pipe
  assign inp_pipe_ready[NUM_INP_REGS]        = im_early_pipe_ready[0];

  // Generate the register stages
  for (genvar i = 0; i < NUM_IM_EARLY_REGS; i++) begin : gen_input_mid_early_pipeline
    // Internal register enable for this stage
    logic reg_ena;
    // Determine the ready signal of the current stage - advance the pipeline:
    // 1. if the next stage is ready for our data
    // 2. if the next stage only holds a bubble (not valid) -> we can pop it
    assign im_early_pipe_ready[i] = im_early_pipe_ready[i+1] | ~im_early_pipe_valid_q[i+1];
    // Valid: enabled by ready signal, synchronous clear with the flush signal
    `FFLARNC(im_early_pipe_valid_q[i+1], im_early_pipe_valid_q[i], reg_enable_i, flush_i, 1'b0, clk_i, rst_ni)
    // Enable register if pipeline ready and a valid data item is present
    assign reg_ena = reg_enable_i;
    // Generate the pipeline registers within the stages, use enable-registers
    `FFL(im_early_pipe_op_q[i+1],               im_early_pipe_op_q[i],               reg_ena, fpnew_pkg::operation_e'('0))
    `FFL(im_early_pipe_operand_a_q[i+1],        im_early_pipe_operand_a_q[i],        reg_ena, '0)
    `FFL(im_early_pipe_operand_b_q[i+1],        im_early_pipe_operand_b_q[i],        reg_ena, '0)
    `FFL(im_early_pipe_operand_c_q[i+1],        im_early_pipe_operand_c_q[i],        reg_ena, '0)
    `FFL(im_early_pipe_operand_d_q[i+1],        im_early_pipe_operand_d_q[i],        reg_ena, '0)
    `FFL(im_early_pipe_operand_e_q[i+1],        im_early_pipe_operand_e_q[i],        reg_ena, '0)
    `FFL(im_early_pipe_operand_a_vsum_q[i+1],   im_early_pipe_operand_a_vsum_q[i],   reg_ena, '0)
    `FFL(im_early_pipe_operand_c_vsum_q[i+1],   im_early_pipe_operand_c_vsum_q[i],   reg_ena, '0)
    `FFL(im_early_pipe_info_a_q[i+1],           im_early_pipe_info_a_q[i],           reg_ena, fpnew_pkg::fp_info_t'('0))
    `FFL(im_early_pipe_info_b_q[i+1],           im_early_pipe_info_b_q[i],           reg_ena, fpnew_pkg::fp_info_t'('0))
    `FFL(im_early_pipe_info_c_q[i+1],           im_early_pipe_info_c_q[i],           reg_ena, fpnew_pkg::fp_info_t'('0))
    `FFL(im_early_pipe_info_d_q[i+1],           im_early_pipe_info_d_q[i],           reg_ena, fpnew_pkg::fp_info_t'('0))
    `FFL(im_early_pipe_info_e_q[i+1],           im_early_pipe_info_e_q[i],           reg_ena, fpnew_pkg::fp_info_t'('0))
    `FFL(im_early_pipe_addend_shamt_q[i+1],     im_early_pipe_addend_shamt_q[i],     reg_ena, '0)
    `FFL(im_early_pipe_eff_sub_first_q[i+1],    im_early_pipe_eff_sub_first_q[i],    reg_ena, 1'b0)
    `FFL(im_early_pipe_tentative_sign_q[i+1],   im_early_pipe_tentative_sign_q[i],   reg_ena, 1'b0)
    `FFL(im_early_pipe_tent_exp_q[i+1],         im_early_pipe_tent_exp_q[i],         reg_ena, '0)
    `FFL(im_early_pipe_exp_int_q[i+1],          im_early_pipe_exp_int_q[i],          reg_ena, '0)
    `FFL(im_early_pipe_exp_min_q[i+1],          im_early_pipe_exp_min_q[i],          reg_ena, '0)
    `FFL(im_early_pipe_info_min_is_zero_q[i+1], im_early_pipe_info_min_is_zero_q[i], reg_ena, 1'b0)
    `FFL(im_early_pipe_info_int_is_zero_q[i+1], im_early_pipe_info_int_is_zero_q[i], reg_ena, 1'b0)
    `FFL(im_early_pipe_info_max_is_zero_q[i+1], im_early_pipe_info_max_is_zero_q[i], reg_ena, 1'b0)
    `FFL(im_early_pipe_addend_min_sign_q[i+1],  im_early_pipe_addend_min_sign_q[i],  reg_ena, 1'b0)
    `FFL(im_early_pipe_addend_int_sign_q[i+1],  im_early_pipe_addend_int_sign_q[i],  reg_ena, 1'b0)
    `FFL(im_early_pipe_addend_max_sign_q[i+1],  im_early_pipe_addend_max_sign_q[i],  reg_ena, 1'b0)
    `FFL(im_early_pipe_exp_cmp_q[i+1],          im_early_pipe_exp_cmp_q[i],          reg_ena, '0)
    `FFL(im_early_pipe_a_sign_q[i+1],           im_early_pipe_a_sign_q[i],           reg_ena, 1'b0)
    `FFL(im_early_pipe_c_sign_q[i+1],           im_early_pipe_c_sign_q[i],           reg_ena, 1'b0)
    `FFL(im_early_pipe_rnd_mode_q[i+1],         im_early_pipe_rnd_mode_q[i],         reg_ena, fpnew_pkg::RNE)
    `FFL(im_early_pipe_dst_fmt_q[i+1],          im_early_pipe_dst_fmt_q[i],          reg_ena, fpnew_pkg::fp_format_e'('0))
    `FFL(im_early_pipe_special_result_q[i+1],   im_early_pipe_special_result_q[i],   reg_ena, '0)
    `FFL(im_early_pipe_special_status_q[i+1],   im_early_pipe_special_status_q[i],   reg_ena, fpnew_pkg::status_t'('0))
    `FFL(im_early_pipe_res_is_spec_q[i+1],      im_early_pipe_res_is_spec_q[i],      reg_ena, 1'b0)
    `FFL(im_early_pipe_tag_q[i+1],              im_early_pipe_tag_q[i],              reg_ena, TagType'('0))
    `FFL(im_early_pipe_mask_q[i+1],             im_early_pipe_mask_q[i],             reg_ena, '0)
    `FFL(im_early_pipe_aux_q[i+1],              im_early_pipe_aux_q[i],              reg_ena, AuxType'('0))
  end
  // Output stage: assign selected pipe outputs to signals for later use
  assign operand_a_q2 = im_early_pipe_operand_a_q[NUM_IM_EARLY_REGS];
  assign operand_b_q2 = im_early_pipe_operand_b_q[NUM_IM_EARLY_REGS];
  assign operand_c_q2 = im_early_pipe_operand_c_q[NUM_IM_EARLY_REGS];
  assign operand_d_q2 = im_early_pipe_operand_d_q[NUM_IM_EARLY_REGS];
  assign info_a_q2    = im_early_pipe_info_a_q[NUM_IM_EARLY_REGS];
  assign info_b_q2    = im_early_pipe_info_b_q[NUM_IM_EARLY_REGS];
  assign info_c_q2    = im_early_pipe_info_c_q[NUM_IM_EARLY_REGS];
  assign info_d_q2    = im_early_pipe_info_d_q[NUM_IM_EARLY_REGS];

  // ------------------
  // Product data path
  // ------------------
  logic   [2*PRECISION_BITS-1:0] product_x, product_y;  // the p*p product is 2p-bit wide

  opope_sdotp_product #(
    .SrcDotpFpFmtConfig ( SrcDotpFpFmtConfig )
  ) i_fpnew_sdotp_product (
    .operand_a_i ( operand_a_q2 ),
    .operand_b_i ( operand_b_q2 ),
    .operand_c_i ( operand_c_q2 ),
    .operand_d_i ( operand_d_q2 ),
    .info_a_i    ( info_a_q2    ),
    .info_b_i    ( info_b_q2    ),
    .info_c_i    ( info_c_q2    ),
    .info_d_i    ( info_d_q2    ),
    .product_x_o ( product_x    ),
    .product_y_o ( product_y    )
  );

  // -------------------------
  // INP to MID LATE pipeline
  // -------------------------
  // Pipeline output signals as non-arrays
  logic   [2*PRECISION_BITS-1:0]   product_x_q, product_y_q;
  logic   [DST_PRECISION_BITS-1:0] mantissa_e;
  logic   [DST_PRECISION_BITS-1:0] mantissa_a_vsum, mantissa_c_vsum;
  logic   [SHIFT_AMOUNT_WIDTH-1:0] addend_shamt_q;
  logic                            effective_subtraction_first_q;
  logic                            tentative_sign_q;
  logic signed [DST_EXP_WIDTH-1:0] tentative_exponent_q;
  logic signed [DST_EXP_WIDTH-1:0] exponent_int_q;
  logic signed [DST_EXP_WIDTH-1:0] exponent_min_q;
  logic                            info_min_is_zero_q;
  logic                            info_int_is_zero_q;
  logic                            info_max_is_zero_q;
  logic                            addend_min_sign_q;
  logic                            addend_int_sign_q;
  logic                            addend_max_sign_q;
  logic [2:0]                      exponent_cmp_q;
  fpnew_pkg::roundmode_e           rnd_mode_q;

  // Internal pipeline signals, index i holds signal after i register stages
  logic                  [0:NUM_IM_LATE_REGS][2*PRECISION_BITS-1:0]   im_late_pipe_product_x_q;
  logic                  [0:NUM_IM_LATE_REGS][2*PRECISION_BITS-1:0]   im_late_pipe_product_y_q;
  fpnew_pkg::operation_e [0:NUM_IM_LATE_REGS]                         im_late_pipe_op_q;
  fp_src_t               [0:NUM_IM_LATE_REGS]                         im_late_pipe_operand_b_q;
  fp_src_t               [0:NUM_IM_LATE_REGS]                         im_late_pipe_operand_d_q;
  fp_dst_t               [0:NUM_IM_LATE_REGS]                         im_late_pipe_operand_e_q;
  fp_dst_t               [0:NUM_IM_LATE_REGS]                         im_late_pipe_operand_a_vsum_q;
  fp_dst_t               [0:NUM_IM_LATE_REGS]                         im_late_pipe_operand_c_vsum_q;
  fpnew_pkg::fp_info_t   [0:NUM_IM_LATE_REGS]                         im_late_pipe_info_a_q;
  fpnew_pkg::fp_info_t   [0:NUM_IM_LATE_REGS]                         im_late_pipe_info_c_q;
  fpnew_pkg::fp_info_t   [0:NUM_IM_LATE_REGS]                         im_late_pipe_info_e_q;
  logic                  [0:NUM_IM_LATE_REGS][SHIFT_AMOUNT_WIDTH-1:0] im_late_pipe_addend_shamt_q;
  logic                  [0:NUM_IM_LATE_REGS]                         im_late_pipe_eff_sub_first_q;
  logic                  [0:NUM_IM_LATE_REGS]                         im_late_pipe_tentative_sign_q;
  logic signed           [0:NUM_IM_LATE_REGS][DST_EXP_WIDTH-1:0]      im_late_pipe_tent_exp_q;
  logic signed           [0:NUM_IM_LATE_REGS][DST_EXP_WIDTH-1:0]      im_late_pipe_exp_int_q;
  logic signed           [0:NUM_IM_LATE_REGS][DST_EXP_WIDTH-1:0]      im_late_pipe_exp_min_q;
  logic                  [0:NUM_IM_LATE_REGS]                         im_late_pipe_info_min_is_zero_q;
  logic                  [0:NUM_IM_LATE_REGS]                         im_late_pipe_info_int_is_zero_q;
  logic                  [0:NUM_IM_LATE_REGS]                         im_late_pipe_info_max_is_zero_q;
  logic                  [0:NUM_IM_LATE_REGS]                         im_late_pipe_addend_min_sign_q;
  logic                  [0:NUM_IM_LATE_REGS]                         im_late_pipe_addend_int_sign_q;
  logic                  [0:NUM_IM_LATE_REGS]                         im_late_pipe_addend_max_sign_q;
  logic                  [0:NUM_IM_LATE_REGS][2:0]                    im_late_pipe_exp_cmp_q;
  logic                  [0:NUM_IM_LATE_REGS]                         im_late_pipe_a_sign_q;
  logic                  [0:NUM_IM_LATE_REGS]                         im_late_pipe_c_sign_q;
  fpnew_pkg::roundmode_e [0:NUM_IM_LATE_REGS]                         im_late_pipe_rnd_mode_q;
  fpnew_pkg::fp_format_e [0:NUM_IM_LATE_REGS]                         im_late_pipe_dst_fmt_q;
  logic                  [0:NUM_IM_LATE_REGS][DST_WIDTH-1:0]          im_late_pipe_special_result_q;
  fpnew_pkg::status_t    [0:NUM_IM_LATE_REGS]                         im_late_pipe_special_status_q;
  logic                  [0:NUM_IM_LATE_REGS]                         im_late_pipe_res_is_spec_q;
  TagType                [0:NUM_IM_LATE_REGS]                         im_late_pipe_tag_q;
  logic                  [0:NUM_IM_LATE_REGS]                         im_late_pipe_mask_q;
  AuxType                [0:NUM_IM_LATE_REGS]                         im_late_pipe_aux_q;
  logic                  [0:NUM_IM_LATE_REGS]                         im_late_pipe_valid_q;
  // Ready signal is combinatorial for all stages
  logic                  [0:NUM_IM_LATE_REGS]                         im_late_pipe_ready;

  // Input stage: First element of pipeline is taken from upstream logic
  assign im_late_pipe_product_x_q[0]            = product_x;
  assign im_late_pipe_product_y_q[0]            = product_y;
  assign im_late_pipe_op_q[0]                   = im_early_pipe_op_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_operand_b_q[0]            = im_early_pipe_operand_b_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_operand_d_q[0]            = im_early_pipe_operand_d_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_operand_e_q[0]            = im_early_pipe_operand_e_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_operand_a_vsum_q[0]       = im_early_pipe_operand_a_vsum_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_operand_c_vsum_q[0]       = im_early_pipe_operand_c_vsum_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_info_a_q[0]               = im_early_pipe_info_a_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_info_c_q[0]               = im_early_pipe_info_c_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_info_e_q[0]               = im_early_pipe_info_e_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_addend_shamt_q[0]         = im_early_pipe_addend_shamt_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_eff_sub_first_q[0]        = im_early_pipe_eff_sub_first_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_tentative_sign_q[0]       = im_early_pipe_tentative_sign_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_tent_exp_q[0]             = im_early_pipe_tent_exp_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_exp_int_q[0]              = im_early_pipe_exp_int_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_exp_min_q[0]              = im_early_pipe_exp_min_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_info_min_is_zero_q[0]     = im_early_pipe_info_min_is_zero_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_info_int_is_zero_q[0]     = im_early_pipe_info_int_is_zero_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_info_max_is_zero_q[0]     = im_early_pipe_info_max_is_zero_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_addend_min_sign_q[0]      = im_early_pipe_addend_min_sign_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_addend_int_sign_q[0]      = im_early_pipe_addend_int_sign_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_addend_max_sign_q[0]      = im_early_pipe_addend_max_sign_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_exp_cmp_q[0]              = im_early_pipe_exp_cmp_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_a_sign_q[0]               = im_early_pipe_a_sign_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_c_sign_q[0]               = im_early_pipe_c_sign_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_rnd_mode_q[0]             = im_early_pipe_rnd_mode_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_dst_fmt_q[0]              = im_early_pipe_dst_fmt_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_special_result_q[0]       = im_early_pipe_special_result_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_special_status_q[0]       = im_early_pipe_special_status_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_res_is_spec_q[0]          = im_early_pipe_res_is_spec_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_tag_q[0]                  = im_early_pipe_tag_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_mask_q[0]                 = im_early_pipe_mask_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_aux_q[0]                  = im_early_pipe_aux_q[NUM_IM_EARLY_REGS];
  assign im_late_pipe_valid_q[0]                = im_early_pipe_valid_q[NUM_IM_EARLY_REGS];
  // Input stage: Propagate pipeline ready signal to im_early pipe
  assign im_early_pipe_ready[NUM_IM_EARLY_REGS] = im_late_pipe_ready[0];

  // Generate the register stages
  for (genvar i = 0; i < NUM_IM_LATE_REGS; i++) begin : gen_input_mid_late_pipeline
    // Internal register enable for this stage
    logic reg_ena;
    // Determine the ready signal of the current stage - advance the pipeline:
    // 1. if the next stage is ready for our data
    // 2. if the next stage only holds a bubble (not valid) -> we can pop it
    assign im_late_pipe_ready[i] = im_late_pipe_ready[i+1] | ~im_late_pipe_valid_q[i+1];
    // Valid: enabled by ready signal, synchronous clear with the flush signal
    `FFLARNC(im_late_pipe_valid_q[i+1], im_late_pipe_valid_q[i], reg_enable_i, flush_i, 1'b0, clk_i, rst_ni)
    // Enable register if pipeline ready and a valid data item is present
    assign reg_ena = reg_enable_i;
    // Generate the pipeline registers within the stages, use enable-registers
    `FFL(im_late_pipe_product_x_q[i+1],        im_late_pipe_product_x_q[i],        reg_ena, '0)
    `FFL(im_late_pipe_product_y_q[i+1],        im_late_pipe_product_y_q[i],        reg_ena, '0)
    `FFL(im_late_pipe_op_q[i+1],               im_late_pipe_op_q[i],               reg_ena, fpnew_pkg::operation_e'('0))
    `FFL(im_late_pipe_operand_b_q[i+1],        im_late_pipe_operand_b_q[i],        reg_ena, '0)
    `FFL(im_late_pipe_operand_d_q[i+1],        im_late_pipe_operand_d_q[i],        reg_ena, '0)
    `FFL(im_late_pipe_operand_e_q[i+1],        im_late_pipe_operand_e_q[i],        reg_ena, '0)
    `FFL(im_late_pipe_operand_a_vsum_q[i+1],   im_late_pipe_operand_a_vsum_q[i],   reg_ena, '0)
    `FFL(im_late_pipe_operand_c_vsum_q[i+1],   im_late_pipe_operand_c_vsum_q[i],   reg_ena, '0)
    `FFL(im_late_pipe_info_a_q[i+1],           im_late_pipe_info_a_q[i],           reg_ena, fpnew_pkg::fp_info_t'('0))
    `FFL(im_late_pipe_info_c_q[i+1],           im_late_pipe_info_c_q[i],           reg_ena, fpnew_pkg::fp_info_t'('0))
    `FFL(im_late_pipe_info_e_q[i+1],           im_late_pipe_info_e_q[i],           reg_ena, fpnew_pkg::fp_info_t'('0))
    `FFL(im_late_pipe_addend_shamt_q[i+1],     im_late_pipe_addend_shamt_q[i],     reg_ena, '0)
    `FFL(im_late_pipe_eff_sub_first_q[i+1],    im_late_pipe_eff_sub_first_q[i],    reg_ena, 1'b0)
    `FFL(im_late_pipe_tentative_sign_q[i+1],   im_late_pipe_tentative_sign_q[i],   reg_ena, 1'b0)
    `FFL(im_late_pipe_tent_exp_q[i+1],         im_late_pipe_tent_exp_q[i],         reg_ena, '0)
    `FFL(im_late_pipe_exp_int_q[i+1],          im_late_pipe_exp_int_q[i],          reg_ena, '0)
    `FFL(im_late_pipe_exp_min_q[i+1],          im_late_pipe_exp_min_q[i],          reg_ena, '0)
    `FFL(im_late_pipe_info_min_is_zero_q[i+1], im_late_pipe_info_min_is_zero_q[i], reg_ena, 1'b0)
    `FFL(im_late_pipe_info_int_is_zero_q[i+1], im_late_pipe_info_int_is_zero_q[i], reg_ena, 1'b0)
    `FFL(im_late_pipe_info_max_is_zero_q[i+1], im_late_pipe_info_max_is_zero_q[i], reg_ena, 1'b0)
    `FFL(im_late_pipe_addend_min_sign_q[i+1],  im_late_pipe_addend_min_sign_q[i],  reg_ena, 1'b0)
    `FFL(im_late_pipe_addend_int_sign_q[i+1],  im_late_pipe_addend_int_sign_q[i],  reg_ena, 1'b0)
    `FFL(im_late_pipe_addend_max_sign_q[i+1],  im_late_pipe_addend_max_sign_q[i],  reg_ena, 1'b0)
    `FFL(im_late_pipe_exp_cmp_q[i+1],          im_late_pipe_exp_cmp_q[i],          reg_ena, '0)
    `FFL(im_late_pipe_a_sign_q[i+1],           im_late_pipe_a_sign_q[i],           reg_ena, 1'b0)
    `FFL(im_late_pipe_c_sign_q[i+1],           im_late_pipe_c_sign_q[i],           reg_ena, 1'b0)
    `FFL(im_late_pipe_rnd_mode_q[i+1],         im_late_pipe_rnd_mode_q[i],         reg_ena, fpnew_pkg::RNE)
    `FFL(im_late_pipe_dst_fmt_q[i+1],          im_late_pipe_dst_fmt_q[i],          reg_ena, fpnew_pkg::fp_format_e'('0))
    `FFL(im_late_pipe_special_result_q[i+1],   im_late_pipe_special_result_q[i],   reg_ena, '0)
    `FFL(im_late_pipe_special_status_q[i+1],   im_late_pipe_special_status_q[i],   reg_ena, fpnew_pkg::status_t'('0))
    `FFL(im_late_pipe_res_is_spec_q[i+1],      im_late_pipe_res_is_spec_q[i],      reg_ena, 1'b0)
    `FFL(im_late_pipe_tag_q[i+1],              im_late_pipe_tag_q[i],              reg_ena, TagType'('0))
    `FFL(im_late_pipe_mask_q[i+1],             im_late_pipe_mask_q[i],             reg_ena, '0)
    `FFL(im_late_pipe_aux_q[i+1],              im_late_pipe_aux_q[i],              reg_ena, AuxType'('0))
  end

  // Output stage: assign selected pipe outputs to signals for later use
  assign product_x_q                   = im_late_pipe_product_x_q[NUM_IM_LATE_REGS];
  assign product_y_q                   = im_late_pipe_product_y_q[NUM_IM_LATE_REGS];
  assign mantissa_e                    = {im_late_pipe_info_e_q[NUM_IM_LATE_REGS].is_normal,
                                          im_late_pipe_operand_e_q[NUM_IM_LATE_REGS].mantissa};
  assign mantissa_a_vsum               = {im_late_pipe_info_a_q[NUM_IM_LATE_REGS].is_normal,
                                          im_late_pipe_operand_a_vsum_q[NUM_IM_LATE_REGS].mantissa};
  assign mantissa_c_vsum               = {im_late_pipe_info_c_q[NUM_IM_LATE_REGS].is_normal,
                                          im_late_pipe_operand_c_vsum_q[NUM_IM_LATE_REGS].mantissa};
  assign addend_shamt_q                = im_late_pipe_addend_shamt_q[NUM_IM_LATE_REGS];
  assign effective_subtraction_first_q = im_late_pipe_eff_sub_first_q[NUM_IM_LATE_REGS];
  assign tentative_sign_q              = im_late_pipe_tentative_sign_q[NUM_IM_LATE_REGS];
  assign tentative_exponent_q          = im_late_pipe_tent_exp_q[NUM_IM_LATE_REGS];
  assign exponent_int_q                = im_late_pipe_exp_int_q[NUM_IM_LATE_REGS];
  assign exponent_min_q                = im_late_pipe_exp_min_q[NUM_IM_LATE_REGS];
  assign info_min_is_zero_q            = im_late_pipe_info_min_is_zero_q[NUM_IM_LATE_REGS];
  assign info_int_is_zero_q            = im_late_pipe_info_int_is_zero_q[NUM_IM_LATE_REGS];
  assign info_max_is_zero_q            = im_late_pipe_info_max_is_zero_q[NUM_IM_LATE_REGS];
  assign addend_min_sign_q             = im_late_pipe_addend_min_sign_q[NUM_IM_LATE_REGS];
  assign addend_int_sign_q             = im_late_pipe_addend_int_sign_q[NUM_IM_LATE_REGS];
  assign addend_max_sign_q             = im_late_pipe_addend_max_sign_q[NUM_IM_LATE_REGS];
  assign exponent_cmp_q                = im_late_pipe_exp_cmp_q[NUM_IM_LATE_REGS];
  assign rnd_mode_q                    = im_late_pipe_rnd_mode_q[NUM_IM_LATE_REGS];

  // ------------------
  // Shift data path + first adder + second shift
  // ------------------
  logic                                           sticky_before_add;
  logic [2*DST_PRECISION_BITS+2:0]                sum;
  logic                                           sum_carry;
  logic                                           final_sign;
  logic                                           bypass_w;
  logic signed [DST_EXP_WIDTH-1:0]                exponent_w;
  logic [2*DST_PRECISION_BITS+PRECISION_BITS+3:0] addend_min_after_shift;
  logic                                           sticky_before_add_z;
  logic                                           final_sign_zero;

  opope_sdotp_first_shift_add #(
    .SrcDotpFpFmtConfig ( SrcDotpFpFmtConfig ),
    .DstDotpFpFmtConfig ( DstDotpFpFmtConfig )
  ) i_fpnew_sdotp_first_shift_add (
    .op_i                          ( im_late_pipe_op_q[NUM_IM_LATE_REGS] ),
    .product_x_i                   ( product_x_q                    ),
    .product_y_i                   ( product_y_q                    ),
    .mantissa_e_i                  ( mantissa_e                     ),
    .mantissa_a_vsum_i             ( mantissa_a_vsum                ),
    .mantissa_c_vsum_i             ( mantissa_c_vsum                ),
    .addend_shamt_i                ( addend_shamt_q                 ),
    .effective_subtraction_first_i ( effective_subtraction_first_q  ),
    .tentative_sign_i              ( tentative_sign_q               ),
    .tentative_exponent_i          ( tentative_exponent_q           ),
    .exponent_int_i                ( exponent_int_q                 ),
    .exponent_min_i                ( exponent_min_q                 ),
    .info_min_is_zero_i            ( info_min_is_zero_q             ),
    .info_int_is_zero_i            ( info_int_is_zero_q             ),
    .info_max_is_zero_i            ( info_max_is_zero_q             ),
    .addend_min_sign_i             ( addend_min_sign_q              ),
    .addend_int_sign_i             ( addend_int_sign_q              ),
    .addend_max_sign_i             ( addend_max_sign_q              ),
    .exponent_cmp_i                ( exponent_cmp_q                 ),
    .rnd_mode_i                    ( rnd_mode_q                     ),
    .sticky_before_add_o           ( sticky_before_add              ),
    .sum_o                         ( sum                            ),
    .sum_carry_o                   ( sum_carry                      ),
    .final_sign_o                  ( final_sign                     ),
    .bypass_w_o                    ( bypass_w                       ),
    .exponent_w_o                  ( exponent_w                     ),
    .addend_min_after_shift_o      ( addend_min_after_shift         ),
    .sticky_before_add_z_o         ( sticky_before_add_z            ),
    .final_sign_zero_o             ( final_sign_zero                )
  );

  // -----------------
  // Internal pipeline
  // -----------------
  // Pipeline output signals as non-arrays
  logic                                           effective_subtraction_first_q2;
  logic                                           bypass_w_qm;
  logic                                           info_min_is_zero_q2;
  logic                                           addend_min_sign_q2;
  logic signed [DST_EXP_WIDTH-1:0]                exponent_w_q;
  logic                                           sticky_before_add_z_q;   // they are compressed into a single sticky bit
  logic [2*DST_PRECISION_BITS+PRECISION_BITS+3:0] addend_min_after_shift_q;
  logic                                           operand_e_sign_q;
  logic                                           product_x_sign_q;
  logic                                           product_y_sign_q;
  logic [2:0]                                     exponent_cmp_q2;
  logic signed [DST_EXP_WIDTH-1:0]                exponent_min_q2;
  logic [2*DST_PRECISION_BITS+2:0]                sum_q;
  logic                                           final_sign_q;
  logic                                           sum_carry_q;

  // Internal pipeline signals, index i holds signal after i register stages
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_eff_sub_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_final_sign_zero_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_info_min_is_zero_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_info_max_is_zero_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_addend_min_sign_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_bypass_w_q;
  logic signed           [0:NUM_MID_REGS][DST_EXP_WIDTH-1:0]                       mid_pipe_exp_first_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_sticky_before_add_z_q;
  logic                  [0:NUM_MID_REGS][2*DST_PRECISION_BITS+PRECISION_BITS+3:0] mid_pipe_add_min_after_shift_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_op_e_sign_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_prod_x_sign_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_prod_y_sign_q;
  logic                  [0:NUM_MID_REGS][2:0]                                     mid_pipe_exp_cmp_q;
  logic signed           [0:NUM_MID_REGS][DST_EXP_WIDTH-1:0]                       mid_pipe_exp_min_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_sticky_q;
  logic                  [0:NUM_MID_REGS][2*DST_PRECISION_BITS+2:0]                mid_pipe_sum_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_final_sign_q;
  fpnew_pkg::fp_format_e [0:NUM_MID_REGS]                                          mid_pipe_dst_fmt_q;
  fpnew_pkg::roundmode_e [0:NUM_MID_REGS]                                          mid_pipe_rnd_mode_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_res_is_spec_q;
  fp_dst_t               [0:NUM_MID_REGS]                                          mid_pipe_spec_res_q;
  fpnew_pkg::status_t    [0:NUM_MID_REGS]                                          mid_pipe_spec_stat_q;
  TagType                [0:NUM_MID_REGS]                                          mid_pipe_tag_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_mask_q;
  AuxType                [0:NUM_MID_REGS]                                          mid_pipe_aux_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_valid_q;
  logic                  [0:NUM_MID_REGS]                                          mid_pipe_sum_carry_q;
  // Ready signal is combinatorial for all stages
  logic [0:NUM_MID_REGS] mid_pipe_ready;

  // Input stage: First element of pipeline is taken from upstream logic
  assign mid_pipe_eff_sub_q[0]                = im_late_pipe_eff_sub_first_q[NUM_IM_LATE_REGS];
  assign mid_pipe_final_sign_zero_q[0]        = final_sign_zero;
  assign mid_pipe_info_min_is_zero_q[0]       = im_late_pipe_info_min_is_zero_q[NUM_IM_LATE_REGS];
  assign mid_pipe_info_max_is_zero_q[0]       = im_late_pipe_info_max_is_zero_q[NUM_IM_LATE_REGS];
  assign mid_pipe_addend_min_sign_q[0]        = im_late_pipe_addend_min_sign_q[NUM_IM_LATE_REGS];
  assign mid_pipe_bypass_w_q[0]               = bypass_w;
  assign mid_pipe_exp_first_q[0]              = exponent_w;
  assign mid_pipe_sticky_before_add_z_q[0]    = sticky_before_add_z;
  assign mid_pipe_add_min_after_shift_q[0]    = addend_min_after_shift;
  assign mid_pipe_op_e_sign_q[0]              = im_late_pipe_operand_e_q[NUM_IM_LATE_REGS].sign;
  assign mid_pipe_prod_x_sign_q[0]            = im_late_pipe_a_sign_q[NUM_IM_LATE_REGS] ^ im_late_pipe_operand_b_q[NUM_IM_LATE_REGS].sign;
  assign mid_pipe_prod_y_sign_q[0]            = im_late_pipe_c_sign_q[NUM_IM_LATE_REGS] ^ im_late_pipe_operand_d_q[NUM_IM_LATE_REGS].sign;
  assign mid_pipe_exp_cmp_q[0]                = im_late_pipe_exp_cmp_q[NUM_IM_LATE_REGS];
  assign mid_pipe_exp_min_q[0]                = im_late_pipe_exp_min_q[NUM_IM_LATE_REGS];
  assign mid_pipe_sticky_q[0]                 = sticky_before_add;
  assign mid_pipe_sum_q[0]                    = sum;
  assign mid_pipe_final_sign_q[0]             = final_sign;
  assign mid_pipe_rnd_mode_q[0]               = im_late_pipe_rnd_mode_q[NUM_IM_LATE_REGS];
  assign mid_pipe_dst_fmt_q[0]                = im_late_pipe_dst_fmt_q[NUM_IM_LATE_REGS];
  assign mid_pipe_res_is_spec_q[0]            = im_late_pipe_res_is_spec_q[NUM_IM_LATE_REGS];
  assign mid_pipe_spec_res_q[0]               = im_late_pipe_special_result_q[NUM_IM_LATE_REGS];
  assign mid_pipe_spec_stat_q[0]              = im_late_pipe_special_status_q[NUM_IM_LATE_REGS];
  assign mid_pipe_tag_q[0]                    = im_late_pipe_tag_q[NUM_IM_LATE_REGS];
  assign mid_pipe_mask_q[0]                   = im_late_pipe_mask_q[NUM_IM_LATE_REGS];
  assign mid_pipe_aux_q[0]                    = im_late_pipe_aux_q[NUM_IM_LATE_REGS];
  assign mid_pipe_valid_q[0]                  = im_late_pipe_valid_q[NUM_IM_LATE_REGS];
  assign mid_pipe_sum_carry_q[0]              = sum_carry;
  // Input stage: Propagate pipeline ready signal to im_late pipe
  assign im_late_pipe_ready[NUM_IM_LATE_REGS] = mid_pipe_ready[0];

  // Generate the register stages
  for (genvar i = 0; i < NUM_MID_REGS; i++) begin : gen_mid_pipeline
    // Internal register enable for this stage
    logic reg_ena;
    // Determine the ready signal of the current stage - advance the pipeline:
    // 1. if the next stage is ready for our data
    // 2. if the next stage only holds a bubble (not valid) -> we can pop it
    assign mid_pipe_ready[i] = mid_pipe_ready[i+1] | ~mid_pipe_valid_q[i+1];
    // Valid: enabled by ready signal, synchronous clear with the flush signal
    `FFLARNC(mid_pipe_valid_q[i+1], mid_pipe_valid_q[i], reg_enable_i, flush_i, 1'b0, clk_i, rst_ni)
    // Enable register if pipleine ready and a valid data item is present
    assign reg_ena = reg_enable_i;
    // Generate the pipeline registers within the stages, use enable-registers
    `FFL(mid_pipe_eff_sub_q[i+1],             mid_pipe_eff_sub_q[i],             reg_ena, '0)
    `FFL(mid_pipe_final_sign_zero_q[i+1],     mid_pipe_final_sign_zero_q[i],     reg_ena, '0)
    `FFL(mid_pipe_info_min_is_zero_q[i+1],    mid_pipe_info_min_is_zero_q[i],    reg_ena, '0)
    `FFL(mid_pipe_info_max_is_zero_q[i+1],    mid_pipe_info_max_is_zero_q[i],    reg_ena, '0)
    `FFL(mid_pipe_addend_min_sign_q[i+1],     mid_pipe_addend_min_sign_q[i],     reg_ena, '0)
    `FFL(mid_pipe_bypass_w_q[i+1],            mid_pipe_bypass_w_q[i],            reg_ena, '0)
    `FFL(mid_pipe_exp_first_q[i+1],           mid_pipe_exp_first_q[i],           reg_ena, '0)
    `FFL(mid_pipe_sticky_before_add_z_q[i+1], mid_pipe_sticky_before_add_z_q[i], reg_ena, '0)
    `FFL(mid_pipe_add_min_after_shift_q[i+1], mid_pipe_add_min_after_shift_q[i], reg_ena, '0)
    `FFL(mid_pipe_op_e_sign_q[i+1],           mid_pipe_op_e_sign_q[i],           reg_ena, '0)
    `FFL(mid_pipe_prod_x_sign_q[i+1],         mid_pipe_prod_x_sign_q[i],         reg_ena, '0)
    `FFL(mid_pipe_prod_y_sign_q[i+1],         mid_pipe_prod_y_sign_q[i],         reg_ena, '0)
    `FFL(mid_pipe_exp_cmp_q[i+1],             mid_pipe_exp_cmp_q[i],             reg_ena, '0)
    `FFL(mid_pipe_exp_min_q[i+1],             mid_pipe_exp_min_q[i],             reg_ena, '0)
    `FFL(mid_pipe_sticky_q[i+1],              mid_pipe_sticky_q[i],              reg_ena, '0)
    `FFL(mid_pipe_sum_q[i+1],                 mid_pipe_sum_q[i],                 reg_ena, '0)
    `FFL(mid_pipe_final_sign_q[i+1],          mid_pipe_final_sign_q[i],          reg_ena, '0)
    `FFL(mid_pipe_rnd_mode_q[i+1],            mid_pipe_rnd_mode_q[i],            reg_ena, fpnew_pkg::RNE)
    `FFL(mid_pipe_dst_fmt_q[i+1],             mid_pipe_dst_fmt_q[i],             reg_ena, fpnew_pkg::FP16)
    `FFL(mid_pipe_res_is_spec_q[i+1],         mid_pipe_res_is_spec_q[i],         reg_ena, '0)
    `FFL(mid_pipe_spec_res_q[i+1],            mid_pipe_spec_res_q[i],            reg_ena, '0)
    `FFL(mid_pipe_spec_stat_q[i+1],           mid_pipe_spec_stat_q[i],           reg_ena, '0)
    `FFL(mid_pipe_tag_q[i+1],                 mid_pipe_tag_q[i],                 reg_ena, TagType'('0))
    `FFL(mid_pipe_mask_q[i+1],                mid_pipe_mask_q[i],                reg_ena, '0)
    `FFL(mid_pipe_aux_q[i+1],                 mid_pipe_aux_q[i],                 reg_ena, AuxType'('0))
    `FFL(mid_pipe_sum_carry_q[i+1],           mid_pipe_sum_carry_q[i],           reg_ena, '0)
  end
  // Output stage: assign selected pipe outputs to signals for later use
  assign sum_carry_q                    = mid_pipe_sum_carry_q[NUM_MID_REGS];
  assign bypass_w_qm                    = mid_pipe_bypass_w_q[NUM_MID_REGS];
  assign info_min_is_zero_q2            = mid_pipe_info_min_is_zero_q[NUM_MID_REGS];
  assign addend_min_sign_q2             = mid_pipe_addend_min_sign_q[NUM_MID_REGS];
  assign effective_subtraction_first_q2 = mid_pipe_eff_sub_q[NUM_MID_REGS];
  assign exponent_w_q                   = mid_pipe_exp_first_q[NUM_MID_REGS];
  assign sticky_before_add_z_q          = mid_pipe_sticky_before_add_z_q[NUM_MID_REGS];
  assign addend_min_after_shift_q       = mid_pipe_add_min_after_shift_q[NUM_MID_REGS];
  assign operand_e_sign_q               = mid_pipe_op_e_sign_q[NUM_MID_REGS];
  assign product_x_sign_q               = mid_pipe_prod_x_sign_q[NUM_MID_REGS];
  assign product_y_sign_q               = mid_pipe_prod_y_sign_q[NUM_MID_REGS];
  assign exponent_cmp_q2                = mid_pipe_exp_cmp_q[NUM_MID_REGS];
  assign exponent_min_q2                = mid_pipe_exp_min_q[NUM_MID_REGS];
  assign sum_q                          = mid_pipe_sum_q[NUM_MID_REGS];
  assign final_sign_q                   = mid_pipe_final_sign_q[NUM_MID_REGS];

  // ----------------------------------
  // Second Step of the Three-way Adder
  // ----------------------------------
  logic                                           effective_subtraction_z;
  logic signed [DST_EXP_WIDTH-1:0]                final_tentative_exponent;
  logic [2*DST_PRECISION_BITS+PRECISION_BITS+3:0] sum_z;
  logic                                           sum_carry_z;
  logic                                           final_sign_z;

  opope_sdotp_three_way_add2 #(
    .SrcDotpFpFmtConfig ( SrcDotpFpFmtConfig ),
    .DstDotpFpFmtConfig ( DstDotpFpFmtConfig )
  ) i_fpnew_sdotp_three_way_add2 (
    .sum_carry_i                   ( sum_carry_q                    ),
    .bypass_w_i                    ( bypass_w_qm                    ),
    .addend_min_sign_i             ( addend_min_sign_q2             ),
    .effective_subtraction_first_i ( effective_subtraction_first_q2 ),
    .exponent_w_i                  ( exponent_w_q                   ),
    .sticky_before_add_z_i         ( sticky_before_add_z_q          ),
    .addend_min_after_shift_i      ( addend_min_after_shift_q       ),
    .operand_e_sign_i              ( operand_e_sign_q               ),
    .product_x_sign_i              ( product_x_sign_q               ),
    .product_y_sign_i              ( product_y_sign_q               ),
    .exponent_cmp_i                ( exponent_cmp_q2                ),
    .exponent_min_i                ( exponent_min_q2                ),
    .sum_i                         ( sum_q                          ),
    .final_sign_i                  ( final_sign_q                   ),
    .final_tentative_exponent_o    ( final_tentative_exponent       ),
    .effective_subtraction_z_o     ( effective_subtraction_z        ),
    .sum_z_o                       ( sum_z                          ),
    .sum_carry_z_o                 ( sum_carry_z                    ),
    .final_sign_z_o                ( final_sign_z                   )
  );

  // --------------------------
  // MID to OUT EARLY pipeline
  // --------------------------
  logic [2*DST_PRECISION_BITS+PRECISION_BITS+3:0] sum_z_q;
  logic                                           sum_carry_z_q;
  logic                                           effective_subtraction_z_q;

  // Internal pipeline signals, index i holds signal after i register stages
  logic                  [0:NUM_MO_EARLY_REGS][2*DST_PRECISION_BITS+PRECISION_BITS+3:0] mo_early_pipe_sum_z_q;
  logic                  [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_sum_carry_z_q;
  logic signed           [0:NUM_MO_EARLY_REGS][DST_EXP_WIDTH-1:0]                       mo_early_pipe_final_tentative_exponent_q;
  logic                  [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_sticky_before_add_z_q;
  logic                  [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_sticky_before_add_q;
  logic                  [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_eff_sub_first_q;
  logic                  [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_eff_sub_z_q;
  logic                  [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_bypass_w_q;
  logic                  [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_info_min_is_zero_q;
  logic                  [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_info_max_is_zero_q;
  logic                  [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_final_sign_zero_q;
  logic                  [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_final_sign_z_q;
  fpnew_pkg::roundmode_e [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_rnd_mode_q;
  fpnew_pkg::fp_format_e [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_dst_fmt_q;
  logic                  [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_res_is_spec_q;
  fp_dst_t               [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_spec_res_q;
  fpnew_pkg::status_t    [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_spec_stat_q;
  TagType                [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_tag_q;
  logic                  [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_mask_q;
  AuxType                [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_aux_q;
  logic                  [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_valid_q;
  // Ready signal is combinatorial for all stages
  logic                  [0:NUM_MO_EARLY_REGS]                                          mo_early_pipe_ready;

  // Input stage: First element of pipeline is taken from upstream logic
  assign mo_early_pipe_sum_z_q[0]                    = sum_z;
  assign mo_early_pipe_sum_carry_z_q[0]              = sum_carry_z;
  assign mo_early_pipe_final_tentative_exponent_q[0] = final_tentative_exponent;
  assign mo_early_pipe_sticky_before_add_z_q[0]      = sticky_before_add_z_q;
  assign mo_early_pipe_sticky_before_add_q[0]        = mid_pipe_sticky_q[NUM_MID_REGS];
  assign mo_early_pipe_eff_sub_first_q[0]            = effective_subtraction_first_q2;
  assign mo_early_pipe_eff_sub_z_q[0]                = effective_subtraction_z;
  assign mo_early_pipe_bypass_w_q[0]                 = bypass_w_qm;
  assign mo_early_pipe_info_min_is_zero_q[0]         = info_min_is_zero_q2;
  assign mo_early_pipe_info_max_is_zero_q[0]         = mid_pipe_info_max_is_zero_q[NUM_MID_REGS];
  assign mo_early_pipe_final_sign_zero_q[0]          = mid_pipe_final_sign_zero_q[NUM_MID_REGS];
  assign mo_early_pipe_final_sign_z_q[0]             = final_sign_z;
  assign mo_early_pipe_rnd_mode_q[0]                 = mid_pipe_rnd_mode_q[NUM_MID_REGS];
  assign mo_early_pipe_dst_fmt_q[0]                  = mid_pipe_dst_fmt_q[NUM_MID_REGS];
  assign mo_early_pipe_res_is_spec_q[0]              = mid_pipe_res_is_spec_q[NUM_MID_REGS];
  assign mo_early_pipe_spec_res_q[0]                 = mid_pipe_spec_res_q[NUM_MID_REGS];
  assign mo_early_pipe_spec_stat_q[0]                = mid_pipe_spec_stat_q[NUM_MID_REGS];
  assign mo_early_pipe_tag_q[0]                      = mid_pipe_tag_q[NUM_MID_REGS];
  assign mo_early_pipe_mask_q[0]                     = mid_pipe_mask_q[NUM_MID_REGS];
  assign mo_early_pipe_aux_q[0]                      = mid_pipe_aux_q[NUM_MID_REGS];
  assign mo_early_pipe_valid_q[0]                    = mid_pipe_valid_q[NUM_MID_REGS];
  // Input stage: Propagate pipeline ready signal to mid pipe
  assign mid_pipe_ready[NUM_MID_REGS]                = mo_early_pipe_ready[0];

  // Generate the register stages
  for (genvar i = 0; i < NUM_MO_EARLY_REGS; i++) begin : gen_mid_out_early_pipeline
    // Internal register enable for this stage
    logic reg_ena;
    // Determine the ready signal of the current stage - advance the pipeline:
    // 1. if the next stage is ready for our data
    // 2. if the next stage only holds a bubble (not valid) -> we can pop it
    assign mo_early_pipe_ready[i] = mo_early_pipe_ready[i+1] | ~mo_early_pipe_valid_q[i+1];
    // Valid: enabled by ready signal, synchronous clear with the flush signal
    `FFLARNC(mo_early_pipe_valid_q[i+1], mo_early_pipe_valid_q[i], reg_enable_i, flush_i, 1'b0, clk_i, rst_ni)
    // Enable register if pipeline ready and a valid data item is present
    assign reg_ena = reg_enable_i;
    // Generate the pipeline registers within the stages, use enable-registers
    `FFL(mo_early_pipe_sum_z_q[i+1],                    mo_early_pipe_sum_z_q[i],                    reg_ena, '0)
    `FFL(mo_early_pipe_sum_carry_z_q[i+1],              mo_early_pipe_sum_carry_z_q[i],              reg_ena, '0)
    `FFL(mo_early_pipe_final_tentative_exponent_q[i+1], mo_early_pipe_final_tentative_exponent_q[i], reg_ena, '0)
    `FFL(mo_early_pipe_sticky_before_add_z_q[i+1],      mo_early_pipe_sticky_before_add_z_q[i],      reg_ena, '0)
    `FFL(mo_early_pipe_sticky_before_add_q[i+1],        mo_early_pipe_sticky_before_add_q[i],        reg_ena, '0)
    `FFL(mo_early_pipe_eff_sub_first_q[i+1],            mo_early_pipe_eff_sub_first_q[i],            reg_ena, '0)
    `FFL(mo_early_pipe_eff_sub_z_q[i+1],                mo_early_pipe_eff_sub_z_q[i],                reg_ena, '0)
    `FFL(mo_early_pipe_bypass_w_q[i+1],                 mo_early_pipe_bypass_w_q[i],                 reg_ena, '0)
    `FFL(mo_early_pipe_info_min_is_zero_q[i+1],         mo_early_pipe_info_min_is_zero_q[i],         reg_ena, '0)
    `FFL(mo_early_pipe_info_max_is_zero_q[i+1],         mo_early_pipe_info_max_is_zero_q[i],         reg_ena, '0)
    `FFL(mo_early_pipe_final_sign_zero_q[i+1],          mo_early_pipe_final_sign_zero_q[i],          reg_ena, '0)
    `FFL(mo_early_pipe_final_sign_z_q[i+1],             mo_early_pipe_final_sign_z_q[i],             reg_ena, '0)
    `FFL(mo_early_pipe_rnd_mode_q[i+1],                 mo_early_pipe_rnd_mode_q[i],                 reg_ena, fpnew_pkg::RNE)
    `FFL(mo_early_pipe_dst_fmt_q[i+1],                  mo_early_pipe_dst_fmt_q[i],                  reg_ena, fpnew_pkg::fp_format_e'(0))
    `FFL(mo_early_pipe_res_is_spec_q[i+1],              mo_early_pipe_res_is_spec_q[i],              reg_ena, '0)
    `FFL(mo_early_pipe_spec_res_q[i+1],                 mo_early_pipe_spec_res_q[i],                 reg_ena, '0)
    `FFL(mo_early_pipe_spec_stat_q[i+1],                mo_early_pipe_spec_stat_q[i],                reg_ena, '0)
    `FFL(mo_early_pipe_tag_q[i+1],                      mo_early_pipe_tag_q[i],                      reg_ena, TagType'('0))
    `FFL(mo_early_pipe_mask_q[i+1],                     mo_early_pipe_mask_q[i],                     reg_ena, '0)
    `FFL(mo_early_pipe_aux_q[i+1],                      mo_early_pipe_aux_q[i],                      reg_ena, AuxType'('0))
  end
  // Output stage: assign selected pipe outputs to signals for later use
  assign sum_z_q                   = mo_early_pipe_sum_z_q[NUM_MO_EARLY_REGS];
  assign sum_carry_z_q             = mo_early_pipe_sum_carry_z_q[NUM_MO_EARLY_REGS];
  assign effective_subtraction_z_q = mo_early_pipe_eff_sub_z_q[NUM_MO_EARLY_REGS];

  // ----------------
  // Normalization 1
  // ----------------
  logic        [LZC_SUM_WIDTH-1:0]    sum_lower;              // LZC_SUM_WIDTH bits of sum are searched
  logic        [LZC_RESULT_WIDTH-1:0] leading_zero_count;     // the number of leading zeroes
  logic                               lzc_zeroes;             // in case only zeroes found

  opope_sdotp_norm_lzc #(
    .SrcDotpFpFmtConfig ( SrcDotpFpFmtConfig ),
    .DstDotpFpFmtConfig ( DstDotpFpFmtConfig )
  ) i_fpnew_sdotp_norm_lzc (
    .sum_z_i                   ( sum_z_q                   ),
    .sum_carry_z_i             ( sum_carry_z_q             ),
    .effective_subtraction_z_i ( effective_subtraction_z_q ),
    .sum_lower_o               ( sum_lower                 ),
    .leading_zero_count_o      ( leading_zero_count        ),
    .lzc_zeroes_o              ( lzc_zeroes                )
  );

  // -------------------------
  // MID to OUT LATE pipeline
  // -------------------------
  logic                            lzc_zeroes_q;
  logic [LZC_RESULT_WIDTH-1:0]     leading_zero_count_q;
  logic signed [DST_EXP_WIDTH-1:0] final_tentative_exponent_q;
  logic [LZC_SUM_WIDTH-1:0]        sum_lower_q;
  logic                            sticky_before_add_z_q2;
  logic                            bypass_w_q;
  logic                            sticky_before_add_q2;
  logic                            effective_subtraction_first_q3;
  logic                            effective_subtraction_z_q2;
  logic                            info_min_is_zero_q3;
  logic                            info_max_is_zero_q2;
  logic                            final_sign_zero_q;
  logic                            final_sign_z_q;
  fpnew_pkg::roundmode_e           rnd_mode_q2;
  fpnew_pkg::fp_format_e           dst_fmt_q2;
  logic                            result_is_special_q;
  fp_dst_t                         special_result_q;
  fpnew_pkg::status_t              special_status_q;

  // Internal pipeline signals, index i holds signal after i register stages
  logic                  [0:NUM_MO_LATE_REGS]                       mo_late_pipe_lzc_zeroes_q;
  logic                  [0:NUM_MO_LATE_REGS][LZC_RESULT_WIDTH-1:0] mo_late_pipe_leading_zero_count_q;
  logic signed           [0:NUM_MO_LATE_REGS][DST_EXP_WIDTH-1:0]    mo_late_pipe_final_tentative_exp_q;
  logic                  [0:NUM_MO_LATE_REGS][LZC_SUM_WIDTH-1:0]    mo_late_pipe_sum_lower_q;
  logic                  [0:NUM_MO_LATE_REGS]                       mo_late_pipe_sticky_before_add_z_q;
  logic                  [0:NUM_MO_LATE_REGS]                       mo_late_pipe_bypass_w_q;
  logic                  [0:NUM_MO_LATE_REGS]                       mo_late_pipe_sticky_before_add_q;
  logic                  [0:NUM_MO_LATE_REGS]                       mo_late_pipe_eff_sub_first_q;
  logic                  [0:NUM_MO_LATE_REGS]                       mo_late_pipe_eff_sub_z_q;
  logic                  [0:NUM_MO_LATE_REGS]                       mo_late_pipe_info_min_is_zero_q;
  logic                  [0:NUM_MO_LATE_REGS]                       mo_late_pipe_info_max_is_zero_q;
  logic                  [0:NUM_MO_LATE_REGS]                       mo_late_pipe_final_sign_zero_q;
  logic                  [0:NUM_MO_LATE_REGS]                       mo_late_pipe_final_sign_z_q;
  fpnew_pkg::roundmode_e [0:NUM_MO_LATE_REGS]                       mo_late_pipe_rnd_mode_q;
  fpnew_pkg::fp_format_e [0:NUM_MO_LATE_REGS]                       mo_late_pipe_dst_fmt_q;
  logic                  [0:NUM_MO_LATE_REGS]                       mo_late_pipe_res_is_spec_q;
  fp_dst_t               [0:NUM_MO_LATE_REGS]                       mo_late_pipe_spec_res_q;
  fpnew_pkg::status_t    [0:NUM_MO_LATE_REGS]                       mo_late_pipe_spec_stat_q;
  TagType                [0:NUM_MO_LATE_REGS]                       mo_late_pipe_tag_q;
  logic                  [0:NUM_MO_LATE_REGS]                       mo_late_pipe_mask_q;
  AuxType                [0:NUM_MO_LATE_REGS]                       mo_late_pipe_aux_q;
  logic                  [0:NUM_MO_LATE_REGS]                       mo_late_pipe_valid_q;
  // Ready signal is combinatorial for all stages
  logic                  [0:NUM_MO_LATE_REGS]                       mo_late_pipe_ready;

  // Input stage: First element of pipeline is taken from upstream logic
  assign mo_late_pipe_lzc_zeroes_q[0]           = lzc_zeroes;
  assign mo_late_pipe_leading_zero_count_q[0]   = leading_zero_count;
  assign mo_late_pipe_final_tentative_exp_q[0]  = mo_early_pipe_final_tentative_exponent_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_sum_lower_q[0]            = sum_lower;
  assign mo_late_pipe_sticky_before_add_z_q[0]  = mo_early_pipe_sticky_before_add_z_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_bypass_w_q[0]             = mo_early_pipe_bypass_w_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_sticky_before_add_q[0]    = mo_early_pipe_sticky_before_add_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_eff_sub_first_q[0]        = mo_early_pipe_eff_sub_first_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_eff_sub_z_q[0]            = mo_early_pipe_eff_sub_z_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_info_min_is_zero_q[0]     = mo_early_pipe_info_min_is_zero_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_info_max_is_zero_q[0]     = mo_early_pipe_info_max_is_zero_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_final_sign_zero_q[0]      = mo_early_pipe_final_sign_zero_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_final_sign_z_q[0]         = mo_early_pipe_final_sign_z_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_rnd_mode_q[0]             = mo_early_pipe_rnd_mode_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_dst_fmt_q[0]              = mo_early_pipe_dst_fmt_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_res_is_spec_q[0]          = mo_early_pipe_res_is_spec_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_spec_res_q[0]             = mo_early_pipe_spec_res_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_spec_stat_q[0]            = mo_early_pipe_spec_stat_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_tag_q[0]                  = mo_early_pipe_tag_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_mask_q[0]                 = mo_early_pipe_mask_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_aux_q[0]                  = mo_early_pipe_aux_q[NUM_MO_EARLY_REGS];
  assign mo_late_pipe_valid_q[0]                = mo_early_pipe_valid_q[NUM_MO_EARLY_REGS];
  // Input stage: Propagate pipeline ready signal to mo_early pipe
  assign mo_early_pipe_ready[NUM_MO_EARLY_REGS] = mo_late_pipe_ready[0];

  // Generate the register stages
  for (genvar i = 0; i < NUM_MO_LATE_REGS; i++) begin : gen_mid_out_late_pipeline
    // Internal register enable for this stage
    logic reg_ena;
    // Determine the ready signal of the current stage - advance the pipeline:
    // 1. if the next stage is ready for our data
    // 2. if the next stage only holds a bubble (not valid) -> we can pop it
    assign mo_late_pipe_ready[i] = mo_late_pipe_ready[i+1] | ~mo_late_pipe_valid_q[i+1];
    // Valid: enabled by ready signal, synchronous clear with the flush signal
    `FFLARNC(mo_late_pipe_valid_q[i+1], mo_late_pipe_valid_q[i], reg_enable_i, flush_i, 1'b0, clk_i, rst_ni)
    // Enable register if pipeline ready and a valid data item is present
    assign reg_ena = reg_enable_i;
    // Generate the pipeline registers within the stages, use enable-registers
    `FFL(mo_late_pipe_lzc_zeroes_q[i+1],          mo_late_pipe_lzc_zeroes_q[i],          reg_ena, '0)
    `FFL(mo_late_pipe_leading_zero_count_q[i+1],  mo_late_pipe_leading_zero_count_q[i],  reg_ena, '0)
    `FFL(mo_late_pipe_final_tentative_exp_q[i+1], mo_late_pipe_final_tentative_exp_q[i], reg_ena, '0)
    `FFL(mo_late_pipe_sum_lower_q[i+1],           mo_late_pipe_sum_lower_q[i],           reg_ena, '0)
    `FFL(mo_late_pipe_sticky_before_add_z_q[i+1], mo_late_pipe_sticky_before_add_z_q[i], reg_ena, '0)
    `FFL(mo_late_pipe_bypass_w_q[i+1],            mo_late_pipe_bypass_w_q[i],            reg_ena, '0)
    `FFL(mo_late_pipe_sticky_before_add_q[i+1],   mo_late_pipe_sticky_before_add_q[i],   reg_ena, '0)
    `FFL(mo_late_pipe_eff_sub_first_q[i+1],       mo_late_pipe_eff_sub_first_q[i],       reg_ena, '0)
    `FFL(mo_late_pipe_eff_sub_z_q[i+1],           mo_late_pipe_eff_sub_z_q[i],           reg_ena, '0)
    `FFL(mo_late_pipe_info_min_is_zero_q[i+1],    mo_late_pipe_info_min_is_zero_q[i],    reg_ena, '0)
    `FFL(mo_late_pipe_info_max_is_zero_q[i+1],    mo_late_pipe_info_max_is_zero_q[i],    reg_ena, '0)
    `FFL(mo_late_pipe_final_sign_zero_q[i+1],     mo_late_pipe_final_sign_zero_q[i],     reg_ena, '0)
    `FFL(mo_late_pipe_final_sign_z_q[i+1],        mo_late_pipe_final_sign_z_q[i],        reg_ena, '0)
    `FFL(mo_late_pipe_rnd_mode_q[i+1],            mo_late_pipe_rnd_mode_q[i],            reg_ena, fpnew_pkg::RNE)
    `FFL(mo_late_pipe_dst_fmt_q[i+1],             mo_late_pipe_dst_fmt_q[i],             reg_ena, fpnew_pkg::fp_format_e'(0))
    `FFL(mo_late_pipe_res_is_spec_q[i+1],         mo_late_pipe_res_is_spec_q[i],         reg_ena, '0)
    `FFL(mo_late_pipe_spec_res_q[i+1],            mo_late_pipe_spec_res_q[i],            reg_ena, '0)
    `FFL(mo_late_pipe_spec_stat_q[i+1],           mo_late_pipe_spec_stat_q[i],           reg_ena, '0)
    `FFL(mo_late_pipe_tag_q[i+1],                 mo_late_pipe_tag_q[i],                 reg_ena, TagType'('0))
    `FFL(mo_late_pipe_mask_q[i+1],                mo_late_pipe_mask_q[i],                reg_ena, '0)
    `FFL(mo_late_pipe_aux_q[i+1],                 mo_late_pipe_aux_q[i],                 reg_ena, AuxType'('0))
  end
  // Output stage: assign selected pipe outputs to signals for later use
  assign lzc_zeroes_q                   = mo_late_pipe_lzc_zeroes_q[NUM_MO_LATE_REGS];
  assign leading_zero_count_q           = mo_late_pipe_leading_zero_count_q[NUM_MO_LATE_REGS];
  assign final_tentative_exponent_q     = mo_late_pipe_final_tentative_exp_q[NUM_MO_LATE_REGS];
  assign sum_lower_q                    = mo_late_pipe_sum_lower_q[NUM_MO_LATE_REGS];
  assign sticky_before_add_z_q2         = mo_late_pipe_sticky_before_add_z_q[NUM_MO_LATE_REGS];
  assign bypass_w_q                     = mo_late_pipe_bypass_w_q[NUM_MO_LATE_REGS];
  assign sticky_before_add_q2           = mo_late_pipe_sticky_before_add_q[NUM_MO_LATE_REGS];
  assign effective_subtraction_first_q3 = mo_late_pipe_eff_sub_first_q[NUM_MO_LATE_REGS];
  assign effective_subtraction_z_q2     = mo_late_pipe_eff_sub_z_q[NUM_MO_LATE_REGS];
  assign info_min_is_zero_q3            = mo_late_pipe_info_min_is_zero_q[NUM_MO_LATE_REGS];
  assign info_max_is_zero_q2            = mo_late_pipe_info_max_is_zero_q[NUM_MO_LATE_REGS];
  assign final_sign_zero_q              = mo_late_pipe_final_sign_zero_q[NUM_MO_LATE_REGS];
  assign final_sign_z_q                 = mo_late_pipe_final_sign_z_q[NUM_MO_LATE_REGS];
  assign rnd_mode_q2                    = mo_late_pipe_rnd_mode_q[NUM_MO_LATE_REGS];
  assign dst_fmt_q2                     = mo_late_pipe_dst_fmt_q[NUM_MO_LATE_REGS];
  assign result_is_special_q            = mo_late_pipe_res_is_spec_q[NUM_MO_LATE_REGS];
  assign special_result_q               = mo_late_pipe_spec_res_q[NUM_MO_LATE_REGS];
  assign special_status_q               = mo_late_pipe_spec_stat_q[NUM_MO_LATE_REGS];

  // ----------------
  // Normalization 2
  // ----------------
  logic [DST_PRECISION_BITS:0] final_mantissa;  // final mantissa before rounding with round bit
  logic   [DST_PRECISION_BITS+PRECISION_BITS+2:0] sum_sticky_bits;   // remaining p_dst+3 sticky bits after normalization
  logic sticky_after_norm;  // sticky bit after normalization

  logic signed [DST_EXP_WIDTH-1:0] final_exponent;

  opope_sdotp_normalization #(
    .SrcDotpFpFmtConfig ( SrcDotpFpFmtConfig ),
    .DstDotpFpFmtConfig ( DstDotpFpFmtConfig )
  ) i_fpnew_sdotp_normalization (
    .lzc_zeroes_i                  ( lzc_zeroes_q                   ),
    .leading_zero_count_i          ( leading_zero_count_q           ),
    .final_tentative_exponent_i    ( final_tentative_exponent_q     ),
    .sum_lower_i                   ( sum_lower_q                    ),
    .sticky_before_add_z_i         ( sticky_before_add_z_q2         ),
    .bypass_w_i                    ( bypass_w_q                     ),
    .sticky_before_add_i           ( sticky_before_add_q2           ),
    .effective_subtraction_first_i ( effective_subtraction_first_q3 ),
    .effective_subtraction_z_i     ( effective_subtraction_z_q2     ),
    .info_min_is_zero_i            ( info_min_is_zero_q3            ),
    .final_mantissa_o              ( final_mantissa                 ),
    .sum_sticky_bits_o             ( sum_sticky_bits                ),
    .final_exponent_o              ( final_exponent                 ),
    .sticky_after_norm_o           ( sticky_after_norm              )
  );

  // ----------------------------
  // Rounding and classification
  // ----------------------------
  logic enable_rsr;
  assign enable_rsr = (rnd_mode_q2 == fpnew_pkg::RSR) && (mo_late_pipe_ready[NUM_MO_LATE_REGS]
                      && mo_late_pipe_valid_q[NUM_MO_LATE_REGS]);

  // Final results for output pipeline
  logic [DST_WIDTH-1:0] result_d;
  fpnew_pkg::status_t   status_d;

  opope_sdotp_rounding_assembly #(
    .SrcDotpFpFmtConfig          ( SrcDotpFpFmtConfig          ),
    .DstDotpFpFmtConfig          ( DstDotpFpFmtConfig          ),
    .StochasticRndImplementation ( StochasticRndImplementation )
  ) i_fpnew_sdotp_rounding_assembly (
    .clk_i                     ( clk_i                      ),
    .rst_ni                    ( rst_ni                     ),
    .sdotp_hart_id_i           ( sdotp_hart_id_i            ),
    .final_mantissa_i          ( final_mantissa             ),
    .sum_sticky_bits_i         ( sum_sticky_bits            ),
    .final_exponent_i          ( final_exponent             ),
    .sticky_after_norm_i       ( sticky_after_norm          ),
    .info_max_is_zero_i        ( info_max_is_zero_q2        ),
    .final_sign_zero_i         ( final_sign_zero_q          ),
    .final_sign_z_i            ( final_sign_z_q             ),
    .rnd_mode_i                ( rnd_mode_q2                ),
    .dst_fmt_i                 ( dst_fmt_q2                 ),
    .effective_subtraction_z_i ( effective_subtraction_z_q2 ),
    .enable_rsr_i              ( enable_rsr                 ),
    .result_is_special_i       ( result_is_special_q        ),
    .special_result_i          ( special_result_q           ),
    .special_status_i          ( special_status_q           ),
    .result_o                  ( result_d                   ),
    .status_o                  ( status_d                   )
  );

  // ----------------
  // Output Pipeline
  // ----------------
  // Output pipeline signals, index i holds signal after i register stages
  logic               [0:NUM_OUT_REGS][DST_WIDTH-1:0] out_pipe_result_q;
  fpnew_pkg::status_t [0:NUM_OUT_REGS]                out_pipe_status_q;
  TagType             [0:NUM_OUT_REGS]                out_pipe_tag_q;
  logic               [0:NUM_OUT_REGS]                out_pipe_mask_q;
  AuxType             [0:NUM_OUT_REGS]                out_pipe_aux_q;
  logic               [0:NUM_OUT_REGS]                out_pipe_valid_q;
  // Ready signal is combinatorial for all stages
  logic [0:NUM_OUT_REGS] out_pipe_ready;

  // Input stage: First element of pipeline is taken from inputs
  assign out_pipe_result_q[0]                 = result_d;
  assign out_pipe_status_q[0]                 = status_d;
  assign out_pipe_tag_q[0]                    = mo_late_pipe_tag_q[NUM_MO_LATE_REGS];
  assign out_pipe_mask_q[0]                   = mo_late_pipe_mask_q[NUM_MO_LATE_REGS];
  assign out_pipe_aux_q[0]                    = mo_late_pipe_aux_q[NUM_MO_LATE_REGS];
  assign out_pipe_valid_q[0]                  = mo_late_pipe_valid_q[NUM_MO_LATE_REGS];
  // Input stage: Propagate pipeline ready signal to mo_late pipe
  assign mo_late_pipe_ready[NUM_MO_LATE_REGS] = out_pipe_ready[0];
  // Generate the register stages
  for (genvar i = 0; i < NUM_OUT_REGS; i++) begin : gen_output_pipeline
    // Internal register enable for this stage
    logic reg_ena;
    // Determine the ready signal of the current stage - advance the pipeline:
    // 1. if the next stage is ready for our data
    // 2. if the next stage only holds a bubble (not valid) -> we can pop it
    assign out_pipe_ready[i] = out_pipe_ready[i+1] | ~out_pipe_valid_q[i+1];
    // Valid: enabled by ready signal, synchronous clear with the flush signal
    `FFLARNC(out_pipe_valid_q[i+1], out_pipe_valid_q[i], reg_enable_i, flush_i, 1'b0, clk_i, rst_ni)
    // Enable register if pipleine ready and a valid data item is present
    assign reg_ena = reg_enable_i;
    // Generate the pipeline registers within the stages, use enable-registers
    `FFL(out_pipe_result_q[i+1], out_pipe_result_q[i], reg_ena, '0)
    `FFL(out_pipe_status_q[i+1], out_pipe_status_q[i], reg_ena, '0)
    `FFL(out_pipe_tag_q[i+1],    out_pipe_tag_q[i],    reg_ena, TagType'('0))
    `FFL(out_pipe_mask_q[i+1],   out_pipe_mask_q[i],   reg_ena, '0)
    `FFL(out_pipe_aux_q[i+1],    out_pipe_aux_q[i],    reg_ena, AuxType'('0))
  end
  // Output stage: Ready travels backwards from output side, driven by downstream circuitry
  assign out_pipe_ready[NUM_OUT_REGS] = out_ready_i;
  // Output stage: assign module outputs
  assign result_o        = out_pipe_result_q[NUM_OUT_REGS];
  assign status_o        = out_pipe_status_q[NUM_OUT_REGS];
  assign extension_bit_o = 1'b1; // always NaN-Box result
  assign tag_o           = out_pipe_tag_q[NUM_OUT_REGS];
  assign mask_o          = out_pipe_mask_q[NUM_OUT_REGS];
  assign aux_o           = out_pipe_aux_q[NUM_OUT_REGS];
  assign out_valid_o     = out_pipe_valid_q[NUM_OUT_REGS];
  assign busy_o          = (| {inp_pipe_valid_q, im_early_pipe_valid_q, im_late_pipe_valid_q, mid_pipe_valid_q, mo_early_pipe_valid_q, mo_late_pipe_valid_q, out_pipe_valid_q});
endmodule
