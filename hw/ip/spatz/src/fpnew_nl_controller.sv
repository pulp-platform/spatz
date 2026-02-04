// Author: Francesco Murande 
// Description: Controller for Non-Linear operations in FPNew.
//              Encapsulates intermediate state, constants, and sequencing logic.
//              Includes EXPS, COSHS, TANHS, LOGS, RSQRT, SIN, COS.

module fpnew_nl_controller #(
  parameter int unsigned WIDTH        = 64,
  parameter int unsigned NUM_OPERANDS = 3,
  parameter int unsigned NUM_OPGROUPS = 4,
  parameter type         TagType      = logic
) (
  input  logic                               clk_i,
  input  logic                               rst_ni,
  
  // -- Inputs from System --
  input  logic [NUM_OPERANDS-1:0][WIDTH-1:0] operands_i,
  input  TagType                             tag_i,
  input  logic                               in_valid_i,
  
  // -- Feedback from Opgroups (Pipeline Outputs) --
  input  logic [NUM_OPGROUPS-1:0]            opgrp_out_valid_i,
  input  logic [NUM_OPGROUPS-1:0][WIDTH-1:0] opgrp_result_i,
  input  TagType [NUM_OPGROUPS-1:0]          opgrp_tag_i,
  
  // -- Feedback from Opgroups (Pipeline Status) --
  input  logic [NUM_OPGROUPS-1:0]            opgrp_in_ready_i,

  // -- Control Outputs to Opgroups (Pipeline Inputs) --
  output logic [NUM_OPGROUPS-1:0]            nl_use_internal_op_o, // 1 = use internal override
  output logic [NUM_OPGROUPS-1:0]            nl_in_valid_o,        // Force valid high
  
  output logic [NUM_OPGROUPS-1:0][NUM_OPERANDS-1:0][WIDTH-1:0] nl_operands_o,
  output fpnew_pkg::operation_e [NUM_OPGROUPS-1:0]             nl_op_o,
  output logic [NUM_OPGROUPS-1:0]                              nl_op_mod_o,
  output fpnew_pkg::roundmode_e [NUM_OPGROUPS-1:0]             nl_rnd_mode_o,
  output TagType [NUM_OPGROUPS-1:0]                            nl_tag_o,

  // -- Control Outputs for Result Arbitration --
  output logic [NUM_OPGROUPS-1:0]            nl_out_valid_o,       // Masked valid signal
  output logic [NUM_OPGROUPS-1:0]            nl_out_ready_o,       // Ready to consume internal result
  
  output logic [NUM_OPGROUPS-1:0]            nl_result_override_o, // 1 = use patched result
  output logic [WIDTH-1:0]                   nl_result_o,          // The patched result

  // -- Spatz Specific --
  output logic                               next_uop_ready_o
);

  import fpnew_pkg::*;
  import spatz_nl_pkg::*; 

  // -------------------------
  // Registers & Intermediates
  // -------------------------
  logic [WIDTH-1:0] nl_intermediate_0_d, nl_intermediate_1_d, nl_intermediate_0_q, nl_intermediate_1_q;
  logic [WIDTH-1:0] nl_intermediate_2_d, nl_intermediate_3_d, nl_intermediate_2_q, nl_intermediate_3_q;
  logic [3:0]       k_int_q_0, k_int_d_0, k_int_q_1, k_int_d_1;
  logic             k_en_0, k_en_1;
  logic             nl_wr_en, nl_wr_en_1, nl_wr_en_2, nl_wr_en_3;
  logic             is_last_uop_d, is_last_uop_q;

  `include "common_cells/registers.svh"
  `FFL(nl_intermediate_0_q, nl_intermediate_0_d, nl_wr_en, '0)
  `FFL(nl_intermediate_1_q, nl_intermediate_1_d, nl_wr_en_1, '0)
  `FFL(nl_intermediate_2_q, nl_intermediate_2_d, nl_wr_en_2, '0)
  `FFL(nl_intermediate_3_q, nl_intermediate_3_d, nl_wr_en_3, '0)
  `FFL(k_int_q_0, k_int_d_0, k_en_0, '0)
  `FFL(k_int_q_1, k_int_d_1, k_en_1, '0)
  `FF(is_last_uop_q, is_last_uop_d, 1'b0)

  // -------------------------
  // Constants
  // -------------------------
  logic [31:0] a_cheby_tanh_sew, b_cheby_tanh_sew, c_cheby_tanh_sew, quake_constant_sew, c3_halves, c1_half,  pio2_hi, cos_c2, sin_s3, c_one;
  logic [63:0] a_cheby_tanh_vec, b_cheby_tanh_vec, c_cheby_tanh_vec, rsqrt_vec, c3_halves_vec, c1_half_vec, pio2_hi_vec, cos_c2_vec, sin_s3_vec, c_one_vec;
  logic [31:0] log_scale;
  logic [63:0] log_scale_vec;

  localparam logic [31:0] A_CHEBY_TANH   = 32'h3cd981f2;
  localparam logic [31:0] B_CHEBY_TANH   = 32'hbe69c8ac;
  localparam logic [31:0] C_CHEBY_TANH   = 32'h3f7a84b9;
  localparam logic [31:0] QUAKE_CONSTANT = 32'h5f375928; 
  localparam logic [31:0] C3_HALVES      = 32'h3fc00000; // 1.5f
  localparam logic [31:0] C1_HALF        = 32'hbf000000; // 0.5f
  localparam logic [31:0] PIO2_HI        = 32'h3fc90fda; // 1.57...
  localparam logic [31:0] COS_C2         = 32'hbefe4f76;
  localparam logic [31:0] SIN_S3         = 32'hbe2a0903;
  localparam logic [31:0] C_ONE          = 32'h3f800000; // 1
  localparam logic [31:0] LOG_SCALE_CONST = 32'h33b17218; 

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
  assign log_scale          = LOG_SCALE_CONST;

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
  assign log_scale_vec   = {2{log_scale}};

  // -------------------------
  // Internal Signal Definitions
  // -------------------------
  logic nl_concatenate;
  assign nl_concatenate = tag_i.nl;

  // Helper variables for clamping logic
  logic [31:0] x32_0, abs32_0, x32_1, abs32_1;
  logic        sign_0, sign_1;
  logic [7:0]  exp_0, exp_1;
  logic [22:0] mant_0, mant_1;
  logic        is_nan_0, is_inf_0, clamp_mag_0, is_nan_1, is_inf_1, clamp_mag_1;
  logic [WIDTH-1:0] result_tanh;

  // Group operands
  logic [NUM_OPERANDS-1:0][WIDTH-1:0]  operands_fconv, operands_add;
  fpnew_pkg::operation_e nl_op_conv, nl_op_add;
  logic nl_opmode_conv, nl_opmode_add;
  fpnew_pkg::roundmode_e nl_rnd_conv, nl_rnd_add;
  TagType fconv_tag, fadd_tag;

  // -------------------------
  // Combinational Control Logic
  // -------------------------
  always_comb begin : main_control
    // Default values
    nl_use_internal_op_o  = '0;
    nl_in_valid_o         = '0;
    nl_operands_o         = '0;
    nl_op_o               = '0;
    nl_op_mod_o           = '0;
    nl_rnd_mode_o         = '0;
    nl_tag_o              = '0;

    nl_out_ready_o        = '0;
    nl_out_valid_o        = opgrp_out_valid_i; 
    nl_result_override_o  = '0;
    nl_result_o           = '0;
    
    next_uop_ready_o      = 1'b0;

    // Internal loopback defaults
    operands_fconv        = '0;
    operands_add          = '0;
    nl_opmode_conv        = 1'b0;
    nl_opmode_add         = 1'b0;
    nl_op_conv            = fpnew_pkg::F2I;
    nl_op_add             = fpnew_pkg::ADD;
    nl_rnd_conv           = fpnew_pkg::RTZ;
    nl_rnd_add            = fpnew_pkg::RNE;
    fconv_tag             = '0;
    fadd_tag              = '0;

    // Register write enables
    nl_wr_en = 1'b0; nl_wr_en_1 = 1'b0; nl_wr_en_2 = 1'b0; nl_wr_en_3 = 1'b0;
    k_en_0 = 1'b0; k_en_1 = 1'b0;
    nl_intermediate_0_d = nl_intermediate_0_q;
    nl_intermediate_1_d = nl_intermediate_1_q;
    nl_intermediate_2_d = nl_intermediate_2_q;
    nl_intermediate_3_d = nl_intermediate_3_q;
    k_int_d_0 = k_int_q_0;
    k_int_d_1 = k_int_q_1;
    is_last_uop_d = is_last_uop_q;

    // Default Clamping Vars
    x32_0 = 32'b0; sign_0 = 0; abs32_0 = 0; exp_0 = 0; mant_0 = 0; is_nan_0 = 0; is_inf_0 = 0; clamp_mag_0 = 0;
    x32_1 = 32'b0; sign_1 = 0; abs32_1 = 0; exp_1 = 0; mant_1 = 0; is_nan_1 = 0; is_inf_1 = 0; clamp_mag_1 = 0;
    result_tanh = '0;

    unique case(tag_i.nl_op_sel)
      EXPS: begin
        if (nl_concatenate && opgrp_out_valid_i[fpnew_pkg::ADDMUL]) begin
          operands_fconv[0] = opgrp_result_i[fpnew_pkg::ADDMUL];
          nl_op_conv        = fpnew_pkg::F2I;
          nl_rnd_conv       = fpnew_pkg::RTZ;
          fconv_tag         = opgrp_tag_i[fpnew_pkg::ADDMUL];
          
          nl_use_internal_op_o[3] = 1'b1;
          nl_in_valid_o[3]        = 1'b1;
          
          nl_out_ready_o[fpnew_pkg::ADDMUL] = opgrp_in_ready_i[3];
          nl_out_valid_o[fpnew_pkg::ADDMUL] = 1'b0; 
        end 
      end

      COSHS: begin
         if (nl_concatenate && opgrp_out_valid_i[fpnew_pkg::ADDMUL] && !is_last_uop_q) begin
            operands_fconv[0] = opgrp_result_i[fpnew_pkg::ADDMUL];
            nl_op_conv        = fpnew_pkg::F2I;
            nl_rnd_conv       = fpnew_pkg::RTZ;
            fconv_tag         = opgrp_tag_i[fpnew_pkg::ADDMUL];
            
            nl_use_internal_op_o[3] = 1'b1; 
            nl_in_valid_o[3]        = 1'b1;
            nl_out_ready_o[fpnew_pkg::ADDMUL] = opgrp_in_ready_i[3];
            nl_out_valid_o[fpnew_pkg::ADDMUL] = 1'b0; 
         end
         if (nl_concatenate && opgrp_out_valid_i[fpnew_pkg::ADDMUL] && opgrp_tag_i[0].last_phase != 1'b0) begin
            is_last_uop_d = 1'b1;
         end
         if (nl_concatenate && opgrp_out_valid_i[fpnew_pkg::CONV] && opgrp_tag_i[3].nl_phase == NL_FPU_ISSUE_1) begin
            operands_add[0] = nl_intermediate_0_q;
            operands_add[1] = opgrp_result_i[fpnew_pkg::CONV];
            operands_add[2] = nl_intermediate_0_q;
            nl_op_add       = fpnew_pkg::ADD;
            nl_rnd_add      = fpnew_pkg::RNE;
            fadd_tag        = opgrp_tag_i[fpnew_pkg::CONV];
            nl_use_internal_op_o[0] = 1'b1; 
            nl_in_valid_o[0]        = 1'b1;
            nl_out_ready_o[fpnew_pkg::CONV] = opgrp_in_ready_i[0];
            nl_out_valid_o[fpnew_pkg::CONV] = 1'b0;
         end
         if (nl_concatenate && opgrp_out_valid_i[fpnew_pkg::CONV] && opgrp_tag_i[3].nl_phase == NL_FPU_ISSUE_0) begin
            nl_wr_en = 1'b1;
            nl_intermediate_0_d = opgrp_result_i[fpnew_pkg::CONV];
            nl_out_ready_o[3]   = 1'b1; 
            nl_out_valid_o[3]   = 1'b0; 
            next_uop_ready_o    = is_last_uop_q;
         end
         if (nl_tag_o[0].last_phase) is_last_uop_d = 1'b0;
      end

      TANHS: begin
         if (nl_concatenate && in_valid_i) begin
             if (tag_i.last_phase) begin
                 nl_wr_en_1 = 1'b1; nl_intermediate_1_d = operands_i[0];
             end else begin
                 nl_wr_en = 1'b1; nl_intermediate_0_d = operands_i[0];
             end
         end
         if (nl_concatenate && opgrp_out_valid_i[fpnew_pkg::ADDMUL]) begin
             unique case(opgrp_tag_i[0].nl_phase)
                 NL_FPU_ISSUE_0: begin
                     operands_add[0] = opgrp_result_i[fpnew_pkg::ADDMUL];
                     operands_add[1] = a_cheby_tanh_vec;
                     operands_add[2] = b_cheby_tanh_vec;
                     nl_op_add = fpnew_pkg::FMADD;
                     fadd_tag = opgrp_tag_i[0];
                     fadd_tag.nl_phase = NL_FPU_ISSUE_1;
                     nl_use_internal_op_o[0] = 1'b1; nl_in_valid_o[0] = 1'b1;
                     nl_out_ready_o[0] = 1'b1; nl_out_valid_o[0] = 1'b0;
                     if (opgrp_tag_i[0].last_phase) begin
                         nl_wr_en_3 = 1'b1; nl_intermediate_3_d = opgrp_result_i[0];
                     end else begin
                         nl_wr_en_2 = 1'b1; nl_intermediate_2_d = opgrp_result_i[0];
                     end
                 end
                 NL_FPU_ISSUE_1: begin
                     operands_add[1] = opgrp_result_i[0];
                     operands_add[2] = c_cheby_tanh_vec;
                     operands_add[0] = opgrp_tag_i[0].last_phase ? nl_intermediate_3_q : nl_intermediate_2_q;
                     nl_op_add = fpnew_pkg::FMADD;
                     fadd_tag = opgrp_tag_i[0];
                     fadd_tag.nl_phase = NL_FPU_ISSUE_2;
                     nl_use_internal_op_o[0] = 1'b1; nl_in_valid_o[0] = 1'b1;
                     nl_out_ready_o[0] = 1'b1; nl_out_valid_o[0] = 1'b0;
                 end
                 NL_FPU_ISSUE_2: begin
                     operands_add[0] = opgrp_result_i[0];
                     operands_add[1] = opgrp_tag_i[0].last_phase ? nl_intermediate_1_q : nl_intermediate_0_q;
                     if (!opgrp_tag_i[0].last_phase) next_uop_ready_o = 1'b1;
                     nl_op_add = fpnew_pkg::MUL;
                     fadd_tag = opgrp_tag_i[0];
                     fadd_tag.nl_phase = NL_WAIT;
                     nl_use_internal_op_o[0] = 1'b1; nl_in_valid_o[0] = 1'b1;
                     nl_out_ready_o[0] = 1'b1; nl_out_valid_o[0] = 1'b0;
                 end
                 NL_WAIT: begin
                     result_tanh = opgrp_result_i[fpnew_pkg::ADDMUL];
                     if (opgrp_tag_i[0].last_phase) begin
                        x32_0 = nl_intermediate_1_q[63:32]; x32_1 = nl_intermediate_1_q[31:0];
                     end else begin
                        x32_0 = nl_intermediate_0_q[63:32]; x32_1 = nl_intermediate_0_q[31:0];
                     end
                     sign_0 = x32_0[31]; abs32_0 = {1'b0, x32_0[30:0]}; exp_0 = abs32_0[30:23]; mant_0 = abs32_0[22:0];
                     is_nan_0 = (exp_0 == 8'hFF) && (mant_0 != 0); is_inf_0 = (exp_0 == 8'hFF) && (mant_0 == 0); clamp_mag_0 = (exp_0 >= 8'h80);
                     sign_1 = x32_1[31]; abs32_1 = {1'b0, x32_1[30:0]}; exp_1 = abs32_1[30:23]; mant_1 = abs32_1[22:0];
                     is_nan_1 = (exp_1 == 8'hFF) && (mant_1 != 0); is_inf_1 = (exp_1 == 8'hFF) && (mant_1 == 0); clamp_mag_1 = (exp_1 >= 8'h80);

                     if (clamp_mag_0 || is_inf_0) result_tanh[63:32] = {sign_0, 31'h3F800000};
                     else if (is_nan_0) result_tanh[63:32] = x32_0;
                     if (clamp_mag_1 || is_inf_1) result_tanh[31:0] = {sign_1, 31'h3F800000};
                     else if (is_nan_1) result_tanh[31:0] = x32_1;

                     nl_result_override_o[0] = 1'b1;
                     nl_result_o = result_tanh;
                 end
                 default:;
             endcase
         end
      end
      
      LOGS: begin
         if (nl_concatenate && in_valid_i) begin
             x32_0 = operands_i[0][63:32]; x32_1 = operands_i[0][31:0];
             sign_0 = x32_0[31]; sign_1 = x32_1[31];
             is_nan_0 = (x32_0[30:23] == 8'hFF) && (x32_0[22:0] != 0);
             is_nan_1 = (x32_1[30:23] == 8'hFF) && (x32_1[22:0] != 0);
             if (sign_0 || is_nan_0) result_tanh[63:32] = 32'h7FC00000;
             else result_tanh[63:32] = $signed(x32_0) - $signed(32'h3F800000);
             if (sign_1 || is_nan_1) result_tanh[31:0] = 32'h7FC00000;
             else result_tanh[31:0] = $signed(x32_1) - $signed(32'h3F800000);

             operands_fconv[0] = result_tanh;
             nl_op_conv        = fpnew_pkg::I2F;
             nl_rnd_conv       = fpnew_pkg::RNE;
             fconv_tag         = tag_i;
             nl_use_internal_op_o[3] = 1'b1; nl_in_valid_o[3] = 1'b1;
         end
         if (nl_concatenate && opgrp_out_valid_i[fpnew_pkg::CONV]) begin
             operands_add[0] = log_scale_vec;
             operands_add[1] = opgrp_result_i[fpnew_pkg::CONV];
             nl_op_add       = fpnew_pkg::MUL;
             nl_rnd_add      = fpnew_pkg::RNE;
             fadd_tag        = opgrp_tag_i[fpnew_pkg::CONV];
             nl_use_internal_op_o[0] = 1'b1; nl_in_valid_o[0] = 1'b1;
             nl_out_ready_o[3] = 1'b1; nl_out_valid_o[3] = 1'b0;
         end
      end

      RSQRT: begin
        if (nl_concatenate && in_valid_i) begin
          if (tag_i.last_phase) begin
              nl_wr_en_1 = 1'b1; nl_intermediate_1_d = operands_i[0];
          end else begin
              nl_wr_en = 1'b1; nl_intermediate_0_d = operands_i[0];
          end
        
          x32_0 = operands_i[0][63:32]; x32_1 = operands_i[0][31:0];
          sign_0 = x32_0[31]; sign_1 = x32_1[31];
          is_nan_0 = (x32_0[30:23] == 8'hFF) && (x32_0[22:0] != 23'h0);
          is_nan_1 = (x32_1[30:23] == 8'hFF) && (x32_1[22:0] != 23'h0);

          if (sign_0 || is_nan_0) result_tanh[63:32] = 32'h7FC00000;
          else result_tanh[63:32] = $signed(quake_constant_sew) - $signed(x32_0 >> 1);

          if (sign_1 || is_nan_1) result_tanh[31:0]  = 32'h7FC00000;
          else result_tanh[31:0]  = $signed(quake_constant_sew) - $signed(x32_1 >> 1);

          operands_add[0] = result_tanh; operands_add[1] = result_tanh;
          nl_op_add = fpnew_pkg::MUL; nl_rnd_add = fpnew_pkg::RNE; fadd_tag = tag_i;
          
          if (tag_i.last_phase) begin
            nl_wr_en_3 = 1'b1; nl_intermediate_3_d = result_tanh;
          end else begin
            nl_wr_en_2 = 1'b1; nl_intermediate_2_d = result_tanh;
          end
          
          // Initial injection into ADDMUL (Group 0)
          nl_use_internal_op_o[0] = 1'b1; nl_in_valid_o[0] = 1'b1;
        end

        if (nl_concatenate && opgrp_out_valid_i[fpnew_pkg::ADDMUL]) begin
          unique case(opgrp_tag_i[0].nl_phase) 
            NL_FPU_ISSUE_0: begin
              if (opgrp_tag_i[0].last_phase) operands_add[0] = nl_intermediate_1_q;
              else operands_add[0] = nl_intermediate_0_q;
              operands_add[1] = opgrp_result_i[fpnew_pkg::ADDMUL];
              nl_op_add = fpnew_pkg::MUL; nl_rnd_add = fpnew_pkg::RNE; nl_opmode_add = 1'b0;
              fadd_tag = opgrp_tag_i[0]; fadd_tag.nl_phase = NL_FPU_ISSUE_1;
              nl_use_internal_op_o[0] = 1'b1; nl_in_valid_o[0] = 1'b1;
              nl_out_ready_o[0] = 1'b1; nl_out_valid_o[0] = 1'b0;
            end
            NL_FPU_ISSUE_1: begin
              operands_add[0] = opgrp_result_i[0];
              operands_add[1] = c1_half_vec;
              operands_add[2] = c3_halves_vec;
              nl_op_add = fpnew_pkg::FMADD; nl_rnd_add = fpnew_pkg::RNE; nl_opmode_add = 1'b0;
              fadd_tag = opgrp_tag_i[0]; fadd_tag.nl_phase = NL_FPU_ISSUE_2;
              nl_use_internal_op_o[0] = 1'b1; nl_in_valid_o[0] = 1'b1;
              nl_out_ready_o[0] = 1'b1; nl_out_valid_o[0] = 1'b0;
            end
            NL_FPU_ISSUE_2: begin
              if (opgrp_tag_i[0].last_phase) operands_add[0] = nl_intermediate_3_q;
              else operands_add[0] = nl_intermediate_2_q;
              operands_add[1] = opgrp_result_i[0];
              nl_op_add = fpnew_pkg::MUL; nl_rnd_add = fpnew_pkg::RNE; nl_opmode_add = 1'b0;
              fadd_tag = opgrp_tag_i[0]; fadd_tag.nl_phase = NL_WAIT;
              nl_use_internal_op_o[0] = 1'b1; nl_in_valid_o[0] = 1'b1;
              nl_out_ready_o[0] = 1'b1; nl_out_valid_o[0] = 1'b0;
              next_uop_ready_o = 1'b1;
            end
            NL_WAIT: begin 
              // Result valid naturally flows out from ADDMUL in WAIT phase
            end
            default:;
          endcase
        end
      end

      SIN, COS: begin
        if (in_valid_i && nl_concatenate) begin
          if (tag_i.last_phase) begin
              nl_wr_en_1 = 1'b1; nl_intermediate_1_d = operands_i[0];
          end else begin
              nl_wr_en = 1'b1; nl_intermediate_0_d = operands_i[0];
          end
        end

        // Feedback from CONV (Group 3)
        if (nl_concatenate && opgrp_out_valid_i[fpnew_pkg::CONV]) begin
          unique case (opgrp_tag_i[3].nl_phase)
            NL_FPU_ISSUE_1: begin
              if (opgrp_tag_i[3].last_phase) begin
                k_en_1 = 1'b1; k_int_d_1[3:2] = opgrp_result_i[3][33:32]; k_int_d_1[1:0] = opgrp_result_i[3][1:0];
              end else begin
                k_en_0 = 1'b1; k_int_d_0[3:2] = opgrp_result_i[3][33:32]; k_int_d_0[1:0] = opgrp_result_i[3][1:0];
              end
              operands_fconv[0] = opgrp_result_i[3];
              nl_op_conv = fpnew_pkg::I2F; nl_rnd_conv = fpnew_pkg::RNE;
              fconv_tag = opgrp_tag_i[3]; fconv_tag.nl_phase = NL_FPU_ISSUE_2;
              nl_use_internal_op_o[3] = 1'b1; nl_in_valid_o[3] = 1'b1;
              nl_out_ready_o[3] = 1'b1; nl_out_valid_o[3] = 1'b0;
            end
            NL_FPU_ISSUE_2: begin
              operands_add[0] = opgrp_result_i[3]; // k (as float)
              operands_add[1] = pio2_hi_vec;
              if (opgrp_tag_i[3].last_phase) operands_add[2] = nl_intermediate_1_q; // x
              else operands_add[2] = nl_intermediate_0_q; // x
              nl_opmode_add = 1'b0; nl_op_add = fpnew_pkg::FNMSUB; nl_rnd_add = fpnew_pkg::RNE;
              fadd_tag = opgrp_tag_i[3]; fadd_tag.nl_phase = NL_FPU_ISSUE_3;
              nl_use_internal_op_o[0] = 1'b1; nl_in_valid_o[0] = 1'b1; // Send to ADDMUL (Group 0)
              nl_out_ready_o[3] = 1'b1; nl_out_valid_o[3] = 1'b0;
            end
            default:;
          endcase
        end
        
        // Feedback from ADDMUL (Group 0)
        if (nl_concatenate && opgrp_out_valid_i[fpnew_pkg::ADDMUL]) begin
          unique case (opgrp_tag_i[0].nl_phase)
            NL_FPU_ISSUE_0: begin // k = round(X/pipi)
              operands_fconv[0] = opgrp_result_i[0];
              nl_op_conv = fpnew_pkg::F2I; nl_rnd_conv = fpnew_pkg::RNE;
              fconv_tag = opgrp_tag_i[0]; fconv_tag.nl_phase = NL_FPU_ISSUE_1;
              nl_use_internal_op_o[3] = 1'b1; nl_in_valid_o[3] = 1'b1; // Send to CONV (Group 3)
              nl_out_ready_o[0] = 1'b1; nl_out_valid_o[0] = 1'b0;
            end
            NL_FPU_ISSUE_3: begin // Z = R^2
              if (opgrp_tag_i[0].last_phase) begin nl_wr_en_1 = 1'b1; nl_intermediate_1_d = opgrp_result_i[0]; end
              else begin nl_wr_en = 1'b1; nl_intermediate_0_d = opgrp_result_i[0]; end
              operands_add[0] = opgrp_result_i[0]; operands_add[1] = opgrp_result_i[0];
              nl_opmode_add = 1'b0; nl_op_add = fpnew_pkg::MUL; nl_rnd_add = fpnew_pkg::RNE;
              fadd_tag = opgrp_tag_i[0]; fadd_tag.nl_phase = NL_FPU_ISSUE_4;
              nl_use_internal_op_o[0] = 1'b1; nl_in_valid_o[0] = 1'b1;
              nl_out_ready_o[0] = 1'b1; nl_out_valid_o[0] = 1'b0;
            end
            NL_FPU_ISSUE_4: begin // 1 + s3z 
              if (opgrp_tag_i[0].last_phase) begin nl_wr_en_3 = 1'b1; nl_intermediate_3_d = opgrp_result_i[0]; end
              else begin nl_wr_en_2 = 1'b1; nl_intermediate_2_d = opgrp_result_i[0]; end
              operands_add[0] = opgrp_result_i[0]; operands_add[1] = sin_s3_vec; operands_add[2] = c_one_vec;
              nl_opmode_add = 1'b0; nl_op_add = fpnew_pkg::FMADD; nl_rnd_add = fpnew_pkg::RNE;
              fadd_tag = opgrp_tag_i[0]; fadd_tag.nl_phase = NL_FPU_ISSUE_5;
              nl_use_internal_op_o[0] = 1'b1; nl_in_valid_o[0] = 1'b1;
              nl_out_ready_o[0] = 1'b1; nl_out_valid_o[0] = 1'b0;
            end
            NL_FPU_ISSUE_5: begin // final mul SIN 
              operands_add[0] = opgrp_result_i[0];
              if (opgrp_tag_i[0].last_phase) operands_add[1] = nl_intermediate_1_q;
              else operands_add[1] = nl_intermediate_0_q;
              nl_opmode_add = 1'b0; nl_op_add = fpnew_pkg::MUL; nl_rnd_add = fpnew_pkg::RNE;
              fadd_tag = opgrp_tag_i[0]; fadd_tag.nl_phase = NL_FPU_ISSUE_6;
              nl_use_internal_op_o[0] = 1'b1; nl_in_valid_o[0] = 1'b1;
              nl_out_ready_o[0] = 1'b1; nl_out_valid_o[0] = 1'b0;
            end
            NL_FPU_ISSUE_6: begin // cos = 1+ c2*z
              if (opgrp_tag_i[0].last_phase) begin nl_wr_en_1 = 1'b1; nl_intermediate_1_d = opgrp_result_i[0]; end
              else begin nl_wr_en = 1'b1; nl_intermediate_0_d = opgrp_result_i[0]; end
              if (opgrp_tag_i[0].last_phase) operands_add[0] = nl_intermediate_3_q; // z
              else operands_add[0] = nl_intermediate_2_q; // z
              operands_add[1] = cos_c2_vec; operands_add[2] = c_one_vec;
              nl_opmode_add = 1'b0; nl_op_add = fpnew_pkg::FMADD; nl_rnd_add = fpnew_pkg::RNE;
              fadd_tag = opgrp_tag_i[0]; fadd_tag.nl_phase = NL_WAIT;
              nl_use_internal_op_o[0] = 1'b1; nl_in_valid_o[0] = 1'b1;
              nl_out_ready_o[0] = 1'b1; nl_out_valid_o[0] = 1'b0;
              next_uop_ready_o = 1'b1;
            end
            NL_WAIT: begin
              // Final reconstruction based on quadrants (k_int)
              // This is purely combinational fix-up before exiting logic
              if (opgrp_tag_i[0].nl_op_sel == COS) begin
                if (opgrp_tag_i[0].last_phase) begin
                  unique case (k_int_q_1[3:2])
                    2'b00: begin x32_1 = opgrp_result_i[0][63:32]; sign_1 = x32_1[31]; end
                    2'b01: begin x32_1 = nl_intermediate_1_q[63:32]; sign_1 = x32_1[31]^1'b1; end
                    2'b10: begin x32_1 = opgrp_result_i[0][63:32]; sign_1 = x32_1[31]^1'b1; end
                    2'b11: begin x32_1 = nl_intermediate_1_q[63:32]; sign_1 = x32_1[31]; end
                  endcase
                  unique case (k_int_q_1[1:0])
                    2'b00: begin x32_0 = opgrp_result_i[0][31:0]; sign_0 = x32_0[31]; end
                    2'b01: begin x32_0 = nl_intermediate_1_q[31:0]; sign_0 = x32_0[31]^1'b1; end
                    2'b10: begin x32_0 = opgrp_result_i[0][31:0]; sign_0 = x32_0[31]^1'b1; end
                    2'b11: begin x32_0 = nl_intermediate_1_q[31:0]; sign_0 = x32_0[31]; end
                  endcase
                end else begin
                   unique case (k_int_q_0[3:2])
                    2'b00: begin x32_1 = opgrp_result_i[0][63:32]; sign_1 = x32_1[31]; end
                    2'b01: begin x32_1 = nl_intermediate_0_q[63:32]; sign_1 = x32_1[31]^1'b1; end
                    2'b10: begin x32_1 = opgrp_result_i[0][63:32]; sign_1 = x32_1[31]^1'b1; end
                    2'b11: begin x32_1 = nl_intermediate_0_q[63:32]; sign_1 = x32_1[31]; end
                  endcase
                  unique case (k_int_q_0[1:0])
                    2'b00: begin x32_0 = opgrp_result_i[0][31:0]; sign_0 = x32_0[31]; end
                    2'b01: begin x32_0 = nl_intermediate_0_q[31:0]; sign_0 = x32_0[31]^1'b1; end
                    2'b10: begin x32_0 = opgrp_result_i[0][31:0]; sign_0 = x32_0[31]^1'b1; end
                    2'b11: begin x32_0 = nl_intermediate_0_q[31:0]; sign_0 = x32_0[31]; end
                  endcase
                end
              end else begin // SIN
                 if (opgrp_tag_i[0].last_phase) begin
                  unique case (k_int_q_1[3:2])
                    2'b00: begin x32_1 = nl_intermediate_1_q[63:32]; sign_1 = x32_1[31]; end
                    2'b01: begin x32_1 = opgrp_result_i[0][63:32]; sign_1 = x32_1[31]; end
                    2'b10: begin x32_1 = nl_intermediate_1_q[63:32]; sign_1 = x32_1[31]^1'b1; end
                    2'b11: begin x32_1 = opgrp_result_i[0][63:32]; sign_1 = x32_1[31]^1'b1; end
                  endcase
                  unique case (k_int_q_1[1:0])
                    2'b00: begin x32_0 = nl_intermediate_1_q[31:0]; sign_0 = x32_0[31]; end
                    2'b01: begin x32_0 = opgrp_result_i[0][31:0]; sign_0 = x32_0[31]; end
                    2'b10: begin x32_0 = nl_intermediate_1_q[31:0]; sign_0 = x32_0[31]^1'b1; end
                    2'b11: begin x32_0 = opgrp_result_i[0][31:0]; sign_0 = x32_0[31]^1'b1; end
                  endcase
                end else begin
                   unique case (k_int_q_0[3:2])
                    2'b00: begin x32_1 = nl_intermediate_0_q[63:32]; sign_1 = x32_1[31]; end
                    2'b01: begin x32_1 = opgrp_result_i[0][63:32]; sign_1 = x32_1[31]; end
                    2'b10: begin x32_1 = nl_intermediate_0_q[63:32]; sign_1 = x32_1[31]^1'b1; end
                    2'b11: begin x32_1 = opgrp_result_i[0][63:32]; sign_1 = x32_1[31]^1'b1; end
                  endcase
                  unique case (k_int_q_0[1:0])
                    2'b00: begin x32_0 = nl_intermediate_0_q[31:0]; sign_0 = x32_0[31]; end
                    2'b01: begin x32_0 = opgrp_result_i[0][31:0]; sign_0 = x32_0[31]; end
                    2'b10: begin x32_0 = nl_intermediate_0_q[31:0]; sign_0 = x32_0[31]^1'b1; end
                    2'b11: begin x32_0 = opgrp_result_i[0][31:0]; sign_0 = x32_0[31]^1'b1; end
                  endcase
                end
              end
              result_tanh[63:32] = {sign_1, x32_1[30:0]};
              result_tanh[31:0]  = {sign_0, x32_0[30:0]};
              nl_result_override_o[0] = 1'b1;
              nl_result_o = result_tanh;
            end
            default:;
          endcase
        end
      end
      default: ;
    endcase

    // --------------------------------------------------------
    // Muxing Logic for Internal Operations
    // --------------------------------------------------------
    if (nl_use_internal_op_o[0]) begin
        nl_operands_o[0] = operands_add;
        nl_op_o[0]       = nl_op_add;
        nl_op_mod_o[0]   = nl_opmode_add;
        nl_rnd_mode_o[0] = nl_rnd_add;
        nl_tag_o[0]      = fadd_tag;
    end

    if (nl_use_internal_op_o[3]) begin
        nl_operands_o[3] = operands_fconv;
        nl_op_o[3]       = nl_op_conv;
        nl_op_mod_o[3]   = nl_opmode_conv;
        nl_rnd_mode_o[3] = nl_rnd_conv;
        nl_tag_o[3]      = fconv_tag;
    end
  end

endmodule