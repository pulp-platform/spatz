module glass_spill_register #(
  parameter type T      = logic,
  parameter bit  Bypass = 1'b0     // make this spill register transparent
) (
  input  logic clk_i   ,
  input  logic rst_ni  ,
  input  logic valid_i ,
  output logic ready_o ,
  input  T     data_i  ,
  output logic valid_o ,
  output logic valid_queue_o ,
  input  logic ready_i ,
  output T     data_queue_o,
  output T     data_o
);

  glass_spill_register_flushable #(
    .T(T),
    .Bypass(Bypass)
  ) glass_spill_register_flushable_i (
    .clk_i,
    .rst_ni,
    .valid_i,
    .flush_i(1'b0),
    .ready_o,
    .data_i,
    .valid_o,
    .valid_queue_o,
    .ready_i,
    .data_o,
    .data_queue_o
  );

endmodule

/// A register with handshakes that completely cuts any combinational paths
/// between the input and output. This spill register can be flushed.
/// Moreover, its internal signals are exposed
module glass_spill_register_flushable #(
  parameter type T           = logic,
  parameter bit  Bypass      = 1'b0   // make this spill register transparent
) (
  input  logic clk_i   ,
  input  logic rst_ni  ,
  input  logic valid_i ,
  input  logic flush_i ,
  output logic ready_o ,
  input  T     data_i  ,
  output logic valid_o ,
  output logic valid_queue_o ,
  input  logic ready_i ,
  output T     data_queue_o ,
  output T     data_o
);

  if (Bypass) begin : gen_bypass
    assign valid_o = valid_i;
    assign ready_o = ready_i;
    assign data_o  = data_i;
    // Simplify useless signals
    assign valid_queue_o = '0;
    assign data_queue_o  = '0;
  end else begin : gen_spill_reg
    // The A register.
    T a_data_q;
    logic a_full_q;
    logic a_fill, a_drain;

    always_ff @(posedge clk_i or negedge rst_ni) begin : ps_a_data
      if (!rst_ni)
        a_data_q <= '0;
      else if (a_fill)
        a_data_q <= data_i;
    end

    always_ff @(posedge clk_i or negedge rst_ni) begin : ps_a_full
      if (!rst_ni)
        a_full_q <= 0;
      else if (a_fill || a_drain)
        a_full_q <= a_fill;
    end

    // The B register.
    T b_data_q;
    logic b_full_q;
    logic b_fill, b_drain;

    always_ff @(posedge clk_i or negedge rst_ni) begin : ps_b_data
      if (!rst_ni)
        b_data_q <= '0;
      else if (b_fill)
        b_data_q <= a_data_q;
    end

    always_ff @(posedge clk_i or negedge rst_ni) begin : ps_b_full
      if (!rst_ni)
        b_full_q <= 0;
      else if (b_fill || b_drain)
        b_full_q <= b_fill;
    end

    // Fill the A register when the A or B register is empty. Drain the A register
    // whenever it is full and being filled, or if a flush is requested.
    assign a_fill = valid_i && ready_o && (!flush_i);
    assign a_drain = (a_full_q && !b_full_q) || flush_i;

    // Fill the B register whenever the A register is drained, but the downstream
    // circuit is not ready. Drain the B register whenever it is full and the
    // downstream circuit is ready, or if a flush is requested.
    assign b_fill = a_drain && (!ready_i) && (!flush_i);
    assign b_drain = (b_full_q && ready_i) || flush_i;

    // We can accept input as long as register B is not full.
    // Note: flush_i and valid_i must not be high at the same time,
    // otherwise an invalid handshake may occur
    assign ready_o = !a_full_q || !b_full_q;

    // The unit provides output as long as one of the registers is filled.
    assign valid_o = a_full_q | b_full_q;

    // Valid queued if both regs keep valid data
    assign valid_queue_o = a_full_q & b_full_q;

    // We empty the spill register before the slice register.
    assign data_o = b_full_q ? b_data_q : a_data_q;

    // Expose the other register value
    assign data_queue_o = b_full_q ? a_data_q : b_data_q;

    // pragma translate_off
    `ifndef VERILATOR
    flush_valid : assert property (
      @(posedge clk_i) disable iff (~rst_ni) (flush_i |-> ~valid_i)) else
      $warning("Trying to flush and feed the spill register simultaneously. You will lose data!");
   `endif
     // pragma translate_on
  end
endmodule
