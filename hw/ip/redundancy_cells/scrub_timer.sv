module scrub_timer #(
  parameter int unsigned PERIOD = 10_000_000  // 10ms @ 1ns cycle
) (
  input  logic clk_i,
  input  logic rst_ni,
  output logic scrub_trigger_o
);
  logic [$clog2(PERIOD)-1:0] cnt_q;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      cnt_q         <= '0;
      scrub_trigger_o <= '0;
    end else if (cnt_q == PERIOD - 1) begin
      cnt_q         <= '0;
      scrub_trigger_o <= 1'b1;
    end else begin
      cnt_q         <= cnt_q + 1;
      scrub_trigger_o <= 1'b0;
    end
  end

endmodule