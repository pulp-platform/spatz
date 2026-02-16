
package fpnew_nl_pkg;
import spatz_pkg::*;

  // =======================================================================
  // COSH STATES
  // =======================================================================
  typedef enum logic [3:0] {
    COSH_DRAIN,
    COSH_EXP_POS_U,   
    COSH_EXP_NEG_U,   
    COSH_EXP_POS_L,   
    COSH_EXP_NEG_L,   
    COSH_WAIT_U,      
    COSH_SUM_U,       
    COSH_WAIT_L,      
    COSH_SUM_L        
  } cosh_state_e;

  // =======================================================================
  // TANH STATES
  // =======================================================================
  typedef enum logic [3:0] {
    TANH_DRAIN_L,
    TANH_DRAIN_U,
    TANH_X_SQUARE_U,
    TANH_X_SQUARE_L,
    TANH_POLY1_U,   
    TANH_POLY1_L,   
    TANH_POLY2_U,   
    TANH_POLY2_L,   
    TANH_POLY3_U,   
    TANH_POLY3_L
  } tanh_state_e;

  // =======================================================================
  // RSQRT STATES
  // =======================================================================
  typedef enum logic [3:0] {
    RSQRT_DRAIN_L,
    RSQRT_DRAIN_U,
    RSQRT_X_SQUARE_U,
    RSQRT_X_SQUARE_L,
    RSQRT_POLY1_U,   
    RSQRT_POLY1_L,   
    RSQRT_NR1_U,   
    RSQRT_NR1_L,  
    RSQRT_NR2_U,  
    RSQRT_NR2_L          
  } rsqrt_state_e;

  // =======================================================================
  // REC STATES
  // =======================================================================
  typedef enum logic [3:0] {
    REC_DRAIN_L,
    REC_DRAIN_U,
    REC_APPROX_U,
    REC_APPROX_L,
    REC_NR1_MUL_U,   
    REC_NR1_MUL_L,   
    REC_NR1_ACCUM_U,
    REC_NR1_ACCUM_L,
    REC_NR2_MUL_U,   
    REC_NR2_MUL_L  
  } rec_state_e;

  // =======================================================================
  // SIN_COS STATES
  // =======================================================================
  typedef enum logic [4:0] {
    SIN_COS_DRAIN_L,
    SIN_COS_DRAIN_U,
    SIN_COS_POLY1_U,
    SIN_COS_POLY1_L,
    SIN_COS_POLY2_U,
    SIN_COS_POLY2_L,
    SIN_COS_POLY3_U,
    SIN_COS_POLY3_L,
    SIN_COS_POLY4_U,
    SIN_COS_POLY4_L,
    SIN_COS_POLY5_U,
    SIN_COS_POLY5_L,
    SIN_COS_FLOAT_CONV_U,   
    SIN_COS_FLOAT_CONV_L,   
    SIN_COS_INT_CONV_U,
    SIN_COS_INT_CONV_L,
    SIN_COS_RR_U,   
    SIN_COS_RR_L  
  } sin_cos_state_e;

  // =======================================================================
  // CHEBYSHEV COEFFICIENTS (TANH)
  // =======================================================================
  localparam logic [31:0] CHEBY_A_TANH     = 32'h3cd981f2;
  localparam logic [31:0] CHEBY_B_TANH     = 32'hbe69c8ac;
  localparam logic [31:0] CHEBY_C_TANH     = 32'h3f7a84b9;

  // Packed 64-bit Vectors
  localparam logic [63:0] CHEBY_A_TANH_VEC = {2{CHEBY_A_TANH}};
  localparam logic [63:0] CHEBY_B_TANH_VEC = {2{CHEBY_B_TANH}};
  localparam logic [63:0] CHEBY_C_TANH_VEC = {2{CHEBY_C_TANH}};

  // =======================================================================
  // TRIGONOMETRIC & GEOMETRIC CONSTANTS
  // =======================================================================
  localparam logic [31:0] FP32_1_POINT_5   = 32'h3fc00000;   //  1.5f
  localparam logic [31:0] FP32_NEG_0_POINT_5 = 32'hbf000000; // -0.5f
  localparam logic [31:0] FP32_ONE         = 32'h3f800000;   //  1.0f
  localparam logic [31:0] FP32_TWO         = 32'h40000000;   //  2.0f
  
  localparam logic [31:0] PIO2_HI          = 32'h3fc90fda; //  Pi/2
  localparam logic [31:0] COS_C2           = 32'hbefe4f76; // -0.49670f
  localparam logic [31:0] SIN_S3           = 32'hbe2a0903; // -0.16605f

  // Packed 64-bit Vectors
  localparam logic [63:0] C3_HALVES_VEC    = {2{FP32_1_POINT_5}};
  localparam logic [63:0] C1_HALF_VEC      = {2{FP32_NEG_0_POINT_5}};
  localparam logic [63:0] C_ONE_VEC        = {2{FP32_ONE}};
  localparam logic [63:0] PIO2_HI_VEC      = {2{PIO2_HI}};
  localparam logic [63:0] COS_C2_VEC       = {2{COS_C2}};
  localparam logic [63:0] SIN_S3_VEC       = {2{SIN_S3}};
  localparam logic [63:0] C2_VEC           = {2{FP32_TWO}};

   // =======================================================================

  // =======================================================================
  // SPECIAL FUNCTIONS (RSQRT & LOG & REC)
  // =======================================================================
  localparam logic [31:0] QUAKE_MAGIC      = 32'h5f375928; // RSQRT Constant
  localparam logic [31:0] LOG_SCALE        = 32'h33b17218; // Log2(e)/2^23
  localparam logic [31:0] REC_MAGIC        = 32'h7effffff; // REC Constant

  // Packed 64-bit Vectors
  localparam logic [63:0] QUAKE_MAGIC_VEC  = {2{QUAKE_MAGIC}};
  localparam logic [63:0] LOG_SCALE_VEC    = {2{LOG_SCALE}};
  localparam logic [63:0] REC_MAGIC_VEC    = {2{REC_MAGIC}};

  // =======================================================================
  // MASKS
  // =======================================================================
  localparam logic [30:0] ABS_ONE_MASK     = 31'h3F800000; 

  // Schraudolph Constants (Exp/Cosh Approximation)
  localparam logic [31:0] SCH_C_FP32      = 32'h4B38AA3B; 
  localparam logic [31:0] SCH_B_FP32      = 32'h4E7DE250; 
  localparam logic [31:0] SCH_B_COSH_FP32 = 32'h4e7bdf00; 
  localparam logic [31:0] F32_ZERO        = 32'h00000000;
  localparam logic [31:0] INV_PIO2        = 32'h3f22f983;

  // Packed Vectors
  localparam logic [N_FU*ELEN-1:0] SCH_C_VEC       = {N_FU*2{SCH_C_FP32}};
  localparam logic [N_FU*ELEN-1:0] SCH_B_VEC       = {N_FU*2{SCH_B_FP32}};
  localparam logic [N_FU*ELEN-1:0] SCH_B_COSH_VEC  = {N_FU*2{SCH_B_COSH_FP32}};
  localparam logic [N_FU*ELEN-1:0] F32_ZERO_VEC    = {N_FU*2{F32_ZERO}};
  localparam logic [N_FU*ELEN-1:0] INV_PIO2_VEC    = {N_FU*2{INV_PIO2}};

endpackage : fpnew_nl_pkg