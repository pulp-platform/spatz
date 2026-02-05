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

module fpnew_top_nl import spatz_pkg::*; #(
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
// Include FF
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

  // Chebyshev coefficients for tanh approximation
  logic [31:0] a_cheby_tanh_sew, b_cheby_tanh_sew, c_cheby_tanh_sew, quake_constant_sew, c3_halves, c1_half,  pio2_hi, cos_c2, sin_s3, c_one;
  logic [63:0] a_cheby_tanh_vec, b_cheby_tanh_vec, c_cheby_tanh_vec, rsqrt_vec, c3_halves_vec, c1_half_vec, pio2_hi_vec, cos_c2_vec, sin_s3_vec, c_one_vec;

  localparam logic [31:0] A_CHEBY_TANH   = 32'h3cd981f2;
  localparam logic [31:0] B_CHEBY_TANH   = 32'hbe69c8ac;
  localparam logic [31:0] C_CHEBY_TANH   = 32'h3f7a84b9;
  localparam logic [31:0] QUAKE_CONSTANT = 32'h5f375928; // Magic constant for rsqrt
  localparam logic [31:0] C3_HALVES      = 32'h3fc00000; // 1.5f
  localparam logic [31:0] C1_HALF        = 32'hbf000000; // 0.5f
  localparam logic [31:0] PIO2_HI        = 32'h3fc90fda; // 1.57079632f
  localparam logic [31:0] COS_C2         = 32'hbefe4f76; // -0.49670f
  localparam logic [31:0] SIN_S3         = 32'hbe2a0903; // -0.16605f
  localparam logic [31:0] C_ONE          = 32'h3f800000; // 1
  // const float C2 = -0.49670f; 
  //   const float S3 = -0.16605f; 
  //   const float INVPIO2 = 0.63661977f;
  //   const float PIO2_HI = 1.57079632f;

  assign a_cheby_tanh_sew   = A_CHEBY_TANH;
  assign b_cheby_tanh_sew   = B_CHEBY_TANH;
  assign c_cheby_tanh_sew   = C_CHEBY_TANH;
  assign quake_constant_sew = QUAKE_CONSTANT;
  assign c3_halves          = C3_HALVES;
  assign c1_half            = C1_HALF;
  assign pio2_hi            = PIO2_HI;
  assign cos_c2             = COS_C2;
  assign sin_s3             = SIN_S3;
  assign c_one              = C_ONE;

  assign a_cheby_tanh_vec = {2{a_cheby_tanh_sew}};
  assign b_cheby_tanh_vec = {2{b_cheby_tanh_sew}};
  assign c_cheby_tanh_vec = {2{c_cheby_tanh_sew}};
  assign rsqrt_vec        = {2{quake_constant_sew}};
  assign c3_halves_vec   = {2{c3_halves}};
  assign c1_half_vec     = {2{c1_half}};
  assign pio2_hi_vec     = {2{pio2_hi}};
  assign cos_c2_vec      = {2{cos_c2}};
  assign sin_s3_vec      = {2{sin_s3}};
  assign c_one_vec       = {2{c_one}};


  // Constant for log approximation

  logic [31:0] log_scale;
  logic [63:0] log_scale_vec;

  localparam logic [31:0] LOG_SCALE_CONST = 32'h33b17218; // log2(e) / 2^23 33b17218

  assign log_scale = LOG_SCALE_CONST;
  assign log_scale_vec = {2{log_scale}};


  
  fpnew_pkg::operation_e nl_op_conv, nl_op_add;
  fpnew_pkg::roundmode_e nl_rnd_conv, nl_rnd_add, nl_rnd;
  assign nl_concatenate = tag_i.nl;

  logic [NUM_OPERANDS-1:0][WIDTH-1:0]  operands_fconv, operands_add;
  
  // Handshake signals for the blocks
  logic [NUM_OPGROUPS-1:0] opgrp_in_ready, opgrp_out_valid, concatenate_out_valid, opgrp_out_ready, opgrp_ext, opgrp_busy, out_opgrp_ready;
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
  logic [NUM_OPGROUPS-1:0] concatenate_in_valid;
  
  logic [31:0] x32_0, abs32_0, x32_1, abs32_1;
  logic        sign_0, sign_1;
  logic [7:0]  exp_0, exp_1;
  logic [22:0] mant_0, mant_1;
  logic        is_nan_0, is_inf_0, clamp_mag_0, is_nan_1, is_inf_1, clamp_mag_1;
  logic [WIDTH-1:0] result_tanh;
  logic next_uop_ready;
  assign next_uop_ready_o = next_uop_ready;

  always_comb begin : select_inputs
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

    x32_0               = 32'b0;
    sign_0              = x32_0[31];
    abs32_0             = {1'b0, x32_0[30:0]};
    exp_0               = abs32_0[30:23];
    mant_0              = abs32_0[22:0];
    is_nan_0            = (exp_0 == 8'hFF) && (mant_0 != 23'h0);
    is_inf_0            = (exp_0 == 8'hFF) && (mant_0 == 23'h0);
    clamp_mag_0         = (exp_0 >= 8'h80); 

    x32_1               = 32'b0;
    sign_1              = x32_1[31];
    abs32_1             = {1'b0, x32_1[30:0]};
    exp_1               = abs32_1[30:23];
    mant_1              = abs32_1[22:0];
    is_nan_1            = (exp_1 == 8'hFF) && (mant_1 != 23'h0);
    is_inf_1            = (exp_1 == 8'hFF) && (mant_1 == 23'h0);
    clamp_mag_1         = (exp_1 >= 8'h80);
    


    unique case(tag_i.nl_op_sel)
      EXPS: begin
        if (nl_concatenate && opgrp_out_valid[fpnew_pkg::ADDMUL]) begin
          operands_fconv[0]                   = opgrp_outputs[0].result;
          operands_fconv[1]                   = '0;
          operands_fconv[2]                   = '0;   
          nl_opmode_conv                      = 1'b0;
          nl_op_conv                          = fpnew_pkg::F2I;
          nl_rnd_conv                         = fpnew_pkg::RTZ;
          fconv_tag                           =  opgrp_outputs[fpnew_pkg::ADDMUL].tag;
          out_opgrp_ready[0]                  = opgrp_in_ready[3]; 
          //out_opgrp_ready[NUM_OPGROUPS-1 :1]  = opgrp_out_ready[NUM_OPGROUPS-1 :1]; 
        end 
      end
      COSHS: begin
        if (nl_concatenate && opgrp_out_valid[fpnew_pkg::ADDMUL] && is_last_uop_q != 1'b1) begin
          operands_fconv[0]                   = opgrp_outputs[0].result;
          operands_fconv[1]                   = '0;
          operands_fconv[2]                   = '0;   
          nl_opmode_conv                      = 1'b0;
          nl_op_conv                          = fpnew_pkg::F2I;
          nl_rnd_conv                         = fpnew_pkg::RTZ;
          fconv_tag                           =  opgrp_outputs[fpnew_pkg::ADDMUL].tag;
          out_opgrp_ready[0]                  = opgrp_in_ready[3]; 
          
        end 
        if (nl_concatenate && opgrp_out_valid[fpnew_pkg::ADDMUL] && opgrp_outputs[0].tag.last_phase != 1'b0) begin
          is_last_uop_d                       = 1'b1;
        end
      
        if (nl_concatenate && opgrp_out_valid[fpnew_pkg::CONV] && opgrp_outputs[3].tag.nl_phase == NL_FPU_ISSUE_1) begin
          operands_add[0]                     = nl_intermediate_0_q;
          operands_add[1]                     = opgrp_outputs[fpnew_pkg::CONV].result;
          operands_add[2]                     = nl_intermediate_0_q;   
          nl_opmode_add                       = 1'b0;
          nl_op_add                           = fpnew_pkg::ADD;
          nl_rnd_add                          = fpnew_pkg::RNE;
          fadd_tag                            = opgrp_outputs[fpnew_pkg::CONV].tag;
          out_opgrp_ready[3]                  = opgrp_in_ready[0]; 
        end

        if (nl_concatenate && opgrp_out_valid[fpnew_pkg::CONV] && opgrp_outputs[3].tag.nl_phase== NL_FPU_ISSUE_0) begin
          nl_wr_en = 1'b1;
          nl_intermediate_0_d                 = opgrp_outputs[fpnew_pkg::CONV].result;
          out_opgrp_ready[3]                  = 1'b1;
          next_uop_ready                      = is_last_uop_q;
        end
        if (tag_o.last_phase) is_last_uop_d   = 1'b0;
      end
      TANHS: begin
        if (nl_concatenate && in_valid_i)begin
          if (tag_i.last_phase) begin
              nl_wr_en_1                      = 1'b1;
              nl_intermediate_1_d             = operands_i[0]; // x
          end else begin
              nl_wr_en                        = 1'b1;
              nl_intermediate_0_d             = operands_i[0]; // x
          end
        end
        if (nl_concatenate && opgrp_out_valid[fpnew_pkg::ADDMUL]) begin
          unique case(opgrp_outputs[0].tag.nl_phase)
            NL_FPU_ISSUE_0: begin
              operands_add[0]                 = opgrp_outputs[fpnew_pkg::ADDMUL].result;
              operands_add[1]                 = a_cheby_tanh_vec;
              operands_add[2]                 = b_cheby_tanh_vec;
              nl_opmode_add                   = 1'b0;
              nl_op_add                       = fpnew_pkg::FMADD;
              nl_rnd_add                      = fpnew_pkg::RNE;
              fadd_tag                        = opgrp_outputs[0].tag;
              fadd_tag.nl_phase               = NL_FPU_ISSUE_1;
              out_opgrp_ready[fpnew_pkg::ADDMUL]                = 1'b1; 
              if (opgrp_outputs[0].tag.last_phase) begin
                nl_wr_en_3                    = 1'b1; 
                nl_intermediate_3_d           = opgrp_outputs[0].result;
              end else begin
                nl_wr_en_2                    = 1'b1; 
                nl_intermediate_2_d           = opgrp_outputs[0].result;
              end
              
            end

            NL_FPU_ISSUE_1: begin
              operands_add[1]                   = opgrp_outputs[0].result;
              operands_add[2]                   = c_cheby_tanh_vec;
              if (opgrp_outputs[fpnew_pkg::ADDMUL].tag.last_phase) begin
                operands_add[0]                 = nl_intermediate_3_q;   
              end else begin
                operands_add[0]                 = nl_intermediate_2_q;   
              end
              nl_opmode_add                     = 1'b0;
              nl_op_add                         = fpnew_pkg::FMADD;
              nl_rnd_add                        = fpnew_pkg::RNE;
              fadd_tag                          = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
              fadd_tag.nl_phase                 =  NL_FPU_ISSUE_2;
              out_opgrp_ready[fpnew_pkg::ADDMUL]                = 1'b1;
            end
            NL_FPU_ISSUE_2: begin
              operands_add[0]                   = opgrp_outputs[fpnew_pkg::ADDMUL].result;
              if (opgrp_outputs[fpnew_pkg::ADDMUL].tag.last_phase) begin
                operands_add[1]                 = nl_intermediate_1_q;  
              end else begin
                operands_add[1]                 = nl_intermediate_0_q;
                next_uop_ready                  = 1'b1;   
              end 
              nl_opmode_add                     = 1'b0;
              nl_op_add                         = fpnew_pkg::MUL;
              nl_rnd_add                        = fpnew_pkg::RNE;
              fadd_tag                          = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
              fadd_tag.nl_phase                 = NL_WAIT;
              out_opgrp_ready[fpnew_pkg::ADDMUL]                = 1'b1;
            end
            NL_WAIT: begin
              
              result_tanh = opgrp_outputs[fpnew_pkg::ADDMUL].result;
              if (opgrp_outputs[fpnew_pkg::ADDMUL].tag.last_phase) begin
                x32_0   = nl_intermediate_1_q[63:32];
                x32_1   = nl_intermediate_1_q[31:0];
              end else begin
                x32_0   = nl_intermediate_0_q[63:32];
                x32_1   = nl_intermediate_0_q[31:0];
              end
              sign_0              = x32_0[31];
              abs32_0             = {1'b0, x32_0[30:0]};
              exp_0               = abs32_0[30:23];
              mant_0              = abs32_0[22:0];
              is_nan_0            = (exp_0 == 8'hFF) && (mant_0 != 23'h0);
              is_inf_0            = (exp_0 == 8'hFF) && (mant_0 == 23'h0);
              clamp_mag_0         = (exp_0 >= 8'h80); 

              sign_1              = x32_1[31];
              abs32_1             = {1'b0, x32_1[30:0]};
              exp_1               = abs32_1[30:23];
              mant_1              = abs32_1[22:0];
              is_nan_1            = (exp_1 == 8'hFF) && (mant_1 != 23'h0);
              is_inf_1            = (exp_1 == 8'hFF) && (mant_1 == 23'h0);
              clamp_mag_1         = (exp_1 >= 8'h80);

              if (clamp_mag_0 || is_inf_0) begin
                result_tanh[63:32]      = {sign_0, 31'h3F800000}; // +/-1.0f
              end else if (is_nan_0) begin
                result_tanh[63:32]      = x32_0;                  // propagate NaN
              end
              if (clamp_mag_1 || is_inf_1) begin
                result_tanh[31:0]       = {sign_1, 31'h3F800000}; // +/-1.0f
              end else if (is_nan_1) begin
                result_tanh[31:0]       = x32_1;                  // propagate NaN
              end
            end
          endcase 
        end
      end
      LOGS: begin
        if (nl_concatenate && in_valid_i) begin
            x32_0 = operands_i[0][63:32];
            x32_1 = operands_i[0][31:0];

            sign_0 = x32_0[31];
            sign_1 = x32_1[31];
            is_nan_0 = (x32_0[30:23] == 8'hFF) && (x32_0[22:0] != 23'h0);
            is_nan_1 = (x32_1[30:23] == 8'hFF) && (x32_1[22:0] != 23'h0);

            if (sign_0 || is_nan_0) begin
                result_tanh[63:32] = 32'h7FC00000; 
            end else begin
                result_tanh[63:32] = $signed(x32_0) - $signed(32'h3F800000); 
            end

            if (sign_1 || is_nan_1) begin
                result_tanh[31:0]  = 32'h7FC00000; 
            end else begin
                result_tanh[31:0]  = $signed(x32_1) - $signed(32'h3F800000);
            end

            operands_fconv[0]   = result_tanh;
            nl_op_conv          = fpnew_pkg::I2F; 
            nl_rnd_conv         = fpnew_pkg::RNE; 
            fconv_tag           = tag_i;
            out_opgrp_ready[0]  = opgrp_in_ready[3]; 
        end
        if (nl_concatenate && opgrp_out_valid[fpnew_pkg::CONV]) begin

            operands_add[0]    = log_scale_vec; 
            operands_add[1]    = opgrp_outputs[fpnew_pkg::CONV].result;
            nl_op_add          = fpnew_pkg::MUL;
            nl_rnd_add         = fpnew_pkg::RNE;
            nl_opmode_add      = 1'b0;
            fadd_tag           = opgrp_outputs[fpnew_pkg::CONV].tag;
            out_opgrp_ready[3] = 1'b1;
        end
      end
      RSQRT: begin
        if (nl_concatenate && in_valid_i) begin
          if (tag_i.last_phase) begin
              nl_wr_en_1                        = 1'b1;
              nl_intermediate_1_d               = operands_i[0]; // x
          end else begin
              nl_wr_en                        = 1'b1;
              nl_intermediate_0_d               = operands_i[0]; // x
          end
        
          x32_0 = operands_i[0][63:32];
          x32_1 = operands_i[0][31:0];

          is_nan_0 = (x32_0[30:23] == 8'hFF) && (x32_0[22:0] != 23'h0);
          is_nan_1 = (x32_1[30:23] == 8'hFF) && (x32_1[22:0] != 23'h0);

          if (sign_0 || is_nan_0) begin
              result_tanh[63:32] = 32'h7FC00000; 
          end else begin
              result_tanh[63:32] = $signed(quake_constant_sew) - $signed(x32_0 >> 1); 
          end

          if (sign_1 || is_nan_1) begin
              result_tanh[31:0]  = 32'h7FC00000; 
          end else begin
              result_tanh[31:0]  = $signed(quake_constant_sew) - $signed(x32_1 >> 1);
          end

          operands_add[0]    = result_tanh;
          operands_add[1]    = result_tanh;
          nl_op_add           = fpnew_pkg::MUL; 
          nl_rnd_add          = fpnew_pkg::RNE; 
          fadd_tag            = tag_i;
          if (tag_i.last_phase) begin
            nl_wr_en_3          = 1'b1;
            nl_intermediate_3_d = result_tanh;
          end else begin
            nl_wr_en_2          = 1'b1;
            nl_intermediate_2_d = result_tanh;
          end
        end
        if (nl_concatenate && opgrp_out_valid[fpnew_pkg::ADDMUL]) begin
          unique case(opgrp_outputs[0].tag.nl_phase) 
          
            NL_FPU_ISSUE_0: begin
              if (opgrp_outputs[0].tag.last_phase) begin
                operands_add[0]     = nl_intermediate_1_q; 
              end else begin
                operands_add[0]     = nl_intermediate_0_q;
              end
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
              operands_add[1]       = c1_half_vec;
              operands_add[2]       = c3_halves_vec;
              nl_op_add             = fpnew_pkg::FMADD;
              nl_rnd_add            = fpnew_pkg::RNE;
              nl_opmode_add         = 1'b0;
              fadd_tag              = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
              fadd_tag.nl_phase     = NL_FPU_ISSUE_2;
              out_opgrp_ready[0]    = 1'b1;
            end

            NL_FPU_ISSUE_2: begin
              if (opgrp_outputs[0].tag.last_phase) begin
                operands_add[0]     = nl_intermediate_3_q; 
              end else begin
                operands_add[0]     = nl_intermediate_2_q; 
              end
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
        if (in_valid_i && nl_concatenate) begin
          if (tag_i.last_phase) begin
              nl_wr_en_1                        = 1'b1;
              nl_intermediate_1_d               = operands_i[0]; // x
          end else begin
              nl_wr_en                        = 1'b1;
              nl_intermediate_0_d               = operands_i[0]; // x
          end
        end
        if (nl_concatenate && opgrp_out_valid[fpnew_pkg::CONV]) begin
          unique case (opgrp_outputs[3].tag.nl_phase)
            NL_FPU_ISSUE_1: begin
              if (opgrp_outputs[3].tag.last_phase) begin
                k_en_1          = 1'b1;
                k_int_d_1[3:2]  = opgrp_outputs[fpnew_pkg::CONV].result[33:32]; // k_int 
                k_int_d_1[1:0]  = opgrp_outputs[fpnew_pkg::CONV].result[1:0]; // k_int
              end else begin
                k_en_0          = 1'b1;
                k_int_d_0[3:2]  = opgrp_outputs[fpnew_pkg::CONV].result[33:32]; // k_int 
                k_int_d_0[1:0]  = opgrp_outputs[fpnew_pkg::CONV].result[1:0]; // k_int
              end
              operands_fconv[0]      = opgrp_outputs[fpnew_pkg::CONV].result;
              nl_op_conv             = fpnew_pkg::I2F;
              nl_rnd_conv            = fpnew_pkg::RNE;
              fconv_tag              = opgrp_outputs[fpnew_pkg::CONV].tag;
              fconv_tag.nl_phase     = NL_FPU_ISSUE_2;
              out_opgrp_ready[3]     = 1'b1; 
            end
            NL_FPU_ISSUE_2: begin

              operands_add[0]        = opgrp_outputs[fpnew_pkg::CONV].result; // k
              if (opgrp_outputs[fpnew_pkg::CONV].tag.last_phase) begin
                operands_add[2]      = nl_intermediate_1_q;  //x
              end else begin
                operands_add[2]      = nl_intermediate_0_q;  //x
              end
              operands_add[1]        = pio2_hi_vec; 
              nl_opmode_add          = 1'b0;
              nl_op_add              = fpnew_pkg::FNMSUB;
              nl_rnd_add             = fpnew_pkg::RNE;
              fadd_tag               = opgrp_outputs[fpnew_pkg::CONV].tag;
              fadd_tag.nl_phase      = NL_FPU_ISSUE_3;
              out_opgrp_ready[3]     = opgrp_in_ready[0]; 
            end
          endcase
        end
        
  
        if (nl_concatenate && opgrp_out_valid[fpnew_pkg::ADDMUL]) begin
          unique case (opgrp_outputs[0].tag.nl_phase)
            NL_FPU_ISSUE_0: begin // k = round(X/pipi)
              operands_fconv[0]                   = opgrp_outputs[fpnew_pkg::ADDMUL].result;
              nl_op_conv                          = fpnew_pkg::F2I;
              nl_rnd_conv                         = fpnew_pkg::RNE;
              fconv_tag                           =  opgrp_outputs[fpnew_pkg::ADDMUL].tag;
              fconv_tag.nl_phase                  = NL_FPU_ISSUE_1;
              out_opgrp_ready[0]                  = opgrp_in_ready[3]; 
              
            end

            NL_FPU_ISSUE_3: begin //Z= R^2
              if (opgrp_outputs[0].tag.last_phase) begin
                nl_wr_en_1          = 1'b1;
                nl_intermediate_1_d = opgrp_outputs[fpnew_pkg::ADDMUL].result;; //r 
              end else begin
                nl_wr_en            = 1'b1;
                nl_intermediate_0_d = opgrp_outputs[fpnew_pkg::ADDMUL].result;; // r
              end
              operands_add[0]                     = opgrp_outputs[fpnew_pkg::ADDMUL].result;
              operands_add[1]                     = opgrp_outputs[fpnew_pkg::ADDMUL].result;; //r 
              nl_opmode_add                       = 1'b0;
              nl_op_add                           = fpnew_pkg::MUL;
              nl_rnd_add                          = fpnew_pkg::RNE;
              fadd_tag                            = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
              fadd_tag.nl_phase                   = NL_FPU_ISSUE_4;
              out_opgrp_ready[fpnew_pkg::ADDMUL]  = 1'b1; 
            end

            NL_FPU_ISSUE_4: begin //1 + s3z 
              if (opgrp_outputs[0].tag.last_phase) begin
                nl_wr_en_3            = 1'b1;
                nl_intermediate_3_d   = opgrp_outputs[fpnew_pkg::ADDMUL].result;; //z 
              end else begin
                nl_wr_en_2            = 1'b1;
                nl_intermediate_2_d   = opgrp_outputs[fpnew_pkg::ADDMUL].result;; // z
              end
              operands_add[0]                     = opgrp_outputs[fpnew_pkg::ADDMUL].result; //z
              operands_add[1]                     = sin_s3_vec; //c3
              operands_add[2]                     = c_one_vec; //1
              nl_opmode_add                       = 1'b0;
              nl_op_add                           = fpnew_pkg::FMADD;
              nl_rnd_add                          = fpnew_pkg::RNE;
              fadd_tag                            = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
              fadd_tag.nl_phase                   = NL_FPU_ISSUE_5;
              out_opgrp_ready[fpnew_pkg::ADDMUL]  = 1'b1; 
            end
            NL_FPU_ISSUE_5: begin // final mul SIN 
              operands_add[0]                     = opgrp_outputs[fpnew_pkg::ADDMUL].result; 
              if (opgrp_outputs[0].tag.last_phase) begin
                operands_add[1]                   = nl_intermediate_1_q;  
              end else begin
                operands_add[1]                   = nl_intermediate_0_q;  
              end
              nl_opmode_add                       = 1'b0;
              nl_op_add                           = fpnew_pkg::MUL;
              nl_rnd_add                          = fpnew_pkg::RNE;
              fadd_tag                            = opgrp_outputs[fpnew_pkg::ADDMUL].tag; 
              fadd_tag.nl_phase                   = NL_FPU_ISSUE_6;
              out_opgrp_ready[fpnew_pkg::ADDMUL]  = 1'b1; 
            end
            
            NL_FPU_ISSUE_6: begin //cos = 1+ c2*z
              if (opgrp_outputs[0].tag.last_phase) begin
                nl_wr_en_1            = 1'b1;
                nl_intermediate_1_d   = opgrp_outputs[fpnew_pkg::ADDMUL].result;; //sin 
              end else begin
                nl_wr_en              = 1'b1;
                nl_intermediate_0_d   = opgrp_outputs[fpnew_pkg::ADDMUL].result;; // sin
              end
              if (opgrp_outputs[0].tag.last_phase) begin
                operands_add[0]                     = nl_intermediate_3_q;  //z
              end else begin
                operands_add[0]                     = nl_intermediate_2_q;  //z
              end
              operands_add[1]                     = cos_c2_vec; //c2
              operands_add[2]                     = c_one_vec; //1
              nl_opmode_add                       = 1'b0;
              nl_op_add                           = fpnew_pkg::FMADD;
              nl_rnd_add                          = fpnew_pkg::RNE;
              fadd_tag                            = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
              fadd_tag.nl_phase                   = NL_WAIT;
              out_opgrp_ready[fpnew_pkg::ADDMUL]  = 1'b1; 
              next_uop_ready            = 1'b1;
            end

            NL_WAIT: begin
              if (opgrp_outputs[0].tag.nl_op_sel == COS) begin
                if (opgrp_outputs[0].tag.last_phase) begin
                  unique case (k_int_q_1[3:2])
                    2'b00: begin
                      x32_1 = opgrp_outputs[0].result[63:32];
                      sign_1 = x32_1[31];
                    end
                    2'b01: begin
                      x32_1 = nl_intermediate_1_q[63:32];
                      sign_1 = x32_1[31]^1'b1;
                    end
                    2'b10: begin
                      x32_1 = opgrp_outputs[0].result[63:32];
                      sign_1 = x32_1[31]^1'b1;
                    end
                    2'b11: begin
                      x32_1 = nl_intermediate_1_q[63:32];
                      sign_1 = x32_1[31];
                    end
                  endcase
                  unique case (k_int_q_1[1:0])
                    2'b00: begin
                      x32_0 = opgrp_outputs[0].result[31:0];
                      sign_0 = x32_0[31];
                    end
                    2'b01: begin
                      x32_0 = nl_intermediate_1_q[31:0];
                      sign_0 = x32_0[31]^1'b1;
                    end
                    2'b10: begin
                      x32_0 = opgrp_outputs[0].result[31:0];
                      sign_0 = x32_0[31]^1'b1;
                    end
                    2'b11: begin
                      x32_0 = nl_intermediate_1_q[31:0];
                      sign_0 = x32_0[31];
                    end
                  endcase
                end else begin
                  unique case (k_int_q_0[3:2])
                    2'b00: begin
                      x32_1 = opgrp_outputs[0].result[63:32];
                      sign_1 = x32_1[31];
                    end
                    2'b01: begin
                      x32_1 = nl_intermediate_0_q[63:32];
                      sign_1 = x32_1[31]^1'b1;
                    end
                    2'b10: begin
                      x32_1 = opgrp_outputs[0].result[63:32];
                      sign_1 = x32_1[31]^1'b1;
                    end
                    2'b11: begin
                      x32_1 = nl_intermediate_0_q[63:32];
                      sign_1 = x32_1[31];
                    end
                  endcase
                  unique case (k_int_q_0[1:0])
                    2'b00: begin
                      x32_0 = opgrp_outputs[0].result[31:0];
                      sign_0 = x32_0[31];
                    end
                    2'b01: begin
                      x32_0 = nl_intermediate_0_q[31:0];
                      sign_0 = x32_0[31]^1'b1;
                    end
                    2'b10: begin
                      x32_0 = opgrp_outputs[0].result[31:0];
                      sign_0 = x32_0[31]^1'b1;
                    end
                    2'b11: begin
                      x32_0 = nl_intermediate_0_q[31:0];
                      sign_0 = x32_0[31];
                    end
                  endcase
                end
              end else begin
                if (opgrp_outputs[0].tag.last_phase) begin
                  unique case (k_int_q_1[3:2])
                    2'b00: begin
                      x32_1 = nl_intermediate_1_q[63:32];
                      sign_1 = x32_1[31];
                    end
                    2'b01: begin
                      x32_1 = opgrp_outputs[0].result[63:32];
                      sign_1 = x32_1[31];
                    end
                    2'b10: begin
                      x32_1 = nl_intermediate_1_q[63:32];
                      sign_1 = x32_1[31]^1'b1;
                    end
                    2'b11: begin
                      x32_1 = opgrp_outputs[0].result[63:32];
                      sign_1 = x32_1[31]^1'b1;
                    end
                  endcase
                  unique case (k_int_q_1[1:0])
                    2'b00: begin
                      x32_0 = nl_intermediate_1_q[31:0];
                      sign_0 = x32_0[31];
                    end
                    2'b01: begin
                      x32_0 = opgrp_outputs[0].result[31:0];
                      sign_0 = x32_0[31];
                    end
                    2'b10: begin
                      x32_0 = nl_intermediate_1_q[31:0];
                      sign_0 = x32_0[31]^1'b1;
                    end
                    2'b11: begin
                      x32_0 = opgrp_outputs[0].result[31:0];
                      sign_0 = x32_0[31]^1'b1;
                    end
                  endcase
                end else begin
                  unique case (k_int_q_0[3:2])
                    2'b00: begin
                      x32_1 = nl_intermediate_0_q[63:32];
                      sign_1 = x32_1[31];
                    end
                    2'b01: begin
                      x32_1 = opgrp_outputs[0].result[63:32];
                      sign_1 = x32_1[31];
                    end
                    2'b10: begin
                      x32_1 = nl_intermediate_0_q[63:32];
                      sign_1 = x32_1[31]^1'b1;
                    end
                    2'b11: begin
                      x32_1 = opgrp_outputs[0].result[63:32];
                      sign_1 = x32_1[31]^1'b1;
                    end
                  endcase
                  unique case (k_int_q_0[1:0])
                    2'b00: begin
                      x32_0 = nl_intermediate_0_q[31:0];
                      sign_0 = x32_0[31];
                    end
                    2'b01: begin
                      x32_0 = opgrp_outputs[0].result[31:0];
                      sign_0 = x32_0[31];
                    end
                    2'b10: begin
                      x32_0 = nl_intermediate_0_q[31:0];
                      sign_0 = x32_0[31]^1'b1;
                    end
                    2'b11: begin
                      x32_0 = opgrp_outputs[0].result[31:0];
                      sign_0 = x32_0[31]^1'b1;
                    end
                  endcase
                end
              end
              result_tanh[63:32]      = {sign_1, x32_1[30:0]};
              result_tanh[31:0]       = {sign_0, x32_0[30:0]};
              
            end

            default : begin
              
            end
          endcase
        end
      end
    endcase
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
      unique case(tag_i.nl_op_sel)
        EXPS: begin
          if (nl_concatenate) begin
            concatenate_in_valid[opgrp] = (in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp))) || (opgrp_out_valid[0] && (opgrp == 3));
            rnd_mode_in     = (opgrp == 3) ?  nl_rnd_conv    :  rnd_mode_i;
            op_in           = (opgrp == 3) ?  nl_op_conv     :  op_i;
            opmode_in       = (opgrp == 3) ?  nl_opmode_conv :  op_mod_i;
            operands_input  = (opgrp == 3) ?  operands_fconv :  operands_i;
            opgrp_tag_in    = (opgrp == 3) ?  fconv_tag      :  tag_i;
          end else begin
            concatenate_in_valid[opgrp] = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
            rnd_mode_in     = rnd_mode_i;
            op_in           = op_i;
            opmode_in       = op_mod_i;
            operands_input  = operands_i;
            opgrp_tag_in    = tag_i;
          end
        end
        COSHS: begin
            // Default (fallback): pass-through input
          concatenate_in_valid[opgrp] =
              in_valid_i &&
              (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));

          rnd_mode_in     = rnd_mode_i;
          op_in           = op_i;
          opmode_in       = op_mod_i;
          operands_input  = operands_i;
          opgrp_tag_in    = tag_i;

          if (nl_concatenate) begin
            if (opgrp_out_valid[fpnew_pkg::ADDMUL] && (opgrp == 3) && is_last_uop_q != 1'b1) begin
              concatenate_in_valid[opgrp] = 1'b1;
              rnd_mode_in     = nl_rnd_conv;
              op_in           = nl_op_conv;
              opmode_in       = nl_opmode_conv;
              operands_input  = operands_fconv;
              opgrp_tag_in    = fconv_tag;

            end else if (opgrp_out_valid[fpnew_pkg::CONV] && (opgrp == 0) && opgrp_outputs[3].tag.nl_phase == NL_FPU_ISSUE_1) begin
              concatenate_in_valid[opgrp] = 1'b1;
              rnd_mode_in     = nl_rnd_add;
              op_in           = nl_op_add;
              opmode_in       = nl_opmode_add;
              operands_input  = operands_add;
              opgrp_tag_in    = fadd_tag;
            end
          end
        end
        TANHS: begin
          if (opgrp_out_valid[fpnew_pkg::ADDMUL] && (opgrp == 0) && opgrp_outputs[fpnew_pkg::ADDMUL].tag.nl_phase != NL_WAIT) begin
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
          if (nl_concatenate) begin
            concatenate_in_valid[opgrp] = (in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp))) || (opgrp_out_valid[3] && (opgrp == 0));
            rnd_mode_in     = (opgrp == 0) ?  nl_rnd_add    :  rnd_mode_i;
            op_in           = (opgrp == 0) ?  nl_op_add     :  op_i;
            opmode_in       = (opgrp == 0) ?  nl_opmode_add :  op_mod_i;
            operands_input  = (opgrp == 0) ?  operands_add :  operands_fconv;
            opgrp_tag_in    = (opgrp == 0) ?  fadd_tag      :  tag_i;
          end else begin
            concatenate_in_valid[opgrp] = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
            rnd_mode_in     = rnd_mode_i;
            op_in           = op_i;
            opmode_in       = op_mod_i;
            operands_input  = operands_i;
            opgrp_tag_in    = tag_i;
          end
        end
        RSQRT: begin
          if (opgrp_out_valid[fpnew_pkg::ADDMUL] && (opgrp == 0) && opgrp_outputs[fpnew_pkg::ADDMUL].tag.nl_phase != NL_WAIT) begin
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
          // Default (fallback): pass-through input
          concatenate_in_valid[opgrp] = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
          rnd_mode_in     = rnd_mode_i;
          op_in           = op_i;
          opmode_in       = op_mod_i;
          operands_input  = operands_i;
          opgrp_tag_in    = tag_i;
          if (nl_concatenate) begin
            if (in_valid_i)begin
              concatenate_in_valid[opgrp] = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
              rnd_mode_in     = rnd_mode_i;
              op_in           = op_i;
              opmode_in       = op_mod_i;
              operands_input  = operands_i;
              opgrp_tag_in    = tag_i;
            end else if (opgrp_out_valid[fpnew_pkg::ADDMUL] && (opgrp == 3) && opgrp_outputs[fpnew_pkg::ADDMUL].tag.nl_phase == NL_FPU_ISSUE_0) begin
              concatenate_in_valid[opgrp] = 1'b1;
              rnd_mode_in     = nl_rnd_conv;
              op_in           = nl_op_conv;
              opmode_in       = nl_opmode_conv;
              operands_input  = operands_fconv;
              opgrp_tag_in    = fconv_tag;

            end else if (opgrp_out_valid[fpnew_pkg::CONV] && (opgrp == 3) && opgrp_outputs[3].tag.nl_phase == NL_FPU_ISSUE_1) begin
              concatenate_in_valid[opgrp] = 1'b1;
              rnd_mode_in     = nl_rnd_conv;
              op_in           = nl_op_conv;
              opmode_in       = nl_opmode_conv;
              operands_input  = operands_fconv;
              opgrp_tag_in    = fconv_tag;

            end else if (opgrp_out_valid[fpnew_pkg::CONV] && (opgrp == 0) && opgrp_outputs[fpnew_pkg::CONV].tag.nl_phase == NL_FPU_ISSUE_2) begin
              concatenate_in_valid[opgrp] = 1'b1;
              rnd_mode_in     = nl_rnd_add;
              op_in           = nl_op_add;
              opmode_in       = nl_opmode_add;
              operands_input  = operands_add;
              opgrp_tag_in    = fadd_tag;
            end else if  (opgrp_out_valid[fpnew_pkg::ADDMUL] 
                          && opgrp_outputs[fpnew_pkg::ADDMUL].tag.nl_phase != NL_WAIT
                          && opgrp_outputs[fpnew_pkg::ADDMUL].tag.nl_phase != NL_FPU_ISSUE_0
                          && (opgrp == 0) )begin 

              concatenate_in_valid[opgrp] = 1'b1;
              rnd_mode_in     = nl_rnd_add;
              op_in           = nl_op_add;
              opmode_in       = nl_opmode_add;
              operands_input  = operands_add;
              opgrp_tag_in    = fadd_tag;
            end
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
     always_comb begin : mux_out_inp_concat
      if (nl_concatenate) begin
        unique case(tag_i.nl_op_sel)
          EXPS: concatenate_out_valid[opgrp] = opgrp_out_valid[opgrp] & (opgrp == 3);
          COSHS:begin 
            if (is_last_uop_q) concatenate_out_valid[opgrp] = opgrp_out_valid[opgrp] & (opgrp == 0) ; else concatenate_out_valid[opgrp] = '0;
          end
          TANHS:begin
            if (opgrp_outputs[0].tag.nl_phase == NL_WAIT) begin
              concatenate_out_valid[opgrp] = opgrp_out_valid[opgrp] & (opgrp == 0);
            end else begin
              concatenate_out_valid[opgrp] = '0;
            end
          end
          LOGS: concatenate_out_valid[opgrp] = opgrp_out_valid[opgrp] & (opgrp == 0);
          RSQRT: begin
            if (opgrp_outputs[0].tag.nl_phase == NL_WAIT) begin
              concatenate_out_valid[opgrp] = opgrp_out_valid[opgrp] & (opgrp == 0);
            end else begin
              concatenate_out_valid[opgrp] = '0;
            end
          end
          SIN, COS:begin
            if (opgrp_outputs[0].tag.nl_phase == NL_WAIT) begin
                concatenate_out_valid[opgrp] = opgrp_out_valid[opgrp] & (opgrp == 0);
            end else begin
              concatenate_out_valid[opgrp] = '0;
            end
          end
          default : concatenate_out_valid[opgrp] = opgrp_out_valid[opgrp];
          
          
        endcase
      end else begin
        concatenate_out_valid[opgrp] =  opgrp_out_valid[opgrp];
      end
    end

    always_comb begin : clamping 
      nl_outputs[opgrp] = opgrp_outputs[opgrp];
        if (nl_concatenate && tag_i.nl_op_sel == TANHS && (opgrp == 0)) begin
          nl_outputs[opgrp].result = result_tanh;
        end else if ( nl_concatenate && (tag_i.nl_op_sel == COS || tag_i.nl_op_sel == SIN)  && (opgrp == 0) ) begin
          nl_outputs[opgrp].result = result_tanh;
        end else begin
          nl_outputs[opgrp] = opgrp_outputs[opgrp];
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
