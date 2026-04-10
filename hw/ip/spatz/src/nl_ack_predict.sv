// Author: Francesco Murande <fmurande@ethz.ch>

module fpnew_nl_predicted_ready #(
  parameter int unsigned LATENCY = 2
)(
  input  logic clk_i,
  input  logic rst_ni,
  input  logic flush_i,

  // Snoop on input handshake (upstream side of the opgroup)
  input  logic in_valid_i,
  input  logic in_ready_i,

  // Predicted ready (drives opgrp_out_ready — no combinational dependency on out_valid)
  output logic out_ready_o
);

  logic [LATENCY-1:0] inflight_q;
  logic               accepted;

  assign accepted = in_valid_i & in_ready_i;

  if (LATENCY == 1) begin : gen_lat1
    // Special case: result arrives next cycle
    always_ff @(posedge clk_i or negedge rst_ni) begin
      if (!rst_ni)       inflight_q <= '0;
      else if (flush_i)  inflight_q <= '0;
      else               inflight_q <= accepted;
    end
  end else begin : gen_latN
    always_ff @(posedge clk_i or negedge rst_ni) begin
      if (!rst_ni)       inflight_q <= '0;
      else if (flush_i)  inflight_q <= '0;
      else               inflight_q <= {inflight_q[LATENCY-2:0], accepted};
    end
  end

  assign out_ready_o = inflight_q[LATENCY-1];
endmodule