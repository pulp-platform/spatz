// Author: Francesco Murande <fmurande@ethz.ch>
//
// Spatz Non-Linear Functions Package
// Provides type definitions, constants, and utilities for NL operations
// Currently supports: FP32
// Future support: FP16

package spatz_nl_pkg;

  import fpnew_pkg::*;
  import spatz_pkg::*;
  import rvv_pkg::*;

  // ═══════════════════════════════════════════════════════════════════════════
  // Configuration Parameters
  // ═══════════════════════════════════════════════════════════════════════════
  
  // Supported formats for NL operations
  localparam bit NL_SUPPORT_FP32 = 1'b1;
  localparam bit NL_SUPPORT_FP16 = 1'b0;  // Future extension

  // Number of intermediate registers needed per FPU instance
  // This is determined by worst-case NL operation requirement
  localparam int unsigned NL_NUM_INTERMEDIATE_REGS = 4;

  // ═══════════════════════════════════════════════════════════════════════════
  // Type Definitions
  // ═══════════════════════════════════════════════════════════════════════════
  
  // NL operation phases
  typedef enum logic [3:0] { 
    NL_IDLE,
    NL_FPU_ISSUE_0,
    NL_FPU_ISSUE_1,
    NL_FPU_ISSUE_2 ,
    NL_FPU_ISSUE_3, 
    NL_FPU_ISSUE_4, 
    NL_FPU_ISSUE_5 , 
    NL_FPU_ISSUE_6 , 
    NL_WAIT 
  } nl_phase_e;

  // NL operation enumeration
  typedef enum logic [2:0] {
    EXPS, 
    COSHS, 
    TANHS, 
    LOGS, 
    RSQRT, 
    COS, 
    SIN 
  } nl_op_e;
  // Coefficient selector enumeration (type-safe, extensible)
  typedef enum logic [4:0] {
    NL_COEFF_NONE       = 5'd0,   // No coefficient
    NL_COEFF_SCH_C      = 5'd1,   // Schraudolph C constant
    NL_COEFF_SCH_B_EXP  = 5'd2,   // Schraudolph B for exp
    NL_COEFF_SCH_B_COSH = 5'd3,   // Schraudolph B for cosh
    NL_COEFF_CHEBY_A    = 5'd4,   // Chebyshev A (tanh approx)
    NL_COEFF_CHEBY_B    = 5'd5,   // Chebyshev B (tanh approx)
    NL_COEFF_CHEBY_C    = 5'd6,   // Chebyshev C (tanh approx)
    NL_COEFF_QUAKE      = 5'd7,   // Quake fast inverse sqrt magic constant
    NL_COEFF_1P5        = 5'd8,   // Constant 1.5
    NL_COEFF_0P5        = 5'd9,   // Constant -0.5
    NL_COEFF_INV_PIO2   = 5'd10,  // 1/(π/2) for range reduction
    NL_COEFF_PIO2_HI    = 5'd11,  // π/2 high bits
    NL_COEFF_COS_C2     = 5'd12,  // Cosine C2 coefficient
    NL_COEFF_SIN_S3     = 5'd13,  // Sine S3 coefficient
    NL_COEFF_ONE        = 5'd14,  // Constant 1.0
    NL_COEFF_ZERO       = 5'd15,  // Constant 0.0
    NL_COEFF_POS_HALF   = 5'd16,  // Constant +0.5
    NL_COEFF_NAN        = 5'd17   // Constant NaN
  } nl_coeff_sel_e;

  // Operand source selector
  typedef enum logic [1:0] {
    NL_OP_SRC_VRF   = 2'd0,  // Source from VRF (normal datapath)
    NL_OP_SRC_COEFF = 2'd1,  // Source from coefficient bank
    NL_OP_SRC_INTER = 2'd2,  // Source from intermediate storage
    NL_OP_SRC_ZERO  = 2'd3   // Source is constant zero
  } nl_operand_src_e;

  // Intermediate storage control (per FPU instance)
  typedef struct packed {
    logic [1:0] wr_idx;    // Which intermediate register to write (0-3)
    logic       wr_en;     // Write enable
    logic [1:0] rd_idx_0;  // Read index for operand 0
    logic [1:0] rd_idx_1;  // Read index for operand 1
    logic [1:0] rd_idx_2;  // Read index for operand 2
    logic       rd_en_0;   // Read enable for operand 0
    logic       rd_en_1;   // Read enable for operand 1
    logic       rd_en_2;   // Read enable for operand 2
  } nl_storage_ctrl_t;

  // NL control bundle: VFU → FPU communication
  // This encapsulates all control signals needed for NL operation
  typedef struct packed {
    logic                  active;         // NL operation is active
    nl_phase_e             phase;          // Current phase of NL operation
    nl_op_e                operation;      // Which NL operation
    fpnew_pkg::operation_e fpu_op;         // FPU operation to execute
    logic                  fpu_op_mod;     // FPU operation modifier
    fpnew_pkg::roundmode_e fpu_rm;         // Rounding mode
    nl_operand_src_e       op0_src;        // Source selector for operand 0
    nl_operand_src_e       op1_src;        // Source selector for operand 1
    nl_operand_src_e       op2_src;        // Source selector for operand 2
    nl_coeff_sel_e         coeff0_sel;     // Coefficient for operand 0
    nl_coeff_sel_e         coeff1_sel;     // Coefficient for operand 1
    nl_coeff_sel_e         coeff2_sel;     // Coefficient for operand 2
    nl_storage_ctrl_t      storage_ctrl;   // Intermediate storage control
  } nl_control_t;

  // Default/idle NL control
  localparam nl_control_t NL_CTRL_IDLE = '{
    active:       1'b0,
    phase:        NL_IDLE,
    operation:    EXPS,
    fpu_op:       fpnew_pkg::FMADD,
    fpu_op_mod:   1'b0,
    fpu_rm:       fpnew_pkg::RNE,
    op0_src:      NL_OP_SRC_VRF,
    op1_src:      NL_OP_SRC_VRF,
    op2_src:      NL_OP_SRC_VRF,
    coeff0_sel:   NL_COEFF_ZERO,
    coeff1_sel:   NL_COEFF_ZERO,
    coeff2_sel:   NL_COEFF_ZERO,
    storage_ctrl: '{default: '0}
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // FP32 Coefficient Constants
  // ═══════════════════════════════════════════════════════════════════════════
  
  // Schraudolph exponential approximation
  // exp(x) ≈ 2^(x/ln(2)) using integer arithmetic trick
  localparam logic [31:0] NL_SCH_C_FP32      = 32'h4B38AA3B;  // C = 12102203.0
  localparam logic [31:0] NL_SCH_B_EXP_FP32  = 32'h4E7DE250;  // B = 1064866805.0
  localparam logic [31:0] NL_SCH_B_COSH_FP32 = 32'h4E7BDF00;  // B = 1056964608.0

  // Chebyshev polynomial coefficients for tanh approximation
  // tanh(x) ≈ Ax^3 + Bx^2 + Cx for |x| < 1
  localparam logic [31:0] NL_CHEBY_A_FP32    = 32'h3CD981F2;  // A ≈ 0.0265
  localparam logic [31:0] NL_CHEBY_B_FP32    = 32'hBE69C8AC;  // B ≈ -0.228
  localparam logic [31:0] NL_CHEBY_C_FP32    = 32'h3F7A84B9;  // C ≈ 0.978

  // Fast inverse square root (Quake III algorithm)
  localparam logic [31:0] NL_QUAKE_FP32      = 32'h5F375928;  // Magic constant

  // Common mathematical constants
  localparam logic [31:0] NL_1P5_FP32        = 32'h3FC00000;  // 1.5
  localparam logic [31:0] NL_0P5_FP32        = 32'hBF000000;  // -0.5
  localparam logic [31:0] NL_INV_PIO2_FP32   = 32'h3F22F983;  // 1/(π/2) ≈ 0.6366
  localparam logic [31:0] NL_PIO2_HI_FP32    = 32'h3FC90FDA;  // π/2 ≈ 1.5708

  // Trigonometric polynomial coefficients
  localparam logic [31:0] NL_COS_C2_FP32     = 32'hBEFE4F76;  // cos C2 ≈ -0.4967
  localparam logic [31:0] NL_SIN_S3_FP32     = 32'hBE2A0903;  // sin S3 ≈ -0.1661

  // Basic constants
  localparam logic [31:0] NL_ONE_FP32        = 32'h3F800000;  // 1.0
  localparam logic [31:0] NL_ZERO_FP32       = 32'h00000000;  // 0.0
  localparam logic [31:0] NL_POS_HALF_FP32   = 32'h3F000000;  // +0.5 
  localparam logic [31:0] NL_NAN_FP32        = 32'h7FC00000;  // Quiet NaN


  // ═══════════════════════════════════════════════════════════════════════════
  // FP16 Coefficient Constants (Future Extension)
  // ═══════════════════════════════════════════════════════════════════════════
  
  // TODO: Define FP16 constants when FP16 support is added
  // These will be properly calculated FP16 representations, not truncations

  // ═══════════════════════════════════════════════════════════════════════════
  // Utility Functions
  // ═══════════════════════════════════════════════════════════════════════════
  
  // Get single coefficient value in FP32 format
  function automatic logic [31:0] nl_get_coeff_fp32(nl_coeff_sel_e sel);
    case (sel)
      NL_COEFF_SCH_C:      return NL_SCH_C_FP32;
      NL_COEFF_SCH_B_EXP:  return NL_SCH_B_EXP_FP32;
      NL_COEFF_SCH_B_COSH: return NL_SCH_B_COSH_FP32;
      NL_COEFF_CHEBY_A:    return NL_CHEBY_A_FP32;
      NL_COEFF_CHEBY_B:    return NL_CHEBY_B_FP32;
      NL_COEFF_CHEBY_C:    return NL_CHEBY_C_FP32;
      NL_COEFF_QUAKE:      return NL_QUAKE_FP32;
      NL_COEFF_1P5:        return NL_1P5_FP32;
      NL_COEFF_0P5:        return NL_0P5_FP32;
      NL_COEFF_INV_PIO2:   return NL_INV_PIO2_FP32;
      NL_COEFF_PIO2_HI:    return NL_PIO2_HI_FP32;
      NL_COEFF_COS_C2:     return NL_COS_C2_FP32;
      NL_COEFF_SIN_S3:     return NL_SIN_S3_FP32;
      NL_COEFF_ONE:        return NL_ONE_FP32;
      NL_COEFF_ZERO:       return NL_ZERO_FP32;
      NL_COEFF_POS_HALF:   return NL_POS_HALF_FP32;
      NL_COEFF_NAN:        return NL_NAN_FP32;
      default:             return NL_ZERO_FP32;
    endcase
  endfunction

  // Replicate coefficient across full 256-bit VRF word width
  // For FP32: 8 copies (8×32-bit = 256-bit)
  // For FP16: 16 copies (16×16-bit = 256-bit) - Future
  function automatic logic [255:0] nl_replicate_coeff_256b(
    nl_coeff_sel_e sel,
    vew_e vsew
  );
    logic [31:0] coeff_fp32;
    logic [15:0] coeff_fp16;
    
    // Get base coefficient
    coeff_fp32 = nl_get_coeff_fp32(sel);
    
    case (vsew)
      EW_32: begin
        // FP32: Replicate 8 times for 256-bit
        return {8{coeff_fp32}};
      end
      
      EW_16: begin
        // FP16: Future support
        // TODO: Implement FP16 coefficient conversion
        // For now, return zero and flag error
        if (NL_SUPPORT_FP16) begin
          coeff_fp16 = coeff_fp32[15:0];  // Placeholder
          return {16{coeff_fp16}};
        end else begin
          return '0;
        end
      end
      
      default: begin
        // Unsupported element width
        return '0;
      end
    endcase
  endfunction

  // Check if element width is supported for NL operations
  function automatic logic nl_is_format_supported(vew_e vsew);
    case (vsew)
      EW_32: return NL_SUPPORT_FP32;
      EW_16: return NL_SUPPORT_FP16;
      default: return 1'b0;
    endcase
  endfunction

  // Get number of elements in 256-bit word for given element width
  function automatic int unsigned nl_elements_per_word(vew_e vsew);
    case (vsew)
      EW_8:  return 32;
      EW_16: return 16;
      EW_32: return 8;
      EW_64: return 4;
      default: return 0;
    endcase
  endfunction

  // ═══════════════════════════════════════════════════════════════════════════
  // Debug/Verification Helpers
  // ═══════════════════════════════════════════════════════════════════════════
  
  // Convert nl_op_e to string for debug
  function automatic string nl_op_to_string(nl_op_e op);
    case (op)
      EXPS:   return "EXPS";
      COSHS:  return "COSHS";
      TANHS:  return "TANHS";
      LOGS:   return "LOGS";
      RSQRT:  return "RSQRT";
      COS:    return "COS";
      SIN:    return "SIN";
      default: return "UNKNOWN";
    endcase
  endfunction

  // Convert nl_phase_e to string for debug
  function automatic string nl_phase_to_string(nl_phase_e phase);
    case (phase)
      NL_IDLE:         return "IDLE";
      NL_FPU_ISSUE_0:  return "ISSUE_0";
      NL_FPU_ISSUE_1:  return "ISSUE_1";
      NL_FPU_ISSUE_2:  return "ISSUE_2";
      NL_FPU_ISSUE_3:  return "ISSUE_3";
      NL_FPU_ISSUE_4:  return "ISSUE_4";
      NL_FPU_ISSUE_5:  return "ISSUE_5";
      NL_FPU_ISSUE_6:  return "ISSUE_6";
      NL_WAIT:         return "WAIT";
      default:         return "UNKNOWN";
    endcase
  endfunction

endpackage : spatz_nl_pkg