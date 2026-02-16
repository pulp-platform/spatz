// Copyright 2019 ETH Zurich and University of Bologna.
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

// Author: Stefan Mach <smach@iis.ee.ethz.ch>

module fpnew_top_nl
  import fpnew_pkg::*;
  #(
  // FPU configuration
  parameter fpnew_pkg::fpu_features_t       Features       = fpnew_pkg::RV64D_Xsflt,
  parameter fpnew_pkg::fpu_implementation_t Implementation = fpnew_pkg::DEFAULT_NOREGS,
  // PulpDivSqrt = 0 enables T-head-based DivSqrt unit. Supported only for FP32-only instances of Fpnew
  parameter logic                           PulpDivsqrt    = 1'b1,
  parameter type                            TagType        = logic,
  parameter logic                           TrueSIMDClass  = 1'b0,
  parameter logic                           EnableSIMDMask = 1'b0,
  parameter logic                           CompressedVecCmpResult = 1'b0, // conceived for RV32FD cores
  parameter fpnew_pkg::rsr_impl_t           StochasticRndImplementation = fpnew_pkg::DEFAULT_NO_RSR,
  // Do not change
  localparam int unsigned NumLanes     = fpnew_pkg::max_num_lanes(Features.Width, Features.FpFmtMask, Features.EnableVectors),
  localparam type         MaskType     = logic [NumLanes-1:0],
  localparam int unsigned WIDTH        = Features.Width,
  localparam int unsigned NUM_OPERANDS = 3
) (
  input logic                               clk_i,
  input logic                               rst_ni,
  input logic [31:0]                        hart_id_i,
  // Input signals
  input logic [NUM_OPERANDS-1:0][WIDTH-1:0] operands_i,
  input fpnew_pkg::roundmode_e              rnd_mode_i,
  input fpnew_pkg::operation_e              op_i,
  input logic                               op_mod_i,
  input fpnew_pkg::fp_format_e              src_fmt_i,
  input fpnew_pkg::fp_format_e              dst_fmt_i,
  input fpnew_pkg::int_format_e             int_fmt_i,
  input logic                               vectorial_op_i,
  input TagType                             tag_i,
  input MaskType                            simd_mask_i,
  // Input Handshake
  input  logic                              in_valid_i,
  output logic                              in_ready_o,
  input  logic                              flush_i,
  // Output signals
  output logic [WIDTH-1:0]                  result_o,
  output fpnew_pkg::status_t                status_o,
  output TagType                            tag_o,
  // Output handshake
  output logic                              out_valid_o,
  input  logic                              out_ready_i,
  // Indication of valid data in flight
  output logic                              busy_o
 );

  localparam int unsigned NUM_OPGROUPS = fpnew_pkg::NUM_OPGROUPS;
  localparam int unsigned NUM_FORMATS  = fpnew_pkg::NUM_FP_FORMATS;

  // ----------------
  // Type Definition
  // ----------------
  typedef struct packed {
    logic [WIDTH-1:0]   result;
    fpnew_pkg::status_t status;
    TagType             tag;
  } output_t;

  // Handshake signals for the blocks
  logic [NUM_OPGROUPS-1:0] opgrp_in_ready, opgrp_out_valid, opgrp_out_ready, opgrp_ext, opgrp_busy;
  output_t [NUM_OPGROUPS-1:0] opgrp_outputs;

  logic [NUM_FORMATS-1:0][NUM_OPERANDS-1:0] is_boxed;

  
  // --------------
  // NL Detection
  // ---------------
  logic is_nl_op;
  operation_e fpu_op;
  assign is_nl_op  = op_i inside {[EXPS:REC]};

  // -------------------------
  // NL Controller Signals
  // -------------------------

  // ADDMUL muxed inputs (from NL controller)
  logic [NUM_OPERANDS-1:0][WIDTH-1:0] nl_addmul_operands;
  fpnew_pkg::roundmode_e              nl_addmul_rnd_mode;
  fpnew_pkg::operation_e              nl_addmul_op;
  logic                               nl_addmul_op_mod;
  fpnew_pkg::fp_format_e              nl_addmul_src_fmt;
  fpnew_pkg::fp_format_e              nl_addmul_dst_fmt;
  fpnew_pkg::int_format_e             nl_addmul_int_fmt;
  TagType                             nl_addmul_tag;
  logic                               nl_addmul_in_valid;

  // CONV muxed inputs (from NL controller)
  logic [NUM_OPERANDS-1:0][WIDTH-1:0] nl_conv_operands;
  fpnew_pkg::roundmode_e              nl_conv_rnd_mode;
  fpnew_pkg::operation_e              nl_conv_op;
  logic                               nl_conv_op_mod;
  fpnew_pkg::fp_format_e              nl_conv_src_fmt;
  fpnew_pkg::fp_format_e              nl_conv_dst_fmt;
  fpnew_pkg::int_format_e             nl_conv_int_fmt;
  TagType                             nl_conv_tag;
  logic                               nl_conv_in_valid;

  // Managed arbiter handshake (from NL controller)
  logic [NUM_OPGROUPS-1:0] arb_gnt;
  logic [NUM_OPGROUPS-1:0] arb_req;    
  // NL status
  logic nl_busy;
// Reconstructed result feedback (from NL controller)
  logic [WIDTH-1:0] reconstructed_result;
  logic             reconstructed_result_valid;
  output_t [NUM_OPGROUPS-1:0] reconstruct_output_sel; // Modified to perform postprocessing 

  fpnew_nl_controller #(
    .WIDTH        ( WIDTH        ),
    .NUM_OPERANDS ( NUM_OPERANDS ),
    .TagType      ( TagType      ),
    .NUM_OPGROUPS ( NUM_OPGROUPS ),
    .MAX_INFLIGHT ( 16           )
  ) i_nl_ctrl (
    .clk_i,
    .rst_ni,
    .flush_i,

    // External interface (from VFU)
    .operands_i,
    .rnd_mode_i,
    .op_i,
    .op_mod_i,
    .src_fmt_i,
    .dst_fmt_i,
    .int_fmt_i,
    .vectorial_op_i,
    .tag_i,
    .in_valid_i,

    // NL detection
    .is_nl_op_i   ( is_nl_op  ),

    // ADDMUL opgroup muxed outputs
    .addmul_operands_o ( nl_addmul_operands ),
    .addmul_rnd_mode_o ( nl_addmul_rnd_mode ),
    .addmul_op_o       ( nl_addmul_op       ),
    .addmul_op_mod_o   ( nl_addmul_op_mod   ),
    .addmul_src_fmt_o  ( nl_addmul_src_fmt  ),
    .addmul_dst_fmt_o  ( nl_addmul_dst_fmt  ),
    .addmul_int_fmt_o  ( nl_addmul_int_fmt  ),
    .addmul_tag_o      ( nl_addmul_tag      ),
    .addmul_in_valid_o ( nl_addmul_in_valid ),
    .addmul_in_ready_i ( opgrp_in_ready[0]  ),

    // CONV opgroup muxed outputs
    .conv_operands_o   ( nl_conv_operands  ),
    .conv_rnd_mode_o   ( nl_conv_rnd_mode  ),
    .conv_op_o         ( nl_conv_op        ),
    .conv_op_mod_o     ( nl_conv_op_mod    ),
    .conv_src_fmt_o    ( nl_conv_src_fmt   ),
    .conv_dst_fmt_o    ( nl_conv_dst_fmt   ),
    .conv_int_fmt_o    ( nl_conv_int_fmt   ),
    .conv_tag_o        ( nl_conv_tag       ),
    .conv_in_valid_o   ( nl_conv_in_valid  ),
    .conv_in_ready_i   ( opgrp_in_ready[3] ),

    // Feedback from opgroup results
    .addmul_result_i     ( opgrp_outputs[0].result ),
    .addmul_tag_result_i ( opgrp_outputs[0].tag    ),
    .addmul_out_valid_i  ( opgrp_out_valid[0]      ),
    .conv_result_i       ( opgrp_outputs[3].result ),
    .conv_tag_result_i   ( opgrp_outputs[3].tag    ),
    .conv_out_valid_i    ( opgrp_out_valid[3]      ),

    // Arbiter handshake management
    .arb_gnt_i          ( arb_gnt          ),
    .opgrp_out_ready_o  ( opgrp_out_ready  ),
    .opgrp_out_valid_i  ( opgrp_out_valid  ),
    .arb_req_o          ( arb_req          ),

    // Input ready
    .opgrp_in_ready_i   ( opgrp_in_ready   ),
    .in_ready_o         ( in_ready_o       ),

    // Reconstructed result feedback
    .reconstructed_result_o ( reconstructed_result ),
    .reconstructed_result_valid_o ( reconstructed_result_valid ),

    // Status
    .nl_busy_o          ( nl_busy )
  );
 
  // -----------
  // Input Side
  // -----------

  // NaN-boxing check
  for (genvar fmt = 0; fmt < int'(NUM_FORMATS); fmt++) begin : gen_nanbox_check
    localparam int unsigned FP_WIDTH = fpnew_pkg::fp_width(fpnew_pkg::fp_format_e'(fmt));
    // NaN boxing is only generated if it's enabled and needed
    if (Features.EnableNanBox && (FP_WIDTH < WIDTH)) begin : check
      for (genvar op = 0; op < int'(NUM_OPERANDS); op++) begin : operands
        assign is_boxed[fmt][op] = (!vectorial_op_i)
                                   ? operands_i[op][WIDTH-1:FP_WIDTH] == '1
                                   : 1'b1;
      end
    end else begin : no_check
      assign is_boxed[fmt] = '1;
    end
  end

  // Filter out the mask if not used
  MaskType simd_mask;
  assign simd_mask = simd_mask_i | ~{NumLanes{EnableSIMDMask}};
  
  // -------------------------
  // Generate Operation Blocks
  // -------------------------
  for (genvar opgrp = 0; opgrp < int'(NUM_OPGROUPS); opgrp++) begin : gen_operation_groups
    localparam int unsigned NUM_OPS = fpnew_pkg::num_operands(fpnew_pkg::opgroup_e'(opgrp));

    // Per-opgroup input signals (muxed)
    logic                               grp_in_valid;
    logic [NUM_OPERANDS-1:0][WIDTH-1:0] grp_operands;
    fpnew_pkg::roundmode_e              grp_rnd_mode;
    fpnew_pkg::operation_e              grp_op;
    logic                               grp_op_mod;
    TagType                             grp_tag;
    fpnew_pkg::fp_format_e              grp_src_fmt;
    fpnew_pkg::fp_format_e              grp_dst_fmt;
    fpnew_pkg::int_format_e             grp_int_fmt;
    
    logic [NUM_FORMATS-1:0][NUM_OPS-1:0] input_boxed;
    
    // slice out input boxing
    always_comb begin : slice_inputs
      for (int unsigned fmt = 0; fmt < NUM_FORMATS; fmt++)
        input_boxed[fmt] = is_boxed[fmt][NUM_OPS-1:0];
    end
  // ── Input Muxing ──
    if (opgrp == 0) begin : gen_addmul_mux
      // ADDMUL opgroup: NL controller can override inputs
      assign grp_operands = nl_addmul_operands;
      assign grp_rnd_mode = nl_addmul_rnd_mode;
      assign grp_op       = nl_addmul_op;
      assign grp_op_mod   = nl_addmul_op_mod;
      assign grp_tag      = nl_addmul_tag;
      assign grp_in_valid = nl_addmul_in_valid;
      assign grp_src_fmt  = nl_addmul_src_fmt;
      assign grp_dst_fmt  = nl_addmul_dst_fmt;
      assign grp_int_fmt  = nl_addmul_int_fmt;

    end else if (opgrp == 3) begin : gen_conv_mux
      // CONV opgroup: NL controller can override inputs
      assign grp_operands = nl_conv_operands;
      assign grp_rnd_mode = nl_conv_rnd_mode;
      assign grp_op       = nl_conv_op;
      assign grp_op_mod   = nl_conv_op_mod;
      assign grp_tag      = nl_conv_tag;
      assign grp_in_valid = nl_conv_in_valid;
      assign grp_src_fmt  = nl_conv_src_fmt;
      assign grp_dst_fmt  = nl_conv_dst_fmt;
      assign grp_int_fmt  = nl_conv_int_fmt;

    end else begin : gen_passthrough
      // Other opgroups: direct pass-through, NL controller doesn't touch these
      assign grp_operands = operands_i;
      assign grp_rnd_mode = rnd_mode_i;
      assign grp_op       = op_i;
      assign grp_op_mod   = op_mod_i;
      assign grp_tag      = tag_i;
      assign grp_src_fmt  = src_fmt_i;
      assign grp_dst_fmt  = dst_fmt_i;
      assign grp_int_fmt  = int_fmt_i;
      assign grp_in_valid =  !is_nl_op & in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
    end
    fpnew_opgroup_block #(
      .OpGroup       ( fpnew_pkg::opgroup_e'(opgrp)    ),
      .Width         ( WIDTH                           ),
      .EnableVectors ( Features.EnableVectors          ),
      .PulpDivsqrt   ( PulpDivsqrt                     ),
      .FpFmtMask     ( Features.FpFmtMask              ),
      .IntFmtMask    ( Features.IntFmtMask             ),
      .FmtPipeRegs   ( Implementation.PipeRegs[opgrp]  ),
      .FmtUnitTypes  ( Implementation.UnitTypes[opgrp] ),
      .PipeConfig    ( Implementation.PipeConfig       ),
      .TagType       ( TagType                         ),
      .TrueSIMDClass ( TrueSIMDClass                   ),
      .CompressedVecCmpResult ( CompressedVecCmpResult ),
      .StochasticRndImplementation ( StochasticRndImplementation )
    ) i_opgroup_block (
      .clk_i,
      .rst_ni,
      .hart_id_i,
      .operands_i      ( grp_operands[NUM_OPS-1:0] ),
      .is_boxed_i      ( input_boxed                ),
      .rnd_mode_i      ( grp_rnd_mode               ),
      .op_i            ( grp_op                     ),
      .op_mod_i        ( grp_op_mod                 ),
      .src_fmt_i       ( grp_src_fmt                ),
      .dst_fmt_i       ( grp_dst_fmt                ),
      .int_fmt_i       ( grp_int_fmt                ),
      .vectorial_op_i,
      .tag_i           ( grp_tag                    ),
      .simd_mask_i     ( simd_mask                  ),
      .in_valid_i      ( grp_in_valid               ),
      .in_ready_o      ( opgrp_in_ready[opgrp]      ),
      .flush_i,
      .result_o        ( opgrp_outputs[opgrp].result ),
      .status_o        ( opgrp_outputs[opgrp].status ),
      .extension_bit_o ( opgrp_ext[opgrp]            ),
      .tag_o           ( opgrp_outputs[opgrp].tag    ),
      .out_valid_o     ( opgrp_out_valid[opgrp]      ),
      .out_ready_i     ( opgrp_out_ready[opgrp]      ),
      .busy_o          ( opgrp_busy[opgrp]           )
    );
   end

  // ------------------
  // Arbitrate Outputs
  // ------------------
  output_t arbiter_output;

  always_comb begin
    reconstruct_output_sel = opgrp_outputs;
    if (reconstructed_result_valid) begin
      reconstruct_output_sel[0].result = reconstructed_result;
    end 
  end

  // Round-Robin arbiter to decide which result to use
  rr_arb_tree #(
    .NumIn     ( NUM_OPGROUPS ),
    .DataType  ( output_t     ),
    .AxiVldRdy ( 1'b1         )
  ) i_arbiter (
    .clk_i,
    .rst_ni,
    .flush_i,
    .rr_i   ( '0              ),
    .req_i  ( arb_req         ),  
    .gnt_o  ( arb_gnt         ), 
    .data_i ( reconstruct_output_sel   ),
    .gnt_i  ( out_ready_i     ),
    .req_o  ( out_valid_o     ),
    .data_o ( arbiter_output  ),
    .idx_o  ( /* unused */    )
  ); 

  // Unpack output
  assign result_o        = arbiter_output.result;
  assign status_o        = arbiter_output.status;
  assign tag_o           = arbiter_output.tag;

  assign busy_o = (| opgrp_busy);

endmodule
