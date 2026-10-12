// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Fabian Schuiki <fschuiki@iis.ee.ethz.ch>

/// A refiller for cache lines.
module snitch_icache_refill #(
  parameter snitch_icache_pkg::config_t CFG = '0,
  parameter type axi_req_t = logic,
  parameter type axi_rsp_t = logic
) (
  input  logic clk_i,
  input  logic rst_ni,

  input  logic [CFG.FETCH_AW-1:0]     in_req_addr_i,
  input  logic [CFG.PENDING_IW-1:0]   in_req_id_i,
  input  logic                        in_req_bypass_i,
  input  logic                        in_req_valid_i,
  output logic                        in_req_ready_o,

  output logic [CFG.LINE_WIDTH-1:0]   in_rsp_data_o,
  output logic                        in_rsp_error_o,
  output logic [CFG.PENDING_IW-1:0]   in_rsp_id_o,
  output logic                        in_rsp_bypass_o,
  output logic                        in_rsp_valid_o,
  input  logic                        in_rsp_ready_i,

  output axi_req_t                    axi_req_o,
  input  axi_rsp_t                    axi_rsp_i,

  /// Set when i_fifo_id_queue's internal rel_fifo TMR-corrects a pointer fault.
  output logic                        fifo_tmr_fault_o
);

`ifndef SYNTHESIS
  initial assert(CFG != '0);
`endif

  // How many response beats are necessary to refill one cache line.
  localparam int unsigned BeatsPerRefill =
    CFG.LINE_WIDTH >= CFG.FILL_DW ? CFG.LINE_WIDTH/CFG.FILL_DW : 1;

  // Sub-line offset width: nonzero only when one AXI fill beat spans multiple
  // cache lines (CFG.LINE_WIDTH < CFG.FILL_DW), in which case we need to
  // remember which slice of the beat belongs to each pending request.
  localparam int unsigned AddrQueueDepth =
    (CFG.LINE_WIDTH < CFG.FILL_DW) ? CFG.FILL_ALIGN-CFG.LINE_ALIGN : 0;

  // The response queue holds metadata (id/bypass, and -- when applicable --
  // the sub-line offset) for the issued requests in order. This used to be
  // two separate fifo_v3 instances sharing the same push_i/pop_i but each
  // keeping its own independent read/write pointers; a single-bit fault in
  // either one's pointer silently desyncs it from the other, e.g. making the
  // addr-offset queue return the wrong slice for a given request's id/bypass
  // -- feeding the wrong half of the AXI beat into the cache line as if it
  // were correct, with nothing downstream able to detect it. Folding both
  // fields into one queue removes the redundant pointer state entirely,
  // rather than trying to keep two copies of it consistent.
  logic queue_full;
  logic queue_push;
  logic queue_pop;

  localparam int unsigned TransactionQueueDepth = 4;

  logic [CFG.PENDING_IW+AddrQueueDepth:0] queue_data_i, queue_data_o;

  rel_fifo #(
    .Depth     ( TransactionQueueDepth ),
    .DataWidth ( CFG.PENDING_IW+AddrQueueDepth+1 )
  ) i_fifo_id_queue (
    .clk_i       ( clk_i        ),
    .rst_ni      ( rst_ni       ),
    .flush_i     ( 1'b0         ),
    .testmode_i  ( 1'b0         ),
    .full_o      ( queue_full   ),
    .empty_o     (              ),
    .usage_o     (              ),
    .data_i      ( queue_data_i ),
    .push_i      ( queue_push   ),
    .data_o      ( queue_data_o ),
    .pop_i       ( queue_pop    ),
    .fault_o     (fifo_tmr_fault_o)
  );

  // Accept incoming requests, push the ID (and sub-line offset, if any)
  // into the queue, and issue the corresponding request.
  assign in_req_ready_o  = axi_rsp_i.ar_ready & ~queue_full;
  assign queue_push      = axi_req_o.ar_valid & axi_rsp_i.ar_ready;

  // Assemble incoming responses if the cache line is wider than the bus data width.
  logic [CFG.LINE_WIDTH-1:0] response_data;

  if (CFG.LINE_WIDTH > CFG.FILL_DW) begin : g_data_concat
    assign queue_data_i = {in_req_bypass_i, in_req_id_i};
    assign {in_rsp_bypass_o, in_rsp_id_o} = queue_data_o;

    always_ff @(posedge clk_i, negedge rst_ni) begin
      if (!rst_ni) begin
        response_data[CFG.LINE_WIDTH-CFG.FILL_DW-1:0] <= '0;
      end else if (axi_rsp_i.r_valid && axi_req_o.r_ready) begin
        response_data[CFG.LINE_WIDTH-CFG.FILL_DW-1:0]
                <= response_data[CFG.LINE_WIDTH-1:CFG.FILL_DW];
      end
    end
    assign response_data[CFG.LINE_WIDTH-1:CFG.LINE_WIDTH-CFG.FILL_DW] = axi_rsp_i.r.data;
  end else if (CFG.LINE_WIDTH < CFG.FILL_DW) begin : g_data_slice
    logic [AddrQueueDepth-1:0] addr_offset;
    assign queue_data_i = {in_req_bypass_i, in_req_addr_i[CFG.FILL_ALIGN-1:CFG.LINE_ALIGN], in_req_id_i};
    assign {in_rsp_bypass_o, addr_offset, in_rsp_id_o} = queue_data_o;
    assign response_data =
      axi_rsp_i.r.data >> (addr_offset * CFG.LINE_WIDTH);
  end else begin : g_data_passthrough
    assign queue_data_i = {in_req_bypass_i, in_req_id_i};
    assign {in_rsp_bypass_o, in_rsp_id_o} = queue_data_o;
    assign response_data = axi_rsp_i.r.data;
  end

  // Accept response beats. Upon the last beat, pop the ID off the queue
  // and return the response.
  always_comb begin : p_response
    // Tie-off unused ports
    axi_req_o = '0;
    axi_req_o.b_ready  = 1'b1;
    axi_req_o.ar.addr  = in_req_addr_i;
    axi_req_o.ar.size  = $clog2(CFG.FILL_DW/8);
    axi_req_o.ar.burst = axi_pkg::BURST_INCR;
    axi_req_o.ar.len   = $unsigned(BeatsPerRefill-1);
    axi_req_o.ar.cache = axi_pkg::CACHE_MODIFIABLE;
    axi_req_o.ar_valid = in_req_valid_i & ~queue_full;

    in_rsp_data_o  = response_data;
    in_rsp_error_o = axi_rsp_i.r.resp[1];
    in_rsp_valid_o = 0;
    queue_pop      = 0;
    axi_req_o.r_ready  = 0;

    if (axi_rsp_i.r_valid) begin
      if (!axi_rsp_i.r.last) begin
        axi_req_o.r_ready  = 1;
      end else begin
        in_rsp_valid_o = 1;
        if (in_rsp_ready_i) begin
          axi_req_o.r_ready  = 1;
          queue_pop      = 1;
        end
      end
    end
  end

endmodule
