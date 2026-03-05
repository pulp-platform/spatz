module fpnew_nl_controller
  import fpnew_pkg::*;
  import fpnew_nl_pkg::*;
#(
  parameter int unsigned WIDTH          = 64,
  parameter int unsigned NUM_OPERANDS   = 3,
  parameter type         TagType        = logic,
  parameter int unsigned NUM_OPGROUPS   = fpnew_pkg::NUM_OPGROUPS,
  parameter int unsigned MAX_INFLIGHT   = 16
)(
  input  logic                               clk_i,
  input  logic                               rst_ni,
  input  logic                               flush_i,

  //  External Interface (from VFU / pipeline)
  input  logic [NUM_OPERANDS-1:0][WIDTH-1:0] operands_i,
  input  fpnew_pkg::roundmode_e              rnd_mode_i,
  input  fpnew_pkg::operation_e              op_i,
  input  logic                               op_mod_i,
  input  fpnew_pkg::fp_format_e              src_fmt_i,
  input  fpnew_pkg::fp_format_e              dst_fmt_i,
  input  fpnew_pkg::int_format_e             int_fmt_i,
  input  TagType                             tag_i,
  input  logic                               in_valid_i,

  // NL detection (active for ALL elements of a vector NL instruction)
  input  logic                               is_nl_op_i,

  //  Muxed Outputs → ADDMUL
  output logic [NUM_OPERANDS-1:0][WIDTH-1:0] addmul_operands_o,
  output fpnew_pkg::roundmode_e              addmul_rnd_mode_o,
  output fpnew_pkg::operation_e              addmul_op_o,
  output logic                               addmul_op_mod_o,
  output fpnew_pkg::fp_format_e              addmul_src_fmt_o,
  output fpnew_pkg::fp_format_e              addmul_dst_fmt_o,
  output fpnew_pkg::int_format_e             addmul_int_fmt_o,
  output TagType                             addmul_tag_o,
  output logic                               addmul_in_valid_o,
  input  logic                               addmul_in_ready_i,
  input  fpnew_pkg::status_t                 addmul_status_i,

  //  Muxed Outputs → CONV 
  output logic [NUM_OPERANDS-1:0][WIDTH-1:0] conv_operands_o,
  output fpnew_pkg::roundmode_e              conv_rnd_mode_o,
  output fpnew_pkg::operation_e              conv_op_o,
  output logic                               conv_op_mod_o,
  output fpnew_pkg::fp_format_e              conv_src_fmt_o,
  output fpnew_pkg::fp_format_e              conv_dst_fmt_o,
  output fpnew_pkg::int_format_e             conv_int_fmt_o,
  output TagType                             conv_tag_o,
  output logic                               conv_in_valid_o,
  input  logic                               conv_in_ready_i,

  //  Feedback from Opgroup Results
  input  logic [WIDTH-1:0]                   addmul_result_i,
  input  TagType                             addmul_tag_result_i,
  input  logic                               addmul_out_valid_i,

  input  logic [WIDTH-1:0]                   conv_result_i,
  input  TagType                             conv_tag_result_i,
  input  logic                               conv_out_valid_i,

  //  Arbiter Handshake Management
  input  logic [NUM_OPGROUPS-1:0]            arb_gnt_i,         
  output logic [NUM_OPGROUPS-1:0]            opgrp_out_ready_o,  
  input  logic [NUM_OPGROUPS-1:0]            opgrp_out_valid_i,  
  output logic [NUM_OPGROUPS-1:0]            arb_req_o,          

  //  Input-side ready (back to VFU)
  input  logic [NUM_OPGROUPS-1:0]            opgrp_in_ready_i,  
  output logic                               in_ready_o,

  output logic [WIDTH-1:0]                   reconstructed_result_o,
  output TagType                             reconstructed_tag_o,
  output fpnew_pkg::status_t                 reconstructed_status_o,
  output logic                               reconstructed_result_valid_o,

  //  Status
  output logic                               nl_busy_o
);

`include "common_cells/registers.svh"
// NL in-flight OR still being issued
  localparam int unsigned CNT_WIDTH  = $clog2(MAX_INFLIGHT + 1);

  logic [CNT_WIDTH-1:0] nl_inflight_q;
  logic                 cnt_inc, cnt_dec;
  logic                 nl_active;
  logic                 allow_issue_nl;

  assign cnt_inc = is_nl_op_i && in_valid_i && addmul_in_ready_i && allow_issue_nl;

  // Decrement: NL final result consumed at CONV output by arbiter
  assign cnt_dec = nl_active && ((op_i == EXPS) ? (conv_out_valid_i   && arb_gnt_i[3]) : (addmul_out_valid_i  && arb_gnt_i[0]));

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni)
      nl_inflight_q <= '0;
    else if (flush_i)
      nl_inflight_q <= '0;
    else begin
      unique case ({cnt_inc, cnt_dec})
        2'b10:   nl_inflight_q <= nl_inflight_q + CNT_WIDTH'(1);
        2'b01:   nl_inflight_q <= nl_inflight_q - CNT_WIDTH'(1);
        default: nl_inflight_q <= nl_inflight_q;  
      endcase
    end
  end

  assign nl_active = (nl_inflight_q != '0) || is_nl_op_i;
  assign nl_busy_o = (nl_inflight_q != '0);

 logic is_fp32, is_fp16, is_bf16;
 assign is_fp32 = (src_fmt_i == fpnew_pkg::FP32);
 assign is_fp16 = (src_fmt_i == fpnew_pkg::FP16);
 assign is_bf16 = (src_fmt_i == fpnew_pkg::FP16ALT);

// ── Constant selection (one mux per constant, gated by SEW) ──
  logic [WIDTH-1:0] sel_cheby_a, sel_cheby_b, sel_cheby_c;
  logic [WIDTH-1:0] sel_c3_halves, sel_c1_half, sel_c_one, sel_c2;
  logic [WIDTH-1:0] sel_pio2_hi, sel_cos_c2, sel_sin_s3;
  logic [WIDTH-1:0] sel_log_scale;
  logic [WIDTH-1:0] sel_sch_c, sel_sch_b, sel_sch_b_cosh, sel_inv_pio2;

  assign sel_cheby_a    = is_bf16 ? CHEBY_A_TANH_VEC_BF16 : (is_fp16 ? CHEBY_A_TANH_VEC_F16 : CHEBY_A_TANH_VEC);
  assign sel_cheby_b    = is_bf16 ? CHEBY_B_TANH_VEC_BF16 : (is_fp16 ? CHEBY_B_TANH_VEC_F16 : CHEBY_B_TANH_VEC);
  assign sel_cheby_c    = is_bf16 ? CHEBY_C_TANH_VEC_BF16 : (is_fp16 ? CHEBY_C_TANH_VEC_F16 : CHEBY_C_TANH_VEC);
  assign sel_c3_halves  = is_bf16 ? C3_HALVES_VEC_BF16    : (is_fp16 ? C3_HALVES_VEC_F16    : C3_HALVES_VEC);
  assign sel_c1_half    = is_bf16 ? C1_HALF_VEC_BF16      : (is_fp16 ? C1_HALF_VEC_F16      : C1_HALF_VEC);
  assign sel_c_one      = is_bf16 ? C_ONE_VEC_BF16        : (is_fp16 ? C_ONE_VEC_F16        : C_ONE_VEC);
  assign sel_c2         = is_bf16 ? C2_VEC_BF16           : (is_fp16 ? C2_VEC_F16           : C2_VEC);
  assign sel_pio2_hi    = is_bf16 ? PIO2_HI_VEC_BF16      : (is_fp16 ? PIO2_HI_VEC_F16      : PIO2_HI_VEC);
  assign sel_cos_c2     = is_bf16 ? COS_C2_VEC_BF16       : (is_fp16 ? COS_C2_VEC_F16       : COS_C2_VEC);
  assign sel_sin_s3     = is_bf16 ? SIN_S3_VEC_BF16       : (is_fp16 ? SIN_S3_VEC_F16       : SIN_S3_VEC);
  assign sel_log_scale  = is_bf16 ? LOG_SCALE_VEC_BF16    : (is_fp16 ? LOG_SCALE_VEC_F16    : LOG_SCALE_VEC);
  assign sel_sch_c      = is_bf16 ? SCH_C_VEC_BF16        : (is_fp16 ? SCH_C_VEC_F16        : SCH_C_VEC);
  assign sel_sch_b      = is_bf16 ? SCH_B_VEC_BF16        : (is_fp16 ? SCH_B_VEC_F16        : SCH_B_VEC);
  assign sel_sch_b_cosh = is_bf16 ? SCH_B_COSH_VEC_BF16   : (is_fp16 ? SCH_B_COSH_VEC_F16   : SCH_B_COSH_VEC);
  assign sel_inv_pio2   = is_bf16 ? INV_PIO2_VEC_BF16     : (is_fp16 ? INV_PIO2_VEC_F16     : INV_PIO2_VEC);

// State Machines
  cosh_state_e    nl_state_q,      nl_state_d;
  tanh_state_e    tanh_state_q,    tanh_state_d;
  rsqrt_state_e   rsqrt_state_q,   rsqrt_state_d;
  rec_state_e     rec_state_q,     rec_state_d;
  sin_cos_state_e sin_cos_state_q, sin_cos_state_d;

  `FF(nl_state_q,      nl_state_d,      COSH_EXP_POS_U)
  `FF(tanh_state_q,    tanh_state_d,    TANH_X_SQUARE_U)
  `FF(rsqrt_state_q,   rsqrt_state_d,   RSQRT_X_SQUARE_U)
  `FF(rec_state_q,     rec_state_d,     REC_APPROX_U)
  `FF(sin_cos_state_q, sin_cos_state_d, SIN_COS_RR_U)

// Ack prediction
 logic addmul_predicted_ready;
 logic conv_predicted_ready;

  fpnew_nl_predicted_ready #(.LATENCY(2)) i_pred_ready_addmul (
    .clk_i      (clk_i),
    .rst_ni     (rst_ni),
    .flush_i    (flush_i),
    .in_valid_i (addmul_in_valid_o),
    .in_ready_i (addmul_in_ready_i),
    .out_ready_o(addmul_predicted_ready)   
  );

  fpnew_nl_predicted_ready #(.LATENCY(2)) i_pred_ready_conv (
    .clk_i      (clk_i),
    .rst_ni     (rst_ni),
    .flush_i    (flush_i),
    .in_valid_i (conv_in_valid_o),
    .in_ready_i (conv_in_ready_i),
    .out_ready_o(conv_predicted_ready)    
  );

  logic [WIDTH-1:0]      nl_out_result_q;
  TagType                nl_out_tag_q;
  fpnew_pkg::status_t    nl_out_status_q;
  logic                  nl_out_valid_q;
  logic                  nl_out_capture, nl_out_drain;
  logic                  needs_reconstruction;

 // State machines control logic 
  always_comb begin : in_ready_logic
    unique case (op_i)
        COSHS:    allow_issue_nl = (nl_state_q      == COSH_EXP_NEG_U   || nl_state_q      == COSH_SUM_L   );
        TANHS:    allow_issue_nl = (tanh_state_q    == TANH_X_SQUARE_U  || tanh_state_q    == TANH_POLY3_L );
        RSQRT:    allow_issue_nl = (rsqrt_state_q   == RSQRT_X_SQUARE_U || rsqrt_state_q   == RSQRT_NR2_L  );
        REC:      allow_issue_nl = (rec_state_q     == REC_APPROX_U     || rec_state_q     == REC_NR2_MUL_L);
        SIN, COS: allow_issue_nl = (sin_cos_state_q == SIN_COS_RR_U     || sin_cos_state_q == SIN_COS_RR_L );
        default:  allow_issue_nl = 1'b1;
    endcase
  end

  always_comb begin : state_control
      nl_state_d      = nl_state_q;
      tanh_state_d    = tanh_state_q;
      rsqrt_state_d   = rsqrt_state_q;
      rec_state_d     = rec_state_q;
      sin_cos_state_d = sin_cos_state_q;

      unique case (nl_state_q)
        COSH_EXP_POS_U:   if (is_nl_op_i && in_valid_i && op_i == COSHS && addmul_in_ready_i) nl_state_d = COSH_EXP_NEG_U;
        COSH_EXP_NEG_U:   if (addmul_in_ready_i)                    nl_state_d = COSH_EXP_POS_L;
        COSH_EXP_POS_L:   if (addmul_in_ready_i)                    nl_state_d = COSH_EXP_NEG_L;
        COSH_EXP_NEG_L:   if (addmul_in_ready_i && conv_in_ready_i) nl_state_d = COSH_WAIT_U;
        COSH_WAIT_U:      if (conv_out_valid_i)                     nl_state_d = COSH_SUM_U;
        COSH_SUM_U:       if (addmul_in_ready_i)                    nl_state_d = COSH_WAIT_L;
        COSH_WAIT_L:      if (conv_out_valid_i)                     nl_state_d = COSH_SUM_L;
        COSH_SUM_L:       if (addmul_in_ready_i)                    nl_state_d = COSH_DRAIN;
        COSH_DRAIN:       if (addmul_out_valid_i)                   nl_state_d = COSH_EXP_POS_U;
        default:                                                    nl_state_d = COSH_EXP_POS_U;
      endcase
      unique case (tanh_state_q)
        TANH_X_SQUARE_U:  if (is_nl_op_i && in_valid_i && op_i == TANHS && addmul_in_ready_i) tanh_state_d = TANH_X_SQUARE_L;
        TANH_X_SQUARE_L:  if (addmul_in_ready_i)                    tanh_state_d = TANH_POLY1_U;
        TANH_POLY1_U:     if (addmul_in_valid_o)                    tanh_state_d = TANH_POLY1_L;
        TANH_POLY1_L:     if (addmul_in_valid_o)                    tanh_state_d = TANH_POLY2_U;
        TANH_POLY2_U:     if (addmul_in_valid_o)                    tanh_state_d = TANH_POLY2_L;
        TANH_POLY2_L:     if (addmul_in_valid_o)                    tanh_state_d = TANH_POLY3_U;
        TANH_POLY3_U:     if (addmul_in_valid_o)                    tanh_state_d = TANH_POLY3_L;
        TANH_POLY3_L:     if (addmul_in_valid_o)                    tanh_state_d = TANH_DRAIN_U;
        TANH_DRAIN_U:     if (addmul_out_valid_i)                   tanh_state_d = TANH_DRAIN_L;
        TANH_DRAIN_L:     if (~nl_out_capture)                      tanh_state_d = TANH_X_SQUARE_U;
        default:                                                    tanh_state_d = TANH_X_SQUARE_U;
      endcase
      unique case (rsqrt_state_q)
        RSQRT_X_SQUARE_U: if (is_nl_op_i && in_valid_i && op_i == RSQRT && addmul_in_ready_i) rsqrt_state_d = RSQRT_X_SQUARE_L;
        RSQRT_X_SQUARE_L: if (addmul_in_ready_i)                    rsqrt_state_d = RSQRT_POLY1_U;
        RSQRT_POLY1_U:    if (addmul_in_valid_o)                    rsqrt_state_d = RSQRT_POLY1_L;
        RSQRT_POLY1_L:    if (addmul_in_valid_o)                    rsqrt_state_d = RSQRT_NR1_U;
        RSQRT_NR1_U:      if (addmul_in_valid_o)                    rsqrt_state_d = RSQRT_NR1_L;
        RSQRT_NR1_L:      if (addmul_in_valid_o)                    rsqrt_state_d = RSQRT_NR2_U;
        RSQRT_NR2_U:      if (addmul_in_valid_o)                    rsqrt_state_d = RSQRT_NR2_L;
        RSQRT_NR2_L:      if (addmul_in_valid_o)                    rsqrt_state_d = RSQRT_DRAIN_U;
        RSQRT_DRAIN_U:    if (addmul_out_valid_i)                   rsqrt_state_d = RSQRT_DRAIN_L;
        RSQRT_DRAIN_L:    if (addmul_out_valid_i)                   rsqrt_state_d = RSQRT_X_SQUARE_U;
        default:                                                    rsqrt_state_d = RSQRT_X_SQUARE_U;
      endcase
      unique case (rec_state_q)
        REC_APPROX_U:    if (is_nl_op_i && in_valid_i && op_i == REC && addmul_in_ready_i) rec_state_d = REC_APPROX_L;
        REC_APPROX_L:    if (addmul_in_ready_i)                     rec_state_d = REC_NR1_MUL_U;
        REC_NR1_MUL_U:   if (addmul_in_valid_o)                     rec_state_d = REC_NR1_MUL_L;
        REC_NR1_MUL_L:   if (addmul_in_valid_o)                     rec_state_d = REC_NR1_ACCUM_U;
        REC_NR1_ACCUM_U: if (addmul_in_valid_o)                     rec_state_d = REC_NR1_ACCUM_L;
        REC_NR1_ACCUM_L: if (addmul_in_valid_o)                     rec_state_d = REC_NR2_MUL_U;
        REC_NR2_MUL_U:   if (addmul_in_valid_o)                     rec_state_d = REC_NR2_MUL_L;
        REC_NR2_MUL_L:   if (addmul_in_valid_o)                     rec_state_d = REC_DRAIN_U;
        REC_DRAIN_U:     if (addmul_out_valid_i)                    rec_state_d = REC_DRAIN_L;
        REC_DRAIN_L:     if (addmul_out_valid_i)                    rec_state_d = REC_APPROX_U;
        default:                                                    rec_state_d = REC_APPROX_U;
      endcase    
      unique case (sin_cos_state_q)
        SIN_COS_RR_U:         if ( is_nl_op_i && in_valid_i && (op_i == SIN || op_i == COS) && addmul_in_ready_i) sin_cos_state_d = SIN_COS_RR_L;
        SIN_COS_RR_L:         if ( in_valid_i && addmul_in_ready_i) sin_cos_state_d = SIN_COS_INT_CONV_U;
        SIN_COS_INT_CONV_U:   if ( addmul_out_valid_i )             sin_cos_state_d = SIN_COS_INT_CONV_L;
        SIN_COS_INT_CONV_L:   if ( addmul_out_valid_i )             sin_cos_state_d = SIN_COS_FLOAT_CONV_U;
        SIN_COS_FLOAT_CONV_U: if ( conv_in_valid_o    )             sin_cos_state_d = SIN_COS_FLOAT_CONV_L;
        SIN_COS_FLOAT_CONV_L: if ( conv_in_valid_o    )             sin_cos_state_d = SIN_COS_POLY1_U;
        SIN_COS_POLY1_U:      if ( conv_out_valid_i   )             sin_cos_state_d = SIN_COS_POLY1_L;
        SIN_COS_POLY1_L:      if ( conv_out_valid_i   )             sin_cos_state_d = SIN_COS_POLY2_U;
        SIN_COS_POLY2_U:      if ( addmul_in_valid_o  )             sin_cos_state_d = SIN_COS_POLY2_L;
        SIN_COS_POLY2_L:      if ( addmul_in_valid_o  )             sin_cos_state_d = SIN_COS_POLY3_U;
        SIN_COS_POLY3_U:      if ( addmul_in_valid_o  )             sin_cos_state_d = SIN_COS_POLY3_L;
        SIN_COS_POLY3_L:      if ( addmul_in_valid_o  )             sin_cos_state_d = SIN_COS_POLY4_U;
        SIN_COS_POLY4_U:      if ( addmul_in_valid_o  )             sin_cos_state_d = SIN_COS_POLY4_L;
        SIN_COS_POLY4_L:      if ( addmul_in_valid_o  )             sin_cos_state_d = SIN_COS_POLY5_U;
        SIN_COS_POLY5_U:      if ( addmul_in_valid_o  )             sin_cos_state_d = SIN_COS_POLY5_L;
        SIN_COS_POLY5_L:      if ( addmul_in_valid_o  )             sin_cos_state_d = SIN_COS_DRAIN_U;
        SIN_COS_DRAIN_U:      if ( addmul_out_valid_i )             sin_cos_state_d = SIN_COS_DRAIN_L;
        SIN_COS_DRAIN_L:      if ( ~nl_out_capture    )             sin_cos_state_d = SIN_COS_RR_U;
        default:                                                    sin_cos_state_d = SIN_COS_RR_U;
      endcase   
  end

// Input pre-processing 
  logic en_log, en_rsqrt, en_rec;

  assign en_log   = is_nl_op_i && (op_i == LOGS);
  assign en_rsqrt = is_nl_op_i && (op_i == RSQRT);
  assign en_rec   = is_nl_op_i && (op_i == REC);

  logic [31:0] log_op_hi, log_op_lo;
  logic [31:0] rsqrt_op_hi, rsqrt_op_lo;
  logic [31:0] rec_op_hi, rec_op_lo;

  assign log_op_hi   = en_log   ? operands_i[0][63:32] : 32'b0;
  assign log_op_lo   = en_log   ? operands_i[0][31:0]  : 32'b0;
  assign rsqrt_op_hi = en_rsqrt ? operands_i[0][63:32] : 32'b0;
  assign rsqrt_op_lo = en_rsqrt ? operands_i[0][31:0]  : 32'b0;
  assign rec_op_hi   = en_rec   ? operands_i[0][63:32] : 32'b0;
  assign rec_op_lo   = en_rec   ? operands_i[0][31:0]  : 32'b0;

  // LOGS Pre-calculation: (X - 1.0f)
  logic neg_or_nan_hi, neg_or_nan_lo;
  logic [31:0] log_res_hi, log_res_lo;

  assign neg_or_nan_hi       = log_op_hi[31] || ((log_op_hi[30:23] == 8'hFF) && (log_op_hi[22:0] != '0));
  assign neg_or_nan_lo       = log_op_lo[31] || ((log_op_lo[30:23] == 8'hFF) && (log_op_lo[22:0] != '0));
  assign log_res_hi          = neg_or_nan_hi ? 32'h7FC00000 : ($signed(log_op_hi) - $signed(32'h3F800000));
  assign log_res_lo          = neg_or_nan_lo ? 32'h7FC00000 : ($signed(log_op_lo) - $signed(32'h3F800000));

  // RSQRT Pre-calculation: Magic Constant - (X >> 1)
  logic [31:0] rsqrt_res_hi, rsqrt_res_lo;
  logic rsqrt_neg_or_nan_hi, rsqrt_neg_or_nan_lo;

  assign rsqrt_neg_or_nan_hi = rsqrt_op_hi[31] || ((rsqrt_op_hi[30:23] == 8'hFF) && (rsqrt_op_hi[22:0] != '0));
  assign rsqrt_neg_or_nan_lo = rsqrt_op_lo[31] || ((rsqrt_op_lo[30:23] == 8'hFF) && (rsqrt_op_lo[22:0] != '0));
  assign rsqrt_res_hi        = rsqrt_neg_or_nan_hi ? 32'h7FC00000 : ($signed(QUAKE_MAGIC) - $signed(rsqrt_op_hi >> 1));
  assign rsqrt_res_lo        = rsqrt_neg_or_nan_lo ? 32'h7FC00000 : ($signed(QUAKE_MAGIC) - $signed(rsqrt_op_lo >> 1));

   // REC Pre-calculation: Magic Constant - X
  logic rec_hi_nan_or_sign, rec_lo_nan_or_sign;
  logic [31:0] rec_res_hi, rec_res_lo;

  assign rec_hi_nan_or_sign  = (rec_op_hi == 32'h00000000) || ((rec_op_hi[30:23] == 8'hFF) && (rec_op_hi[22:0] != '0));
  assign rec_lo_nan_or_sign  = (rec_op_lo == 32'h00000000) || ((rec_op_lo[30:23] == 8'hFF) && (rec_op_lo[22:0] != '0));
  assign rec_res_hi          = rec_hi_nan_or_sign ? 32'h7FC00000 : ($signed(REC_MAGIC) - $signed(rec_op_hi));
  assign rec_res_lo          = rec_lo_nan_or_sign ? 32'h7FC00000 : ($signed(REC_MAGIC) - $signed(rec_op_lo));

  // ── Unified pre-results: FP16 (4 × 16-bit) or FP32 (2 × 32-bit) ──
  logic [WIDTH-1:0] log_pre, rsqrt_pre, rec_pre;

  always_comb begin : nl_pre_mux
    // Default: FP32 path
    log_pre   = {log_res_hi, log_res_lo};
    rsqrt_pre = {rsqrt_res_hi, rsqrt_res_lo};
    rec_pre   = {rec_res_hi, rec_res_lo};

    if (is_fp16) begin
      for (int l = 0; l < 4; l++) begin
        // LOG: (x − 1.0h) as signed integer, fed to I2F
        if (en_log) begin
          if (operands_i[0][l*16 + 15]
              || (operands_i[0][l*16+10 +: 5] == 5'h1F && operands_i[0][l*16 +: 10] != '0))
            log_pre[l*16 +: 16] = 16'h7E00;
          else
            log_pre[l*16 +: 16] = operands_i[0][l*16 +: 16] - 16'h3C00;
        end
        // RSQRT: magic − (x >> 1)
        if (en_rsqrt) begin
          if (operands_i[0][l*16 + 15]
              || (operands_i[0][l*16+10 +: 5] == 5'h1F && operands_i[0][l*16 +: 10] != '0))
            rsqrt_pre[l*16 +: 16] = 16'h7E00;
          else
            rsqrt_pre[l*16 +: 16] = QUAKE_MAGIC_F16 - (operands_i[0][l*16 +: 16] >> 1);
        end
        // REC: magic − x  (zero or NaN → NaN)
        if (en_rec) begin
          if ((operands_i[0][l*16 +: 16] == 16'h0000)
              || (operands_i[0][l*16+10 +: 5] == 5'h1F && operands_i[0][l*16 +: 10] != '0))
            rec_pre[l*16 +: 16] = 16'h7E00;
          else
            rec_pre[l*16 +: 16] = REC_MAGIC_F16 - operands_i[0][l*16 +: 16];
        end
      end
    end

    if (is_bf16) begin
      for (int l = 0; l < 4; l++) begin
        // LOG: (x − 1.0bf16) as signed integer, fed to I2F
        // BF16: exp[14:7] (8 bits), mantissa[6:0] (7 bits), qNaN=0x7FC0
        if (en_log) begin
          if (operands_i[0][l*16 + 15]
              || (operands_i[0][l*16+7 +: 8] == 8'hFF && operands_i[0][l*16 +: 7] != '0))
            log_pre[l*16 +: 16] = 16'h7FC0;
          else
            log_pre[l*16 +: 16] = operands_i[0][l*16 +: 16] - 16'h3F80;
        end
        // RSQRT: magic − (x >> 1)
        if (en_rsqrt) begin
          if (operands_i[0][l*16 + 15]
              || (operands_i[0][l*16+7 +: 8] == 8'hFF && operands_i[0][l*16 +: 7] != '0))
            rsqrt_pre[l*16 +: 16] = 16'h7FC0;
          else
            rsqrt_pre[l*16 +: 16] = QUAKE_MAGIC_BF16 - (operands_i[0][l*16 +: 16] >> 1);
        end
        // REC: magic − x  (zero or NaN → NaN)
        if (en_rec) begin
          if ((operands_i[0][l*16 +: 16] == 16'h0000)
              || (operands_i[0][l*16+7 +: 8] == 8'hFF && operands_i[0][l*16 +: 7] != '0))
            rec_pre[l*16 +: 16] = 16'h7FC0;
          else
            rec_pre[l*16 +: 16] = REC_MAGIC_BF16 - operands_i[0][l*16 +: 16];
        end
      end
    end
  end

// Buffers
  logic [WIDTH-1:0] nl_intermediate_0_d, nl_intermediate_1_d, nl_intermediate_2_d, nl_intermediate_3_d;
  logic [WIDTH-1:0] nl_intermediate_0_q, nl_intermediate_1_q, nl_intermediate_2_q, nl_intermediate_3_q;
  logic             nl_wr_en_0, nl_wr_en_1, nl_wr_en_2, nl_wr_en_3, k_en_0, k_en_1;
  logic [7:0] k_int_d_0, k_int_d_1, k_int_q_0, k_int_q_1;

  `FFL(nl_intermediate_0_q, nl_intermediate_0_d, nl_wr_en_0, '0)
  `FFL(nl_intermediate_1_q, nl_intermediate_1_d, nl_wr_en_1, '0)
  `FFL(nl_intermediate_2_q, nl_intermediate_2_d, nl_wr_en_2, '0)
  `FFL(nl_intermediate_3_q, nl_intermediate_3_d, nl_wr_en_3, '0)
  `FFL(k_int_q_0, k_int_d_0, k_en_0, '0)
  `FFL(k_int_q_1, k_int_d_1, k_en_1, '0)

  always_comb begin : buffer_control
    nl_intermediate_0_d = '0; nl_intermediate_1_d = '0; nl_intermediate_2_d = '0; nl_intermediate_3_d = '0;
    nl_wr_en_0 = 1'b0; nl_wr_en_1 = 1'b0; nl_wr_en_2 = 1'b0; nl_wr_en_3 = 1'b0;
    k_int_d_0 = '0; k_int_d_1 = '0; 
    k_en_0 = 1'b0; k_en_1 = 1'b0;

    unique case (op_i)
    COSHS: begin
        nl_intermediate_0_d = conv_result_i;
        nl_wr_en_0          = (nl_state_q == COSH_WAIT_U || nl_state_q == COSH_WAIT_L) && conv_out_valid_i;
    end
    TANHS: begin
        nl_wr_en_0          = (tanh_state_q == TANH_X_SQUARE_U) && in_valid_i;
        nl_wr_en_1          = (tanh_state_q == TANH_X_SQUARE_L) && in_valid_i;
        nl_wr_en_2          = (tanh_state_q == TANH_POLY1_U)    && addmul_out_valid_i;
        nl_wr_en_3          = (tanh_state_q == TANH_POLY1_L)    && addmul_out_valid_i;
        nl_intermediate_0_d = operands_i[0];
        nl_intermediate_1_d = operands_i[0];
        nl_intermediate_2_d = addmul_result_i;
        nl_intermediate_3_d = addmul_result_i;
    end
    RSQRT: begin       
        nl_wr_en_0          = (rsqrt_state_q == RSQRT_X_SQUARE_U) && in_valid_i;
        nl_wr_en_1          = (rsqrt_state_q == RSQRT_X_SQUARE_L) && in_valid_i;
        nl_wr_en_2          = (rsqrt_state_q == RSQRT_X_SQUARE_U) && in_valid_i;
        nl_wr_en_3          = (rsqrt_state_q == RSQRT_X_SQUARE_L) && in_valid_i;
        nl_intermediate_0_d = operands_i[0];
        nl_intermediate_1_d = operands_i[0]; 
        nl_intermediate_2_d = rsqrt_pre;
        nl_intermediate_3_d = rsqrt_pre;       
    end 
    REC: begin       
        nl_wr_en_0          =  (rec_state_q == REC_APPROX_U) && in_valid_i;
        nl_wr_en_1          =  (rec_state_q == REC_APPROX_L) && in_valid_i;
        nl_wr_en_2          = ((rec_state_q == REC_APPROX_U) && in_valid_i) || ((rec_state_q == REC_NR1_ACCUM_U) && addmul_out_valid_i);
        nl_wr_en_3          = ((rec_state_q == REC_APPROX_L) && in_valid_i) || ((rec_state_q == REC_NR1_ACCUM_L) && addmul_out_valid_i);
        nl_intermediate_0_d = operands_i[0];
        nl_intermediate_1_d = operands_i[0]; 
        nl_intermediate_2_d = rec_state_q == REC_NR1_ACCUM_U ? addmul_result_i : rec_pre;
        nl_intermediate_3_d = rec_state_q == REC_NR1_ACCUM_L ? addmul_result_i : rec_pre; 
    end
    SIN,COS: begin 
        nl_wr_en_0          = ((sin_cos_state_q == SIN_COS_RR_U)    && in_valid_i) 
                           || ((sin_cos_state_q == SIN_COS_POLY2_U) && addmul_out_valid_i)
                           || ((sin_cos_state_q == SIN_COS_POLY5_U) && addmul_out_valid_i);
        nl_wr_en_1          = ((sin_cos_state_q == SIN_COS_RR_L)    && in_valid_i)
                           || ((sin_cos_state_q == SIN_COS_POLY2_L) && addmul_out_valid_i)
                           || ((sin_cos_state_q == SIN_COS_POLY5_L) && addmul_out_valid_i);
        nl_wr_en_2          = ((sin_cos_state_q == SIN_COS_POLY3_U) && addmul_out_valid_i);
        nl_wr_en_3          = ((sin_cos_state_q == SIN_COS_POLY3_L) && addmul_out_valid_i);
        nl_intermediate_0_d =   sin_cos_state_q == SIN_COS_RR_U ? operands_i[0]: addmul_result_i;
        nl_intermediate_1_d =   sin_cos_state_q == SIN_COS_RR_L ? operands_i[0]: addmul_result_i; 
        nl_intermediate_2_d = addmul_result_i;
        nl_intermediate_3_d = addmul_result_i;

        k_en_1              = sin_cos_state_q == SIN_COS_FLOAT_CONV_U;
        k_en_0              = sin_cos_state_q == SIN_COS_FLOAT_CONV_L;
        if (is_fp16 || is_bf16) begin
          // FP16/BF16: 4 × 16-bit F2I results — extract 2 LSBs per lane
          for (int l = 0; l < 4; l++) begin
            k_int_d_1[l*2 +: 2] = conv_result_i[l*16 +: 2];
            k_int_d_0[l*2 +: 2] = conv_result_i[l*16 +: 2];
          end
        end else begin
          // FP32: 2 × 32-bit F2I results
          k_int_d_1[3:2] = conv_result_i[33:32];
          k_int_d_1[1:0] = conv_result_i[1:0];
          k_int_d_0[3:2] = conv_result_i[33:32];
          k_int_d_0[1:0] = conv_result_i[1:0];
        end

    end
    default: begin
    end
    endcase
  end

// Input Muxes
  always_comb begin : addmul_input_mux

    addmul_src_fmt_o  = src_fmt_i;
    addmul_dst_fmt_o  = dst_fmt_i;
    addmul_int_fmt_o  = int_fmt_i;
    addmul_tag_o      = tag_i;
    addmul_in_valid_o = in_valid_i;
    addmul_operands_o = operands_i;
    addmul_rnd_mode_o = rnd_mode_i;
    addmul_op_o       = op_i;
    addmul_op_mod_o   = op_mod_i;

    if (is_nl_op_i) begin
      unique case (op_i)
        EXPS: begin
          addmul_operands_o[0] = sel_sch_c;       
          addmul_operands_o[1] = operands_i[0];   
          addmul_operands_o[2] = sel_sch_b;       
          addmul_rnd_mode_o    = fpnew_pkg::RNE;
          addmul_op_o          = fpnew_pkg::FMADD;
          addmul_op_mod_o      = 1'b0;
        end
        COSHS: begin
          unique case (nl_state_q)
            COSH_EXP_POS_U, COSH_EXP_POS_L: begin
              addmul_operands_o[0] = sel_sch_c;    
              addmul_operands_o[1] = operands_i[0];     
              addmul_operands_o[2] = sel_sch_b_cosh;      
              addmul_rnd_mode_o    = fpnew_pkg::RNE;
              addmul_op_o          = fpnew_pkg::FMADD;
              addmul_op_mod_o      = 1'b0;
              addmul_in_valid_o    = nl_state_q == COSH_EXP_POS_L? addmul_out_valid_i : in_valid_i;
            end
            COSH_EXP_NEG_U, COSH_EXP_NEG_L: begin
              addmul_operands_o[0] = sel_sch_c;
              addmul_operands_o[1] = operands_i[0];
              addmul_operands_o[2] = sel_sch_b_cosh;
              addmul_rnd_mode_o    = fpnew_pkg::RNE;
              addmul_op_o          = fpnew_pkg::FNMSUB;
              addmul_op_mod_o      = 1'b0;
            end
            COSH_SUM_U, COSH_SUM_L: begin
              addmul_operands_o[2] = nl_intermediate_0_q;   
              addmul_operands_o[1] = conv_result_i; 
              addmul_operands_o[0] = '0;              
              addmul_rnd_mode_o    = fpnew_pkg::RNE;
              addmul_op_o          = fpnew_pkg::ADD;
              addmul_op_mod_o      = 1'b0;
              addmul_tag_o         = conv_tag_result_i;
            end
            COSH_WAIT_U,COSH_WAIT_L,COSH_DRAIN: begin
                addmul_in_valid_o = 1'b0;
            end
            default:;
          endcase
        end
        TANHS: begin
            unique case (tanh_state_q)
                TANH_X_SQUARE_U, TANH_X_SQUARE_L: begin
                addmul_operands_o[0] = operands_i[0]; 
                addmul_operands_o[1] = operands_i[0]; 
                addmul_operands_o[2] = '0;            
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                end
                TANH_POLY1_U, TANH_POLY1_L: begin
                addmul_operands_o[0] = addmul_result_i; 
                addmul_operands_o[1] = sel_cheby_a; 
                addmul_operands_o[2] = sel_cheby_b; 
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::FMADD;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_tag_result_i;
                addmul_in_valid_o    = addmul_out_valid_i;
                end
                TANH_POLY2_U, TANH_POLY2_L: begin
                addmul_operands_o[0] = tanh_state_q == TANH_POLY2_U ? nl_intermediate_2_q : nl_intermediate_3_q;
                addmul_operands_o[1] = addmul_result_i; 
                addmul_operands_o[2] = sel_cheby_c;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::FMADD;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_tag_result_i;
                addmul_in_valid_o    = addmul_out_valid_i;
                end
                TANH_POLY3_U, TANH_POLY3_L: begin
                addmul_operands_o[0] = addmul_result_i; 
                addmul_operands_o[1] = tanh_state_q == TANH_POLY3_U ? nl_intermediate_0_q : nl_intermediate_1_q; 
                addmul_operands_o[2] = '0;           
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_tag_result_i;
                addmul_in_valid_o    = addmul_out_valid_i;
                end
                TANH_DRAIN_U, TANH_DRAIN_L: begin
                    addmul_in_valid_o = 1'b0;
                end
                default:;      
            endcase
        end
        LOGS: begin
           addmul_operands_o[0] = sel_log_scale;       
           addmul_operands_o[1] = conv_result_i;
           addmul_operands_o[2] = '0;
           addmul_rnd_mode_o    = fpnew_pkg::RNE;
           addmul_op_o          = fpnew_pkg::MUL;
           addmul_op_mod_o      = 1'b0;
           addmul_tag_o         = conv_tag_result_i;
           addmul_in_valid_o    = conv_out_valid_i;
        end
        RSQRT: begin
          unique case(rsqrt_state_q)
            RSQRT_X_SQUARE_U, RSQRT_X_SQUARE_L: begin
                addmul_operands_o[0] = rsqrt_pre;       
                addmul_operands_o[1] = rsqrt_pre;
                addmul_operands_o[2] = '0;   
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
            end
            RSQRT_POLY1_U, RSQRT_POLY1_L: begin
                addmul_operands_o[0] = addmul_result_i;       
                addmul_operands_o[1] = rsqrt_state_q == RSQRT_POLY1_U ? nl_intermediate_0_q : nl_intermediate_1_q;
                addmul_operands_o[2] = '0;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_tag_result_i;
                addmul_in_valid_o    = addmul_out_valid_i;
            end
            RSQRT_NR1_U, RSQRT_NR1_L: begin
                addmul_operands_o[0] = addmul_result_i;       
                addmul_operands_o[1] = sel_c1_half; 
                addmul_operands_o[2] = sel_c3_halves;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::FMADD;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_tag_result_i;
                addmul_in_valid_o    = addmul_out_valid_i;
            end
            RSQRT_NR2_U, RSQRT_NR2_L: begin
                addmul_operands_o[0] = addmul_result_i;       
                addmul_operands_o[1] = rsqrt_state_q == RSQRT_NR2_U ? nl_intermediate_2_q : nl_intermediate_3_q;
                addmul_operands_o[2] = '0;  
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_tag_result_i;
                addmul_in_valid_o    = addmul_out_valid_i;
            end
            RSQRT_DRAIN_U, RSQRT_DRAIN_L: begin
                addmul_in_valid_o = 1'b0;
            end
            endcase
        end
        REC: begin
          unique case(rec_state_q)
            REC_APPROX_U, REC_APPROX_L: begin
                addmul_operands_o[0] = rec_pre;       
                addmul_operands_o[1] = operands_i[0];
                addmul_operands_o[2] = sel_c2;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::FNMSUB;
                addmul_op_mod_o      = 1'b0;
            end
            REC_NR1_MUL_U, REC_NR1_MUL_L: begin
                addmul_operands_o[0] = rec_state_q == REC_NR1_MUL_U ? nl_intermediate_2_q : nl_intermediate_3_q;       
                addmul_operands_o[1] = addmul_result_i;
                addmul_operands_o[2] = '0;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_tag_result_i;
                addmul_in_valid_o    = addmul_out_valid_i;
            end
            REC_NR1_ACCUM_U, REC_NR1_ACCUM_L: begin
                addmul_operands_o[0] = rec_state_q == REC_NR1_ACCUM_U ? nl_intermediate_0_q : nl_intermediate_1_q;       
                addmul_operands_o[1] = addmul_result_i;
                addmul_operands_o[2] = sel_c2;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::FNMSUB;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_tag_result_i;
                addmul_in_valid_o    = addmul_out_valid_i;
            end
            REC_NR2_MUL_U, REC_NR2_MUL_L: begin
                addmul_operands_o[0] = rec_state_q == REC_NR2_MUL_U ? nl_intermediate_2_q : nl_intermediate_3_q;       
                addmul_operands_o[1] = addmul_result_i;
                addmul_operands_o[2] = '0;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_tag_result_i;
                addmul_in_valid_o    = addmul_out_valid_i;
            end
            REC_DRAIN_U, REC_DRAIN_L: begin
                addmul_in_valid_o = 1'b0;
            end
            endcase
        end
        SIN,COS:begin
            unique case(sin_cos_state_q)
                SIN_COS_RR_U, SIN_COS_RR_L: begin
                    addmul_operands_o[0] = operands_i[0];       
                    addmul_operands_o[1] = sel_inv_pio2;
                    addmul_operands_o[2] = '0;
                    addmul_rnd_mode_o    = fpnew_pkg::RNE;
                    addmul_op_o          = fpnew_pkg::MUL;
                    addmul_op_mod_o      = 1'b0;
                end
                SIN_COS_POLY1_U, SIN_COS_POLY1_L: begin 
                    addmul_operands_o[0] = conv_result_i;
                    addmul_operands_o[1] = sel_pio2_hi;
                    addmul_operands_o[2] = sin_cos_state_q == SIN_COS_POLY1_U ? nl_intermediate_0_q : nl_intermediate_1_q;
                    addmul_rnd_mode_o    = fpnew_pkg::RNE;
                    addmul_op_o          = fpnew_pkg::FNMSUB;
                    addmul_op_mod_o      = 1'b0;
                    addmul_tag_o         = conv_tag_result_i;
                    addmul_in_valid_o    = conv_out_valid_i;
                end
                SIN_COS_POLY2_U, SIN_COS_POLY2_L: begin 
                    addmul_operands_o[0] = addmul_result_i;       
                    addmul_operands_o[1] = addmul_result_i;
                    addmul_operands_o[2] = '0;
                    addmul_rnd_mode_o    = fpnew_pkg::RNE;
                    addmul_op_o          = fpnew_pkg::MUL;
                    addmul_op_mod_o      = 1'b0;
                    addmul_tag_o         = addmul_tag_result_i;
                    addmul_in_valid_o    = addmul_out_valid_i;
                end
                SIN_COS_POLY3_U, SIN_COS_POLY3_L: begin 
                    addmul_operands_o[0] =  addmul_result_i;       
                    addmul_operands_o[1] = sel_sin_s3;
                    addmul_operands_o[2] = sel_c_one;
                    addmul_rnd_mode_o    = fpnew_pkg::RNE;
                    addmul_op_o          = fpnew_pkg::FMADD;
                    addmul_op_mod_o      = 1'b0;
                    addmul_tag_o         = addmul_tag_result_i;
                    addmul_in_valid_o    = addmul_out_valid_i;
                end
                SIN_COS_POLY4_U, SIN_COS_POLY4_L: begin 
                    addmul_operands_o[0] =  addmul_result_i;       
                    addmul_operands_o[1] = sin_cos_state_q == SIN_COS_POLY4_U ? nl_intermediate_0_q : nl_intermediate_1_q;
                    addmul_operands_o[2] = '0;
                    addmul_rnd_mode_o    = fpnew_pkg::RNE;
                    addmul_op_o          = fpnew_pkg::MUL;
                    addmul_op_mod_o      = 1'b0;
                    addmul_tag_o         = addmul_tag_result_i;
                    addmul_in_valid_o    = addmul_out_valid_i;
                end
                SIN_COS_POLY5_U, SIN_COS_POLY5_L: begin 
                    addmul_operands_o[0] = sin_cos_state_q == SIN_COS_POLY5_U ? nl_intermediate_2_q : nl_intermediate_3_q;       
                    addmul_operands_o[1] = sel_cos_c2;
                    addmul_operands_o[2] = sel_c_one;
                    addmul_rnd_mode_o    = fpnew_pkg::RNE;
                    addmul_op_o          = fpnew_pkg::FMADD;
                    addmul_op_mod_o      = 1'b0;
                    addmul_tag_o         = addmul_tag_result_i;
                    addmul_in_valid_o    = addmul_out_valid_i;
                end
                SIN_COS_DRAIN_L, SIN_COS_DRAIN_U,
                SIN_COS_INT_CONV_L,SIN_COS_INT_CONV_U,
                SIN_COS_FLOAT_CONV_L, SIN_COS_FLOAT_CONV_U: begin
                    addmul_in_valid_o = 1'b0;
                end
            endcase
        end
      endcase
    end else begin
      addmul_operands_o = operands_i;
      addmul_rnd_mode_o = rnd_mode_i;
      addmul_op_o       = op_i;
      addmul_op_mod_o   = op_mod_i;
      addmul_src_fmt_o  = src_fmt_i;
      addmul_dst_fmt_o  = dst_fmt_i;
      addmul_int_fmt_o  = int_fmt_i;
      addmul_tag_o      = tag_i;
      addmul_in_valid_o = in_valid_i
                          & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(0));
    end
  end

  always_comb begin : conv_input_mux
    conv_operands_o = operands_i;
    conv_rnd_mode_o = rnd_mode_i;
    conv_op_o       = op_i;
    conv_op_mod_o   = op_mod_i;
    conv_src_fmt_o  = src_fmt_i;
    conv_dst_fmt_o  = dst_fmt_i;
    conv_int_fmt_o  = int_fmt_i;
    conv_tag_o      = tag_i;
    conv_in_valid_o = in_valid_i
                           & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(3));
    unique case (op_i)
    EXPS, COSHS: begin
        conv_operands_o[0] = addmul_result_i;   
        conv_operands_o[1] = '0;
        conv_operands_o[2] = '0;
        conv_rnd_mode_o    = fpnew_pkg::RTZ;  
        conv_op_o          = fpnew_pkg::F2I;
        conv_op_mod_o      = 1'b0;
        conv_src_fmt_o     = src_fmt_i;           
        conv_dst_fmt_o     = dst_fmt_i;
        conv_int_fmt_o     = int_fmt_i;           
        conv_tag_o         = addmul_tag_result_i;
        conv_in_valid_o    = addmul_out_valid_i  && !((op_i == COSHS) && (nl_state_q == COSH_SUM_L || nl_state_q == COSH_DRAIN));
    end
    LOGS: begin 
        conv_operands_o[0] = log_pre;   
        conv_operands_o[1] = '0;
        conv_operands_o[2] = '0;
        conv_rnd_mode_o    = fpnew_pkg::RNE;  
        conv_op_o          = fpnew_pkg::I2F;
        conv_op_mod_o      = 1'b0;
        conv_in_valid_o    = in_valid_i;
    end
    SIN,COS: begin 
        unique case(sin_cos_state_q)
            SIN_COS_INT_CONV_U, SIN_COS_INT_CONV_L: begin
                conv_operands_o[0] = addmul_result_i;
                conv_operands_o[1] = '0;
                conv_operands_o[2] = '0;
                conv_rnd_mode_o    = fpnew_pkg::RNE;  
                conv_op_o          = fpnew_pkg::F2I;
                conv_op_mod_o      = 1'b0;
                conv_in_valid_o    = addmul_out_valid_i;
                conv_tag_o         = addmul_tag_result_i;
            end
            SIN_COS_FLOAT_CONV_U, SIN_COS_FLOAT_CONV_L: begin
                conv_operands_o[0] = conv_result_i;   
                conv_operands_o[1] = '0;
                conv_operands_o[2] = '0;
                conv_rnd_mode_o    = fpnew_pkg::RNE;  
                conv_op_o          = fpnew_pkg::I2F;
                conv_op_mod_o      = 1'b0;
                conv_in_valid_o    = conv_out_valid_i;
                conv_tag_o         = conv_tag_result_i;
            end
            default:; 
        endcase
    end
    default:;
    endcase
  end

//  OUTPUT POST-PROCESSING 
  logic [WIDTH-1:0] reconstructed_result;
  assign nl_out_capture = needs_reconstruction
                        & addmul_out_valid_i;

  assign nl_out_drain   = nl_out_valid_q & arb_gnt_i[0];

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni)
      nl_out_valid_q <= 1'b0;
    else if (flush_i)
      nl_out_valid_q <= 1'b0;
    else if (nl_out_capture)
      nl_out_valid_q <= 1'b1;
    else
      nl_out_valid_q <= 1'b0;
  end

  `FFL(nl_out_result_q, reconstructed_result, nl_out_capture, '0)
  `FFL(nl_out_tag_q,    addmul_tag_result_i,  nl_out_capture, '0)
  `FFL(nl_out_status_q, addmul_status_i,      nl_out_capture, '0)

  // Output port assignments — always registered
  assign reconstructed_result_o       = nl_out_result_q;
  assign reconstructed_tag_o          = nl_out_tag_q;
  assign reconstructed_status_o       = nl_out_status_q;
  assign reconstructed_result_valid_o = nl_out_valid_q;

  
  logic [WIDTH-1:0] nl_intermediate_sel;
  logic [7:0]       k_int_sel, k_adj;
  logic [31:0]      x32_0, x32_1, abs32_0, abs32_1;
  logic             sign_0, sign_1;
  logic             is_nan_0, is_inf_0, clamp_mag_0;
  logic             is_nan_1, is_inf_1, clamp_mag_1;
  logic [1:0]       sin_adj;

  assign nl_intermediate_sel = (tanh_state_q == TANH_DRAIN_U || sin_cos_state_q == SIN_COS_DRAIN_U) ? nl_intermediate_0_q : nl_intermediate_1_q;
  assign k_int_sel           = sin_cos_state_q == SIN_COS_DRAIN_U ? k_int_q_1 : k_int_q_0;
  assign sin_adj             = (op_i == SIN) ? 2'd1 : 2'd0;

  for (genvar gl = 0; gl < 4; gl++)
    assign k_adj[gl*2 +: 2] = k_int_sel[gl*2 +: 2] - sin_adj;

  always_comb begin : output_postproc
    reconstructed_result = addmul_result_i; 
    x32_0    = 32'b0; x32_1    = 32'b0;
    sign_0   = 1'b0;  sign_1   = 1'b0;
    abs32_0  = 32'b0; abs32_1  = 32'b0;
    is_nan_0 = 1'b0;  is_inf_0 = 1'b0; clamp_mag_0 = 1'b0;
    is_nan_1 = 1'b0;  is_inf_1 = 1'b0; clamp_mag_1 = 1'b0;
    if (needs_reconstruction) begin
    // --- TANH Reconstruction ---
        if (op_i == TANHS) begin
          if (is_bf16) begin
            for (int l = 0; l < 4; l++)
              if (nl_intermediate_sel[l*16 + 14])                       
                reconstructed_result[l*16 +: 16] = {nl_intermediate_sel[l*16 + 15], 15'h3F80};
          end else if (is_fp16) begin
            for (int l = 0; l < 4; l++)
              if (nl_intermediate_sel[l*16 + 14])                      
                reconstructed_result[l*16 +: 16] = {nl_intermediate_sel[l*16 + 15], 15'h3C00};
          end else begin
            // FP32: 2 × 32-bit lanes
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
        end
        // SIN/COS Quadrant Reconstruction --- 
        else if ((op_i == SIN || op_i == COS)) begin
          if (is_fp16 || is_bf16) begin
            for (int l = 0; l < 4; l++) begin
              if (k_adj[l*2])  // cos path
                reconstructed_result[l*16 +: 16] = {nl_intermediate_sel[l*16 + 15] ^ (k_adj[l*2] ^ k_adj[l*2 + 1]),
                                                     nl_intermediate_sel[l*16 +: 15]};
              else              // sin path
                reconstructed_result[l*16 +: 16] = {addmul_result_i[l*16 + 15] ^ (k_adj[l*2] ^ k_adj[l*2 + 1]),
                                                     addmul_result_i[l*16 +: 15]};
            end
          end else begin
            // FP32: 2 × 32-bit lanes
            for (int l = 0; l < 2; l++) begin
              if (k_adj[l*2]) 
                reconstructed_result[l*32 +: 32] = {nl_intermediate_sel[l*32 + 31] ^ (k_adj[l*2] ^ k_adj[l*2 + 1]),
                                                     nl_intermediate_sel[l*32 +: 31]};
              else             
                reconstructed_result[l*32 +: 32] = {addmul_result_i[l*32 + 31] ^ (k_adj[l*2] ^ k_adj[l*2 + 1]),
                                                     addmul_result_i[l*32 +: 31]};
            end
          end
        end
    end
  end

// Arbitration
  logic   tanh_draining, rsqrt_draining, rec_draining, sin_cos_draining;

  assign tanh_draining    = (tanh_state_q    == TANH_DRAIN_U   ) || (tanh_state_q    == TANH_DRAIN_L   );
  assign rsqrt_draining   = (rsqrt_state_q   == RSQRT_DRAIN_U  ) || (rsqrt_state_q   == RSQRT_DRAIN_L  );
  assign rec_draining     = (rec_state_q     == REC_DRAIN_U    ) || (rec_state_q     == REC_DRAIN_L    );
  assign sin_cos_draining = (sin_cos_state_q == SIN_COS_DRAIN_U) || (sin_cos_state_q == SIN_COS_DRAIN_L);
  assign needs_reconstruction = sin_cos_draining || tanh_draining;

  always_comb begin : handshake_management
    opgrp_out_ready_o            = arb_gnt_i;
    arb_req_o                    = opgrp_out_valid_i;

    if (nl_active) begin
      unique case (op_i) 
        EXPS:begin
          opgrp_out_ready_o[0] = addmul_predicted_ready && addmul_out_valid_i;
          arb_req_o[0]         = 1'b0;
        end
        COSHS : begin
          opgrp_out_ready_o[0] = (nl_state_q == COSH_SUM_L  || nl_state_q == COSH_DRAIN)
                                 ? arb_gnt_i[0]                              
                                 : addmul_predicted_ready;
          opgrp_out_ready_o[3] = conv_predicted_ready;                              
          arb_req_o[3]         = 1'b0;
          arb_req_o[0]         = (nl_state_q == COSH_SUM_L  || nl_state_q == COSH_DRAIN) && addmul_out_valid_i; 
        end
        LOGS: begin
          opgrp_out_ready_o[3] = conv_predicted_ready;
          arb_req_o[3]         = 1'b0;
        end
        TANHS, RSQRT, REC: begin
          opgrp_out_ready_o[0] = addmul_predicted_ready;
          arb_req_o[0]         = ((tanh_draining && nl_out_valid_q) || (rsqrt_draining || rec_draining) && addmul_out_valid_i);
        end
        SIN,COS: begin
          opgrp_out_ready_o[0] = addmul_predicted_ready ;
          opgrp_out_ready_o[3] = conv_predicted_ready;
          arb_req_o[3]         = 1'b0;
          arb_req_o[0]         = sin_cos_draining && nl_out_valid_q; 
        end
        default:;
      endcase
    end
  end

  assign in_ready_o = is_nl_op_i
                    ? in_valid_i & addmul_in_ready_i & allow_issue_nl
                    : in_valid_i & opgrp_in_ready_i[fpnew_pkg::get_opgroup(op_i)];

endmodule