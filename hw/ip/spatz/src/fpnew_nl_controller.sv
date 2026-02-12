
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

  cosh_state_e      nl_state_q, nl_state_d;
  logic [WIDTH-1:0] intermediate_q;
  logic             intermediate_en;
  logic             block_issue_nl;

  always_ff @(posedge clk_i or negedge rst_ni) begin
      if (!rst_ni) begin
      nl_state_q     <= COSH_EXP_POS_U;
      intermediate_q <= '0;
      end else if (flush_i) begin
      nl_state_q     <= COSH_EXP_POS_U;
      end else begin
      nl_state_q     <= nl_state_d;
      if (intermediate_en) intermediate_q <= conv_result_i; 
      end
  end

  assign block_issue_nl = (op_i == COSHS) && (~(nl_state_q == COSH_EXP_NEG_U || nl_state_q == COSH_SUM_L));
  assign intermediate_en = (nl_state_q == COSH_WAIT_L && conv_out_valid_i) || (nl_state_q == COSH_WAIT_U && conv_out_valid_i);
  always_comb begin : state_control
      nl_state_d = nl_state_q;
      unique case (nl_state_q)
      COSH_EXP_POS_U: if (is_nl_op_i && in_valid_i && op_i == COSHS && addmul_in_ready_i) nl_state_d = COSH_EXP_NEG_U;
      COSH_EXP_NEG_U: if (addmul_in_ready_i) nl_state_d = COSH_EXP_POS_L;
      COSH_EXP_POS_L: if (addmul_in_ready_i) nl_state_d = COSH_EXP_NEG_L;
      COSH_EXP_NEG_L: if (addmul_in_ready_i && conv_in_ready_i) nl_state_d = COSH_WAIT_U;
      COSH_WAIT_U:    if (conv_out_valid_i)  nl_state_d = COSH_SUM_U;
      COSH_SUM_U:     if (addmul_in_ready_i) nl_state_d = COSH_WAIT_L;
      COSH_WAIT_L:    if (conv_out_valid_i)  nl_state_d = COSH_SUM_L;
      COSH_SUM_L:     if (addmul_in_ready_i) nl_state_d = COSH_DRAIN;
      COSH_DRAIN:     if (addmul_out_valid_i)  nl_state_d = COSH_EXP_POS_U;
      default:        nl_state_d = COSH_EXP_POS_U;
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
              addmul_operands_o[0] = intermediate_q;   
              addmul_operands_o[1] = conv_result_i; 
              addmul_operands_o[2] = '0;              
              addmul_rnd_mode_o    = fpnew_pkg::RNE;
              addmul_op_o          = fpnew_pkg::ADD;
              addmul_op_mod_o      = 1'b0;
              addmul_tag_o         = conv_tag_result_i;
            end
            COSH_WAIT_U,COSH_WAIT_L,COSH_DRAIN: begin
                addmul_in_valid_o = 1'b0;
            end
          endcase
        end
        default: begin
          addmul_operands_o    = operands_i;
          addmul_rnd_mode_o    = rnd_mode_i;
          addmul_op_o          = op_i;
          addmul_op_mod_o      = op_mod_i;
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
    conv_operands_o    = operands_i;
    conv_rnd_mode_o    = rnd_mode_i;
    conv_op_o          = op_i;
    conv_op_mod_o      = op_mod_i;
    conv_src_fmt_o     = src_fmt_i;
    conv_dst_fmt_o     = dst_fmt_i;
    conv_int_fmt_o     = int_fmt_i;
    conv_tag_o         = tag_i;
    conv_in_valid_o    = in_valid_i
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
    default: begin
      conv_operands_o    = operands_i;
      conv_rnd_mode_o    = rnd_mode_i;
      conv_op_o          = op_i;
      conv_op_mod_o      = op_mod_i;
      conv_src_fmt_o     = src_fmt_i;
      conv_dst_fmt_o     = dst_fmt_i;
      conv_int_fmt_o     = int_fmt_i;
      conv_tag_o         = tag_i;
      conv_in_valid_o    = in_valid_i
                           & (fpnew_pkg::get_opgroup(op_i) == fpnew_pkg::opgroup_e'(3));

    end
    endcase
  end

  always_comb begin : handshake_management
    opgrp_out_ready_o = arb_gnt_i;
    arb_req_o         = opgrp_out_valid_i;

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
          arb_req_o[0]         = (nl_state_q == COSH_SUM_L || nl_state_q == COSH_DRAIN && addmul_out_valid_i) ? 1'b1 : 1'b0; 
        end
      endcase
    end
  end

  assign in_ready_o = is_nl_op_i
                    ? in_valid_i & addmul_in_ready_i & !block_issue_nl
                    : in_valid_i & opgrp_in_ready_i[fpnew_pkg::get_opgroup(op_i)];

endmodule
