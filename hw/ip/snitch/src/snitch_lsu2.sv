// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Florian Zaruba <zarubaf@iis.ee.ethz.ch>

/// Load Store Unit (can handle `NumOutstandingLoads` outstanding loads and
/// `NumOutstandingMem` requests in total) and optionally NaNBox if used in a
/// floating-point setting. It expects its memory sub-system to keep order (as if
/// issued with a single ID).

module snitch_lsu2
  import cf_math_pkg::idx_width;
#(
  parameter int unsigned AddrWidth           = 32,
  parameter int unsigned DataWidth           = 32,
  /// Tag passed from input to output. All transactions are in-order.
  parameter type tag_t                       = logic [4:0],
  /// Number of outstanding memory transactions.
  parameter int unsigned NumOutstandingMem   = 1,
  /// Number of outstanding loads.
  parameter int unsigned NumOutstandingLoads = 1,
  /// Whether to NaN Box values. Used for floating-point load/stores.
  parameter bit          NaNBox              = 0,
  parameter type         dreq_t              = logic,
  parameter type         drsp_t              = logic,
  /// Derived parameter *Do not override*
  localparam int unsigned IdWidth = idx_width(NumOutstandingLoads),
  parameter type addr_t = logic [AddrWidth-1:0],
  parameter type data_t = logic [DataWidth-1:0]
) (
  input  logic                 clk_i,
  input  logic                 rst_i,
  // request channel
  input  tag_t                 lsu_qtag_i,
  input  logic                 lsu_qwrite_i,
  input  logic                 lsu_qsigned_i,
  input  addr_t                lsu_qaddr_i,
  input  data_t                lsu_qdata_i,
  input  logic [1:0]           lsu_qsize_i,
  input  reqrsp_pkg::amo_op_e  lsu_qamo_i,
  input  logic                 lsu_qvalid_i,
  output logic                 lsu_qready_o,
  // response channel
  output data_t                lsu_pdata_o,
  output tag_t                 lsu_ptag_o,
  output logic                 lsu_perror_o,
  output logic                 lsu_pvalid_o,
  input  logic                 lsu_pready_i,
  /// High if there is currently no transaction pending.
  output logic                 lsu_empty_o,
  // Memory Interface Channel
  output dreq_t                data_req_o,
  input  drsp_t                data_rsp_i
);

  localparam int unsigned DataAlign = $clog2(DataWidth/8);
  logic [31:0] ld_result;
  logic [31:0] lsu_qdata, data_qdata;

  typedef struct packed {
    logic                  write;
    tag_t                  tag;
    logic                  sign_ext;
    logic [DataAlign-1:0]  offset;
    logic [1:0]            size;
  } laq_t;

  typedef logic [IdWidth-1:0] id_t;

  // ID Table
  logic   [NumOutstandingLoads-1:0] id_available_d, id_available_q;
  laq_t   [NumOutstandingLoads-1:0] laq_d,          laq_q;
  laq_t                             req_laq,        rsp_laq;
  id_t                              req_id,         rsp_id;
  id_t                              req_id_d,       req_id_q;
  logic                             hs_pending_d,   hs_pending_q;
  logic                             id_table_push,  id_table_pop;
  logic                             id_table_full;

  // Load Address Queue (LAQ)
  always_comb begin
    id_available_d = id_available_q;
    laq_d          = laq_q;

    // new req, store laq for this id
    if (id_table_push) begin
      id_available_d[req_id] = 1'b0;
      laq_d[req_id]          = req_laq;
    end

    // free id and take out data
    if (id_table_pop) begin
      id_available_d[rsp_id] = 1'b1;
    end
  end

  assign req_laq = '{
    write:    lsu_qwrite_i,
    tag:      lsu_qtag_i,
    sign_ext: lsu_qsigned_i,
    offset:   lsu_qaddr_i[DataAlign-1:0],
    size:     lsu_qsize_i
  };

  assign rsp_laq = laq_q[rsp_id];

  // No pending requests if all IDs are available
  assign lsu_empty_o = &id_available_q;

  // Pop if response accepted
  assign id_table_pop = data_rsp_i.p_valid & data_req_o.p_ready;

  // Push if load requested even if downstream is not ready yet, to keep the interface stable.
  assign id_table_push = data_req_o.q_valid && !hs_pending_q;

  // Buffer whether we are pushing an ID but still waiting for the ready
  always_comb begin
    req_id_d = req_id_q;
    if (id_table_push && !data_rsp_i.q_ready) begin
      req_id_d = req_id;
    end
  end

  // Search available ID for request
  lzc #(
    .WIDTH ( NumOutstandingLoads )
  ) i_req_id (
    .in_i   ( id_available_q ),
    .cnt_o  ( req_id         ),
    .empty_o( id_table_full  )
  );

  // Only make a request when we got a valid request and if it is a load also
  // check that we can actually store the necessary information to process it in
  // the upcoming cycle(s).


  // Generate byte enable mask.
  always_comb begin
    data_req_o.q = '0;

    data_req_o.q_valid = lsu_qvalid_i & (~id_table_full | hs_pending_q);
    data_req_o.q.write = lsu_qwrite_i;
    data_req_o.q.addr = lsu_qaddr_i;
    data_req_o.q.amo  = lsu_qamo_i;
    data_req_o.q.size = lsu_qsize_i;
    data_req_o.q.user.req_id   = hs_pending_q ? req_id_q : req_id;
    data_req_o.q.data = data_qdata[DataWidth-1:0];

    unique case (lsu_qsize_i)
      2'b00: data_req_o.q.strb = ('b1    << lsu_qaddr_i[DataAlign-1:0]);
      2'b01: data_req_o.q.strb = ('b11   << lsu_qaddr_i[DataAlign-1:0]);
      2'b10: data_req_o.q.strb = ('b1111 << lsu_qaddr_i[DataAlign-1:0]);
      2'b11: data_req_o.q.strb = '1;
      default: data_req_o.q.strb = '0;
    endcase
  end

  // Re-align write data.
  /* verilator lint_off WIDTH */
  assign lsu_qdata = $unsigned(lsu_qdata_i);
  always_comb begin
    unique case (lsu_qaddr_i[DataAlign-1:0])
      2'b00: data_qdata = lsu_qdata;
      2'b01: data_qdata = {lsu_qdata[23:0], lsu_qdata[31:24]};
      2'b10: data_qdata = {lsu_qdata[15:0], lsu_qdata[31:16]};
      2'b11: data_qdata = {lsu_qdata[ 7:0], lsu_qdata[31: 8]};
      default: data_qdata = lsu_qdata;
    endcase
  end
  /* verilator lint_on WIDTH */

  // The interface didn't accept our request yet
  assign hs_pending_d = data_req_o.q_valid & ~data_rsp_i.q_ready;
  assign lsu_qready_o = ~hs_pending_d & ~id_table_full;

  // Return Path
  // shift the load data back
  logic [31:0] shifted_data;
  assign shifted_data = data_rsp_i.p.data >> {rsp_laq.offset, 3'b000};
  always_comb begin
    unique case (rsp_laq.size)
      2'b00: ld_result = {{24{(shifted_data[7]  | NaNBox) & rsp_laq.sign_ext}}, shifted_data[7:0]};
      2'b01: ld_result = {{16{(shifted_data[15] | NaNBox) & rsp_laq.sign_ext}}, shifted_data[15:0]};
      2'b10: ld_result = shifted_data;
      2'b11: ld_result = shifted_data;
      default: ld_result = shifted_data;
    endcase
  end

  assign rsp_id = data_rsp_i.p.user.req_id;

  assign lsu_perror_o = data_rsp_i.p.error;
  assign lsu_pdata_o  = ld_result[DataWidth-1:0];
  assign lsu_ptag_o   = rsp_laq.tag;
  // In case of a write, don't signal a valid transaction. Stores are always
  // without ans answer to the core.
  assign lsu_pvalid_o = data_rsp_i.p_valid & ~rsp_laq.write;
  assign data_req_o.p_ready = lsu_pready_i | rsp_laq.write;

  always_ff @(posedge clk_i or posedge rst_i) begin
    if (rst_i) begin
      id_available_q    <= '1;
      laq_q             <= '0;
      req_id_q          <= '0;
      hs_pending_q      <= '0;
    end else begin
      id_available_q    <= id_available_d;
      laq_q             <= laq_d;
      req_id_q          <= req_id_d;
      hs_pending_q      <= hs_pending_d;
    end
  end

  full_write : assert property(
      @(posedge clk_i) disable iff (rst_i) (id_table_full |-> !id_table_push))
  else $fatal (1, "Trying to request an ID although the ROB is full.");

endmodule
