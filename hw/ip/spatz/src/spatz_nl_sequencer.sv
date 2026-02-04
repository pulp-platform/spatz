// Author: Francesco Murande 
//
// Spatz Non-Linear Functions Sequencer
// Encapsulates FSM and Override Logic for VFEXPF, VFCOSHF, VFTANHF, etc.

module spatz_nl_sequencer 
  import spatz_pkg::*;
  import spatz_nl_pkg::*;
  import fpnew_pkg::*;
  import rvv_pkg::*;
#(
  parameter int unsigned NrFpu    = 4,   // N_FPU
  parameter int unsigned NrFu     = 4,   // N_FU
  parameter int unsigned Elen     = 32,  // ELEN
  parameter type         TagType  = logic
) (
  input  logic             clk_i,
  input  logic             rst_ni,

  // -- VFU State Inputs --
  input  spatz_req_t       spatz_req_i,
  input  logic             spatz_req_valid_i,
  input  logic             spatz_req_ready_i, // From VFU control
  input  logic             running_i,         // Is VFU running this ID? (running_q[id])
  input  vlen_t            vl_i,              // Current VLEN counter (vl_q)
  input  logic [$clog2(NrFu*(Elen/8)):0] nr_elem_word_i, 
  input  logic             word_issued_i,     // Did VFU issue a word this cycle?
  
  // -- Feedback from Execution Pipeline --
  input  TagType          result_tag_i,
  input  logic [NrFu*ELENB-1:0] result_valid_i,
  input  logic [NrFu*ELENB-1:0] pending_results_i,
  input  logic             is_next_uop_ready_i,

  // -- VRF Data for Operand Construction --
  input  vrf_data_t [2:0]  vrf_rdata_i,

  // -- Control Outputs to VFU --
  output logic             nl_active_o,       // Is NL mode active? (nl_active_eff)
  output nl_phase_e        nl_phase_o,        // Current phase (nl_phase_eff)
  output nl_op_e           nl_func_o,         // Function tag
  output logic             nl_stop_issue_o,   // Stall VFU issue logic
  output logic             is_last_uop_o,     // Tagging for writeback
  output logic             nl_uop_last_issue_o, // Tagging for VFU control

  // -- Operand Readiness Masks --
  output logic             need_vs1_o,
  output logic             need_vs2_o,
  output logic             need_vd_o,

  output logic             is_nl_op_o,        

  // -- Data Overrides --
  output logic             nl_override_operands_o,
  output logic [NrFu*Elen-1:0] nl_op1_ovr_o,
  output logic [NrFu*Elen-1:0] nl_op2_ovr_o,
  output logic [NrFu*Elen-1:0] nl_op3_ovr_o,

  // -- FPU Control Overrides --
  output logic             nl_override_fpu_o,
  output fpnew_pkg::operation_e nl_fpu_op_ovr_o,
  output logic             nl_fpu_op_mode_ovr_o,
  output fpnew_pkg::roundmode_e nl_fpu_rm_ovr_o,
  output fpnew_pkg::int_format_e nl_fpu_int_fmt_ovr_o
);

  // Include FF
  `include "common_cells/registers.svh"

  // -------------------------
  // Constants (Schraudolph)
  // -------------------------
  localparam logic [31:0] SCH_C_FP32      = 32'h4B38AA3B; // 12.102.203.0f
  localparam logic [31:0] SCH_B_FP32      = 32'h4E7DE250; // 1.064.866.805.0f
  localparam logic [31:0] SCH_B_COSH_FP32 = 32'h4e7bdf00; // 1.056.964.608.0f
  localparam logic [31:0] F32_ZERO        = 32'h00000000;
  localparam logic [31:0] INV_PIO2        = 32'h3f22f983; // 0.63661977236

  // Vectorized Constants
  logic [NrFu*Elen-1:0] sch_c_vec, sch_b_vec, f32_zero_vec, sch_b_cosh_vec, inv_pio2_vec;

  // Assuming Elen=32 for simplicity of assignment, logic handles expansion
  assign sch_c_vec        = {NrFu{SCH_C_FP32}};
  assign sch_b_vec        = {NrFu{SCH_B_FP32}};
  assign f32_zero_vec     = {NrFu{F32_ZERO}};
  assign sch_b_cosh_vec   = {NrFu{SCH_B_COSH_FP32}};
  assign inv_pio2_vec     = {NrFu{INV_PIO2}};

  // -------------------------
  // State Machine Signals
  // -------------------------
  nl_phase_e nl_phase_q, nl_phase_d;
  logic      nl_clear;
  logic      nl_start;
  nl_phase_e nl_phase_eff;
  logic      is_nl_op;
  logic      nl_loopback_q, nl_loopback_d;
  
  // Phase drain detection
  logic      nl_last_uop_execution_ff, nl_last_en;
  
  `FFL(nl_last_uop_execution_ff, nl_uop_last_issue_o, nl_last_en, '0)
  `FF(nl_phase_q, nl_phase_d, NL_IDLE)
  `FF(nl_loopback_q, nl_loopback_d, 1'b0)

  // -------------------------
  // Logic Implementation
  // -------------------------

  // Opcode Detection
  assign is_nl_op = (spatz_req_i.op inside {VFEXPF, VFCOSHF, VFTANHF, VFLOG, VFRSQRT7, VFCOS, VFSIN});

  // Function Tag Mapping
  always_comb begin
    unique case (spatz_req_i.op)
      VFEXPF:   nl_func_o = EXPS;
      VFCOSHF:  nl_func_o = COSHS;
      VFTANHF:  nl_func_o = TANHS;
      VFLOG:    nl_func_o = LOGS;
      VFRSQRT7:  nl_func_o = RSQRT;
      VFSIN:    nl_func_o = SIN;
      VFCOS:    nl_func_o = COS;
      default:  nl_func_o = EXPS; // Default safe
    endcase
  end

  // Start/Stop Conditions
  assign nl_clear = spatz_req_ready_i; // Clear when VFU deques the instruction
  
  // NL start: only on fresh instruction, only EW_32, only NL ops
  assign nl_start = spatz_req_valid_i && !running_i && 
                    is_nl_op && (spatz_req_i.vtype.vsew == EW_32);

  // Effective Phase Logic (Combinational P0 entry)
  always_comb begin
    nl_phase_eff  = nl_phase_q;
    if (nl_phase_q == NL_IDLE && nl_start) nl_phase_eff = NL_FPU_ISSUE_0;
  end

  assign nl_active_o = (nl_phase_eff != NL_IDLE);
  assign nl_phase_o  = nl_phase_eff;

  // Last UOP issued detection
  assign nl_uop_last_issue_o = word_issued_i && ((vl_i + nr_elem_word_i) >= spatz_req_i.vl);

  // -------------------------
  // FSM Next State Logic
  // -------------------------
  always_comb begin
    nl_phase_d = nl_phase_q;

    if (nl_clear) begin
      nl_phase_d = NL_IDLE;
    end else begin
      unique case (spatz_req_i.op)
        VFEXPF: begin
          unique case (nl_phase_q)
            NL_IDLE:        if (nl_start)           nl_phase_d = NL_FPU_ISSUE_0;
            NL_FPU_ISSUE_0: if (nl_uop_last_issue_o)  nl_phase_d = NL_WAIT;
            NL_WAIT: begin
              if (result_tag_i.uop_last && &(result_valid_i | ~pending_results_i)) 
                nl_phase_d = NL_IDLE;
            end
            default: nl_phase_d = NL_IDLE;
          endcase
        end
        
        VFCOSHF: begin
          if (result_tag_i.uop_last && &(result_valid_i | ~pending_results_i)) begin
             nl_phase_d = NL_IDLE;
          end
          unique case (nl_phase_q)
            NL_IDLE:        if (nl_start) nl_phase_d = NL_FPU_ISSUE_0;
            NL_FPU_ISSUE_0:               nl_phase_d = NL_FPU_ISSUE_1;
            NL_FPU_ISSUE_1:               nl_phase_d = nl_uop_last_issue_o ? NL_WAIT : (nl_loopback_q ? NL_FPU_ISSUE_2 : NL_FPU_ISSUE_0);
            NL_FPU_ISSUE_2: if (is_next_uop_ready_i) nl_phase_d = NL_FPU_ISSUE_0;
            NL_WAIT :       if (result_tag_i.uop_last && &(result_valid_i | ~pending_results_i)) nl_phase_d = NL_IDLE;
            default: nl_phase_d = NL_IDLE;
          endcase
        end

        VFTANHF: begin
            unique case (nl_phase_q)
            NL_IDLE:        if (nl_start)       nl_phase_d = NL_FPU_ISSUE_0;
            NL_FPU_ISSUE_0: if (nl_loopback_q)  nl_phase_d = NL_WAIT;
            NL_WAIT: begin
              if (nl_last_uop_execution_ff) begin
                if (result_tag_i.uop_last && &(result_valid_i | ~pending_results_i)) 
                  nl_phase_d = NL_IDLE;
              end else if (is_next_uop_ready_i) begin
                  nl_phase_d = NL_FPU_ISSUE_0;
              end
            end
            default: nl_phase_d = NL_IDLE;
          endcase
        end

        VFLOG: begin
          unique case (nl_phase_q)
            NL_IDLE:        if (nl_start)         nl_phase_d = NL_FPU_ISSUE_0;
            NL_FPU_ISSUE_0: if (nl_uop_last_issue_o)    nl_phase_d = NL_WAIT;
            NL_WAIT: begin
              if (result_tag_i.uop_last && &(result_valid_i | ~pending_results_i))
                nl_phase_d = NL_IDLE;
            end
            default: nl_phase_d = NL_IDLE;
          endcase
        end

        VFRSQRT7: begin
          unique case (nl_phase_q)
            NL_IDLE:        if (nl_start)         nl_phase_d = NL_FPU_ISSUE_0;
            NL_FPU_ISSUE_0: if (nl_loopback_q)    nl_phase_d = NL_WAIT;
            NL_WAIT: begin
              if (nl_last_uop_execution_ff) begin
                if (result_tag_i.uop_last && &(result_valid_i | ~pending_results_i))
                  nl_phase_d = NL_IDLE;
              end else if (is_next_uop_ready_i) begin
                  nl_phase_d = NL_FPU_ISSUE_0;
              end
            end
            default: nl_phase_d = NL_IDLE;
          endcase
        end

        VFSIN, VFCOS: begin
          unique case (nl_phase_q)
            NL_IDLE:        if (nl_start)         nl_phase_d = NL_FPU_ISSUE_0;
            NL_FPU_ISSUE_0: if (nl_loopback_q)    nl_phase_d = NL_WAIT;
            NL_WAIT: begin
              if (nl_last_uop_execution_ff) begin
                if (result_tag_i.uop_last && &(result_valid_i | ~pending_results_i))
                  nl_phase_d = NL_IDLE;
              end else if (is_next_uop_ready_i) begin
                  nl_phase_d = NL_FPU_ISSUE_0;
              end
            end
            default: nl_phase_d = NL_IDLE;
          endcase
        end
        default: nl_phase_d = NL_IDLE;
      endcase
    end
  end

  // -------------------------
  // Decoder / Override Logic
  // -------------------------
  always_comb begin
    // Defaults: Pass-through
    need_vs1_o  = spatz_req_i.use_vs1;
    need_vs2_o  = spatz_req_i.use_vs2;
    need_vd_o   = spatz_req_i.vd_is_src;
    is_last_uop_o = 1'b0;
    nl_stop_issue_o = 1'b0;
    nl_loopback_d = nl_loopback_q;
    nl_override_operands_o = 1'b0;
    nl_op1_ovr_o = '0;
    nl_op2_ovr_o = '0;
    nl_op3_ovr_o = '0;

    nl_override_fpu_o    = 1'b0;
    nl_fpu_op_ovr_o      = fpnew_pkg::FMADD;
    nl_fpu_op_mode_ovr_o = 1'b0;
    nl_fpu_rm_ovr_o      = spatz_req_i.rm;
    nl_fpu_int_fmt_ovr_o = fpnew_pkg::INT32;
    nl_last_en         = 1'b0;

    if (nl_active_o) begin
      unique case (spatz_req_i.op)
        VFEXPF: begin
          unique case (nl_phase_eff)
            NL_FPU_ISSUE_0: begin // vd <- B (FMADD(0,0,B))
              need_vs1_o           = 1'b1; // x on vrf_rdata_i[1]
              need_vs2_o           = 1'b0;
              need_vd_o            = 1'b0;
              nl_override_operands_o = 1'b1;
              nl_op1_ovr_o         = sch_c_vec;     // C
              nl_op2_ovr_o         = vrf_rdata_i[1]; // x
              nl_op3_ovr_o         = sch_b_vec;     // B
              nl_override_fpu_o    = 1'b1;
              nl_fpu_op_ovr_o      = fpnew_pkg::FMADD;
              is_last_uop_o        = 1'b1;
            end
            NL_WAIT: begin
              need_vs1_o = 1'b0; need_vs2_o = 1'b0; need_vd_o = 1'b0;
              is_last_uop_o = 1'b1;
            end
            default:;
          endcase
        end

        VFCOSHF: begin
          unique case (nl_phase_eff)
            NL_FPU_ISSUE_0: begin
              need_vs1_o = 1'b1; need_vs2_o = 1'b0; need_vd_o = 1'b0;
              nl_override_operands_o = 1'b1;
              nl_op1_ovr_o = sch_c_vec;
              nl_op2_ovr_o = vrf_rdata_i[1];
              nl_op3_ovr_o = sch_b_cosh_vec;
              nl_stop_issue_o = 1'b1;
              nl_override_fpu_o = 1'b1;
              nl_fpu_op_ovr_o = fpnew_pkg::FMADD;
            end
            NL_FPU_ISSUE_1: begin
              nl_stop_issue_o = 1'b0;
              need_vs1_o = 1'b1; need_vs2_o = 1'b0; need_vd_o = 1'b0;
              nl_override_operands_o = 1'b1;
              nl_op1_ovr_o = sch_c_vec;
              nl_op2_ovr_o = vrf_rdata_i[1];
              nl_op3_ovr_o = sch_b_cosh_vec;
              nl_override_fpu_o = 1'b1;
              nl_fpu_op_ovr_o = fpnew_pkg::FNMSUB;
              nl_loopback_d = 1'b1;
              if (nl_loopback_q) is_last_uop_o = 1'b1;
            end
            NL_FPU_ISSUE_2: begin
               need_vs1_o = 0; need_vs2_o = 0; need_vd_o = 0;
               is_last_uop_o = 1'b1;
               nl_loopback_d = 1'b0;
               nl_stop_issue_o = 1'b1;
            end
            NL_WAIT: begin
               need_vs1_o = 0; need_vs2_o = 0; need_vd_o = 0;
               nl_loopback_d = 1'b0;
               nl_stop_issue_o = 1'b1;
            end
            default:;
          endcase
        end
        VFTANHF: begin
            unique case (nl_phase_eff)
                NL_FPU_ISSUE_0: begin
                    need_vs1_o = 1'b1; need_vs2_o = 1'b1; need_vd_o = 1'b0;
                    nl_override_operands_o = 1'b1;
                    nl_op1_ovr_o = vrf_rdata_i[1];
                    nl_op2_ovr_o = vrf_rdata_i[1];
                    nl_op3_ovr_o = f32_zero_vec;
                    nl_override_fpu_o = 1'b1;
                    nl_fpu_op_ovr_o = fpnew_pkg::MUL;
                    nl_loopback_d = nl_loopback_q ? 1'b1 : 1'b0;
                    is_last_uop_o = nl_loopback_q ? 1'b1 : 1'b0;
                    nl_last_en = 1'b1;
                end
                NL_WAIT: begin
                    need_vs1_o = 0; need_vs2_o = 0; need_vd_o = 0;
                    nl_stop_issue_o = 1'b1;
                end
                default:;
            endcase
        end
        
        VFLOG: begin
             unique case (nl_phase_eff)
            NL_FPU_ISSUE_0: begin
              need_vs1_o           = 1'b1; need_vs2_o = 1'b0; need_vd_o = 1'b0;
              nl_override_operands_o = 1'b1;
              nl_op1_ovr_o         = vrf_rdata_i[1];
              nl_op2_ovr_o         = f32_zero_vec;
              nl_op3_ovr_o         = f32_zero_vec;
              nl_override_fpu_o    = 1'b1;
              nl_fpu_op_ovr_o      = fpnew_pkg::I2F;
              nl_loopback_d         = word_issued_i ? 1'b1 : 1'b0;
              is_last_uop_o           = nl_loopback_q ? 1'b1 : 1'b0;
              nl_last_en            = 1'b1;
            end
            NL_WAIT: begin
               need_vs1_o = 0; need_vs2_o = 0; need_vd_o = 0;
               nl_stop_issue_o = 1'b1;
            end
             endcase
        end

        VFRSQRT7: begin
           unique case (nl_phase_eff)
            NL_FPU_ISSUE_0: begin
              need_vs1_o           = 1'b1; need_vs2_o = 1'b0; need_vd_o = 1'b0;
              nl_override_operands_o = 1'b1;
              nl_op1_ovr_o         = vrf_rdata_i[1];
              nl_op2_ovr_o         = f32_zero_vec;
              nl_op3_ovr_o         = f32_zero_vec;
              nl_override_fpu_o    = 1'b1;
              nl_fpu_op_ovr_o      = fpnew_pkg::MUL;
              nl_loopback_d         = word_issued_i ? 1'b1 : 1'b0;
              is_last_uop_o           = nl_loopback_q ? 1'b1 : 1'b0;
              nl_last_en            = 1'b1;
            end
            NL_WAIT: begin
                need_vs1_o = 0; need_vs2_o = 0; need_vd_o = 0;
                nl_stop_issue_o = 1'b1;
                nl_loopback_d = 1'b0;
            end
           endcase
        end

        VFSIN, VFCOS: begin
            unique case (nl_phase_eff)
            NL_FPU_ISSUE_0: begin
              need_vs1_o           = 1'b1; need_vs2_o = 1'b0; need_vd_o = 1'b0;
              nl_override_operands_o = 1'b1;
              nl_op1_ovr_o         = vrf_rdata_i[1];
              nl_op2_ovr_o         = inv_pio2_vec;
              nl_op3_ovr_o         = f32_zero_vec;
              nl_override_fpu_o    = 1'b1;
              nl_fpu_op_ovr_o      = fpnew_pkg::MUL;
              nl_loopback_d        = word_issued_i ? 1'b1 : 1'b0;
              is_last_uop_o        = nl_loopback_q ? 1'b1 : 1'b0;
              nl_last_en           = 1'b1;
            end
            NL_WAIT: begin
                need_vs1_o = 0; need_vs2_o = 0; need_vd_o = 0;
                nl_stop_issue_o = 1'b1;
                nl_loopback_d = 1'b0;
            end
            endcase
        end
      endcase
    end
  end

endmodule : spatz_nl_sequencer