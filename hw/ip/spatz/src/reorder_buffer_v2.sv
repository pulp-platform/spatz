// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Diyou Shen ETH Zurich
//
// This generic module provides an interface through which responses can
// be read in order, despite being written out of order. The responses
// must be indexed with an ID that identifies it within the ROB.
//
// v2 Modifications:
// 1. Add id valid check for ensuring the id will not be freed before retired
// 2. Add functionality support for multiple data push and pop

module reorder_buffer_v2
  import cf_math_pkg::idx_width;
#(
  parameter int unsigned DataWidth = 0,
  parameter int unsigned NumWords  = 0,
  parameter int unsigned BurstLen  = 1,
  parameter bit UseBurst           = 1'b0,
  parameter bit FallThrough        = 1'b0,
  parameter int unsigned RspGF  = 1,
  // Dependant parameters. Do not change!
  parameter int unsigned IdWidth   = idx_width(NumWords),
  parameter type data_t            = logic [DataWidth-1:0],
  parameter type id_t              = logic [IdWidth-1:0]
) (
  input  logic  clk_i,
  input  logic  rst_ni,
  // Data write
  input  data_t [RspGF-1:0] data_i,
  input  id_t   id_i,
  input  logic  push_i,
  input  logic  gpush_i,     // are response grouped? (parallel incoming data)
  // Data read
  output data_t [RspGF-1:0] data_o,
  output logic  valid_o,
  output logic  gvalid_o,    // group pop valid
  output id_t   id_read_o,
  input  logic  pop_i,
  input  logic  gpop_i,      // group pop?
  // ID request
  input  logic  id_req_i,
  input  id_t   req_l_i,     // request length
  output id_t   id_o,
  output logic  id_valid_o,  // is the next id valid?
  output logic  bvalid_o,    // is there enough entries for a burst?
  // status
  output logic  full_o,
  output logic  empty_o
);

  /*************
   *  Signals  *
   *************/
  id_t              read_pointer_d, read_pointer_q;
  id_t              write_pointer_d, write_pointer_q;
  // pointing to next write element, so that it will roll back
  id_t              write_next_ptr;

  // Used to see which ID is available
  logic [NumWords-1:0] id_valid_d, id_valid_q;
  // Keep track of the ROB utilization
  logic [IdWidth:0]    status_cnt_d, status_cnt_q;
  // How many elements read per cycle?
  logic [RspGF:0]      read_elem;

  // Memory
  data_t [NumWords-1:0] mem_d,   mem_q;
  logic  [NumWords-1:0] valid_d, valid_q;
  // masks and rotated-to-left masks
  // id mask
  logic  [NumWords-1:0] id_mask,  id_mask_rl;
  // burst mask
  logic  [NumWords-1:0] bid_mask, bid_mask_rl;
  // group valid mask
  logic  [NumWords-1:0] gv_mask,  gv_mask_rl;

  id_t req_len;

  // Status flags
  assign full_o         = (status_cnt_q == NumWords);
  assign empty_o        = (status_cnt_q == 'd0);
  assign id_o           = write_pointer_q;
  assign id_read_o      = read_pointer_q;
  assign write_next_ptr = write_pointer_q + 1;
  assign id_valid_o     = id_valid_q[write_pointer_q] & id_valid_q[write_next_ptr];

  assign read_elem      = gpop_i  ? RspGF : 2'b1;
  assign req_len        = UseBurst ? req_l_i  : 'd1;

  // masks used to easily check validity of signals
  assign  id_mask       = (1'b1 << req_len) - 1'b1;
  assign  id_mask_rl    = (id_mask  << write_pointer_q) | (id_mask  >> (NumWords-write_pointer_q));

  assign  bid_mask      = (1'b1 << BurstLen) - 1'b1;
  assign  bid_mask_rl   = (bid_mask << write_pointer_q) | (bid_mask >> (NumWords-write_pointer_q));

  assign  gv_mask       = (1'b1 << RspGF) - 1'b1;
  assign  gv_mask_rl    = (gv_mask  << read_pointer_q)  | (gv_mask  >> (NumWords-read_pointer_q));

  // For the rob, several functions need to be taken care of
  // 1) requst ID
  // 2) pop in data
  // 3) push out data
  id_t  [RspGF-1:0] out_temp_ptr, push_temp_ptr, pop_temp_ptr;
  // Read and Write logic
  always_comb begin: read_write_comb
    // Maintain state
    read_pointer_d  = read_pointer_q;
    write_pointer_d = write_pointer_q;
    status_cnt_d    = status_cnt_q;
    mem_d           = mem_q;
    valid_d         = valid_q;
    id_valid_d      = id_valid_q;

    data_o          = '0;
    out_temp_ptr    = '0;
    push_temp_ptr   = '0;
    pop_temp_ptr    = '0;

    // Output data
    data_o[0]    = mem_q[read_pointer_q];
    bvalid_o     = &((bid_mask_rl & id_valid_q) | ~bid_mask_rl);
    valid_o      = valid_q[read_pointer_q];
    gvalid_o   = &((gv_mask_rl & valid_q) | ~gv_mask_rl);
    if ((RspGF > 1) & gvalid_o) begin
      for (int unsigned i = 1; i < RspGF; i++) begin
        // when gvalid_o is ready, fill in all data fields
        out_temp_ptr[i] = read_pointer_q+i;
        data_o[i] = mem_q[out_temp_ptr[i]];
      end
    end

    // Request ID
    // Assign entries depending on length
    if (id_req_i && !full_o) begin
      if (req_len == 1'b1) begin
        // Normal request
        write_pointer_d = write_pointer_q + 1;
        id_valid_d[write_pointer_q] = 1'b0;
        status_cnt_d    = status_cnt_q + 1'b1;
      end else begin
        // Burst request
        if (bvalid_o) begin
          write_pointer_d = write_pointer_q + req_len;
          id_valid_d      = id_valid_q & (~id_mask_rl);
          status_cnt_d    = status_cnt_q + req_len;
        end
      end
    end

    // Push data
    if (push_i) begin
      if (gpush_i) begin
        // grouped push, write in multiple data
        for (int unsigned i = 0; i < RspGF; i++) begin
          // use intermediate signal to ensure the counter can roll over
          push_temp_ptr = id_i + i;
          mem_d[push_temp_ptr]   = data_i[i];
          valid_d[push_temp_ptr] = 1'b1;
        end
      end else begin
        // normal push
        mem_d[id_i]   = data_i;
        valid_d[id_i] = 1'b1;
      end
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
      if (gpop_i) begin
        // group pop data out
        if (gvalid_o) begin
          // check if group read is valid
          for (int unsigned i = 0; i < RspGF; i++) begin
            // use intermediate signal to ensure the counter can roll over
            pop_temp_ptr = read_pointer_q + i;
            // multiple data were consumed
            valid_d[pop_temp_ptr] = 1'b0;
            // multiple ids were freed
            id_valid_d[pop_temp_ptr] = 1'b1;
          end
        end
      end else begin
        // Word was consumed
        valid_d[read_pointer_q] = 1'b0;
        // Mark ID as available
        id_valid_d[read_pointer_q] = 1'b1;
      end

      // Increment the read pointer
      // read_pointer is in id_t, it will automatically roll back if reach the end
      read_pointer_d = read_pointer_q + read_elem;
      // Decrement the overall counter
      status_cnt_d = status_cnt_q - read_elem;
    end

    // Keep the overall counter stable if we request new ID and pop at the same time
    if ((id_req_i && !full_o) && (pop_i && valid_o)) begin
      if (req_len == 1'b1) begin
        status_cnt_d = status_cnt_q;
      end else begin
        if (bvalid_o) begin
          status_cnt_d = status_cnt_q + req_len - read_elem;
        end
      end
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
  else $error ("Trying to request an ID although the ROB is full.");

  empty_read : assert property(
      @(posedge clk_i) disable iff (!rst_ni) (!valid_o |-> !pop_i))
  else $error ("Trying to pop data although the top of the ROB is not valid.");

  overflow:    assert property(
      @(posedge clk_i) disable iff (!rst_ni) (status_cnt_q <= NumWords))
  else $error ("Overflow on status counter");

  overwrite:   assert property(
      @(posedge clk_i) disable iff (!rst_ni) (!(|(id_valid_q & valid_q))))
  else $error ("ID marked valid before data is taken");

  // pragma translate_on
  `endif

endmodule: reorder_buffer_v2
