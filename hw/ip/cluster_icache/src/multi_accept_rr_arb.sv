// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Michael Rogenmoser <michaero@iis.ee.ethz.ch>

module multi_accept_rr_arb #(
  parameter type         data_t = logic,
  parameter int unsigned NumInp = 0
) (
  input  logic               clk_i,
  input  logic               rst_ni,
  input  logic               flush_i,

  input  data_t [NumInp-1:0] inp_data_i,
  input  logic  [NumInp-1:0] inp_valid_i,
  output logic  [NumInp-1:0] inp_ready_o,

  output data_t              oup_data_o,
  output logic               oup_valid_o,
  input  logic               oup_ready_i
);

  // lock arbiter decision in case we got at least one req and no acknowledge
  logic  lock_d, lock_q;
  logic [NumInp-1:0] req_d, req_q;

  assign lock_d     = oup_valid_o & ~oup_ready_i;
  assign req_d      = (lock_q) ? req_q : inp_valid_i;

  always_ff @(posedge clk_i or negedge rst_ni) begin : p_lock_reg
    if (!rst_ni) begin
      lock_q <= '0;
    end else begin
      if (flush_i) begin
        lock_q <= '0;
      end else begin
        lock_q <= lock_d;
      end
    end
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin : p_req_regs
    if (!rst_ni) begin
      req_q  <= '0;
    end else begin
      if (flush_i) begin
        req_q  <= '0;
      end else begin
        req_q  <= req_d;
      end
    end
  end

  localparam int unsigned IdxWidth   = (NumInp > 32'd1) ? unsigned'($clog2(NumInp)) : 32'd1;
  logic [IdxWidth-1:0] idx;

  `ifndef SYNTHESIS
  `ifndef COMMON_CELLS_ASSERTS_OFF
    lock: assert property(
      @(posedge clk_i) disable iff (!rst_ni || flush_i)
          oup_valid_o && (!oup_ready_i && !flush_i) |=> idx == $past(idx)) else
          $fatal (1, "Lock implies same arbiter decision in next cycle if output is not ready.");

    lock_req: assume property(
      @(posedge clk_i) disable iff (!rst_ni || flush_i)
          lock_d |=> inp_valid_i[idx] == req_q[idx]) else
          $fatal (1, "It is disallowed to deassert the selected unserved request signals.");
  `endif
  `endif




  rr_arb_tree #(
    .NumIn      (NumInp),
    .DataType   (data_t),
    .ExtPrio    (1'b0),
    .AxiVldRdy  (1'b1),
    .LockIn     (1'b0)
  ) i_arbiter (
    .clk_i,
    .rst_ni,
    .flush_i,
    .rr_i   ('0),
    .req_i  (req_d),
    .gnt_o  (inp_ready_o),
    .data_i (inp_data_i),
    .gnt_i  (oup_ready_i),
    .req_o  (oup_valid_o),
    .data_o (oup_data_o),
    .idx_o  (idx)
  );

endmodule
