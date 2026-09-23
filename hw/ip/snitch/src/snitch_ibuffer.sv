// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
// Pei-Yu Lin <peilin@ethz.ch>

// Two independently tagged halves per core; one shared AXI refill transaction.
module snitch_ibuffer #(
  parameter int unsigned DEPTH = 32,
  parameter int unsigned NR_FETCH_PORTS = 1,
  parameter int unsigned FETCH_AW = 32,
  parameter int unsigned FETCH_DW = 32,
  parameter int unsigned FILL_AW = 32,
  parameter int unsigned FILL_DW = 512,
  parameter type axi_req_t = logic,
  parameter type axi_rsp_t = logic
) (
  input logic clk_i, rst_ni,
  input logic enable_prefetching_i,
  input logic [NR_FETCH_PORTS-1:0] flush_valid_i,
  output logic [NR_FETCH_PORTS-1:0] flush_ready_o,
  input logic [NR_FETCH_PORTS-1:0][FETCH_AW-1:0] inst_addr_i,
  input logic [NR_FETCH_PORTS-1:0] inst_cacheable_i,
  input logic [NR_FETCH_PORTS-1:0] inst_valid_i,
  output logic [NR_FETCH_PORTS-1:0][FETCH_DW-1:0] inst_data_o,
  output logic [NR_FETCH_PORTS-1:0] inst_ready_o,
  output logic [NR_FETCH_PORTS-1:0] inst_error_o,
  output axi_req_t axi_req_o,
  input axi_rsp_t axi_rsp_i
);
  localparam int unsigned BufferBits = DEPTH * FETCH_DW;
  localparam int unsigned HalfBits = BufferBits / 2;
  localparam int unsigned HalfBytes = HalfBits / 8;
  localparam int unsigned HalfOffset = $clog2(HalfBytes);
  localparam int unsigned WordOffset = $clog2(FETCH_DW / 8);
  localparam int unsigned TagWidth = FETCH_AW - HalfOffset - 1;
  localparam int unsigned FillOffset = $clog2(FILL_DW / 8);
  localparam int unsigned FullBeats = BufferBits / FILL_DW;
  localparam int unsigned HalfBeats = HalfBits / FILL_DW;
  localparam int unsigned BeatWidth = (FullBeats > 1) ? $clog2(FullBeats) : 1;
  localparam int unsigned PortWidth = (NR_FETCH_PORTS > 1) ? $clog2(NR_FETCH_PORTS) : 1;
  typedef logic [PortWidth-1:0] port_t;
  typedef logic [FETCH_AW-1:0] addr_t;
  typedef enum logic [1:0] {Idle, SendAddress, ReceiveData, UncachedResponse} state_t;
  state_t state_q;
  logic [NR_FETCH_PORTS-1:0][1:0][HalfBits-1:0] data_q;
  logic [NR_FETCH_PORTS-1:0][1:0][TagWidth-1:0] tag_q;
  logic [NR_FETCH_PORTS-1:0][1:0] valid_q, error_q;
  logic [NR_FETCH_PORTS-1:0] hit, half_sel;
  logic [NR_FETCH_PORTS-1:0][FETCH_AW-1:0] next_half_addr;
  logic select_valid, select_full, select_uncached;
  port_t select_port, owner_q, priority_q, candidate;
  addr_t select_addr, refill_addr_q;
  logic full_q, uncached_q, refill_error_q, refill_error;
  logic [BeatWidth-1:0] beat_q;
  logic [BufferBits-1:0] fill_q, fill_next;
  logic [FETCH_DW-1:0] uncached_data_q;

  always_comb begin
    inst_ready_o = '0;
    inst_error_o = '0;
    inst_data_o = '0;
    flush_ready_o = (state_q == Idle) ? flush_valid_i : '0;
    for (int unsigned p = 0; p < NR_FETCH_PORTS; p++) begin
      half_sel[p] = inst_addr_i[p][HalfOffset];
      hit[p] = valid_q[p][half_sel[p]] &&
          (tag_q[p][half_sel[p]] == inst_addr_i[p][FETCH_AW-1:HalfOffset+1]);
      next_half_addr[p] = {inst_addr_i[p][FETCH_AW-1:HalfOffset],
                          {HalfOffset{1'b0}}} + addr_t'(HalfBytes);
      if (inst_valid_i[p] && inst_cacheable_i[p] && hit[p] && !(|flush_valid_i)) begin
        inst_ready_o[p] = 1'b1;
        inst_error_o[p] = error_q[p][half_sel[p]];
        inst_data_o[p] = data_q[p][half_sel[p]][
            inst_addr_i[p][HalfOffset-1:WordOffset]*FETCH_DW +: FETCH_DW];
      end
    end
    if ((state_q == UncachedResponse) && !(|flush_valid_i) &&
        inst_valid_i[owner_q] && (inst_addr_i[owner_q] == refill_addr_q)) begin
      inst_ready_o[owner_q] = 1'b1;
      inst_error_o[owner_q] = refill_error_q;
      inst_data_o[owner_q] = uncached_data_q;
    end
  end

  // Round-robin demand arbitration, then speculative next-half refill.
  always_comb begin
    select_valid = 1'b0;
    select_full = 1'b0;
    select_uncached = 1'b0;
    select_port = '0;
    select_addr = '0;
    candidate = priority_q;
    for (int unsigned p = 0; p < NR_FETCH_PORTS; p++) begin
      if (!select_valid && inst_valid_i[candidate] &&
          (!inst_cacheable_i[candidate] || !hit[candidate])) begin
        select_valid = 1'b1;
        select_port = candidate;
        select_uncached = !inst_cacheable_i[candidate];
        select_full = inst_cacheable_i[candidate];
        select_addr = inst_cacheable_i[candidate] ?
            {inst_addr_i[candidate][FETCH_AW-1:HalfOffset+1], {(HalfOffset+1){1'b0}}} :
            inst_addr_i[candidate];
      end
      candidate = (candidate == port_t'(NR_FETCH_PORTS-1)) ? '0 : candidate + 1'b1;
    end
    candidate = priority_q;
    for (int unsigned p = 0; p < NR_FETCH_PORTS; p++) begin
      if (!select_valid && enable_prefetching_i && inst_valid_i[candidate] &&
          inst_cacheable_i[candidate] && hit[candidate] &&
          !error_q[candidate][half_sel[candidate]] &&
          (!valid_q[candidate][!half_sel[candidate]] ||
           tag_q[candidate][!half_sel[candidate]] !=
               next_half_addr[candidate][FETCH_AW-1:HalfOffset+1])) begin
        select_valid = 1'b1;
        select_port = candidate;
        select_addr = next_half_addr[candidate];
      end
      candidate = (candidate == port_t'(NR_FETCH_PORTS-1)) ? '0 : candidate + 1'b1;
    end
  end

  always_comb begin
    axi_req_o = '0;
    axi_req_o.ar_valid = (state_q == SendAddress);
    axi_req_o.ar.addr = refill_addr_q;
    axi_req_o.ar.len = uncached_q ? 8'd0 :
        (full_q ? 8'(FullBeats-1) : 8'(HalfBeats-1));
    axi_req_o.ar.size = uncached_q ? 3'(WordOffset) : 3'(FillOffset);
    axi_req_o.ar.burst = 2'b01; // INCR
    axi_req_o.ar.cache = uncached_q ? 4'b0000 : 4'b0010;
    axi_req_o.ar.prot = 3'b100; // Instruction access
    axi_req_o.r_ready = (state_q == ReceiveData);
    fill_next = fill_q;
    fill_next[beat_q*FILL_DW +: FILL_DW] = axi_rsp_i.r.data;
    refill_error = refill_error_q || axi_rsp_i.r.resp[1];
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      state_q <= Idle;
      valid_q <= '0;
      error_q <= '0;
      tag_q <= '0;
      data_q <= '0;
      fill_q <= '0;
      owner_q <= '0;
      priority_q <= '0;
      refill_addr_q <= '0;
      full_q <= 1'b0;
      uncached_q <= 1'b0;
      refill_error_q <= 1'b0;
      beat_q <= '0;
      uncached_data_q <= '0;
    end else begin
      case (state_q)
        Idle: begin
          // Acknowledge fence.i only after any outstanding burst has drained.
          if (|flush_valid_i) begin
            valid_q <= '0;
          end else if (select_valid) begin
            owner_q <= select_port;
            priority_q <= (select_port == port_t'(NR_FETCH_PORTS-1)) ?
                '0 : select_port + 1'b1;
            refill_addr_q <= select_addr;
            full_q <= select_full;
            uncached_q <= select_uncached;
            refill_error_q <= 1'b0;
            beat_q <= '0;
            if (!select_uncached) begin
              if (select_full) valid_q[select_port] <= '0;
              else valid_q[select_port][select_addr[HalfOffset]] <= 1'b0;
            end
            state_q <= SendAddress;
          end
        end
        SendAddress: if (axi_rsp_i.ar_ready) state_q <= ReceiveData;
        ReceiveData: begin
          if (axi_rsp_i.r_valid) begin
            fill_q <= fill_next;
            refill_error_q <= refill_error;
            if (axi_rsp_i.r.last) begin
              if (uncached_q) begin
                uncached_data_q <= axi_rsp_i.r.data[
                    refill_addr_q[FillOffset-1:WordOffset]*FETCH_DW +: FETCH_DW];
                state_q <= UncachedResponse;
              end else begin
                if (full_q) begin
                  data_q[owner_q] <= fill_next;
                  tag_q[owner_q][0] <= refill_addr_q[FETCH_AW-1:HalfOffset+1];
                  tag_q[owner_q][1] <= refill_addr_q[FETCH_AW-1:HalfOffset+1];
                  error_q[owner_q] <= {2{refill_error}};
                  valid_q[owner_q] <= '1;
                end else begin
                  data_q[owner_q][refill_addr_q[HalfOffset]] <= fill_next[HalfBits-1:0];
                  tag_q[owner_q][refill_addr_q[HalfOffset]] <=
                      refill_addr_q[FETCH_AW-1:HalfOffset+1];
                  error_q[owner_q][refill_addr_q[HalfOffset]] <= refill_error;
                  valid_q[owner_q][refill_addr_q[HalfOffset]] <= 1'b1;
                end
                state_q <= Idle;
              end
            end else beat_q <= beat_q + 1'b1;
          end
        end
        UncachedResponse: state_q <= Idle;
        default: state_q <= Idle;
      endcase
    end
  end

  // pragma translate_off
  initial begin
    assert (FETCH_DW == 32 && FETCH_AW == FILL_AW);
    assert (DEPTH >= 4 && (DEPTH & (DEPTH-1)) == 0);
    assert (NR_FETCH_PORTS > 0);
    assert (FILL_DW > FETCH_DW && FILL_DW <= HalfBits &&
            (FILL_DW & (FILL_DW-1)) == 0);
    assert (FullBeats <= 256 && BufferBits/8 <= 4096);
  end
  always @(posedge clk_i) begin
    if (rst_ni && (state_q == ReceiveData) && axi_rsp_i.r_valid) begin
      assert (axi_rsp_i.r.id == '0);
      assert (axi_rsp_i.r.last == (uncached_q ||
          (beat_q == (full_q ? BeatWidth'(FullBeats-1) : BeatWidth'(HalfBeats-1)))));
    end
  end
  // pragma translate_on
endmodule
