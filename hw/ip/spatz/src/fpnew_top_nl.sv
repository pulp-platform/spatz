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
  import fpnew_nl_pkg::*;
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
  output logic                              busy_o,
  output logic                              next_uop_ready_o
 );

  `include "common_cells/registers.svh"
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

  TagType fconv_tag, fadd_tag;
  logic nl_concatenate;
  logic nl_opmode_conv, nl_opmode_add;
  logic nl_wr_en, nl_wr_en_1, nl_wr_en_2, nl_wr_en_3, is_last_uop_d, is_last_uop_q;
  `FF(is_last_uop_q, is_last_uop_d, 1'b0)
  
  fpnew_pkg::operation_e nl_op_conv, nl_op_add;
  fpnew_pkg::roundmode_e nl_rnd_conv, nl_rnd_add;
  assign nl_concatenate = tag_i.nl;

  logic [NUM_OPERANDS-1:0][WIDTH-1:0]  operands_fconv, operands_add;

  // Handshake signals for the blocks
  logic [NUM_OPGROUPS-1:0] opgrp_in_ready, opgrp_out_valid, opgrp_out_ready, opgrp_ext, opgrp_busy, concatenate_out_valid, concatenate_in_valid, out_opgrp_ready;
  logic [WIDTH-1:0] nl_intermediate_0_d, nl_intermediate_1_d, nl_intermediate_0_q, nl_intermediate_1_q, nl_intermediate_2_d, nl_intermediate_3_d, nl_intermediate_2_q, nl_intermediate_3_q;
  logic [3:0] k_int_q_0, k_int_d_0, k_int_q_1, k_int_d_1;
  logic k_en_0, k_en_1;
 
  `FFL(nl_intermediate_0_q, nl_intermediate_0_d, nl_wr_en, '0)
  `FFL(nl_intermediate_1_q, nl_intermediate_1_d, nl_wr_en_1, '0)
  `FFL(nl_intermediate_2_q, nl_intermediate_2_d, nl_wr_en_2, '0)
  `FFL(nl_intermediate_3_q, nl_intermediate_3_d, nl_wr_en_3, '0)
  `FFL(k_int_q_0, k_int_d_0, k_en_0, '0)
  `FFL(k_int_q_1, k_int_d_1, k_en_1, '0)
  output_t [NUM_OPGROUPS-1:0] opgrp_outputs;
  output_t [NUM_OPGROUPS-1:0] nl_outputs;

  logic [NUM_FORMATS-1:0][NUM_OPERANDS-1:0] is_boxed;
  logic [31:0] x32_0, abs32_0, x32_1, abs32_1;
  logic        sign_0, sign_1;
  logic        is_nan_0, is_inf_0, clamp_mag_0, is_nan_1, is_inf_1, clamp_mag_1;

 // Gating signal
  logic enable_precalc;
  assign enable_precalc = in_valid_i && (tag_i.nl_op_sel == LOGS || tag_i.nl_op_sel == RSQRT);

  logic [31:0] op0_hi, op0_lo;
  // Force to 0 if not a relevant op
  assign op0_hi = enable_precalc ? operands_i[0][63:32] : 32'b0;
  assign op0_lo = enable_precalc ? operands_i[0][31:0]  : 32'b0;

  // LOGS Pre-calculation: (X - 1.0f)
  logic log_hi_nan_or_sign, log_lo_nan_or_sign;
  logic [31:0] log_res_hi, log_res_lo;

  assign log_hi_nan_or_sign = op0_hi[31] || ((op0_hi[30:23] == 8'hFF) && (op0_hi[22:0] != '0));
  assign log_lo_nan_or_sign = op0_lo[31] || ((op0_lo[30:23] == 8'hFF) && (op0_lo[22:0] != '0));

  assign log_res_hi = (log_hi_nan_or_sign) ? 32'h7FC00000 : ($signed(op0_hi) - $signed(32'h3F800000));
  assign log_res_lo = (log_lo_nan_or_sign) ? 32'h7FC00000 : ($signed(op0_lo) - $signed(32'h3F800000));

  // RSQRT Pre-calculation: Magic Constant - (X >> 1)
  logic rsqrt_hi_nan_or_sign, rsqrt_lo_nan_or_sign;
  logic [31:0] rsqrt_res_hi, rsqrt_res_lo;

  assign rsqrt_hi_nan_or_sign = op0_hi[31] || ((op0_hi[30:23] == 8'hFF) && (op0_hi[22:0] != '0));
  assign rsqrt_lo_nan_or_sign = op0_lo[31] || ((op0_lo[30:23] == 8'hFF) && (op0_lo[22:0] != '0));

  // Note: Logical shift (>>) is intended here before subtraction
  assign rsqrt_res_hi = (rsqrt_hi_nan_or_sign) ? 32'h7FC00000 : ($signed(QUAKE_MAGIC) - $signed(op0_hi >> 1));
  assign rsqrt_res_lo = (rsqrt_lo_nan_or_sign) ? 32'h7FC00000 : ($signed(QUAKE_MAGIC) - $signed(op0_lo >> 1));

  logic [WIDTH-1:0] reconstructed_result;
  logic next_uop_ready;
  assign next_uop_ready_o = next_uop_ready;

  logic addmul_out_valid, conv_out_valid;
  logic addmul_last_phase, conv_last_phase, input_last_phase;
  logic [WIDTH-1:0] addmul_inter_01_sel, addmul_inter_23_sel; 
  logic [WIDTH-1:0] conv_inter_01_sel;

  assign conv_inter_01_sel = conv_last_phase ? nl_intermediate_1_q : nl_intermediate_0_q;

  assign addmul_out_valid  = opgrp_out_valid[fpnew_pkg::ADDMUL];
  assign conv_out_valid    = opgrp_out_valid[fpnew_pkg::CONV];

  assign addmul_last_phase = opgrp_outputs[fpnew_pkg::ADDMUL].tag.last_phase;
  assign conv_last_phase   = opgrp_outputs[fpnew_pkg::CONV].tag.last_phase;
  assign input_last_phase  = tag_i.last_phase;

  assign addmul_inter_01_sel = addmul_last_phase ? nl_intermediate_1_q : nl_intermediate_0_q;
  assign addmul_inter_23_sel = addmul_last_phase ? nl_intermediate_3_q : nl_intermediate_2_q;


  always_comb begin : nl_phases_ctrl
    operands_fconv      = '0;
    operands_add        = '0;
    nl_opmode_conv      = 1'b0;
    nl_opmode_add       = 1'b0;
    nl_op_conv          = fpnew_pkg::F2I;
    nl_op_add           = fpnew_pkg::ADD;
    nl_rnd_conv         = fpnew_pkg::RTZ;
    nl_rnd_add          = fpnew_pkg::RNE;
    fconv_tag           = '0;
    fadd_tag            = '0;
    out_opgrp_ready     = opgrp_out_ready;
    nl_wr_en            = 1'b0;
    is_last_uop_d       = is_last_uop_q;
    next_uop_ready      = 1'b0;

    nl_intermediate_0_d = nl_intermediate_0_q;
    nl_intermediate_1_d = nl_intermediate_1_q;
    nl_intermediate_2_d = nl_intermediate_2_q;
    nl_intermediate_3_d = nl_intermediate_3_q;
    k_int_d_0           = k_int_q_0; 
    k_int_d_1           = k_int_q_1; 


    nl_wr_en_1          = 1'b0;
    nl_wr_en_2          = 1'b0;
    nl_wr_en_3          = 1'b0;
    k_en_0              = 1'b0;
    k_en_1              = 1'b0;

    

    if(nl_concatenate) begin
      unique case(tag_i.nl_op_sel)
        EXPS: begin
          if (addmul_out_valid) begin
            operands_fconv[0]                   = opgrp_outputs[0].result;  
            nl_opmode_conv                      = 1'b0;
            nl_op_conv                          = fpnew_pkg::F2I;
            nl_rnd_conv                         = fpnew_pkg::RTZ;
            fconv_tag                           = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
            out_opgrp_ready[0]                  = opgrp_in_ready[3]; 
          end 
        end
        COSHS: begin
          if (addmul_out_valid && is_last_uop_q != 1'b1) begin
            operands_fconv[0]                   = opgrp_outputs[0].result; 
            nl_opmode_conv                      = 1'b0;
            nl_op_conv                          = fpnew_pkg::F2I;
            nl_rnd_conv                         = fpnew_pkg::RTZ;
            fconv_tag                           =  opgrp_outputs[fpnew_pkg::ADDMUL].tag;
            out_opgrp_ready[0]                  = opgrp_in_ready[3]; 
            is_last_uop_d                       = addmul_out_valid && addmul_last_phase;
          end
          if ( conv_out_valid && opgrp_outputs[3].tag.nl_phase == NL_FPU_ISSUE_1) begin
            operands_add[0]                     = nl_intermediate_0_q;
            operands_add[1]                     = opgrp_outputs[fpnew_pkg::CONV].result;
            operands_add[2]                     = nl_intermediate_0_q;   
            nl_opmode_add                       = 1'b0;
            nl_op_add                           = fpnew_pkg::ADD;
            nl_rnd_add                          = fpnew_pkg::RNE;
            fadd_tag                            = opgrp_outputs[fpnew_pkg::CONV].tag;
            out_opgrp_ready[3]                  = opgrp_in_ready[0]; 
          end

          if ( conv_out_valid && opgrp_outputs[3].tag.nl_phase== NL_FPU_ISSUE_0) begin
            nl_wr_en = 1'b1;
            nl_intermediate_0_d                 = opgrp_outputs[fpnew_pkg::CONV].result;
            out_opgrp_ready[3]                  = 1'b1;
            next_uop_ready                      = is_last_uop_q;
          end
          if (tag_o.last_phase) is_last_uop_d   = 1'b0;
        end
        TANHS: begin
          if (in_valid_i)begin
              nl_wr_en_1                           = input_last_phase;
              nl_wr_en                             = ~input_last_phase;
              nl_intermediate_1_d                  = operands_i[0]; 
              nl_intermediate_0_d                  = operands_i[0]; 
          end
          if (addmul_out_valid) begin
            unique case(opgrp_outputs[0].tag.nl_phase)
              NL_FPU_ISSUE_0: begin
                operands_add[0]                    = opgrp_outputs[fpnew_pkg::ADDMUL].result;
                operands_add[1]                    = CHEBY_A_TANH_VEC;
                operands_add[2]                    = CHEBY_B_TANH_VEC;
                nl_opmode_add                      = 1'b0;
                nl_op_add                          = fpnew_pkg::FMADD;
                nl_rnd_add                         = fpnew_pkg::RNE;
                fadd_tag                           = opgrp_outputs[0].tag;
                fadd_tag.nl_phase                  = NL_FPU_ISSUE_1;
                out_opgrp_ready[fpnew_pkg::ADDMUL] = 1'b1; 
                nl_wr_en_3                         = addmul_last_phase;
                nl_wr_en_2                         = ~addmul_last_phase;
                nl_intermediate_3_d                = opgrp_outputs[0].result;
                nl_intermediate_2_d                = opgrp_outputs[0].result;
              end

              NL_FPU_ISSUE_1: begin
                operands_add[1]                    = opgrp_outputs[0].result;
                operands_add[2]                    = CHEBY_C_TANH_VEC;
                operands_add[0]                    = addmul_inter_23_sel;
                nl_opmode_add                      = 1'b0;
                nl_op_add                          = fpnew_pkg::FMADD;
                nl_rnd_add                         = fpnew_pkg::RNE;
                fadd_tag                           = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
                fadd_tag.nl_phase                  =  NL_FPU_ISSUE_2;
                out_opgrp_ready[fpnew_pkg::ADDMUL] = 1'b1;
              end
              NL_FPU_ISSUE_2: begin
                operands_add[0]                    = opgrp_outputs[fpnew_pkg::ADDMUL].result;
                operands_add[1]                    = addmul_inter_01_sel; 
                next_uop_ready                     = 1'b1;
                nl_opmode_add                      = 1'b0;
                nl_op_add                          = fpnew_pkg::MUL;
                nl_rnd_add                         = fpnew_pkg::RNE;
                fadd_tag                           = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
                fadd_tag.nl_phase                  = NL_WAIT;
                out_opgrp_ready[fpnew_pkg::ADDMUL] = 1'b1;
              end
              NL_WAIT: begin
              end
            endcase 
          end
        end
        LOGS: begin
          if (in_valid_i) begin
            operands_fconv[0]   = {log_res_hi, log_res_lo};
            nl_op_conv          = fpnew_pkg::I2F;
            nl_rnd_conv         = fpnew_pkg::RNE;
            fconv_tag           = tag_i;
            out_opgrp_ready[0]  = opgrp_in_ready[3];
          end
          if ( opgrp_out_valid[fpnew_pkg::CONV]) begin
            operands_add[0]     = LOG_SCALE_VEC; 
            operands_add[1]     = opgrp_outputs[fpnew_pkg::CONV].result;
            nl_op_add           = fpnew_pkg::MUL;
            nl_rnd_add          = fpnew_pkg::RNE;
            nl_opmode_add       = 1'b0;
            fadd_tag            = opgrp_outputs[fpnew_pkg::CONV].tag;
            out_opgrp_ready[3]  = 1'b1;
          end
        end
        RSQRT: begin
          if (in_valid_i) begin
            nl_wr_en_1          = input_last_phase;
            nl_wr_en            = ~input_last_phase;
            nl_intermediate_1_d = operands_i[0]; 
            nl_intermediate_0_d = operands_i[0]; 
          end
      
            operands_add[0]     = {rsqrt_res_hi, rsqrt_res_lo};
            operands_add[1]     = {rsqrt_res_hi, rsqrt_res_lo};
            nl_op_add           = fpnew_pkg::MUL;
            nl_rnd_add          = fpnew_pkg::RNE;
            fadd_tag            = tag_i;
            
            nl_wr_en_3          = input_last_phase;
            nl_wr_en_2          = ~input_last_phase;
            nl_intermediate_3_d = {rsqrt_res_hi, rsqrt_res_lo};
            nl_intermediate_2_d = {rsqrt_res_hi, rsqrt_res_lo};
          if ( addmul_out_valid) begin
            unique case(opgrp_outputs[0].tag.nl_phase) 
              NL_FPU_ISSUE_0: begin
                operands_add[0]       = addmul_inter_01_sel;
                operands_add[1]       = opgrp_outputs[fpnew_pkg::ADDMUL].result;
                nl_op_add             = fpnew_pkg::MUL;
                nl_rnd_add            = fpnew_pkg::RNE;
                nl_opmode_add         = 1'b0;
                fadd_tag              = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
                fadd_tag.nl_phase     = NL_FPU_ISSUE_1;
                out_opgrp_ready[0]    = 1'b1;
              end

              NL_FPU_ISSUE_1: begin
                operands_add[0]       = opgrp_outputs[fpnew_pkg::ADDMUL].result;
                operands_add[1]       = C1_HALF_VEC;
                operands_add[2]       = C3_HALVES_VEC;
                nl_op_add             = fpnew_pkg::FMADD;
                nl_rnd_add            = fpnew_pkg::RNE;
                nl_opmode_add         = 1'b0;
                fadd_tag              = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
                fadd_tag.nl_phase     = NL_FPU_ISSUE_2;
                out_opgrp_ready[0]    = 1'b1;
              end

              NL_FPU_ISSUE_2: begin

                operands_add[0]       = addmul_inter_23_sel; 
                operands_add[1]       = opgrp_outputs[fpnew_pkg::ADDMUL].result; 
                nl_op_add             = fpnew_pkg::MUL;
                nl_rnd_add            = fpnew_pkg::RNE;
                nl_opmode_add         = 1'b0;
                fadd_tag              = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
                fadd_tag.nl_phase     = NL_WAIT;
                out_opgrp_ready[0]    = 1'b1;
                next_uop_ready        = 1'b1;
              end
              NL_WAIT: begin
              end
            endcase
          end
        end
        SIN, COS: begin
          if (in_valid_i) begin
            nl_wr_en_1              = input_last_phase;
            nl_wr_en                = ~input_last_phase;
            nl_intermediate_1_d     = operands_i[0]; // x
            nl_intermediate_0_d     = operands_i[0]; // x
          end
          if (conv_out_valid) begin
            unique case (opgrp_outputs[3].tag.nl_phase)
              NL_FPU_ISSUE_1: begin
                k_en_1              = conv_last_phase;
                k_en_0              = ~conv_last_phase;
                k_int_d_1[3:2]      = opgrp_outputs[fpnew_pkg::CONV].result[33:32];
                k_int_d_1[1:0]      = opgrp_outputs[fpnew_pkg::CONV].result[1:0];
                k_int_d_0[3:2]      = opgrp_outputs[fpnew_pkg::CONV].result[33:32];
                k_int_d_0[1:0]      = opgrp_outputs[fpnew_pkg::CONV].result[1:0];
                
                operands_fconv[0]   = opgrp_outputs[fpnew_pkg::CONV].result;
                nl_op_conv          = fpnew_pkg::I2F;
                nl_rnd_conv         = fpnew_pkg::RNE;
                fconv_tag           = opgrp_outputs[fpnew_pkg::CONV].tag;
                fconv_tag.nl_phase  = NL_FPU_ISSUE_2;
                out_opgrp_ready[3]  = 1'b1; 
              end
              NL_FPU_ISSUE_2: begin
                operands_add[0]     = opgrp_outputs[fpnew_pkg::CONV].result;
                operands_add[1]     = PIO2_HI_VEC;
                operands_add[2]     = conv_inter_01_sel;
                nl_opmode_add       = 1'b0;
                nl_op_add           = fpnew_pkg::FNMSUB;
                nl_rnd_add          = fpnew_pkg::RNE;
                fadd_tag            = opgrp_outputs[fpnew_pkg::CONV].tag;
                fadd_tag.nl_phase   = NL_FPU_ISSUE_3;
                out_opgrp_ready[3]  = opgrp_in_ready[0]; 
              end
            endcase
          end

          if ( addmul_out_valid) begin
            unique case (opgrp_outputs[0].tag.nl_phase)
              NL_FPU_ISSUE_0: begin // k = round(X/pipi)
                operands_fconv[0]                   = opgrp_outputs[fpnew_pkg::ADDMUL].result;
                nl_op_conv                          = fpnew_pkg::F2I;
                nl_rnd_conv                         = fpnew_pkg::RNE;
                fconv_tag                           = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
                fconv_tag.nl_phase                  = NL_FPU_ISSUE_1;
                out_opgrp_ready[0]                  = opgrp_in_ready[3]; 
              end

              NL_FPU_ISSUE_3: begin //Z= R^2
                // Store r based on last_phase
                nl_wr_en_1                          = addmul_last_phase;
                nl_wr_en                            = ~addmul_last_phase;
                nl_intermediate_1_d                 = opgrp_outputs[fpnew_pkg::ADDMUL].result;
                nl_intermediate_0_d                 = opgrp_outputs[fpnew_pkg::ADDMUL].result;
                operands_add[0]                     = opgrp_outputs[fpnew_pkg::ADDMUL].result;
                operands_add[1]                     = opgrp_outputs[fpnew_pkg::ADDMUL].result;
                nl_opmode_add                       = 1'b0;
                nl_op_add                           = fpnew_pkg::MUL;
                nl_rnd_add                          = fpnew_pkg::RNE;
                fadd_tag                            = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
                fadd_tag.nl_phase                   = NL_FPU_ISSUE_4;
                out_opgrp_ready[fpnew_pkg::ADDMUL]  = 1'b1; 
              end

              NL_FPU_ISSUE_4: begin //1 + s3z 
                
                nl_wr_en_3                          = addmul_last_phase;
                nl_wr_en_2                          = ~addmul_last_phase;
                nl_intermediate_3_d                 = opgrp_outputs[fpnew_pkg::ADDMUL].result;
                nl_intermediate_2_d                 = opgrp_outputs[fpnew_pkg::ADDMUL].result;
                operands_add[0]                     = opgrp_outputs[fpnew_pkg::ADDMUL].result;
                operands_add[1]                     = SIN_S3_VEC;
                operands_add[2]                     = C_ONE_VEC;
                nl_opmode_add                       = 1'b0;
                nl_op_add                           = fpnew_pkg::FMADD;
                nl_rnd_add                          = fpnew_pkg::RNE;
                fadd_tag                            = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
                fadd_tag.nl_phase                   = NL_FPU_ISSUE_5;
                out_opgrp_ready[fpnew_pkg::ADDMUL]  = 1'b1; 
              end
              NL_FPU_ISSUE_5: begin // final mul SIN 
                operands_add[0]                     = opgrp_outputs[fpnew_pkg::ADDMUL].result; 
                operands_add[1]                     = addmul_inter_01_sel; 
                nl_opmode_add                       = 1'b0;
                nl_op_add                           = fpnew_pkg::MUL;
                nl_rnd_add                          = fpnew_pkg::RNE;
                fadd_tag                            = opgrp_outputs[fpnew_pkg::ADDMUL].tag; 
                fadd_tag.nl_phase                   = NL_FPU_ISSUE_6;
                out_opgrp_ready[fpnew_pkg::ADDMUL]  = 1'b1; 
              end
              
              NL_FPU_ISSUE_6: begin //cos = 1+ c2*z
                // Store sin based on last_phase
                nl_wr_en_1                          = addmul_last_phase;
                nl_wr_en                            = ~addmul_last_phase;
                nl_intermediate_1_d                 = opgrp_outputs[fpnew_pkg::ADDMUL].result;
                nl_intermediate_0_d                 = opgrp_outputs[fpnew_pkg::ADDMUL].result;
                operands_add[0]                     = addmul_inter_23_sel;
                operands_add[1]                     = COS_C2_VEC;
                operands_add[2]                     = C_ONE_VEC;
                nl_opmode_add                       = 1'b0;
                nl_op_add                           = fpnew_pkg::FMADD;
                nl_rnd_add                          = fpnew_pkg::RNE;
                fadd_tag                            = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
                fadd_tag.nl_phase                   = NL_WAIT;
                out_opgrp_ready[fpnew_pkg::ADDMUL]  = 1'b1; 
                next_uop_ready            = 1'b1;
              end

              NL_WAIT: begin
              end

              default : begin
              end
            endcase
          end
        end
      endcase
    end
  end

  // ========================
  //  OUTPUT POST-PROCESSING 
  // ========================
  
  logic        use_last_phase_postproc;
  logic [63:0] nl_intermediate_sel;
  logic [3:0]  k_int_sel;
  // todo: Gating these signals
  assign use_last_phase_postproc = opgrp_outputs[fpnew_pkg::ADDMUL].tag.last_phase;
  assign nl_intermediate_sel     = use_last_phase_postproc ? nl_intermediate_1_q : nl_intermediate_0_q;
  assign k_int_sel               = use_last_phase_postproc ? k_int_q_1 : k_int_q_0;

  always_comb begin : output_postproc
    // Default assignment
    reconstructed_result = opgrp_outputs[fpnew_pkg::ADDMUL].result; 
    
    // Reset intermediate variables
    x32_0 = 32'b0; x32_1 = 32'b0;
    sign_0 = 1'b0; sign_1 = 1'b0;
    
    // --- TANH Reconstruction ---
    if (opgrp_outputs[fpnew_pkg::ADDMUL].tag.nl_op_sel == TANHS && 
        opgrp_outputs[fpnew_pkg::ADDMUL].tag.nl_phase == NL_WAIT) begin
      
      // Use pre-selected intermediate (removes one mux level from critical path)
      x32_0 = nl_intermediate_sel[63:32];
      x32_1 = nl_intermediate_sel[31:0];
      
      {sign_0, abs32_0} = {x32_0[31], 1'b0, x32_0[30:0]};
      {sign_1, abs32_1} = {x32_1[31], 1'b0, x32_1[30:0]};
      
      is_nan_0    = (abs32_0[30:23] == 8'hFF) && (abs32_0[22:0] != 0);
      is_inf_0    = (abs32_0[30:23] == 8'hFF) && (abs32_0[22:0] == 0);
      clamp_mag_0 = (abs32_0[30:23] >= 8'h80);

      is_nan_1    = (abs32_1[30:23] == 8'hFF) && (abs32_1[22:0] != 0);
      is_inf_1    = (abs32_1[30:23] == 8'hFF) && (abs32_1[22:0] == 0);
      clamp_mag_1 = (abs32_1[30:23] >= 8'h80);

      if (clamp_mag_0 || is_inf_0) reconstructed_result[63:32] = {sign_0, 31'h3F800000}; 
      else if (is_nan_0)           reconstructed_result[63:32] = x32_0; 
      
      if (clamp_mag_1 || is_inf_1) reconstructed_result[31:0]  = {sign_1, 31'h3F800000}; 
      else if (is_nan_1)           reconstructed_result[31:0]  = x32_1; 
    end

    // --- COSINE Reconstruction ---
    else if (opgrp_outputs[0].tag.nl_op_sel == COS && 
             opgrp_outputs[0].tag.nl_phase == NL_WAIT) begin
      
      // Lane 1 (Upper 32 bits) - use pre-selected k_int
      unique case (k_int_sel[3:2])
        2'b00: {sign_1, x32_1} = {opgrp_outputs[0].result[63],         opgrp_outputs[0].result[63:32]};
        2'b01: {sign_1, x32_1} = {nl_intermediate_sel[63] ^ 1'b1,      nl_intermediate_sel[63:32]};
        2'b10: {sign_1, x32_1} = {opgrp_outputs[0].result[63] ^ 1'b1,  opgrp_outputs[0].result[63:32]};
        2'b11: {sign_1, x32_1} = {nl_intermediate_sel[63],             nl_intermediate_sel[63:32]};
      endcase
      
      // Lane 0 (Lower 32 bits)
      unique case (k_int_sel[1:0])
        2'b00: {sign_0, x32_0} = {opgrp_outputs[0].result[31],        opgrp_outputs[0].result[31:0]};
        2'b01: {sign_0, x32_0} = {nl_intermediate_sel[31] ^ 1'b1,     nl_intermediate_sel[31:0]};
        2'b10: {sign_0, x32_0} = {opgrp_outputs[0].result[31] ^ 1'b1, opgrp_outputs[0].result[31:0]};
        2'b11: {sign_0, x32_0} = {nl_intermediate_sel[31],            nl_intermediate_sel[31:0]};
      endcase
      
      reconstructed_result[63:32] = {sign_1, x32_1[30:0]};
      reconstructed_result[31:0]  = {sign_0, x32_0[30:0]};
    end

    // --- SINE Reconstruction ---
    else if (opgrp_outputs[0].tag.nl_op_sel == SIN && 
             opgrp_outputs[0].tag.nl_phase == NL_WAIT) begin
      
      // Lane 1 (Upper 32 bits)
      unique case (k_int_sel[3:2])
        2'b00: {sign_1, x32_1} = {nl_intermediate_sel[63],            nl_intermediate_sel[63:32]};
        2'b01: {sign_1, x32_1} = {opgrp_outputs[0].result[63],        opgrp_outputs[0].result[63:32]};
        2'b10: {sign_1, x32_1} = {nl_intermediate_sel[63] ^ 1'b1,     nl_intermediate_sel[63:32]};
        2'b11: {sign_1, x32_1} = {opgrp_outputs[0].result[63] ^ 1'b1, opgrp_outputs[0].result[63:32]};
      endcase
      
      // Lane 0 (Lower 32 bits)
      unique case (k_int_sel[1:0])
        2'b00: {sign_0, x32_0} = {nl_intermediate_sel[31],            nl_intermediate_sel[31:0]};
        2'b01: {sign_0, x32_0} = {opgrp_outputs[0].result[31],        opgrp_outputs[0].result[31:0]};
        2'b10: {sign_0, x32_0} = {nl_intermediate_sel[31] ^ 1'b1,     nl_intermediate_sel[31:0]};
        2'b11: {sign_0, x32_0} = {opgrp_outputs[0].result[31] ^ 1'b1, opgrp_outputs[0].result[31:0]};
      endcase

      reconstructed_result[63:32] = {sign_1, x32_1[30:0]};
      reconstructed_result[31:0]  = {sign_0, x32_0[30:0]};
    end
  end

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

  // Precompute values for concatenation
    logic addmul_phase_is_0, addmul_phase_is_wait;
    logic conv_phase_is_1, conv_phase_is_2;
    logic addmul_phase_not_wait_not_0;
    
    assign addmul_phase_is_0           = (opgrp_outputs[fpnew_pkg::ADDMUL].tag.nl_phase == NL_FPU_ISSUE_0);
    assign addmul_phase_is_wait        = (opgrp_outputs[fpnew_pkg::ADDMUL].tag.nl_phase == NL_WAIT);
    assign conv_phase_is_1             = (opgrp_outputs[fpnew_pkg::CONV].tag.nl_phase == NL_FPU_ISSUE_1);
    assign conv_phase_is_2             = (opgrp_outputs[fpnew_pkg::CONV].tag.nl_phase == NL_FPU_ISSUE_2);
    assign addmul_phase_not_wait       = !addmul_phase_is_wait;
    assign addmul_phase_not_wait_not_0 = addmul_out_valid && !addmul_phase_is_wait && !addmul_phase_is_0;
  
    // Pre-computed routing signals for SIN/COS (most complex case)
    logic sincos_route_to_conv_from_addmul;
    logic sincos_route_to_conv_from_conv;
    logic sincos_route_to_addmul_from_conv;
    logic sincos_route_to_addmul_from_addmul;
    
    assign sincos_route_to_conv_from_addmul   = addmul_out_valid && addmul_phase_is_0;
    assign sincos_route_to_conv_from_conv     = conv_out_valid && conv_phase_is_1;
    assign sincos_route_to_addmul_from_conv   = conv_out_valid && conv_phase_is_2;
    assign sincos_route_to_addmul_from_addmul = addmul_phase_not_wait_not_0;
  
    // Pre-computed routing for TANHS and RSQRT (same pattern)
    logic tanhs_rsqrt_route_to_addmul;
    assign tanhs_rsqrt_route_to_addmul = addmul_out_valid && addmul_phase_not_wait;

    // Pre-computed routing for COSHS
    logic coshs_route_to_conv;
    logic coshs_route_to_addmul;
    assign coshs_route_to_conv   = addmul_out_valid && !is_last_uop_q;
    assign coshs_route_to_addmul = conv_out_valid && conv_phase_is_1;

  // -------------------------
  // Generate Operation Blocks
  // -------------------------
  for (genvar opgrp = 0; opgrp < int'(NUM_OPGROUPS); opgrp++) begin : gen_operation_groups
    localparam int unsigned NUM_OPS = fpnew_pkg::num_operands(fpnew_pkg::opgroup_e'(opgrp));

    logic [NUM_FORMATS-1:0][NUM_OPS-1:0] input_boxed;
    fpnew_pkg::roundmode_e rnd_mode_in;
    fpnew_pkg::operation_e op_in;
    logic opmode_in;
    logic [NUM_OPERANDS-1:0][WIDTH-1:0] operands_input;
    TagType opgrp_tag_in;

    always_comb begin : select_opgrp_inputs
      if (nl_concatenate)begin
        unique case(tag_i.nl_op_sel)
          EXPS: begin
            concatenate_in_valid[opgrp] = (in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp))) || (opgrp_out_valid[0] && (opgrp == 3));
            rnd_mode_in     = (opgrp == 3) ?  nl_rnd_conv    :  rnd_mode_i;
            op_in           = (opgrp == 3) ?  nl_op_conv     :  op_i;
            opmode_in       = (opgrp == 3) ?  nl_opmode_conv :  op_mod_i;
            operands_input  = (opgrp == 3) ?  operands_fconv :  operands_i;
            opgrp_tag_in    = (opgrp == 3) ?  fconv_tag      :  tag_i;
          end
          COSHS: begin
            if (coshs_route_to_conv && (opgrp == 3)) begin
              concatenate_in_valid[opgrp] = 1'b1;
              rnd_mode_in     = nl_rnd_conv;
              op_in           = nl_op_conv;
              opmode_in       = nl_opmode_conv;
              operands_input  = operands_fconv;
              opgrp_tag_in    = fconv_tag;

            end else if (coshs_route_to_addmul && (opgrp == 0)) begin
              concatenate_in_valid[opgrp] = 1'b1;
              rnd_mode_in     = nl_rnd_add;
              op_in           = nl_op_add;
              opmode_in       = nl_opmode_add;
              operands_input  = operands_add;
              opgrp_tag_in    = fadd_tag;
            end else begin
              concatenate_in_valid[opgrp] = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
              rnd_mode_in     = rnd_mode_i;
              op_in           = op_i;
              opmode_in       = op_mod_i;
              operands_input  = operands_i;
              opgrp_tag_in    = tag_i;
            end
          end
          TANHS: begin
            if (tanhs_rsqrt_route_to_addmul && (opgrp == 0)) begin
              concatenate_in_valid[opgrp] = 1'b1;
              rnd_mode_in     = nl_rnd_add;
              op_in           = nl_op_add;
              opmode_in       = nl_opmode_add;
              operands_input  = operands_add;
              opgrp_tag_in    = fadd_tag;
            end else begin
              concatenate_in_valid[opgrp] = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
              rnd_mode_in     = rnd_mode_i;
              op_in           = op_i;
              opmode_in       = op_mod_i;
              operands_input  = operands_i;
              opgrp_tag_in    = tag_i;
            end
          end
          LOGS: begin
            concatenate_in_valid[opgrp] = (in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp))) || (opgrp_out_valid[3] && (opgrp == 0));
            rnd_mode_in     = (opgrp == 0) ?  nl_rnd_add    :  rnd_mode_i;
            op_in           = (opgrp == 0) ?  nl_op_add     :  op_i;
            opmode_in       = (opgrp == 0) ?  nl_opmode_add :  op_mod_i;
            operands_input  = (opgrp == 0) ?  operands_add :  operands_fconv;
            opgrp_tag_in    = (opgrp == 0) ?  fadd_tag      :  tag_i;
          end           
          RSQRT: begin
            if (tanhs_rsqrt_route_to_addmul && (opgrp == 0)) begin
              concatenate_in_valid[opgrp] = 1'b1;
              rnd_mode_in     = nl_rnd_add;
              op_in           = nl_op_add;
              opmode_in       = nl_opmode_add;
              operands_input  = operands_add;
              opgrp_tag_in    = fadd_tag;
            end else begin
              concatenate_in_valid[opgrp] = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
              rnd_mode_in     = rnd_mode_i;
              op_in           = op_i;
              opmode_in       = op_mod_i;
              operands_input  = operands_add;
              opgrp_tag_in    = tag_i;
            end
          end
          SIN, COS: begin
            if (in_valid_i)begin
              concatenate_in_valid[opgrp] = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
              rnd_mode_in     = rnd_mode_i;
              op_in           = op_i;
              opmode_in       = op_mod_i;
              operands_input  = operands_i;
              opgrp_tag_in    = tag_i;
            end else if (sincos_route_to_conv_from_addmul && (opgrp == 3)) begin
              concatenate_in_valid[opgrp] = 1'b1;
              rnd_mode_in     = nl_rnd_conv;
              op_in           = nl_op_conv;
              opmode_in       = nl_opmode_conv;
              operands_input  = operands_fconv;
              opgrp_tag_in    = fconv_tag;

            end else if (sincos_route_to_conv_from_conv && (opgrp == 3)) begin
              concatenate_in_valid[opgrp] = 1'b1;
              rnd_mode_in     = nl_rnd_conv;
              op_in           = nl_op_conv;
              opmode_in       = nl_opmode_conv;
              operands_input  = operands_fconv;
              opgrp_tag_in    = fconv_tag;

            end else if (sincos_route_to_addmul_from_conv && (opgrp == 0)) begin
              concatenate_in_valid[opgrp] = 1'b1;
              rnd_mode_in     = nl_rnd_add;
              op_in           = nl_op_add;
              opmode_in       = nl_opmode_add;
              operands_input  = operands_add;
              opgrp_tag_in    = fadd_tag;
            end else if  (sincos_route_to_addmul_from_addmul && (opgrp == 0)) begin 
              concatenate_in_valid[opgrp] = 1'b1;
              rnd_mode_in     = nl_rnd_add;
              op_in           = nl_op_add;
              opmode_in       = nl_opmode_add;
              operands_input  = operands_add;
              opgrp_tag_in    = fadd_tag;
            end else begin
              concatenate_in_valid[opgrp] = 1'b0;
              rnd_mode_in     = rnd_mode_i;
              op_in           = op_i;
              opmode_in       = op_mod_i;
              operands_input  = operands_i;
              opgrp_tag_in    = tag_i;
            end
          end
          default: begin
            concatenate_in_valid[opgrp] = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
            rnd_mode_in     = rnd_mode_i;
            op_in           = op_i;
            opmode_in       = op_mod_i;
            operands_input  = operands_i;
            opgrp_tag_in    = tag_i;
          end
        endcase
      end else begin
        concatenate_in_valid[opgrp] = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
        rnd_mode_in     = rnd_mode_i;
        op_in           = op_i;
        opmode_in       = op_mod_i;
        operands_input  = operands_i;
        opgrp_tag_in    = tag_i;
      end
    end
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
      .operands_i      ( operands_input[NUM_OPS-1:0] ),
      .is_boxed_i      ( input_boxed                 ),
      .rnd_mode_i      ( rnd_mode_in                 ),
      .op_i            ( op_in                       ),
      .op_mod_i        ( opmode_in                   ),
      .src_fmt_i,
      .dst_fmt_i,
      .int_fmt_i,
      .vectorial_op_i,
      .tag_i           ( opgrp_tag_in               ),
      .simd_mask_i     ( simd_mask                  ),
      .in_valid_i      ( concatenate_in_valid[opgrp]),
      .in_ready_o      ( opgrp_in_ready[opgrp]      ),
      .flush_i,
      .result_o        ( opgrp_outputs[opgrp].result ),
      .status_o        ( opgrp_outputs[opgrp].status ),
      .extension_bit_o ( opgrp_ext[opgrp]            ),
      .tag_o           ( opgrp_outputs[opgrp].tag    ),
      .out_valid_o     ( opgrp_out_valid[opgrp]      ),
      .out_ready_i     ( out_opgrp_ready[opgrp]      ),
      .busy_o          ( opgrp_busy[opgrp]           )
    );

    always_comb begin : arbiter_valid_result_mux
     concatenate_out_valid[opgrp] = opgrp_out_valid[opgrp];
     if (opgrp_out_valid[opgrp] && opgrp_outputs[opgrp].tag.nl) begin
       unique case(opgrp_outputs[opgrp].tag.nl_op_sel)
         EXPS: begin
            concatenate_out_valid[opgrp] = (opgrp == 3); 
         end
         COSHS: begin 
            concatenate_out_valid[opgrp] = (opgrp == 0) && is_last_uop_q; 
         end
         TANHS, RSQRT, SIN, COS: begin
            concatenate_out_valid[opgrp] = (opgrp == 0) && (opgrp_outputs[opgrp].tag.nl_phase == NL_WAIT);
         end
         LOGS: begin
            concatenate_out_valid[opgrp] = (opgrp == 0);
         end
         default: begin
            concatenate_out_valid[opgrp] = opgrp_out_valid[opgrp];
         end
       endcase
     end
    end
 
    always_comb begin : arbiter_input_result_mux
        nl_outputs[opgrp] = opgrp_outputs[opgrp];
       if ((opgrp == 0) && (opgrp_outputs[opgrp].tag.nl_op_sel == TANHS ||
                           opgrp_outputs[opgrp].tag.nl_op_sel == COS ||
                           opgrp_outputs[opgrp].tag.nl_op_sel == SIN)) begin
         nl_outputs[opgrp].result = reconstructed_result;
       end
    end

  end

  // ------------------
  // Arbitrate Outputs
  // ------------------
  output_t arbiter_output;

  // Round-Robin arbiter to decide which result to use
  rr_arb_tree #(
    .NumIn     ( NUM_OPGROUPS ),
    .DataType  ( output_t     ),
    .AxiVldRdy ( 1'b1         )
  ) i_arbiter (
    .clk_i,
    .rst_ni,
    .flush_i,
    .rr_i   ( '0                    ),
    .req_i  ( concatenate_out_valid ),
    .gnt_o  ( opgrp_out_ready       ),
    .data_i ( nl_outputs            ),
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
