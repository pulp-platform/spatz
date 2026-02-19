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

// Spill Registers
  typedef struct packed {
    logic [WIDTH-1:0]   result;
    TagType             tag;
  } spill_res_t;

  spill_res_t addmul_res_d, addmul_res_q;
  logic        addmul_res_valid_q;
  logic        addmul_res_ready_d; // Ready signal from Controller into Spill Reg
  logic        addmul_out_valid_str, addmul_out_ready_str; // Valid signal from Spill Reg to Controller (streaming over same gruop operations)
  
  assign addmul_res_d = {addmul_result_i, addmul_tag_result_i};

  spill_res_t  conv_res_d, conv_res_q;
  logic        conv_res_valid_q;
  logic        conv_res_ready_d; // Ready signal from Controller into Spill Reg
  logic        conv_out_valid_str, conv_out_ready_str; // Valid signal from Spill Reg to Controller (streaming over same gruop operations)
  assign conv_out_valid_str = conv_out_valid_i && (op_i == SIN || op_i == COS || op_i == LOGS || op_i == COSHS);
  assign conv_res_d = {conv_result_i, conv_tag_result_i};

  spill_register_flushable #(
    .T      (spill_res_t),
    .Bypass (1'b0) // Force register mode to break timing paths
  ) i_spill_register_addmul (
    .clk_i   (clk_i),
    .rst_ni  (rst_ni),
    .valid_i (addmul_out_valid_str),
    .flush_i (flush_i),
    .ready_o (addmul_out_ready_str), 
    .data_i  (addmul_res_d),
    .valid_o (addmul_res_valid_q),
    .ready_i (addmul_res_ready_d),   
    .data_o  (addmul_res_q)
  );

  spill_register_flushable #(
    .T      (spill_res_t),
    .Bypass (1'b0) // Force register mode to break timing paths
  ) i_spill_register_conv (
    .clk_i   (clk_i),
    .rst_ni  (rst_ni),
    .valid_i (conv_out_valid_str),
    .flush_i (flush_i),
    .ready_o (conv_out_ready_str), 
    .data_i  (conv_res_d),
    .valid_o (conv_res_valid_q),
    .ready_i (conv_res_ready_d),   
    .data_o  (conv_res_q)
  );
  
  always_comb begin : in_ready_logic
    unique case (op_i)
        COSHS:    allow_issue_nl = (nl_state_q      == COSH_EXP_NEG_U   || nl_state_q      == COSH_SUM_L);
        TANHS:    allow_issue_nl = (tanh_state_q    == TANH_X_SQUARE_U  || tanh_state_q    == TANH_POLY3_L);
        RSQRT:    allow_issue_nl = (rsqrt_state_q   == RSQRT_X_SQUARE_U || rsqrt_state_q   == RSQRT_NR2_L);
        REC:      allow_issue_nl = (rec_state_q     == REC_APPROX_U     || rec_state_q     == REC_NR2_MUL_L);
        SIN, COS: allow_issue_nl = (sin_cos_state_q == SIN_COS_RR_U     || sin_cos_state_q == SIN_COS_RR_L);
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
        COSH_WAIT_U:      if (conv_res_valid_q)                     nl_state_d = COSH_SUM_U;
        COSH_SUM_U:       if (addmul_in_ready_i)                    nl_state_d = COSH_WAIT_L;
        COSH_WAIT_L:      if (conv_res_valid_q)                     nl_state_d = COSH_SUM_L;
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
        TANH_DRAIN_L:     if (addmul_out_valid_i)                   tanh_state_d = TANH_X_SQUARE_U;
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
        SIN_COS_RR_U:         if (is_nl_op_i && in_valid_i && (op_i == SIN || op_i == COS) && addmul_in_ready_i) sin_cos_state_d = SIN_COS_RR_L;
        SIN_COS_RR_L:         if (in_valid_i && addmul_in_ready_i)  sin_cos_state_d = SIN_COS_INT_CONV_U;
        SIN_COS_INT_CONV_U:   if (addmul_res_valid_q)               sin_cos_state_d = SIN_COS_INT_CONV_L;
        SIN_COS_INT_CONV_L:   if (addmul_res_valid_q)               sin_cos_state_d = SIN_COS_FLOAT_CONV_U;
        SIN_COS_FLOAT_CONV_U: if (conv_in_valid_o)                  sin_cos_state_d = SIN_COS_FLOAT_CONV_L;
        SIN_COS_FLOAT_CONV_L: if (conv_in_valid_o)                  sin_cos_state_d = SIN_COS_POLY1_U;
        SIN_COS_POLY1_U:      if (conv_res_valid_q)                 sin_cos_state_d = SIN_COS_POLY1_L;
        SIN_COS_POLY1_L:      if (conv_res_valid_q)                 sin_cos_state_d = SIN_COS_POLY2_U;
        SIN_COS_POLY2_U:      if (addmul_in_valid_o)                sin_cos_state_d = SIN_COS_POLY2_L;
        SIN_COS_POLY2_L:      if (addmul_in_valid_o)                sin_cos_state_d = SIN_COS_POLY3_U;
        SIN_COS_POLY3_U:      if (addmul_in_valid_o)                sin_cos_state_d = SIN_COS_POLY3_L;
        SIN_COS_POLY3_L:      if (addmul_in_valid_o)                sin_cos_state_d = SIN_COS_POLY4_U;
        SIN_COS_POLY4_U:      if (addmul_in_valid_o)                sin_cos_state_d = SIN_COS_POLY4_L;
        SIN_COS_POLY4_L:      if (addmul_in_valid_o)                sin_cos_state_d = SIN_COS_POLY5_U;
        SIN_COS_POLY5_U:      if (addmul_in_valid_o)                sin_cos_state_d = SIN_COS_POLY5_L;
        SIN_COS_POLY5_L:      if (addmul_in_valid_o)                sin_cos_state_d = SIN_COS_DRAIN_U;
        SIN_COS_DRAIN_U:      if (addmul_out_valid_i)               sin_cos_state_d = SIN_COS_DRAIN_L;
        SIN_COS_DRAIN_L:      if (addmul_out_valid_i)               sin_cos_state_d = SIN_COS_RR_U;
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

// Buffers
  logic [WIDTH-1:0] nl_intermediate_0_d, nl_intermediate_1_d, nl_intermediate_2_d, nl_intermediate_3_d;
  logic [WIDTH-1:0] nl_intermediate_0_q, nl_intermediate_1_q, nl_intermediate_2_q, nl_intermediate_3_q;
  logic             nl_wr_en_0, nl_wr_en_1, nl_wr_en_2, nl_wr_en_3, k_en_0, k_en_1;
  logic [3:0] k_int_d_0, k_int_d_1, k_int_q_0, k_int_q_1;

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
        nl_intermediate_0_d = conv_res_q.result;
        nl_wr_en_0          = (nl_state_q == COSH_WAIT_U || nl_state_q == COSH_WAIT_L) && conv_res_valid_q;
    end
    TANHS: begin
        nl_wr_en_0          = (tanh_state_q == TANH_X_SQUARE_U) && in_valid_i        ;
        nl_wr_en_1          = (tanh_state_q == TANH_X_SQUARE_L) && in_valid_i        ;
        nl_wr_en_2          = (tanh_state_q == TANH_POLY1_U)    && addmul_out_ready_str;
        nl_wr_en_3          = (tanh_state_q == TANH_POLY1_L)    && addmul_out_ready_str;
        nl_intermediate_0_d = operands_i[0];
        nl_intermediate_1_d = operands_i[0];
        nl_intermediate_2_d = addmul_res_q.result;
        nl_intermediate_3_d = addmul_res_q.result;
    end
    RSQRT: begin       
        nl_wr_en_0          = (rsqrt_state_q == RSQRT_X_SQUARE_U) && in_valid_i;
        nl_wr_en_1          = (rsqrt_state_q == RSQRT_X_SQUARE_L) && in_valid_i;
        nl_wr_en_2          = (rsqrt_state_q == RSQRT_X_SQUARE_U) && in_valid_i;
        nl_wr_en_3          = (rsqrt_state_q == RSQRT_X_SQUARE_L) && in_valid_i;
        nl_intermediate_0_d = operands_i[0];
        nl_intermediate_1_d = operands_i[0]; 
        nl_intermediate_2_d = {rsqrt_res_hi, rsqrt_res_lo};
        nl_intermediate_3_d = {rsqrt_res_hi, rsqrt_res_lo};       
    end 
    REC: begin       
        nl_wr_en_0          =  (rec_state_q == REC_APPROX_U) && in_valid_i;
        nl_wr_en_1          =  (rec_state_q == REC_APPROX_L) && in_valid_i;
        nl_wr_en_2          = ((rec_state_q == REC_APPROX_U) && in_valid_i) || ((rec_state_q == REC_NR1_ACCUM_U) && addmul_out_ready_str);
        nl_wr_en_3          = ((rec_state_q == REC_APPROX_L) && in_valid_i) || ((rec_state_q == REC_NR1_ACCUM_L) && addmul_out_ready_str);
        nl_intermediate_0_d = operands_i[0];
        nl_intermediate_1_d = operands_i[0]; 
        nl_intermediate_2_d = rec_state_q == REC_NR1_ACCUM_U ? addmul_res_q.result : {rec_res_hi, rec_res_lo};
        nl_intermediate_3_d = rec_state_q == REC_NR1_ACCUM_L ? addmul_res_q.result : {rec_res_hi, rec_res_lo}; 
    end
    SIN,COS: begin 
        nl_wr_en_0          = ((sin_cos_state_q == SIN_COS_RR_U)    && in_valid_i) 
                           || ((sin_cos_state_q == SIN_COS_POLY2_U) && addmul_out_ready_str)
                           || ((sin_cos_state_q == SIN_COS_POLY5_U) && addmul_out_ready_str);
        nl_wr_en_1          = ((sin_cos_state_q == SIN_COS_RR_L)    && in_valid_i)
                           || ((sin_cos_state_q == SIN_COS_POLY2_L) && addmul_out_ready_str)
                           || ((sin_cos_state_q == SIN_COS_POLY5_L) && addmul_out_ready_str);
        nl_wr_en_2          = ((sin_cos_state_q == SIN_COS_POLY3_U) && addmul_out_ready_str);
        nl_wr_en_3          = ((sin_cos_state_q == SIN_COS_POLY3_L) && addmul_out_ready_str);
        nl_intermediate_0_d =   sin_cos_state_q == SIN_COS_RR_U ? operands_i[0]: addmul_res_q.result;
        nl_intermediate_1_d =   sin_cos_state_q == SIN_COS_RR_L ? operands_i[0]: addmul_res_q.result; 
        nl_intermediate_2_d = addmul_res_q.result;
        nl_intermediate_3_d = addmul_res_q.result;

        k_en_1              = sin_cos_state_q == SIN_COS_FLOAT_CONV_U;
        k_en_0              = sin_cos_state_q == SIN_COS_FLOAT_CONV_L;
        k_int_d_1[3:2]      = conv_res_q.result[33:32];
        k_int_d_1[1:0]      = conv_res_q.result[1:0];
        k_int_d_0[3:2]      = conv_res_q.result[33:32];
        k_int_d_0[1:0]      = conv_res_q.result[1:0];

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
          addmul_operands_o[0] = SCH_C_VEC;       
          addmul_operands_o[1] = operands_i[0];   
          addmul_operands_o[2] = SCH_B_VEC;       
          addmul_rnd_mode_o    = fpnew_pkg::RNE;
          addmul_op_o          = fpnew_pkg::FMADD;
          addmul_op_mod_o      = 1'b0;
        end
        COSHS: begin
          unique case (nl_state_q)
            COSH_EXP_POS_U, COSH_EXP_POS_L: begin
              addmul_operands_o[0] = SCH_C_VEC;    
              addmul_operands_o[1] = operands_i[0];     
              addmul_operands_o[2] = SCH_B_COSH_VEC;      
              addmul_rnd_mode_o    = fpnew_pkg::RNE;
              addmul_op_o          = fpnew_pkg::FMADD;
              addmul_op_mod_o      = 1'b0;
              addmul_in_valid_o    = nl_state_q == COSH_EXP_POS_L? addmul_res_valid_q : in_valid_i;
            end
            COSH_EXP_NEG_U, COSH_EXP_NEG_L: begin
              addmul_operands_o[0] = SCH_C_VEC;
              addmul_operands_o[1] = operands_i[0];
              addmul_operands_o[2] = SCH_B_COSH_VEC;
              addmul_rnd_mode_o    = fpnew_pkg::RNE;
              addmul_op_o          = fpnew_pkg::FNMSUB;
              addmul_op_mod_o      = 1'b0;
            end
            COSH_SUM_U, COSH_SUM_L: begin
              addmul_operands_o[2] = nl_intermediate_0_q;   
              addmul_operands_o[1] = conv_res_q.result; 
              addmul_operands_o[0] = '0;              
              addmul_rnd_mode_o    = fpnew_pkg::RNE;
              addmul_op_o          = fpnew_pkg::ADD;
              addmul_op_mod_o      = 1'b0;
              addmul_tag_o         = conv_res_q.tag;
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
                addmul_operands_o[0] = addmul_res_q.result; 
                addmul_operands_o[1] = CHEBY_A_TANH_VEC; 
                addmul_operands_o[2] = CHEBY_B_TANH_VEC; 
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::FMADD;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
                addmul_in_valid_o    = addmul_res_valid_q;
                end
                TANH_POLY2_U, TANH_POLY2_L: begin
                addmul_operands_o[0] = tanh_state_q == TANH_POLY2_U ? nl_intermediate_2_q : nl_intermediate_3_q;
                addmul_operands_o[1] = addmul_res_q.result; 
                addmul_operands_o[2] = CHEBY_C_TANH_VEC;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::FMADD;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
                addmul_in_valid_o    = addmul_res_valid_q;
                end
                TANH_POLY3_U, TANH_POLY3_L: begin
                addmul_operands_o[0] = addmul_res_q.result; 
                addmul_operands_o[1] = tanh_state_q == TANH_POLY3_U ? nl_intermediate_0_q : nl_intermediate_1_q; 
                addmul_operands_o[2] = '0;           
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
                addmul_in_valid_o    = addmul_res_valid_q;
                end
                TANH_DRAIN_U, TANH_DRAIN_L: begin
                    addmul_in_valid_o = 1'b0;
                end
                default:;      
            endcase
        end
        LOGS: begin
           addmul_operands_o[0] = LOG_SCALE_VEC;       
           addmul_operands_o[1] = conv_res_q.result;
           addmul_operands_o[2] = '0;
           addmul_rnd_mode_o    = fpnew_pkg::RNE;
           addmul_op_o          = fpnew_pkg::MUL;
           addmul_op_mod_o      = 1'b0;
           addmul_tag_o         = conv_res_q.tag;
           addmul_in_valid_o    = conv_res_valid_q;
        end
        RSQRT: begin
          unique case(rsqrt_state_q)
            RSQRT_X_SQUARE_U, RSQRT_X_SQUARE_L: begin
                addmul_operands_o[0] = {rsqrt_res_hi, rsqrt_res_lo};       
                addmul_operands_o[1] = {rsqrt_res_hi, rsqrt_res_lo};
                addmul_operands_o[2] = '0;   
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
            end
            RSQRT_POLY1_U, RSQRT_POLY1_L: begin
                addmul_operands_o[0] = addmul_res_q.result;       
                addmul_operands_o[1] = rsqrt_state_q == RSQRT_POLY1_U ? nl_intermediate_0_q : nl_intermediate_1_q;
                addmul_operands_o[2] = '0;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
                addmul_in_valid_o    = addmul_res_valid_q;
            end
            RSQRT_NR1_U, RSQRT_NR1_L: begin
                addmul_operands_o[0] = addmul_res_q.result;       
                addmul_operands_o[1] = C1_HALF_VEC; 
                addmul_operands_o[2] = C3_HALVES_VEC;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::FMADD;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
                addmul_in_valid_o    = addmul_res_valid_q;
            end
            RSQRT_NR2_U, RSQRT_NR2_L: begin
                addmul_operands_o[0] = addmul_res_q.result;       
                addmul_operands_o[1] = rsqrt_state_q == RSQRT_NR2_U ? nl_intermediate_2_q : nl_intermediate_3_q;
                addmul_operands_o[2] = '0;  
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
                addmul_in_valid_o    = addmul_res_valid_q;
            end
            RSQRT_DRAIN_U, RSQRT_DRAIN_L: begin
                addmul_in_valid_o = 1'b0;
            end
            endcase
        end
        REC: begin
          unique case(rec_state_q)
            REC_APPROX_U, REC_APPROX_L: begin
                addmul_operands_o[0] = {rec_res_hi, rec_res_lo};       
                addmul_operands_o[1] = operands_i[0];
                addmul_operands_o[2] = C2_VEC;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::FNMSUB;
                addmul_op_mod_o      = 1'b0;
            end
            REC_NR1_MUL_U, REC_NR1_MUL_L: begin
                addmul_operands_o[0] = rec_state_q == REC_NR1_MUL_U ? nl_intermediate_2_q : nl_intermediate_3_q;       
                addmul_operands_o[1] = addmul_res_q.result;
                addmul_operands_o[2] = '0;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
                addmul_in_valid_o    = addmul_res_valid_q;
            end
            REC_NR1_ACCUM_U, REC_NR1_ACCUM_L: begin
                addmul_operands_o[0] = rec_state_q == REC_NR1_ACCUM_U ? nl_intermediate_0_q : nl_intermediate_1_q;       
                addmul_operands_o[1] = addmul_res_q.result;
                addmul_operands_o[2] = C2_VEC;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::FNMSUB;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
                addmul_in_valid_o    = addmul_res_valid_q;
            end
            REC_NR2_MUL_U, REC_NR2_MUL_L: begin
                addmul_operands_o[0] = rec_state_q == REC_NR2_MUL_U ? nl_intermediate_2_q : nl_intermediate_3_q;       
                addmul_operands_o[1] = addmul_res_q.result;
                addmul_operands_o[2] = '0;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
                addmul_in_valid_o    = addmul_res_valid_q;
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
                    addmul_operands_o[1] = INV_PIO2_VEC;
                    addmul_operands_o[2] = '0;
                    addmul_rnd_mode_o    = fpnew_pkg::RNE;
                    addmul_op_o          = fpnew_pkg::MUL;
                    addmul_op_mod_o      = 1'b0;
                end
                SIN_COS_POLY1_U, SIN_COS_POLY1_L: begin 
                    addmul_operands_o[0] = conv_res_q.result;
                    addmul_operands_o[1] = PIO2_HI_VEC;
                    addmul_operands_o[2] = sin_cos_state_q == SIN_COS_POLY1_U ? nl_intermediate_0_q : nl_intermediate_1_q;
                    addmul_rnd_mode_o    = fpnew_pkg::RNE;
                    addmul_op_o          = fpnew_pkg::FNMSUB;
                    addmul_op_mod_o      = 1'b0;
                    addmul_tag_o         = conv_res_q.tag;
                    addmul_in_valid_o    = conv_res_valid_q;
                end
                SIN_COS_POLY2_U, SIN_COS_POLY2_L: begin 
                    addmul_operands_o[0] = addmul_res_q.result;       
                    addmul_operands_o[1] = addmul_res_q.result;
                    addmul_operands_o[2] = '0;
                    addmul_rnd_mode_o    = fpnew_pkg::RNE;
                    addmul_op_o          = fpnew_pkg::MUL;
                    addmul_op_mod_o      = 1'b0;
                    addmul_tag_o         = addmul_res_q.tag;
                    addmul_in_valid_o    = addmul_res_valid_q;
                end
                SIN_COS_POLY3_U, SIN_COS_POLY3_L: begin 
                    addmul_operands_o[0] =  addmul_res_q.result;       
                    addmul_operands_o[1] = SIN_S3_VEC;
                    addmul_operands_o[2] = C_ONE_VEC;
                    addmul_rnd_mode_o    = fpnew_pkg::RNE;
                    addmul_op_o          = fpnew_pkg::FMADD;
                    addmul_op_mod_o      = 1'b0;
                    addmul_tag_o         = addmul_res_q.tag;
                    addmul_in_valid_o    = addmul_res_valid_q;
                end
                SIN_COS_POLY4_U, SIN_COS_POLY4_L: begin 
                    addmul_operands_o[0] =  addmul_res_q.result;       
                    addmul_operands_o[1] = sin_cos_state_q == SIN_COS_POLY4_U ? nl_intermediate_0_q : nl_intermediate_1_q;
                    addmul_operands_o[2] = '0;
                    addmul_rnd_mode_o    = fpnew_pkg::RNE;
                    addmul_op_o          = fpnew_pkg::MUL;
                    addmul_op_mod_o      = 1'b0;
                    addmul_tag_o         = addmul_res_q.tag;
                    addmul_in_valid_o    = addmul_res_valid_q;
                end
                SIN_COS_POLY5_U, SIN_COS_POLY5_L: begin 
                    addmul_operands_o[0] = sin_cos_state_q == SIN_COS_POLY5_U ? nl_intermediate_2_q : nl_intermediate_3_q;       
                    addmul_operands_o[1] = COS_C2_VEC;
                    addmul_operands_o[2] = C_ONE_VEC;
                    addmul_rnd_mode_o    = fpnew_pkg::RNE;
                    addmul_op_o          = fpnew_pkg::FMADD;
                    addmul_op_mod_o      = 1'b0;
                    addmul_tag_o         = addmul_res_q.tag;
                    addmul_in_valid_o    = addmul_res_valid_q;
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
        conv_operands_o[0] = addmul_res_q.result;   
        conv_operands_o[1] = '0;
        conv_operands_o[2] = '0;
        conv_rnd_mode_o    = fpnew_pkg::RTZ;  
        conv_op_o          = fpnew_pkg::F2I;
        conv_op_mod_o      = 1'b0;
        conv_src_fmt_o     = src_fmt_i;           
        conv_dst_fmt_o     = dst_fmt_i;
        conv_int_fmt_o     = int_fmt_i;           
        conv_tag_o         = addmul_res_q.tag; 
        conv_in_valid_o    = addmul_res_valid_q  && !((op_i == COSHS) && (nl_state_q == COSH_SUM_L || nl_state_q == COSH_DRAIN));
    end
    LOGS: begin 
        conv_operands_o[0] = {log_res_hi, log_res_lo};   
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
                conv_operands_o[0] = addmul_res_q.result;     // from spill reg: registered ✓
                conv_operands_o[1] = '0;
                conv_operands_o[2] = '0;
                conv_rnd_mode_o    = fpnew_pkg::RNE;  
                conv_op_o          = fpnew_pkg::F2I;
                conv_op_mod_o      = 1'b0;
                conv_in_valid_o    = addmul_res_valid_q;
                conv_tag_o         = addmul_res_q.tag;
            end
            SIN_COS_FLOAT_CONV_U, SIN_COS_FLOAT_CONV_L: begin
                conv_operands_o[0] = conv_res_q.result;   
                conv_operands_o[1] = '0;
                conv_operands_o[2] = '0;
                conv_rnd_mode_o    = fpnew_pkg::RNE;  
                conv_op_o          = fpnew_pkg::I2F;
                conv_op_mod_o      = 1'b0;
                conv_in_valid_o    = conv_res_valid_q;
                conv_tag_o         = conv_res_q.tag;
            end
            default:; 
        endcase
    end
    default:;
    endcase
  end

//  OUTPUT POST-PROCESSING 
  logic [WIDTH-1:0] reconstructed_result;
  logic [WIDTH-1:0] nl_intermediate_sel;
  logic [3:0]       k_int_sel, k_adj;
  logic [31:0]      x32_0, x32_1, abs32_0, abs32_1;
  logic             sign_0, sign_1;
  logic             is_nan_0, is_inf_0, clamp_mag_0;
  logic             is_nan_1, is_inf_1, clamp_mag_1;
  logic             needs_reconstruction;
  logic [1:0]       sin_adj;

  assign nl_intermediate_sel = (tanh_state_q == TANH_DRAIN_U || sin_cos_state_q == SIN_COS_DRAIN_U) ? nl_intermediate_0_q : nl_intermediate_1_q;
  assign k_int_sel           = sin_cos_state_q == SIN_COS_DRAIN_U ? k_int_q_1 : k_int_q_0;
  assign sin_adj             = (op_i == SIN) ? 2'd1 : 2'd0;
  assign k_adj[3:2]          = k_int_sel[3:2] - sin_adj;
  assign k_adj[1:0]          = k_int_sel[1:0] - sin_adj;

  always_comb begin : output_postproc
    reconstructed_result = addmul_result_i; 
    x32_0    = 32'b0; x32_1    = 32'b0;
    sign_0   = 1'b0;  sign_1   = 1'b0;
    abs32_0  = 32'b0; abs32_1  = 32'b0;
    is_nan_0 = 1'b0;  is_inf_0 = 1'b0; clamp_mag_0 = 1'b0;
    is_nan_1 = 1'b0;  is_inf_1 = 1'b0; clamp_mag_1 = 1'b0;
    if (needs_reconstruction) begin
    // --- TANH Reconstruction ---
        if (tanh_state_q == TANH_DRAIN_U || tanh_state_q == TANH_DRAIN_L) begin
        
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
        // SIN/COS Quadrant Reconstruction ---
        else if ((op_i == SIN || op_i == COS)) begin
        unique case (k_adj[3:2])
            2'b00: {sign_1, x32_1} = {addmul_result_i[63],                addmul_result_i[63:32]};
            2'b01: {sign_1, x32_1} = {nl_intermediate_sel[63] ^ 1'b1, nl_intermediate_sel[63:32]};
            2'b10: {sign_1, x32_1} = {addmul_result_i[63] ^ 1'b1,         addmul_result_i[63:32]};
            2'b11: {sign_1, x32_1} = {nl_intermediate_sel[63],        nl_intermediate_sel[63:32]};
        endcase
        // Lane 0 (Lower 32 bits)
        unique case (k_adj[1:0])
            2'b00: {sign_0, x32_0} = {addmul_result_i[31],                addmul_result_i[31:0]};
            2'b01: {sign_0, x32_0} = {nl_intermediate_sel[31] ^ 1'b1, nl_intermediate_sel[31:0]};
            2'b10: {sign_0, x32_0} = {addmul_result_i[31] ^ 1'b1,         addmul_result_i[31:0]};
            2'b11: {sign_0, x32_0} = {nl_intermediate_sel[31],        nl_intermediate_sel[31:0]};
        endcase

        reconstructed_result[63:32] = {sign_1, x32_1[30:0]};
        reconstructed_result[31:0]  = {sign_0, x32_0[30:0]};
        end
    end
  end

// Arbitration
  logic  draining_spill_op, tanh_draining, rsqrt_draining, rec_draining, sin_cos_draining, tanh_loading, rsqrt_loading, rec_loading, sin_cos_loading;

  assign tanh_draining    = (tanh_state_q    == TANH_DRAIN_U)     || (tanh_state_q    == TANH_DRAIN_L);
  assign rsqrt_draining   = (rsqrt_state_q   == RSQRT_DRAIN_U)    || (rsqrt_state_q   == RSQRT_DRAIN_L);
  assign rec_draining     = (rec_state_q     == REC_DRAIN_U)      || (rec_state_q     == REC_DRAIN_L);
  assign sin_cos_draining = (sin_cos_state_q == SIN_COS_DRAIN_U)  || (sin_cos_state_q == SIN_COS_DRAIN_L);
 
  assign tanh_loading     = ((tanh_state_q    == TANH_X_SQUARE_L)  || (tanh_state_q    == TANH_X_SQUARE_U))  && (op_i == TANHS);
  assign rsqrt_loading    = ((rsqrt_state_q   == RSQRT_X_SQUARE_L) || (rsqrt_state_q   == RSQRT_X_SQUARE_U)) && (op_i == RSQRT);
  assign rec_loading      = ((rec_state_q     == REC_APPROX_L)     || (rec_state_q     == REC_APPROX_U))     && (op_i == REC);
  assign sin_cos_loading  = ((sin_cos_state_q == SIN_COS_RR_L)     || (sin_cos_state_q == SIN_COS_RR_U))     && (op_i == SIN || op_i == COS );
 
  assign draining_spill_op    = sin_cos_draining || tanh_draining || rsqrt_draining || rec_draining;
  assign needs_reconstruction = sin_cos_draining || tanh_draining;
  assign addmul_out_valid_str = addmul_out_valid_i && (op_i == TANHS || op_i == RSQRT || op_i == REC || op_i == SIN  || op_i == COS|| op_i == EXPS || op_i == COSHS) && !draining_spill_op;

  always_comb begin : handshake_management
    opgrp_out_ready_o            = arb_gnt_i;
    arb_req_o                    = opgrp_out_valid_i;
    addmul_res_ready_d           = 1'b0;
    conv_res_ready_d             = 1'b0;
    reconstructed_result_o       = reconstructed_result;
    reconstructed_result_valid_o = needs_reconstruction && addmul_out_valid_i;

    if (nl_active) begin
      unique case (op_i) 
        EXPS:begin
          opgrp_out_ready_o[0] = addmul_out_ready_str && addmul_out_valid_i;
          addmul_res_ready_d   = conv_in_ready_i;
          arb_req_o[0]         = 1'b0;
        end
        COSHS : begin
          opgrp_out_ready_o[0] = (nl_state_q == COSH_SUM_L  || nl_state_q == COSH_DRAIN)
                                 ? arb_gnt_i[0]                              
                                 : addmul_out_ready_str && addmul_out_valid_i;
          opgrp_out_ready_o[3] = conv_out_ready_str   && conv_out_valid_i;                              
          addmul_res_ready_d   = !(nl_state_q == COSH_SUM_L  || nl_state_q == COSH_DRAIN
                                 || nl_state_q == COSH_WAIT_L)
                                 && conv_in_ready_i;
          conv_res_ready_d     = addmul_in_ready_i || nl_wr_en_0;
          arb_req_o[3]         = 1'b0;
          arb_req_o[0]         = (nl_state_q == COSH_WAIT_L  || nl_state_q == COSH_DRAIN) && addmul_out_valid_i; 
        end
        LOGS: begin
          opgrp_out_ready_o[3] = conv_out_ready_str && conv_out_valid_i;
          conv_res_ready_d     = addmul_in_ready_i;
          arb_req_o[3]         = 1'b0;
        end
        TANHS, RSQRT, REC: begin
          opgrp_out_ready_o[0] = addmul_out_ready_str && addmul_out_valid_i;
          arb_req_o[0]         = (tanh_draining || rsqrt_draining || rec_draining) && addmul_out_valid_i;
          addmul_res_ready_d   = ~(rec_loading || rsqrt_loading || tanh_loading) && addmul_in_ready_i; 
        end
        SIN,COS: begin
          opgrp_out_ready_o[0] = addmul_out_ready_str && addmul_out_valid_i;
          opgrp_out_ready_o[3] = conv_out_ready_str   && conv_out_valid_i;
          addmul_res_ready_d   = ~sin_cos_loading && addmul_in_ready_i || (conv_in_ready_i && (sin_cos_state_q == SIN_COS_INT_CONV_U || sin_cos_state_q == SIN_COS_INT_CONV_L));
          conv_res_ready_d     = (sin_cos_state_q == SIN_COS_FLOAT_CONV_U || sin_cos_state_q == SIN_COS_FLOAT_CONV_L) && conv_in_ready_i 
                                ||(addmul_in_ready_i) && (sin_cos_state_q == SIN_COS_POLY1_U || sin_cos_state_q == SIN_COS_POLY1_L) ;
          arb_req_o[3]         = 1'b0;
          arb_req_o[0]         = sin_cos_draining && addmul_out_valid_i; 
        end
        default:;
      endcase
    end
  end

  assign in_ready_o = is_nl_op_i
                    ? in_valid_i & addmul_in_ready_i & allow_issue_nl
                    : in_valid_i & opgrp_in_ready_i[fpnew_pkg::get_opgroup(op_i)];

endmodule