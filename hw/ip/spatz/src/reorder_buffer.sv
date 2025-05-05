// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich
//
// This generic module provides an interface through which responses can
// be read in order, despite being written out of order. The responses
// must be indexed with an ID that identifies it within the ROB.

module reorder_buffer
  import cf_math_pkg::idx_width;
#(
  parameter int unsigned DataWidth = 0,
  parameter int unsigned NumWords  = 0,
  parameter bit FallThrough        = 1'b0,
  // Dependant parameters. Do not change!
  parameter int unsigned IdWidth   = idx_width(NumWords),
  parameter type data_t            = logic [DataWidth-1:0],
  parameter type id_t              = logic [IdWidth-1:0]
) (
  input  logic  clk_i,
  input  logic  rst_ni,
  // Data write
  input  data_t data_i,
  input  id_t   id_i,
  input  logic  push_i,
  // Data read
  output data_t data_o,
  output logic  valid_o,
  output id_t   id_read_o,
  input  logic  pop_i,
  // ID request
  input  logic  id_req_i,
  output id_t   id_o,
  output logic  id_valid_o,  // is the next id valid?
  output logic  full_o,
  output logic  empty_o
);

  /*************
   *  Signals  *
   *************/

  id_t              read_pointer_d, read_pointer_q;
  id_t              write_pointer_d, write_pointer_q, write_next_ptr;

  // Used to see which ID is available
  logic [NumWords-1:0] id_valid_d, id_valid_q;
  // Keep track of the ROB utilization
  logic [IdWidth:0] status_cnt_d, status_cnt_q;

  // Memory
  data_t [NumWords-1:0] mem_d, mem_q;
  logic  [NumWords-1:0] valid_d, valid_q;

  // Status flags
  assign full_o    = (status_cnt_q == NumWords-1);
  assign empty_o   = (status_cnt_q == 'd0);
  assign id_o      = write_pointer_q;
  assign id_read_o = read_pointer_q;
  assign write_next_ptr = write_pointer_q + 1;
  assign id_valid_o     = id_valid_q[write_pointer_q] & id_valid_q[write_next_ptr];

  // Read and Write logic
  always_comb begin: read_write_comb
    // Maintain state
    read_pointer_d  = read_pointer_q;
    write_pointer_d = write_pointer_q;
    status_cnt_d    = status_cnt_q;
    mem_d           = mem_q;
    valid_d         = valid_q;
    id_valid_d      = id_valid_q;

    // Output data
    data_o  = mem_q[read_pointer_q];
    valid_o = valid_q[read_pointer_q];

    // Request an ID.
    if (id_req_i && !full_o) begin
      // Increment the write pointer
      if (write_pointer_q == NumWords-1) begin
        write_pointer_d = 0;
      end else begin
        write_pointer_d = write_pointer_q + 1;
      end
      id_valid_d[write_pointer_q] = 1'b0;
      // Increment the overall counter
      status_cnt_d = status_cnt_q + 1;
    end

    // Push data
    if (push_i) begin
      mem_d[id_i]   = data_i;
      valid_d[id_i] = 1'b1;
    end

    // ROB is in fall-through mode -> do not change the pointers
    if (FallThrough && push_i && (id_i == read_pointer_q)) begin
      data_o  = data_i;
      valid_o = 1'b1;
      if (pop_i) begin
        valid_d[id_i] = 1'b0;
      end
    end

    // Pop data
    if (pop_i && valid_o) begin
      // Word was consumed
      valid_d[read_pointer_q] = 1'b0;
      // Mark ID as available
      id_valid_d[read_pointer_q] = 1'b1;

      // Increment the read pointer
      if (read_pointer_q == NumWords-1)
        read_pointer_d = '0;
      else
        read_pointer_d = read_pointer_q + 1;
      // Decrement the overall counter
      status_cnt_d = status_cnt_q - 1;
    end

    // Keep the overall counter stable if we request new ID and pop at the same time
    if ((id_req_i && !full_o) && (pop_i && valid_o)) begin
      status_cnt_d = status_cnt_q;
    end
  end: read_write_comb

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      read_pointer_q  <= '0;
      write_pointer_q <= '0;
      status_cnt_q    <= '0;
      mem_q           <= '0;
      valid_q         <= '0;
      // By default, all IDs are available
      id_valid_q      <= '1;
    end else begin
      read_pointer_q  <= read_pointer_d;
      write_pointer_q <= write_pointer_d;
      status_cnt_q    <= status_cnt_d;
      mem_q           <= mem_d;
      valid_q         <= valid_d;
      id_valid_q      <= id_valid_d;
    end
  end

  /****************
   *  Assertions  *
   ****************/

  if (NumWords == 0)
    $error("NumWords cannot be 0.");

  `ifndef VERILATOR
  // pragma translate_off
  full_write : assert property(
      @(posedge clk_i) disable iff (!rst_ni) (full_o |-> !id_req_i))
  else $fatal (1, "Trying to request an ID although the ROB is full.");

  empty_read : assert property(
      @(posedge clk_i) disable iff (!rst_ni) (!valid_o |-> !pop_i))
  else $fatal (1, "Trying to pop data although the top of the ROB is not valid.");
  // pragma translate_on
  `endif

endmodule: reorder_buffer
