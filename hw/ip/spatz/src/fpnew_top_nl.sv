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

module fpnew_top_nl #(
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
  output logic                              busy_o,
  // Next uop ready (for NL sequencer)
  output logic                              next_uop_ready_o
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

  // ------------------------------------
  // NL Controller Instantiation
  // ------------------------------------
  logic [NUM_OPGROUPS-1:0] nl_use_internal;
  logic [NUM_OPGROUPS-1:0] nl_in_valid;
  logic [NUM_OPGROUPS-1:0][NUM_OPERANDS-1:0][WIDTH-1:0] nl_operands;
  fpnew_pkg::operation_e [NUM_OPGROUPS-1:0] nl_op;
  logic [NUM_OPGROUPS-1:0] nl_op_mod;
  fpnew_pkg::roundmode_e [NUM_OPGROUPS-1:0] nl_rnd;
  TagType [NUM_OPGROUPS-1:0] nl_tag;
  
  logic [NUM_OPGROUPS-1:0] nl_out_valid_modified;
  logic [NUM_OPGROUPS-1:0] nl_out_ready_internal;
  logic [NUM_OPGROUPS-1:0] nl_result_override;
  logic [WIDTH-1:0]        nl_result_data;

  // Flatten output struct
  logic [NUM_OPGROUPS-1:0][WIDTH-1:0] opgrp_res_flat;
  TagType [NUM_OPGROUPS-1:0]          opgrp_tag_flat;
  
  for (genvar i=0; i<NUM_OPGROUPS; i++) begin
      assign opgrp_res_flat[i] = opgrp_outputs[i].result;
      assign opgrp_tag_flat[i] = opgrp_outputs[i].tag;
  end

  fpnew_nl_controller #(
    .WIDTH(WIDTH),
    .NUM_OPERANDS(NUM_OPERANDS),
    .NUM_OPGROUPS(NUM_OPGROUPS),
    .TagType(TagType)
  ) i_nl_controller (
    .clk_i,
    .rst_ni,
    .operands_i,
    .tag_i,
    .in_valid_i,
    .opgrp_out_valid_i(opgrp_out_valid),
    .opgrp_result_i   (opgrp_res_flat),
    .opgrp_tag_i      (opgrp_tag_flat),
    .opgrp_in_ready_i (opgrp_in_ready),
    
    .nl_use_internal_op_o(nl_use_internal),
    .nl_in_valid_o(nl_in_valid),
    .nl_operands_o(nl_operands),
    .nl_op_o(nl_op),
    .nl_op_mod_o(nl_op_mod),
    .nl_rnd_mode_o(nl_rnd),
    .nl_tag_o(nl_tag),
    
    .nl_out_valid_o(nl_out_valid_modified),
    .nl_out_ready_o(nl_out_ready_internal),
    .nl_result_override_o(nl_result_override),
    .nl_result_o(nl_result_data),
    .next_uop_ready_o
  );

  // -----------
  // Input Side
  // -----------
  assign in_ready_o = in_valid_i & opgrp_in_ready[fpnew_pkg::get_opgroup(op_i)];

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

    logic in_valid;
    logic [NUM_FORMATS-1:0][NUM_OPS-1:0] input_boxed;
    // Muxing Logic
    logic                               mux_in_valid;
    
    assign mux_in_valid = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
    assign in_valid     = nl_use_internal[opgrp] ? nl_in_valid[opgrp] : mux_in_valid;

    // slice out input boxing
    always_comb begin : slice_inputs
      for (int unsigned fmt = 0; fmt < NUM_FORMATS; fmt++)
        input_boxed[fmt] = is_boxed[fmt][NUM_OPS-1:0];
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
      .operands_i      ( nl_use_internal[opgrp] ? nl_operands[opgrp][NUM_OPS-1:0] : operands_i[NUM_OPS-1:0]  ),
      .is_boxed_i      ( input_boxed             ),
      .rnd_mode_i      ( nl_use_internal[opgrp] ? nl_rnd[opgrp]       : rnd_mode_i),
      .op_i             ( nl_use_internal[opgrp] ? nl_op[opgrp]    : op_i       ),
      .op_mod_i         ( nl_use_internal[opgrp] ? nl_op_mod[opgrp]: op_mod_i   ),
      .src_fmt_i,
      .dst_fmt_i,
      .int_fmt_i,
      .vectorial_op_i,
      .tag_i(nl_use_internal[opgrp] ? nl_tag[opgrp]       : tag_i   ),
      .simd_mask_i     ( simd_mask             ),
      .in_valid_i      ( in_valid              ),
      .in_ready_o      ( opgrp_in_ready[opgrp] ),
      .flush_i,
      .result_o        ( opgrp_outputs[opgrp].result ),
      .status_o        ( opgrp_outputs[opgrp].status ),
      .extension_bit_o ( opgrp_ext[opgrp]            ),
      .tag_o           ( opgrp_outputs[opgrp].tag    ),
      .out_valid_o     ( opgrp_out_valid[opgrp]      ),
      .out_ready_i     ( nl_out_ready_internal[opgrp] | opgrp_out_ready[opgrp]      ),
      .busy_o          ( opgrp_busy[opgrp]           )
    );
  end
 
  // ------------------
  // Arbitrate Outputs
  // ------------------
  output_t arbiter_output;
  output_t [NUM_OPGROUPS-1:0] arbiter_inputs;

  // Prepare inputs for arbiter
  always_comb begin
      arbiter_inputs = opgrp_outputs;
      for (int i=0; i<NUM_OPGROUPS; i++) begin
          if (nl_result_override[i]) begin
              arbiter_inputs[i].result = nl_result_data;
          end
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
    .rr_i   ( '0             ),
    .req_i  ( nl_out_valid_modified ),
    .gnt_o  ( opgrp_out_ready       ),
    .data_i ( arbiter_inputs        ), 
    .gnt_i  ( out_ready_i           ),
    .req_o  ( out_valid_o           ),
    .data_o ( arbiter_output        ),
    .idx_o  ( /* unused */          )
  );
  // Unpack output
  assign result_o        = arbiter_output.result;
  assign status_o        = arbiter_output.status;
  assign tag_o           = arbiter_output.tag;

  assign busy_o = (| opgrp_busy);

endmodule


