
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
  input  logic                               vectorial_op_i,
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

  localparam int unsigned CNT_WIDTH  = $clog2(MAX_INFLIGHT + 1);

  logic [CNT_WIDTH-1:0] nl_inflight_q;
  logic                 cnt_inc, cnt_dec;
  logic                 nl_active;

  // Increment: NL element accepted into ADDMUL pipeline
  assign cnt_inc = is_nl_op_i && in_valid_i && addmul_in_ready_i;

  // Decrement: NL final result consumed at CONV output by arbiter
  assign cnt_dec = nl_active && conv_out_valid_i && arb_gnt_i[3];

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

  // NL routing active while elements are in-flight OR still being issued
  assign nl_active = (nl_inflight_q != '0) || is_nl_op_i;
  assign nl_busy_o = (nl_inflight_q != '0);

// FUNC State Machine
  cosh_state_e      nl_state_q, nl_state_d;
  tanh_state_e      tanh_state_q, tanh_state_d;
  rsqrt_state_e     rsqrt_state_q, rsqrt_state_d;
  rec_state_e     rec_state_q, rec_state_d;
  logic             block_issue_nl;

  always_ff @(posedge clk_i or negedge rst_ni) begin
      if (!rst_ni) begin
        nl_state_q     <= COSH_EXP_POS_U;
        tanh_state_q   <= TANH_X_SQUARE_U;  
        rsqrt_state_q  <= RSQRT_X_SQUARE_U;
        rec_state_q    <= REC_APPROX_U;
      end else if (flush_i) begin
        nl_state_q     <= COSH_EXP_POS_U;
        tanh_state_q   <= TANH_X_SQUARE_U;
        rsqrt_state_q  <= RSQRT_X_SQUARE_U;
        rec_state_q    <= REC_APPROX_U;
      end else begin
        nl_state_q     <= nl_state_d;
        tanh_state_q   <= tanh_state_d;
        rsqrt_state_q  <= rsqrt_state_d;
        rec_state_q    <= rec_state_d;
      end
  end

  // ----------------------------------
  // Spill Register for ADDMUL Output 
  // ----------------------------------
  // Breaks the combinational loop: ADDMUL_valid -> CTRL_ready -> ADDMUL_ready
  typedef struct packed {
    logic [WIDTH-1:0]   result;
    TagType             tag;
  } addmul_res_t;

  addmul_res_t addmul_res_d, addmul_res_q;
  logic        addmul_res_valid_q;
  logic        addmul_res_ready_d; // Ready signal from Controller into Spill Reg
  logic        addmul_out_valid_str, addmul_out_ready_str; // Valid signal from Spill Reg to Controller (streaming over same gruop operations)
  assign addmul_out_valid_str = addmul_out_valid_i && (op_i == TANHS || op_i == RSQRT || op_i == REC );
  assign addmul_res_d = {addmul_result_i, addmul_tag_result_i};

  spill_register_flushable #(
    .T      (addmul_res_t),
    .Bypass (1'b1) // Force register mode to break timing paths
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

  always_comb begin : in_ready_logic
    unique case (op_i)
        COSHS: block_issue_nl = (~(nl_state_q   == COSH_EXP_NEG_U  || nl_state_q   == COSH_SUM_L));
        TANHS: block_issue_nl = (~(tanh_state_q == TANH_X_SQUARE_U || tanh_state_q == TANH_POLY3_L));
        RSQRT: block_issue_nl = (~(rsqrt_state_q == RSQRT_X_SQUARE_U || rsqrt_state_q == RSQRT_NR2_L));
        REC:   block_issue_nl = (~(rec_state_q == REC_APPROX_U || rec_state_q == REC_NR2_MUL_L));
        default:  block_issue_nl = 1'b0;
    endcase
  end

  always_comb begin : state_control
      nl_state_d    = nl_state_q;
      tanh_state_d  = tanh_state_q;
      rsqrt_state_d = rsqrt_state_q;
      rec_state_d   = rec_state_q;
      unique case (nl_state_q)
        COSH_EXP_POS_U:   if (is_nl_op_i && in_valid_i && op_i == COSHS && addmul_in_ready_i) nl_state_d = COSH_EXP_NEG_U;
        COSH_EXP_NEG_U:   if (addmul_in_ready_i)                                              nl_state_d = COSH_EXP_POS_L;
        COSH_EXP_POS_L:   if (addmul_in_ready_i)                                              nl_state_d = COSH_EXP_NEG_L;
        COSH_EXP_NEG_L:   if (addmul_in_ready_i && conv_in_ready_i)                           nl_state_d = COSH_WAIT_U;
        COSH_WAIT_U:      if (conv_out_valid_i)                                               nl_state_d = COSH_SUM_U;
        COSH_SUM_U:       if (addmul_in_ready_i)                                              nl_state_d = COSH_WAIT_L;
        COSH_WAIT_L:      if (conv_out_valid_i)                                               nl_state_d = COSH_SUM_L;
        COSH_SUM_L:       if (addmul_in_ready_i)                                              nl_state_d = COSH_DRAIN;
        COSH_DRAIN:       if (addmul_out_valid_i)                                             nl_state_d = COSH_EXP_POS_U;
        default:                                                                              nl_state_d = COSH_EXP_POS_U;
      endcase
      unique case (tanh_state_q)
        TANH_X_SQUARE_U:  if (is_nl_op_i && in_valid_i && op_i == TANHS && addmul_in_ready_i) tanh_state_d = TANH_X_SQUARE_L;
        TANH_X_SQUARE_L:  if (addmul_in_ready_i)                                              tanh_state_d = TANH_POLY1_U;
        TANH_POLY1_U:     if (addmul_in_ready_i)                                              tanh_state_d = TANH_POLY1_L;
        TANH_POLY1_L:     if (addmul_in_ready_i)                                              tanh_state_d = TANH_POLY2_U;
        TANH_POLY2_U:     if (addmul_in_ready_i)                                              tanh_state_d = TANH_POLY2_L;
        TANH_POLY2_L:     if (addmul_in_ready_i)                                              tanh_state_d = TANH_POLY3_U;
        TANH_POLY3_U:     if (addmul_in_ready_i)                                              tanh_state_d = TANH_POLY3_L;
        TANH_POLY3_L:     if (addmul_in_ready_i)                                              tanh_state_d = TANH_DRAIN_U;
        TANH_DRAIN_U:     if (addmul_out_valid_i)                                             tanh_state_d = TANH_DRAIN_L;
        TANH_DRAIN_L:     if (addmul_out_valid_i)                                             tanh_state_d = TANH_X_SQUARE_U;
        default:                                                                              tanh_state_d = TANH_X_SQUARE_U;
      endcase
      unique case (rsqrt_state_q)
        RSQRT_X_SQUARE_U: if (is_nl_op_i && in_valid_i && op_i == RSQRT && addmul_in_ready_i) rsqrt_state_d = RSQRT_X_SQUARE_L;
        RSQRT_X_SQUARE_L: if (addmul_in_ready_i)                                              rsqrt_state_d = RSQRT_POLY1_U;
        RSQRT_POLY1_U:    if (addmul_in_ready_i)                                              rsqrt_state_d = RSQRT_POLY1_L;
        RSQRT_POLY1_L:    if (addmul_in_ready_i)                                              rsqrt_state_d = RSQRT_NR1_U;
        RSQRT_NR1_U:      if (addmul_in_ready_i)                                              rsqrt_state_d = RSQRT_NR1_L;
        RSQRT_NR1_L:      if (addmul_in_ready_i)                                              rsqrt_state_d = RSQRT_NR2_U;
        RSQRT_NR2_U:      if (addmul_in_ready_i)                                              rsqrt_state_d = RSQRT_NR2_L;
        RSQRT_NR2_L:      if (addmul_in_ready_i)                                              rsqrt_state_d = RSQRT_DRAIN_U;
        RSQRT_DRAIN_U:    if (addmul_out_valid_i)                                             rsqrt_state_d = RSQRT_DRAIN_L;
        RSQRT_DRAIN_L:    if (addmul_out_valid_i)                                             rsqrt_state_d = RSQRT_X_SQUARE_U;
        default:                                                                              rsqrt_state_d = RSQRT_X_SQUARE_U;
      endcase
      unique case (rec_state_q)
        REC_APPROX_U:    if (is_nl_op_i && in_valid_i && op_i == REC && addmul_in_ready_i)    rec_state_d = REC_APPROX_L;
        REC_APPROX_L:    if (addmul_in_ready_i)                                               rec_state_d = REC_NR1_MUL_U;
        REC_NR1_MUL_U:   if (addmul_in_ready_i)                                               rec_state_d = REC_NR1_MUL_L;
        REC_NR1_MUL_L:   if (addmul_in_ready_i)                                               rec_state_d = REC_NR1_ACCUM_U;
        REC_NR1_ACCUM_U: if (addmul_in_ready_i)                                               rec_state_d = REC_NR1_ACCUM_L;
        REC_NR1_ACCUM_L: if (addmul_in_ready_i)                                               rec_state_d = REC_NR2_MUL_U;
        REC_NR2_MUL_U:   if (addmul_in_ready_i)                                               rec_state_d = REC_NR2_MUL_L;
        REC_NR2_MUL_L:   if (addmul_in_ready_i)                                               rec_state_d = REC_DRAIN_U;
        REC_DRAIN_U:     if (addmul_out_valid_i)                                              rec_state_d = REC_DRAIN_L;
        REC_DRAIN_L:     if (addmul_out_valid_i)                                              rec_state_d = REC_APPROX_U;
        default:                                                                              rec_state_d = REC_APPROX_U;
      endcase    
  end

  //----------------
  // Input processing 
  //----------------
  logic enable_precalc;
  assign enable_precalc = is_nl_op_i && (op_i == LOGS || op_i == RSQRT || op_i == REC);
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

   // REC Pre-calculation: Magic Constant - X
  logic rec_hi_nan_or_sign, rec_lo_nan_or_sign;
  logic [31:0] rec_res_hi, rec_res_lo;

  assign rec_hi_nan_or_sign = (op0_hi == 32'h00000000) || ((op0_hi[30:23] == 8'hFF) && (op0_hi[22:0] != '0));
  assign rec_lo_nan_or_sign = (op0_lo == 32'h00000000) || ((op0_lo[30:23] == 8'hFF) && (op0_lo[22:0] != '0));

  // Note: Logical shift (>>) is intended here before subtraction
  assign rec_res_hi = (rec_hi_nan_or_sign) ? 32'h7FC00000 : ($signed(REC_MAGIC) - $signed(op0_hi));
  assign rec_res_lo = (rec_lo_nan_or_sign) ? 32'h7FC00000 : ($signed(REC_MAGIC) - $signed(op0_lo));

  //----------
  // Buffers
  //-----------

   logic [WIDTH-1:0] nl_intermediate_0_d, nl_intermediate_1_d, nl_intermediate_2_d, nl_intermediate_3_d;
   logic [WIDTH-1:0] nl_intermediate_0_q, nl_intermediate_1_q, nl_intermediate_2_q, nl_intermediate_3_q;
   logic             nl_wr_en_0, nl_wr_en_1, nl_wr_en_2, nl_wr_en_3;

  `include "common_cells/registers.svh"
  `FFL(nl_intermediate_0_q, nl_intermediate_0_d, nl_wr_en_0, '0)
  `FFL(nl_intermediate_1_q, nl_intermediate_1_d, nl_wr_en_1, '0)
  `FFL(nl_intermediate_2_q, nl_intermediate_2_d, nl_wr_en_2, '0)
  `FFL(nl_intermediate_3_q, nl_intermediate_3_d, nl_wr_en_3, '0)

  always_comb begin : buffer_control
    nl_intermediate_0_d = '0;
    nl_intermediate_1_d = '0;
    nl_intermediate_2_d = '0;
    nl_intermediate_3_d = '0;
    nl_wr_en_0 = 1'b0;
    nl_wr_en_1 = 1'b0;
    nl_wr_en_2 = 1'b0;
    nl_wr_en_3 = 1'b0;
    unique case (op_i)
    COSHS: begin
          nl_intermediate_0_d = conv_result_i;
          nl_wr_en_0          = (nl_state_q == COSH_WAIT_U || nl_state_q == COSH_WAIT_L) && conv_out_valid_i;
    end
    TANHS: begin
            nl_wr_en_0          = (tanh_state_q == TANH_X_SQUARE_U) && in_valid_i        ;
            nl_wr_en_1          = (tanh_state_q == TANH_X_SQUARE_L) && in_valid_i        ;
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
            nl_intermediate_2_d = {rsqrt_res_hi, rsqrt_res_lo};
            nl_intermediate_3_d = {rsqrt_res_hi, rsqrt_res_lo};
            
    end 
    REC: begin       
            nl_wr_en_0          = (rec_state_q == REC_APPROX_U) && in_valid_i;
            nl_wr_en_1          = (rec_state_q == REC_APPROX_L) && in_valid_i;
            nl_wr_en_2          = ((rec_state_q == REC_APPROX_U) && in_valid_i) || ((rec_state_q == REC_NR1_ACCUM_U) && addmul_out_valid_i);
            nl_wr_en_3          = ((rec_state_q == REC_APPROX_L) && in_valid_i) || ((rec_state_q == REC_NR1_ACCUM_L) && addmul_out_valid_i);
            nl_intermediate_0_d = operands_i[0];
            nl_intermediate_1_d = operands_i[0]; 
            nl_intermediate_2_d = rec_state_q == REC_NR1_ACCUM_U ? addmul_result_i : {rec_res_hi, rec_res_lo};
            nl_intermediate_3_d = rec_state_q == REC_NR1_ACCUM_L ? addmul_result_i : {rec_res_hi, rec_res_lo};
        
    end 
    default: begin
    end
    endcase
  end

  always_comb begin : addmul_input_mux

    addmul_src_fmt_o  = src_fmt_i;
    addmul_dst_fmt_o  = dst_fmt_i;
    addmul_int_fmt_o  = int_fmt_i;
    addmul_tag_o      = tag_i;
    addmul_in_valid_o = in_valid_i;
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
            default: begin
              addmul_operands_o = operands_i;
              addmul_rnd_mode_o = rnd_mode_i;
              addmul_op_o       = op_i;
              addmul_op_mod_o   = op_mod_i;
            end
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
            end
            TANH_POLY2_U, TANH_POLY2_L: begin
              addmul_operands_o[0] = tanh_state_q == TANH_POLY2_U ? nl_intermediate_2_q : nl_intermediate_3_q;
              addmul_operands_o[1] = addmul_res_q.result; 
              addmul_operands_o[2] = CHEBY_C_TANH_VEC;
              addmul_rnd_mode_o    = fpnew_pkg::RNE;
              addmul_op_o          = fpnew_pkg::FMADD;
              addmul_op_mod_o      = 1'b0;
              addmul_tag_o         = addmul_res_q.tag;
            end
            TANH_POLY3_U, TANH_POLY3_L: begin
              addmul_operands_o[0] = addmul_res_q.result; 
              addmul_operands_o[1] = tanh_state_q == TANH_POLY3_U ? nl_intermediate_0_q : nl_intermediate_1_q; 
              addmul_operands_o[2] = '0;            
              addmul_rnd_mode_o    = fpnew_pkg::RNE;
              addmul_op_o          = fpnew_pkg::MUL;
              addmul_op_mod_o      = 1'b0;
              addmul_tag_o         = addmul_res_q.tag;
            end
            TANH_DRAIN_U, TANH_DRAIN_L: begin
                addmul_in_valid_o = 1'b0;
            end
            default: begin
            addmul_operands_o = operands_i;
            addmul_rnd_mode_o = rnd_mode_i;
            addmul_op_o       = op_i;
            addmul_op_mod_o   = op_mod_i;
            end       
        endcase
        end
        LOGS:begin
            if (conv_out_valid_i) begin 
            addmul_operands_o[0] = LOG_SCALE_VEC;       
            addmul_operands_o[1] = conv_result_i;   
            addmul_rnd_mode_o    = fpnew_pkg::RNE;
            addmul_op_o          = fpnew_pkg::MUL;
            addmul_op_mod_o      = 1'b0;
            addmul_tag_o         = conv_tag_result_i;
            addmul_in_valid_o    = 1'b1;
            end
            else begin
                addmul_in_valid_o = 1'b0;
            end
        end
        RSQRT:begin
          unique case(rsqrt_state_q)
            RSQRT_X_SQUARE_U, RSQRT_X_SQUARE_L: begin
                addmul_operands_o[0] = {rsqrt_res_hi, rsqrt_res_lo};       
                addmul_operands_o[1] = {rsqrt_res_hi, rsqrt_res_lo};   
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
            end
            RSQRT_POLY1_U, RSQRT_POLY1_L: begin
                addmul_operands_o[0] = addmul_res_q.result;       
                addmul_operands_o[1] = rsqrt_state_q == RSQRT_POLY1_U ? nl_intermediate_0_q : nl_intermediate_1_q;   
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
            end
            RSQRT_NR1_U, RSQRT_NR1_L: begin
                addmul_operands_o[0] = addmul_res_q.result;       
                addmul_operands_o[1] = C1_HALF_VEC; 
                addmul_operands_o[2] = C3_HALVES_VEC;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::FMADD;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
            end
            RSQRT_NR2_U, RSQRT_NR2_L: begin
                addmul_operands_o[0] = addmul_res_q.result;       
                addmul_operands_o[1] = rsqrt_state_q == RSQRT_NR2_U ? nl_intermediate_2_q : nl_intermediate_3_q;   
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
            end
            RSQRT_DRAIN_U, RSQRT_DRAIN_L: begin
                addmul_in_valid_o = 1'b0;
            end
            endcase
        end
        REC:begin
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
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
            end
            REC_NR1_ACCUM_U, REC_NR1_ACCUM_L: begin
                addmul_operands_o[0] = rec_state_q == REC_NR1_ACCUM_U ? nl_intermediate_0_q : nl_intermediate_1_q;       
                addmul_operands_o[1] = addmul_res_q.result;
                addmul_operands_o[2] = C2_VEC;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::FNMSUB;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
            end
            REC_NR2_MUL_U, REC_NR2_MUL_L: begin
                addmul_operands_o[0] = rec_state_q == REC_NR2_MUL_U ? nl_intermediate_2_q : nl_intermediate_3_q;       
                addmul_operands_o[1] = addmul_res_q.result;
                addmul_rnd_mode_o    = fpnew_pkg::RNE;
                addmul_op_o          = fpnew_pkg::MUL;
                addmul_op_mod_o      = 1'b0;
                addmul_tag_o         = addmul_res_q.tag;
            end
            REC_DRAIN_U, REC_DRAIN_L: begin
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
    EXPS: begin
        if (nl_active && addmul_out_valid_i) begin
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
            conv_in_valid_o    = 1'b1;
        end
    end
    COSHS:begin 
        if (nl_active && addmul_out_valid_i) begin
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
            conv_in_valid_o    = nl_state_q == COSH_SUM_L || nl_state_q == COSH_DRAIN ? 1'b0 : 1'b1;
        end
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

    default: begin
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

    end
    endcase
  end

  // ========================
  //  OUTPUT POST-PROCESSING 
  // ========================
  
  logic [WIDTH-1:0] reconstructed_result;
  logic [31:0]      x32_0, x32_1, abs32_0, abs32_1;
  logic             sign_0, sign_1;
  logic             is_nan_0, is_inf_0, clamp_mag_0;
  logic             is_nan_1, is_inf_1, clamp_mag_1;
  logic [WIDTH-1:0] nl_intermediate_sel;
  assign nl_intermediate_sel = (tanh_state_q == TANH_DRAIN_U) ? nl_intermediate_0_q : nl_intermediate_1_q;

  always_comb begin : output_postproc
    // Default assignment
    reconstructed_result = addmul_result_i; 
    
    // Reset intermediate variables
    x32_0 = 32'b0; x32_1 = 32'b0;
    sign_0 = 1'b0; sign_1 = 1'b0;
    
    // --- TANH Reconstruction ---
    if ( tanh_state_q == TANH_DRAIN_U || tanh_state_q == TANH_DRAIN_L ) begin
      
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
  end

  always_comb begin : handshake_management
    opgrp_out_ready_o = arb_gnt_i;
    arb_req_o         = opgrp_out_valid_i;
    addmul_res_ready_d = 1'b0;
    reconstructed_result_o = reconstructed_result;
    reconstructed_result_valid_o = ((tanh_state_q == TANH_DRAIN_U || tanh_state_q == TANH_DRAIN_L) ) && addmul_out_valid_i;

    if (nl_active) begin
      unique case (op_i) 
        EXPS:begin
          opgrp_out_ready_o[0] = conv_in_ready_i;
          arb_req_o[0]         = 1'b0;
        end
        COSHS : begin
          opgrp_out_ready_o[0] = (nl_state_q == COSH_SUM_L  || nl_state_q == COSH_DRAIN)  ? arb_gnt_i[0] : conv_in_ready_i;
          opgrp_out_ready_o[3] = (nl_state_q == COSH_WAIT_L || nl_state_q == COSH_WAIT_U) ? 1'b1         : addmul_in_ready_i;
          arb_req_o[3]         = 1'b0;
          arb_req_o[0]         = (nl_state_q == COSH_SUM_L  || nl_state_q == COSH_DRAIN && addmul_out_valid_i); 
        end
        TANHS: begin
          opgrp_out_ready_o[0] = addmul_out_ready_str ;
          addmul_res_ready_d   = addmul_out_valid_i;
          arb_req_o[0]         = (tanh_state_q == TANH_DRAIN_U || tanh_state_q == TANH_DRAIN_L && addmul_out_valid_i); 
        end
        LOGS: begin
          opgrp_out_ready_o[3] = addmul_in_ready_i;
          arb_req_o[3]         = 1'b0;
        end
        RSQRT: begin
          opgrp_out_ready_o[0] = addmul_out_ready_str ;
          addmul_res_ready_d   = addmul_out_valid_i;
          arb_req_o[0]         = (rsqrt_state_q == RSQRT_DRAIN_U || rsqrt_state_q == RSQRT_DRAIN_L && addmul_out_valid_i);
        end
        REC: begin
          opgrp_out_ready_o[0] = addmul_out_ready_str ;
          addmul_res_ready_d   = addmul_out_valid_i;
          arb_req_o[0]         = (rec_state_q == REC_DRAIN_U || rec_state_q == REC_DRAIN_L && addmul_out_valid_i);
        end

      endcase
    end
  end

  assign in_ready_o = is_nl_op_i
                    ? in_valid_i & addmul_in_ready_i & !block_issue_nl
                    : in_valid_i & opgrp_in_ready_i[fpnew_pkg::get_opgroup(op_i)];

endmodule

// //   `FFL(k_int_q_0, k_int_d_0, k_en_0, '0)
// //   `FFL(k_int_q_1, k_int_d_1, k_en_1, '0)
// //
// //  // Gating signal
// //   logic enable_precalc;
// //   assign enable_precalc = in_valid_i && (tag_i.nl_op_sel == LOGS || tag_i.nl_op_sel == RSQRT || tag_i.nl_op_sel == REC);

// //   logic [31:0] op0_hi, op0_lo;
// //   // Force to 0 if not a relevant op
// //   assign op0_hi = enable_precalc ? operands_i[0][63:32] : 32'b0;
// //   assign op0_lo = enable_precalc ? operands_i[0][31:0]  : 32'b0;

// //   // LOGS Pre-calculation: (X - 1.0f)
// //   logic log_hi_nan_or_sign, log_lo_nan_or_sign;
// //   logic [31:0] log_res_hi, log_res_lo;

// //   assign log_hi_nan_or_sign = op0_hi[31] || ((op0_hi[30:23] == 8'hFF) && (op0_hi[22:0] != '0));
// //   assign log_lo_nan_or_sign = op0_lo[31] || ((op0_lo[30:23] == 8'hFF) && (op0_lo[22:0] != '0));

// //   assign log_res_hi = (log_hi_nan_or_sign) ? 32'h7FC00000 : ($signed(op0_hi) - $signed(32'h3F800000));
// //   assign log_res_lo = (log_lo_nan_or_sign) ? 32'h7FC00000 : ($signed(op0_lo) - $signed(32'h3F800000));

// //   // RSQRT Pre-calculation: Magic Constant - (X >> 1)
// //   logic rsqrt_hi_nan_or_sign, rsqrt_lo_nan_or_sign;
// //   logic [31:0] rsqrt_res_hi, rsqrt_res_lo;

// //   assign rsqrt_hi_nan_or_sign = op0_hi[31] || ((op0_hi[30:23] == 8'hFF) && (op0_hi[22:0] != '0));
// //   assign rsqrt_lo_nan_or_sign = op0_lo[31] || ((op0_lo[30:23] == 8'hFF) && (op0_lo[22:0] != '0));

// //   // Note: Logical shift (>>) is intended here before subtraction
// //   assign rsqrt_res_hi = (rsqrt_hi_nan_or_sign) ? 32'h7FC00000 : ($signed(QUAKE_MAGIC) - $signed(op0_hi >> 1));
// //   assign rsqrt_res_lo = (rsqrt_lo_nan_or_sign) ? 32'h7FC00000 : ($signed(QUAKE_MAGIC) - $signed(op0_lo >> 1));

// //    // REC Pre-calculation: Magic Constant - X
// //   logic rec_hi_nan_or_sign, rec_lo_nan_or_sign;
// //   logic [31:0] rec_res_hi, rec_res_lo;

// //   assign rec_hi_nan_or_sign = (op0_hi == 32'h00000000) || ((op0_hi[30:23] == 8'hFF) && (op0_hi[22:0] != '0));
// //   assign rec_lo_nan_or_sign = (op0_lo == 32'h00000000) || ((op0_lo[30:23] == 8'hFF) && (op0_lo[22:0] != '0));

// //   // Note: Logical shift (>>) is intended here before subtraction
// //   assign rec_res_hi = (rec_hi_nan_or_sign) ? 32'h7FC00000 : ($signed(REC_MAGIC) - $signed(op0_hi));
// //   assign rec_res_lo = (rec_lo_nan_or_sign) ? 32'h7FC00000 : ($signed(REC_MAGIC) - $signed(op0_lo));

// //   logic [WIDTH-1:0] reconstructed_result;
// //   logic next_uop_ready;
// //   assign next_uop_ready_o = next_uop_ready;

// //   logic addmul_out_valid, conv_out_valid;
// //   logic addmul_last_phase, conv_last_phase, input_last_phase;
// //   logic [WIDTH-1:0] addmul_inter_01_sel, addmul_inter_23_sel; 
// //   logic [WIDTH-1:0] conv_inter_01_sel;

// //   assign conv_inter_01_sel = conv_last_phase ? nl_intermediate_1_q : nl_intermediate_0_q;

// //   assign addmul_out_valid  = opgrp_out_valid[fpnew_pkg::ADDMUL];
// //   assign conv_out_valid    = opgrp_out_valid[fpnew_pkg::CONV];

// //   assign addmul_last_phase = opgrp_outputs[fpnew_pkg::ADDMUL].tag.last_phase;
// //   assign conv_last_phase   = opgrp_outputs[fpnew_pkg::CONV].tag.last_phase;
// //   assign input_last_phase  = tag_i.last_phase;

// //   assign addmul_inter_01_sel = addmul_last_phase ? nl_intermediate_1_q : nl_intermediate_0_q;
// //   assign addmul_inter_23_sel = addmul_last_phase ? nl_intermediate_3_q : nl_intermediate_2_q;

// //   always_comb begin : nl_phases_ctrl
// //     operands_fconv      = '0;
// //     operands_add        = '0;
// //     nl_opmode_conv      = 1'b0;
// //     nl_opmode_add       = 1'b0;
// //     nl_op_conv          = fpnew_pkg::F2I;
// //     nl_op_add           = fpnew_pkg::ADD;
// //     nl_rnd_conv         = fpnew_pkg::RTZ;
// //     nl_rnd_add          = fpnew_pkg::RNE;
// //     fconv_tag           = '0;
// //     fadd_tag            = '0;
// //     out_opgrp_ready     = opgrp_out_ready;
// //     nl_wr_en            = 1'b0;
// //     is_last_uop_d       = is_last_uop_q;
// //     next_uop_ready      = 1'b0;

// //     nl_intermediate_0_d = nl_intermediate_0_q;
// //     nl_intermediate_1_d = nl_intermediate_1_q;
// //     nl_intermediate_2_d = nl_intermediate_2_q;
// //     nl_intermediate_3_d = nl_intermediate_3_q;
// //     k_int_d_0           = k_int_q_0; 
// //     k_int_d_1           = k_int_q_1; 


// //     nl_wr_en_1          = 1'b0;
// //     nl_wr_en_2          = 1'b0;
// //     nl_wr_en_3          = 1'b0;
// //     k_en_0              = 1'b0;
// //     k_en_1              = 1'b0;

// //         SIN, COS: begin
// //           if (in_valid_i) begin
// //             nl_wr_en_1              = input_last_phase;
// //             nl_wr_en                = ~input_last_phase;
// //             nl_intermediate_1_d     = operands_i[0]; // x
// //             nl_intermediate_0_d     = operands_i[0]; // x
// //           end
// //           if (conv_out_valid) begin
// //             unique case (opgrp_outputs[3].tag.nl_phase)
// //               NL_FPU_ISSUE_1: begin
// //                 k_en_1              = conv_last_phase;
// //                 k_en_0              = ~conv_last_phase;
// //                 k_int_d_1[3:2]      = opgrp_outputs[fpnew_pkg::CONV].result[33:32];
// //                 k_int_d_1[1:0]      = opgrp_outputs[fpnew_pkg::CONV].result[1:0];
// //                 k_int_d_0[3:2]      = opgrp_outputs[fpnew_pkg::CONV].result[33:32];
// //                 k_int_d_0[1:0]      = opgrp_outputs[fpnew_pkg::CONV].result[1:0];
                
// //                 operands_fconv[0]   = opgrp_outputs[fpnew_pkg::CONV].result;
// //                 nl_op_conv          = fpnew_pkg::I2F;
// //                 nl_rnd_conv         = fpnew_pkg::RNE;
// //                 fconv_tag           = opgrp_outputs[fpnew_pkg::CONV].tag;
// //                 fconv_tag.nl_phase  = NL_FPU_ISSUE_2;
// //                 out_opgrp_ready[3]  = 1'b1; 
// //               end
// //               NL_FPU_ISSUE_2: begin
// //                 operands_add[0]     = opgrp_outputs[fpnew_pkg::CONV].result;
// //                 operands_add[1]     = PIO2_HI_VEC;
// //                 operands_add[2]     = conv_inter_01_sel;
// //                 nl_opmode_add       = 1'b0;
// //                 nl_op_add           = fpnew_pkg::FNMSUB;
// //                 nl_rnd_add          = fpnew_pkg::RNE;
// //                 fadd_tag            = opgrp_outputs[fpnew_pkg::CONV].tag;
// //                 fadd_tag.nl_phase   = NL_FPU_ISSUE_3;
// //                 out_opgrp_ready[3]  = opgrp_in_ready[0]; 
// //               end
// //             endcase
// //           end

// //           if ( addmul_out_valid) begin
// //             unique case (opgrp_outputs[0].tag.nl_phase)
// //               NL_FPU_ISSUE_0: begin // k = round(X/pipi)
// //                 operands_fconv[0]                   = opgrp_outputs[fpnew_pkg::ADDMUL].result;
// //                 nl_op_conv                          = fpnew_pkg::F2I;
// //                 nl_rnd_conv                         = fpnew_pkg::RNE;
// //                 fconv_tag                           = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
// //                 fconv_tag.nl_phase                  = NL_FPU_ISSUE_1;
// //                 out_opgrp_ready[0]                  = opgrp_in_ready[3]; 
// //               end

// //               NL_FPU_ISSUE_3: begin //Z= R^2
// //                 // Store r based on last_phase
// //                 nl_wr_en_1                          = addmul_last_phase;
// //                 nl_wr_en                            = ~addmul_last_phase;
// //                 nl_intermediate_1_d                 = opgrp_outputs[fpnew_pkg::ADDMUL].result;
// //                 nl_intermediate_0_d                 = opgrp_outputs[fpnew_pkg::ADDMUL].result;
// //                 operands_add[0]                     = opgrp_outputs[fpnew_pkg::ADDMUL].result;
// //                 operands_add[1]                     = opgrp_outputs[fpnew_pkg::ADDMUL].result;
// //                 nl_opmode_add                       = 1'b0;
// //                 nl_op_add                           = fpnew_pkg::MUL;
// //                 nl_rnd_add                          = fpnew_pkg::RNE;
// //                 fadd_tag                            = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
// //                 fadd_tag.nl_phase                   = NL_FPU_ISSUE_4;
// //                 out_opgrp_ready[fpnew_pkg::ADDMUL]  = 1'b1; 
// //               end

// //               NL_FPU_ISSUE_4: begin //1 + s3z 
                
// //                 nl_wr_en_3                          = addmul_last_phase;
// //                 nl_wr_en_2                          = ~addmul_last_phase;
// //                 nl_intermediate_3_d                 = opgrp_outputs[fpnew_pkg::ADDMUL].result;
// //                 nl_intermediate_2_d                 = opgrp_outputs[fpnew_pkg::ADDMUL].result;
// //                 operands_add[0]                     = opgrp_outputs[fpnew_pkg::ADDMUL].result;
// //                 operands_add[1]                     = SIN_S3_VEC;
// //                 operands_add[2]                     = C_ONE_VEC;
// //                 nl_opmode_add                       = 1'b0;
// //                 nl_op_add                           = fpnew_pkg::FMADD;
// //                 nl_rnd_add                          = fpnew_pkg::RNE;
// //                 fadd_tag                            = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
// //                 fadd_tag.nl_phase                   = NL_FPU_ISSUE_5;
// //                 out_opgrp_ready[fpnew_pkg::ADDMUL]  = 1'b1; 
// //               end
// //               NL_FPU_ISSUE_5: begin // final mul SIN 
// //                 operands_add[0]                     = opgrp_outputs[fpnew_pkg::ADDMUL].result; 
// //                 operands_add[1]                     = addmul_inter_01_sel; 
// //                 nl_opmode_add                       = 1'b0;
// //                 nl_op_add                           = fpnew_pkg::MUL;
// //                 nl_rnd_add                          = fpnew_pkg::RNE;
// //                 fadd_tag                            = opgrp_outputs[fpnew_pkg::ADDMUL].tag; 
// //                 fadd_tag.nl_phase                   = NL_FPU_ISSUE_6;
// //                 out_opgrp_ready[fpnew_pkg::ADDMUL]  = 1'b1; 
// //               end
              
// //               NL_FPU_ISSUE_6: begin //cos = 1+ c2*z
// //                 // Store sin based on last_phase
// //                 nl_wr_en_1                          = addmul_last_phase;
// //                 nl_wr_en                            = ~addmul_last_phase;
// //                 nl_intermediate_1_d                 = opgrp_outputs[fpnew_pkg::ADDMUL].result;
// //                 nl_intermediate_0_d                 = opgrp_outputs[fpnew_pkg::ADDMUL].result;
// //                 operands_add[0]                     = addmul_inter_23_sel;
// //                 operands_add[1]                     = COS_C2_VEC;
// //                 operands_add[2]                     = C_ONE_VEC;
// //                 nl_opmode_add                       = 1'b0;
// //                 nl_op_add                           = fpnew_pkg::FMADD;
// //                 nl_rnd_add                          = fpnew_pkg::RNE;
// //                 fadd_tag                            = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
// //                 fadd_tag.nl_phase                   = NL_WAIT;
// //                 out_opgrp_ready[fpnew_pkg::ADDMUL]  = 1'b1; 
// //                 next_uop_ready            = 1'b1;
// //               end

// //               NL_WAIT: begin
// //               end

// //               default : begin
// //               end
// //             endcase
// //           end
// //         end
        // REC: begin
        //   if (in_valid_i) begin
        //     nl_wr_en_1          = input_last_phase;
        //     nl_wr_en            = ~input_last_phase;
        //     nl_intermediate_1_d = operands_i[0]; 
        //     nl_intermediate_0_d = operands_i[0]; 
        //   end
        //     // First approximation is R = (2 - x * first_approx)
        //     operands_add[0]     = {rec_res_hi, rec_res_lo};
        //     operands_add[1]     = operands_i[0];
        //     operands_add[2]     = C2_VEC;
        //     nl_op_add           = fpnew_pkg::FNMSUB;
        //     nl_rnd_add          = fpnew_pkg::RNE;
        //     fadd_tag            = tag_i;
            
        //     nl_wr_en_3          = input_last_phase;
        //     nl_wr_en_2          = ~input_last_phase;
        //     nl_intermediate_3_d = {rec_res_hi, rec_res_lo};
        //     nl_intermediate_2_d = {rec_res_hi, rec_res_lo};
        //   if ( addmul_out_valid) begin
        //     unique case(opgrp_outputs[0].tag.nl_phase) 
        //       NL_FPU_ISSUE_0: begin // Y = R * first_approx  
        //         operands_add[0]       = addmul_inter_23_sel;
        //         operands_add[1]       = opgrp_outputs[fpnew_pkg::ADDMUL].result;
        //         nl_op_add             = fpnew_pkg::MUL;
        //         nl_rnd_add            = fpnew_pkg::RNE;
        //         nl_opmode_add         = 1'b0;
        //         fadd_tag              = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
        //         fadd_tag.nl_phase     = NL_FPU_ISSUE_1;
        //         out_opgrp_ready[0]    = 1'b1;

        //       end

        //       NL_FPU_ISSUE_1: begin   // Y2 = 2 - Y*x 
        //         operands_add[0]       = addmul_inter_01_sel;
        //         operands_add[1]       = opgrp_outputs[fpnew_pkg::ADDMUL].result;
        //         operands_add[2]       = C2_VEC;
        //         nl_op_add             = fpnew_pkg::FNMSUB;
        //         nl_rnd_add            = fpnew_pkg::RNE;
        //         nl_opmode_add         = 1'b0;
        //         fadd_tag              = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
        //         fadd_tag.nl_phase     = NL_FPU_ISSUE_2;
        //         out_opgrp_ready[0]    = 1'b1;
        //         nl_wr_en_3            = addmul_last_phase;
        //         nl_wr_en_2            = ~addmul_last_phase;
        //         nl_intermediate_3_d   = opgrp_outputs[fpnew_pkg::ADDMUL].result;
        //         nl_intermediate_2_d   = opgrp_outputs[fpnew_pkg::ADDMUL].result;
        //       end

        //       NL_FPU_ISSUE_2: begin // Y3 = Y * Y2

        //         operands_add[0]       = addmul_inter_23_sel; 
        //         operands_add[1]       = opgrp_outputs[fpnew_pkg::ADDMUL].result; 
        //         nl_op_add             = fpnew_pkg::MUL;
        //         nl_rnd_add            = fpnew_pkg::RNE;
        //         nl_opmode_add         = 1'b0;
        //         fadd_tag              = opgrp_outputs[fpnew_pkg::ADDMUL].tag;
        //         fadd_tag.nl_phase     = NL_WAIT;
        //         out_opgrp_ready[0]    = 1'b1;
        //         next_uop_ready        = 1'b1;
        //       end
        //       NL_WAIT: begin
        //       end
        //     endcase
    //       end
    //     end
    //   endcase
// //     end
// //   end

  // ========================
  //  OUTPUT POST-PROCESSING 
  // ========================
  
//   logic        use_last_phase_postproc;
//   logic [63:0] nl_intermediate_sel;
//   logic [3:0]  k_int_sel;
//   // todo: Gating these signals
//   assign use_last_phase_postproc = opgrp_outputs[fpnew_pkg::ADDMUL].tag.last_phase;
//   assign nl_intermediate_sel     = use_last_phase_postproc ? nl_intermediate_1_q : nl_intermediate_0_q;
//   assign k_int_sel               = use_last_phase_postproc ? k_int_q_1 : k_int_q_0;

//   always_comb begin : output_postproc
//     // Default assignment
//     reconstructed_result = opgrp_outputs[fpnew_pkg::ADDMUL].result; 
    
//     // Reset intermediate variables
//     x32_0 = 32'b0; x32_1 = 32'b0;
//     sign_0 = 1'b0; sign_1 = 1'b0;
    
//     // --- TANH Reconstruction ---
//     if (opgrp_outputs[fpnew_pkg::ADDMUL].tag.nl_op_sel == TANHS && 
//         opgrp_outputs[fpnew_pkg::ADDMUL].tag.nl_phase == NL_WAIT) begin
      
//       // Use pre-selected intermediate (removes one mux level from critical path)
//       x32_0 = nl_intermediate_sel[63:32];
//       x32_1 = nl_intermediate_sel[31:0];
      
//       {sign_0, abs32_0} = {x32_0[31], 1'b0, x32_0[30:0]};
//       {sign_1, abs32_1} = {x32_1[31], 1'b0, x32_1[30:0]};
      
//       is_nan_0    = (abs32_0[30:23] == 8'hFF) && (abs32_0[22:0] != 0);
//       is_inf_0    = (abs32_0[30:23] == 8'hFF) && (abs32_0[22:0] == 0);
//       clamp_mag_0 = (abs32_0[30:23] >= 8'h80);

//       is_nan_1    = (abs32_1[30:23] == 8'hFF) && (abs32_1[22:0] != 0);
//       is_inf_1    = (abs32_1[30:23] == 8'hFF) && (abs32_1[22:0] == 0);
//       clamp_mag_1 = (abs32_1[30:23] >= 8'h80);

//       if (clamp_mag_0 || is_inf_0) reconstructed_result[63:32] = {sign_0, 31'h3F800000}; 
//       else if (is_nan_0)           reconstructed_result[63:32] = x32_0; 
      
//       if (clamp_mag_1 || is_inf_1) reconstructed_result[31:0]  = {sign_1, 31'h3F800000}; 
//       else if (is_nan_1)           reconstructed_result[31:0]  = x32_1; 
//     end

//     // --- COSINE Reconstruction ---
//     else if (opgrp_outputs[0].tag.nl_op_sel == COS && 
//              opgrp_outputs[0].tag.nl_phase == NL_WAIT) begin
      
//       // Lane 1 (Upper 32 bits) - use pre-selected k_int
//       unique case (k_int_sel[3:2])
//         2'b00: {sign_1, x32_1} = {opgrp_outputs[0].result[63],         opgrp_outputs[0].result[63:32]};
//         2'b01: {sign_1, x32_1} = {nl_intermediate_sel[63] ^ 1'b1,      nl_intermediate_sel[63:32]};
//         2'b10: {sign_1, x32_1} = {opgrp_outputs[0].result[63] ^ 1'b1,  opgrp_outputs[0].result[63:32]};
//         2'b11: {sign_1, x32_1} = {nl_intermediate_sel[63],             nl_intermediate_sel[63:32]};
//       endcase
      
//       // Lane 0 (Lower 32 bits)
//       unique case (k_int_sel[1:0])
//         2'b00: {sign_0, x32_0} = {opgrp_outputs[0].result[31],        opgrp_outputs[0].result[31:0]};
//         2'b01: {sign_0, x32_0} = {nl_intermediate_sel[31] ^ 1'b1,     nl_intermediate_sel[31:0]};
//         2'b10: {sign_0, x32_0} = {opgrp_outputs[0].result[31] ^ 1'b1, opgrp_outputs[0].result[31:0]};
//         2'b11: {sign_0, x32_0} = {nl_intermediate_sel[31],            nl_intermediate_sel[31:0]};
//       endcase
      
//       reconstructed_result[63:32] = {sign_1, x32_1[30:0]};
//       reconstructed_result[31:0]  = {sign_0, x32_0[30:0]};
//     end

//     // --- SINE Reconstruction ---
//     else if (opgrp_outputs[0].tag.nl_op_sel == SIN && 
//              opgrp_outputs[0].tag.nl_phase == NL_WAIT) begin
      
//       // Lane 1 (Upper 32 bits)
//       unique case (k_int_sel[3:2])
//         2'b00: {sign_1, x32_1} = {nl_intermediate_sel[63],            nl_intermediate_sel[63:32]};
//         2'b01: {sign_1, x32_1} = {opgrp_outputs[0].result[63],        opgrp_outputs[0].result[63:32]};
//         2'b10: {sign_1, x32_1} = {nl_intermediate_sel[63] ^ 1'b1,     nl_intermediate_sel[63:32]};
//         2'b11: {sign_1, x32_1} = {opgrp_outputs[0].result[63] ^ 1'b1, opgrp_outputs[0].result[63:32]};
//       endcase
      
//       // Lane 0 (Lower 32 bits)
//       unique case (k_int_sel[1:0])
//         2'b00: {sign_0, x32_0} = {nl_intermediate_sel[31],            nl_intermediate_sel[31:0]};
//         2'b01: {sign_0, x32_0} = {opgrp_outputs[0].result[31],        opgrp_outputs[0].result[31:0]};
//         2'b10: {sign_0, x32_0} = {nl_intermediate_sel[31] ^ 1'b1,     nl_intermediate_sel[31:0]};
//         2'b11: {sign_0, x32_0} = {opgrp_outputs[0].result[31] ^ 1'b1, opgrp_outputs[0].result[31:0]};
//       endcase

//       reconstructed_result[63:32] = {sign_1, x32_1[30:0]};
//       reconstructed_result[31:0]  = {sign_0, x32_0[30:0]};
//     end
//   end

// //   // Precompute values for concatenation
// //     logic addmul_phase_is_0, addmul_phase_is_wait;
// //     logic conv_phase_is_1, conv_phase_is_2;
// //     logic addmul_phase_not_wait_not_0;
    
// //     assign addmul_phase_is_0           = (opgrp_outputs[fpnew_pkg::ADDMUL].tag.nl_phase == NL_FPU_ISSUE_0);
// //     assign addmul_phase_is_wait        = (opgrp_outputs[fpnew_pkg::ADDMUL].tag.nl_phase == NL_WAIT);
// //     assign conv_phase_is_1             = (opgrp_outputs[fpnew_pkg::CONV].tag.nl_phase == NL_FPU_ISSUE_1);
// //     assign conv_phase_is_2             = (opgrp_outputs[fpnew_pkg::CONV].tag.nl_phase == NL_FPU_ISSUE_2);
// //     assign addmul_phase_not_wait       = !addmul_phase_is_wait;
// //     assign addmul_phase_not_wait_not_0 = addmul_out_valid && !addmul_phase_is_wait && !addmul_phase_is_0;
  
// //     // Pre-computed routing signals for SIN/COS (most complex case)
// //     logic sincos_route_to_conv_from_addmul;
// //     logic sincos_route_to_conv_from_conv;
// //     logic sincos_route_to_addmul_from_conv;
// //     logic sincos_route_to_addmul_from_addmul;
    
// //     assign sincos_route_to_conv_from_addmul   = addmul_out_valid && addmul_phase_is_0;
// //     assign sincos_route_to_conv_from_conv     = conv_out_valid && conv_phase_is_1;
// //     assign sincos_route_to_addmul_from_conv   = conv_out_valid && conv_phase_is_2;
// //     assign sincos_route_to_addmul_from_addmul = addmul_phase_not_wait_not_0;
  
// //     // Pre-computed routing for TANHS and RSQRT (same pattern)
// //     logic tanhs_rsqrt_rec_route_to_addmul;
// //     assign tanhs_rsqrt_rec_route_to_addmul = addmul_out_valid && addmul_phase_not_wait;

// //     // Pre-computed routing for COSHS
// //     logic coshs_route_to_conv;
// //     logic coshs_route_to_addmul;
// //     assign coshs_route_to_conv   = addmul_out_valid && !is_last_uop_q;
// //     assign coshs_route_to_addmul = conv_out_valid && conv_phase_is_1;
// // fpnew_pkg::roundmode_e rnd_mode_in;
// //     fpnew_pkg::operation_e op_in;
// //     logic opmode_in;
// //     logic [NUM_OPERANDS-1:0][WIDTH-1:0] operands_input;
// //     TagType opgrp_tag_in;

// //     always_comb begin : select_opgrp_inputs
// //       if (nl_concatenate)begin
// //         unique case(tag_i.nl_op_sel)
// //           EXPS: begin
// //             concatenate_in_valid[opgrp] = (in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp))) || (opgrp_out_valid[0] && (opgrp == 3));
// //             rnd_mode_in     = (opgrp == 3) ?  nl_rnd_conv    :  rnd_mode_i;
// //             op_in           = (opgrp == 3) ?  nl_op_conv     :  op_i;
// //             opmode_in       = (opgrp == 3) ?  nl_opmode_conv :  op_mod_i;
// //             operands_input  = (opgrp == 3) ?  operands_fconv :  operands_i;
// //             opgrp_tag_in    = (opgrp == 3) ?  fconv_tag      :  tag_i;
// //           end
// //           COSHS: begin
// //             if (coshs_route_to_conv && (opgrp == 3)) begin
// //               concatenate_in_valid[opgrp] = 1'b1;
// //               rnd_mode_in     = nl_rnd_conv;
// //               op_in           = nl_op_conv;
// //               opmode_in       = nl_opmode_conv;
// //               operands_input  = operands_fconv;
// //               opgrp_tag_in    = fconv_tag;

// //             end else if (coshs_route_to_addmul && (opgrp == 0)) begin
// //               concatenate_in_valid[opgrp] = 1'b1;
// //               rnd_mode_in     = nl_rnd_add;
// //               op_in           = nl_op_add;
// //               opmode_in       = nl_opmode_add;
// //               operands_input  = operands_add;
// //               opgrp_tag_in    = fadd_tag;
// //             end else begin
// //               concatenate_in_valid[opgrp] = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
// //               rnd_mode_in     = rnd_mode_i;
// //               op_in           = op_i;
// //               opmode_in       = op_mod_i;
// //               operands_input  = operands_i;
// //               opgrp_tag_in    = tag_i;
// //             end
// //           end
// //           TANHS: begin
// //             if (tanhs_rsqrt_rec_route_to_addmul && (opgrp == 0)) begin
// //               concatenate_in_valid[opgrp] = 1'b1;
// //               rnd_mode_in     = nl_rnd_add;
// //               op_in           = nl_op_add;
// //               opmode_in       = nl_opmode_add;
// //               operands_input  = operands_add;
// //               opgrp_tag_in    = fadd_tag;
// //             end else begin
// //               concatenate_in_valid[opgrp] = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
// //               rnd_mode_in     = rnd_mode_i;
// //               op_in           = op_i;
// //               opmode_in       = op_mod_i;
// //               operands_input  = operands_i;
// //               opgrp_tag_in    = tag_i;
// //             end
// //           end
// //           LOGS: begin
// //             concatenate_in_valid[opgrp] = (in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp))) || (opgrp_out_valid[3] && (opgrp == 0));
// //             rnd_mode_in     = (opgrp == 0) ?  nl_rnd_add    :  rnd_mode_i;
// //             op_in           = (opgrp == 0) ?  nl_op_add     :  op_i;
// //             opmode_in       = (opgrp == 0) ?  nl_opmode_add :  op_mod_i;
// //             operands_input  = (opgrp == 0) ?  operands_add :  operands_fconv;
// //             opgrp_tag_in    = (opgrp == 0) ?  fadd_tag      :  tag_i;
// //           end           
// //           RSQRT: begin
// //             if (tanhs_rsqrt_rec_route_to_addmul && (opgrp == 0)) begin
// //               concatenate_in_valid[opgrp] = 1'b1;
// //               rnd_mode_in     = nl_rnd_add;
// //               op_in           = nl_op_add;
// //               opmode_in       = nl_opmode_add;
// //               operands_input  = operands_add;
// //               opgrp_tag_in    = fadd_tag;
// //             end else begin
// //               concatenate_in_valid[opgrp] = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
// //               rnd_mode_in     = rnd_mode_i;
// //               op_in           = op_i;
// //               opmode_in       = op_mod_i;
// //               operands_input  = operands_add;
// //               opgrp_tag_in    = tag_i;
// //             end
// //           end
// //           SIN, COS: begin
// //             if (in_valid_i)begin
// //               concatenate_in_valid[opgrp] = in_valid_i & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(opgrp));
// //               rnd_mode_in     = rnd_mode_i;
// //               op_in           = op_i;
// //               opmode_in       = op_mod_i;
// //               operands_input  = operands_i;
// //               opgrp_tag_in    = tag_i;
// //             end else if (sincos_route_to_conv_from_addmul && (opgrp == 3)) begin
// //               concatenate_in_valid[opgrp] = 1'b1;
// //               rnd_mode_in     = nl_rnd_conv;
// //               op_in           = nl_op_conv;
// //               opmode_in       = nl_opmode_conv;
// //               operands_input  = operands_fconv;
// //               opgrp_tag_in    = fconv_tag;

// //             end else if (sincos_route_to_conv_from_conv && (opgrp == 3)) begin
// //               concatenate_in_valid[opgrp] = 1'b1;
// //               rnd_mode_in     = nl_rnd_conv;
// //               op_in           = nl_op_conv;
// //               opmode_in       = nl_opmode_conv;
// //               operands_input  = operands_fconv;
// //               opgrp_tag_in    = fconv_tag;

// //             end else if (sincos_route_to_addmul_from_conv && (opgrp == 0)) begin
// //               concatenate_in_valid[opgrp] = 1'b1;
// //               rnd_mode_in     = nl_rnd_add;
// //               op_in           = nl_op_add;
// //               opmode_in       = nl_opmode_add;
// //               operands_input  = operands_add;
// //               opgrp_tag_in    = fadd_tag;
// //             end else if  (sincos_route_to_addmul_from_addmul && (opgrp == 0)) begin 
// //               concatenate_in_valid[opgrp] = 1'b1;
// //               rnd_mode_in     = nl_rnd_add;
// //               op_in           = nl_op_add;
// //               opmode_in       = nl_opmode_add;
// //               operands_input  = operands_add;
// //               opgrp_tag_in    = fadd_tag;
// //             end else begin
// //               concatenate_in_valid[opgrp] = 1'b0;
// //               rnd_mode_in     = rnd_mode_i;
// //               op_in           = op_i;
// //               opmode_in       = op_mod_i;
// //               operands_input  = operands_i;
// //               opgrp_tag_in    = tag_i;
// //             end
// //           end
// //          