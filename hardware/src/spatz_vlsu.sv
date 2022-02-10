// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// The vector load/store unit is used to load vectors from memory
// and to the vector register file and store them back again.

module spatz_vlsu
  import spatz_pkg::*;
  import rvv_pkg::*;
  import cf_math_pkg::idx_width;
#(
  parameter NrMemPorts          = 1,
  parameter NrOutstandingLoads  = 8,
  parameter type x_mem_req_t    = logic,
  parameter type x_mem_resp_t   = logic,
  parameter type x_mem_result_t = logic,
  // Dependant parameters. DO NOT CHANGE!
  localparam int unsigned IdWidth = idx_width(NrOutstandingLoads)
) (
  input  logic clk_i,
  input  logic rst_ni,

  // Spatz req
  input  spatz_req_t spatz_req_i,
  input  logic       spatz_req_valid_i,
  output logic       spatz_req_ready_o,

  // VLSU rsp
  output logic      vlsu_rsp_valid_o,
  output vlsu_rsp_t vlsu_rsp_o,

  // VRF
  output vreg_addr_t vrf_waddr_o,
  output vreg_data_t vrf_wdata_o,
  output logic       vrf_we_o,
  output vreg_be_t   vrf_wbe_o,
  input  logic       vrf_wvalid_i,
  output vreg_addr_t vrf_raddr_o,
  output logic       vrf_re_o,
  input  vreg_data_t vrf_rdata_i,
  input  logic       vrf_rvalid_i,

  // X-Interface Memory Request
  output logic          [NrMemPorts-1:0] x_mem_valid_o,
  input  logic          [NrMemPorts-1:0] x_mem_ready_i,
  output x_mem_req_t    [NrMemPorts-1:0] x_mem_req_o,
  input  x_mem_resp_t   [NrMemPorts-1:0] x_mem_resp_i,
  //X-Interface Memory Result
  input  logic          [NrMemPorts-1:0] x_mem_result_valid_i,
  input  x_mem_result_t [NrMemPorts-1:0] x_mem_result_i,
  // X-Interface Memory Finished
  output logic                           x_mem_finished_o
);

  // Include FF
  `include "common_cells/registers.svh"

  /////////////////
  // Localparams //
  /////////////////

  localparam int unsigned NrIPUsPerMemPort        = N_IPU/NrMemPorts;
  localparam int unsigned VregDataWidthPerMemPort = ELEN*NrIPUsPerMemPort;

  localparam int unsigned MemDataWidth  = $bits(x_mem_req_o[0].wdata);
  localparam int unsigned MemDataWidthB = MemDataWidth/8;

  //////////////
  // Typedefs //
  //////////////

  typedef logic [IdWidth-1:0]        id_t;
  typedef logic [$clog2(VELE*8)-1:0] vreg_elem_t;

  /////////////
  // Signals //
  /////////////

  // Spatz request
  spatz_req_t spatz_req_d, spatz_req_q;
  `FF(spatz_req_q, spatz_req_d, '0)

  // Is vfu and the ipu operands ready
  logic vlsu_is_ready;

  // Has a new vlsu execution request arrived
  logic new_vlsu_request;
  assign new_vlsu_request = spatz_req_valid_i & vlsu_is_ready & (spatz_req_i.ex_unit == LSU);

  // Is instruction a load
  logic is_load;
  assign is_load = spatz_req_q.op_mem.is_load;

  // Is the vector length zero (no active instruction)
  logic  is_vl_zero;
  assign is_vl_zero = spatz_req_q.vl == 'd0;

  // Is the memory address unaligned
  logic is_addr_unaligned;
  assign is_addr_unaligned = spatz_req_q.rs1[1:0] != 2'b00;

  // Do we have a strided memory access
  logic is_strided;
  assign is_strided = (spatz_req_q.op == VLSE) | (spatz_req_q.op == VSSE);

  // Do we have to access every single element on its own
  logic is_single_element_operation;
  assign is_single_element_operation = is_addr_unaligned | is_strided;

  // How large is a single element (in bytes)
  logic [2:0] single_element_size;
  assign single_element_size = 1'b1 << spatz_req_q.vtype.vsew;

  // Signal to the controller if we are ready for a new instruction or not
  assign spatz_req_ready_o = vlsu_is_ready;

  // Buffer signals
  elen_t [NrMemPorts-1:0] buffer_wdata;
  id_t   [NrMemPorts-1:0] buffer_wid;
  logic  [NrMemPorts-1:0] buffer_push;
  logic  [NrMemPorts-1:0] buffer_rvalid;
  elen_t [NrMemPorts-1:0] buffer_rdata;
  logic  [NrMemPorts-1:0] buffer_pop;
  id_t   [NrMemPorts-1:0] buffer_rid;
  logic  [NrMemPorts-1:0] buffer_req_id;
  id_t   [NrMemPorts-1:0] buffer_id;
  logic  [NrMemPorts-1:0] buffer_full;
  logic  [NrMemPorts-1:0] buffer_empty;

  logic  [NrMemPorts-1:0][$clog2(NrOutstandingLoads)-1:0] buffer_id_diff;

  // Memory counter signals
  vlen_t [NrMemPorts-1:0] mem_counter_max;
  logic  [NrMemPorts-1:0] mem_counter_clear;
  logic  [NrMemPorts-1:0] mem_counter_en;
  logic  [NrMemPorts-1:0] mem_counter_load;
  vlen_t [NrMemPorts-1:0] mem_counter_delta;
  vlen_t [NrMemPorts-1:0] mem_counter_load_value;
  vlen_t [NrMemPorts-1:0] mem_counter_value;
  logic  [NrMemPorts-1:0] mem_counter_overflow;

  // Vector register file counter signals
  vlen_t [N_IPU-1:0] vreg_counter_max;
  logic  [N_IPU-1:0] vreg_counter_clear;
  logic  [N_IPU-1:0] vreg_counter_en;
  logic  [N_IPU-1:0] vreg_counter_load;
  vlen_t [N_IPU-1:0] vreg_counter_delta;
  vlen_t [N_IPU-1:0] vreg_counter_load_value;
  vlen_t [N_IPU-1:0] vreg_counter_value;
  logic  [N_IPU-1:0] vreg_counter_overflow;

  // Current element id and byte id that are being accessed
  // at the register file
  vreg_elem_t               vreg_elem_id;
  logic [$clog2(ELENB)-1:0] vreg_byte_id;

  // Is the memory operation valid and are we at the last one
  logic [NrMemPorts-1:0] mem_operation_valid;
  logic [NrMemPorts-1:0] mem_operation_last;
  logic [NrMemPorts-1:0] mem_operations_finished;

  // Is the register file operation valid and are we at the last one
  logic [N_IPU-1:0] vreg_operation_valid;
  logic [N_IPU-1:0] vreg_operation_last;
  logic [N_IPU-1:0] vreg_operations_finished;

  ///////////////////
  // State Handler //
  ///////////////////

  // Accept a new operation or clear req register if we are finished
  always_comb begin
    spatz_req_d = spatz_req_q;

    if (new_vlsu_request) begin
      spatz_req_d = spatz_req_i;

      // Convert the vl to number of bytes for all element widths
      unique case (spatz_req_i.vtype.vsew)
        EW_8:  spatz_req_d.vl = spatz_req_i.vl;
        EW_16: spatz_req_d.vl = spatz_req_i.vl << 1;
        EW_32: spatz_req_d.vl = spatz_req_i.vl << 2;
        default: spatz_req_d.vl = '0;
      endcase
    end else if (!is_vl_zero && vlsu_is_ready && !new_vlsu_request) begin
      // If we are ready for a new instruction but there is none, clear req register
      spatz_req_d = '0;
    end
  end

  // Respond to controller if we are finished executing
  always_comb begin : vlsu_rsp
    vlsu_rsp_valid_o = 1'b0;
    vlsu_rsp_o       = '0;

    // Write back accessed register file vector to clear scoreboard entry
    if (vlsu_is_ready && (spatz_req_q.vl != '0)) begin
      vlsu_rsp_o.id    = spatz_req_q.id;
      vlsu_rsp_o.vd    = spatz_req_q.vd;
      vlsu_rsp_valid_o = 1'b1;
    end
  end

  // Are we ready to accept a new instruction from the controller
  assign vlsu_is_ready = (&vreg_operations_finished) & (&mem_operations_finished);

  // Signal when we are finished with with accessing the memory (necessary
  // for the case with more than one memory port)
  assign x_mem_finished_o = vlsu_is_ready & ((spatz_req_q.vl != 'd0) | (new_vlsu_request & spatz_req_i.vl == 'd0));

  ////////////////////////
  // Address Generation //
  ////////////////////////

  elen_t      [NrMemPorts-1:0]      mem_req_addr;
  logic       [NrMemPorts-1:0][1:0] mem_req_addr_offset;

  vreg_addr_t       vreg_addr;
  logic       [1:0] vreg_addr_offset;

  // Calculate the memory address for each memory port
  always_comb begin : gen_mem_req_addr
    for (int unsigned i = 0; i < NrMemPorts; i++) begin : gen_elem_access
      automatic int unsigned stride = is_strided ? spatz_req_q.rs2 >> spatz_req_q.vtype.vsew : 'd1;
      automatic logic [31:0] addr   = spatz_req_q.rs1 + ({mem_counter_value[i][$bits(vlen_t)-1:2] << $clog2(NrMemPorts), mem_counter_value[i][1:0]} + (i << 2)) * stride;

      mem_req_addr[i] = {addr[31:2], 2'b00};
      mem_req_addr_offset[i] = addr[1:0];
    end
  end

  // Calculate the register file address
  always_comb begin : gen_vreg_addr
    vreg_addr = {spatz_req_q.vd, {$clog2(VELE){1'b0}}} + $unsigned(vreg_elem_id);
    vreg_addr_offset = vreg_byte_id * spatz_req_q.rs2 + spatz_req_q.rs1;
  end

  //////////////
  // Counters //
  //////////////

  // FOr each IPU that we have, count how many elements we have already loaded/stored.
  // Multiple counters are necessary for the case where not every signle IPU will
  // receive the same number of elements to work through.
  for (genvar i = 0; i < N_IPU; i++) begin : gen_vreg_counters
    delta_counter #(
      .WIDTH($bits(vlen_t))
    ) i_delta_counter_vreg (
      .clk_i     (clk_i),
      .rst_ni    (rst_ni),
      .clear_i   (vreg_counter_clear[i]),
      .en_i      (vreg_counter_en[i]),
      .load_i    (vreg_counter_load[i]),
      .down_i    (1'b0), // We always count up
      .delta_i   (vreg_counter_delta[i]),
      .d_i       (vreg_counter_load_value[i]),
      .q_o       (vreg_counter_value[i]),
      .overflow_o(vreg_counter_overflow[i])
    );
  end

  /* verilator lint_off SELRANGE */
  always_comb begin
    for (int unsigned i = 0; i < N_IPU; i++) begin
      // The total amount of elements we have to work through
      automatic int unsigned max   = N_IPU == 'd1 ? spatz_req_q.vl : ((spatz_req_q.vl >> ($clog2(N_IPU) + $clog2(ELENB))) << $clog2(ELENB)) + (spatz_req_q.vl[idx_width(N_IPU)+$clog2(ELENB)-1:$clog2(ELENB)] > i ? ELENB : spatz_req_q.vl[idx_width(N_IPU)+$clog2(ELENB)-1:$clog2(ELENB)] == i ? spatz_req_q.vl[$clog2(ELENB)-1:0] : 'd0);
      // How many elements are left to do
      automatic int unsigned delta = max - vreg_counter_value[i];
      automatic logic vrf_transaction_valid = (is_load & vrf_wvalid_i & vrf_we_o) | (~is_load & vrf_rvalid_i & vrf_re_o);

      vreg_operation_valid[i]     = (delta != 'd0) & ~is_vl_zero;
      vreg_operation_last[i]      = vreg_operation_valid[i] & (delta <= (is_single_element_operation ? single_element_size : 'd4));

      vreg_counter_load[i]       = 1'b0;
      vreg_counter_load_value[i] = '0;

      vreg_counter_clear[i] = new_vlsu_request;
      vreg_counter_delta[i] = !vreg_operation_valid[i] ? 'd0 :
                               is_single_element_operation ? single_element_size :
                               vreg_operation_last[i] ? delta : 'd4;

      // If we have more than one IPU per memory port, then we are not able to
      // write back an element every single time and have to wait until the one
      // before us has already received its elements.
      if (NrIPUsPerMemPort == 'd1) begin
        vreg_counter_en[i] = vreg_operation_valid[i] & vrf_transaction_valid;
      end else begin
        if (i%NrIPUsPerMemPort == 'd0) begin
          vreg_counter_en[i] = vreg_operation_valid[i] & vrf_transaction_valid & ((vreg_counter_value[NrIPUsPerMemPort/NrMemPorts-1][$bits(vlen_t)-1:$clog2(ELENB)] == vreg_counter_value[i][$bits(vlen_t)-1:$clog2(ELENB)]) & (vreg_counter_value[i+1][$bits(vlen_t)-1:$clog2(ELENB)] == vreg_counter_value[i][$bits(vlen_t)-1:$clog2(ELENB)]));
        end else begin
          vreg_counter_en[i] = vreg_operation_valid[i] & vrf_transaction_valid & (vreg_counter_value[i-1][$bits(vlen_t)-1:$clog2(ELENB)] != vreg_counter_value[i][$bits(vlen_t)-1:$clog2(ELENB)]);
        end
      end

      vreg_operations_finished[i] = ~vreg_operation_valid[i] | (vreg_operation_last[i] & vreg_counter_en[i]);

      vreg_counter_max[i] = max;
    end

    vreg_elem_id = vreg_counter_value[NrIPUsPerMemPort-1] >> $clog2(ELENB);
    vreg_byte_id = vreg_counter_value[0][$clog2(ELENB)-1:0];
  end
  /* verilator lint_on SELRANGE */

  // For each memory port we count how many elements we have already loaded/stored.
  // Multiple counters are needed all memory ports can work independent of each other.
  for (genvar i = 0; i < NrMemPorts; i++) begin : gen_mem_counters
    delta_counter #(
      .WIDTH($bits(vlen_t))
    ) i_delta_counter_mem (
      .clk_i     (clk_i),
      .rst_ni    (rst_ni),
      .clear_i   (mem_counter_clear[i]),
      .en_i      (mem_counter_en[i]),
      .load_i    (mem_counter_load[i]),
      .down_i    (1'b0), // We always count up
      .delta_i   (mem_counter_delta[i]),
      .d_i       (mem_counter_load_value[i]),
      .q_o       (mem_counter_value[i]),
      .overflow_o(mem_counter_overflow[i])
    );
  end

  always_comb begin
    for (int unsigned i = 0; i < NrMemPorts; i++) begin
      // The total amount of elements we have to work through
      automatic int unsigned max   = NrMemPorts == 'd1 ? spatz_req_q.vl : ((spatz_req_q.vl >> ($clog2(NrMemPorts) + $clog2(MemDataWidthB))) << $clog2(MemDataWidthB)) + (spatz_req_q.vl[idx_width(NrMemPorts)+$clog2(MemDataWidthB)-1:$clog2(MemDataWidthB)] > i ? MemDataWidthB : spatz_req_q.vl[idx_width(NrMemPorts)+$clog2(MemDataWidthB)-1:$clog2(MemDataWidthB)] == i ? spatz_req_q.vl[$clog2(MemDataWidthB)-1:0] : 'd0);
      // How many elements are left to do
      automatic int unsigned delta = max - mem_counter_value[i];

      mem_operation_valid[i]     = (delta != 'd0) & ~is_vl_zero;
      mem_operation_last[i]      = mem_operation_valid[i] & (delta <= (is_single_element_operation ? single_element_size : 'd4));

      mem_counter_load[i]       = 1'b0;
      mem_counter_load_value[i] = '0;

      mem_counter_clear[i] = new_vlsu_request;
      mem_counter_delta[i] = !mem_operation_valid[i] ? 'd0 :
                              is_single_element_operation ? single_element_size :
                              mem_operation_last[i] ? delta : 'd4;
      mem_counter_en[i]    = x_mem_ready_i[i] & x_mem_valid_o[i];

      mem_operations_finished[i] = ~mem_operation_valid[i] | (mem_operation_last[i] & mem_counter_en[i]);

      mem_counter_max[i] = max;
    end
  end

  ////////////////////
  // Reorder Buffer //
  ////////////////////

  // The reorder buffer decouples the memory side from the register file side.
  // All elements from one side to the other go through it.
  for (genvar i = 0; i < NrMemPorts; i++) begin : gen_buffer
    reorder_buffer #(
      .DataWidth  (ELEN),
      .NumWords   (NrOutstandingLoads),
      .FallThrough(1'b0)
    ) i_reorder_buffer (
      .clk_i    (clk_i),
      .rst_ni   (rst_ni),
      .data_i   (buffer_wdata[i]),
      .id_i     (buffer_wid[i]),
      .push_i   (buffer_push[i]),
      .data_o   (buffer_rdata[i]),
      .valid_o  (buffer_rvalid[i]),
      .id_read_o(buffer_rid[i]),
      .pop_i    (buffer_pop[i]),
      .id_req_i (buffer_req_id[i]),
      .id_o     (buffer_id[i]),
      .full_o   (buffer_full[i]),
      .empty_o  (buffer_empty[i])
    );

    assign buffer_id_diff[i] = buffer_id[i] - buffer_rid[i];
  end

  ///////////////////////////////
  // Memory/Vregfile Interface //
  ///////////////////////////////

  // Memory request signals
  id_t  [NrMemPorts-1:0]                   mem_req_id;
  logic [NrMemPorts-1:0][MemDataWidth-1:0] mem_req_data;
  logic [NrMemPorts-1:0]                   mem_req_svalid;
  logic [NrMemPorts-1:0][ELEN/8-1:0]       mem_req_strb;
  logic [NrMemPorts-1:0]                   mem_req_lvalid;
  logic [NrMemPorts-1:0]                   mem_req_last;

  /* verilator lint_off LATCH */
  always_comb begin
    vrf_raddr_o = '0;
    vrf_re_o    = 1'b0;
    vrf_waddr_o = '0;
    vrf_wdata_o = '0;
    vrf_we_o    = 1'b0;
    vrf_wbe_o   = '0;

    buffer_wdata  = '0;
    buffer_wid    = '0;
    buffer_push   = '0;
    buffer_pop    = '0;
    buffer_req_id = '0;

    mem_req_id     = '0;
    mem_req_data   = '0;
    mem_req_strb   = '0;
    mem_req_svalid = '0;
    mem_req_lvalid = '0;
    mem_req_last   = '0;

    if (is_load) begin
      // If we have a valid element in the buffer,
      // store it back to the register file
      if (|vreg_operation_valid) begin
        // Calculate difference between read and write id in buffer
        automatic logic [NrMemPorts-1:0] diff_zero;
        for (int unsigned i = 0; i < NrMemPorts; i++) diff_zero[i] = buffer_id_diff[i] == 'd0;

        vrf_waddr_o = vreg_addr;
        // Enable write back to vregfile if we have a valid element in all buffers that still
        // need to write something back.
        vrf_we_o = &buffer_rvalid | (&(buffer_rvalid | diff_zero) & ~(|mem_operation_valid));

        for (int unsigned i = 0; i < NrMemPorts; i++) begin
          automatic logic [ELEN-1:0] data = buffer_rdata[i];

          // Shift data to correct position if we have an unaligned memory request
          unique case (is_strided ? vreg_addr_offset : spatz_req_q.rs1[1:0])
           2'b00: data = data;
           2'b01: data = {data[7:0], data[31:8]};
           2'b10: data = {data[15:0], data[31:16]};
           2'b11: data = {data[23:0], data[31:24]};
          endcase

          for (int unsigned j = 0; j < NrIPUsPerMemPort; j++) begin
            automatic int unsigned idx = i*NrIPUsPerMemPort + j;

            // Shift data to correct position if we have a strided memory access
            if (is_strided) begin
              unique case (vreg_counter_value[idx][1:0])
               2'b00: data = data;
               2'b01: data = {data[23:0], data[31:24]};
               2'b10: data = {data[15:0], data[31:16]};
               2'b11: data = {data[7:0], data[31:8]};
              endcase
            end
            vrf_wdata_o[ELEN*idx +: ELEN] = data;

            // Create write byte enable mask for register file
            if (vreg_counter_en[idx]) begin
              if (is_single_element_operation) begin
                automatic logic [$clog2(ELENB)-1:0] shift = vreg_counter_value[idx][$clog2(ELENB)-1:0];
                automatic logic [ELENB-1:0]          mask = spatz_req_q.vtype.vsew == rvv_pkg::EW_8  ? 1'b1 :
                                                            spatz_req_q.vtype.vsew == rvv_pkg::EW_16 ? 2'b11 : 4'b1111;
                vrf_wbe_o[ELENB*idx +: ELENB] = mask << shift;
              end else begin
                for (int unsigned k = 0; k < ELENB; k++) begin
                  vrf_wbe_o[ELENB*idx+k +: 'd1] = k < vreg_counter_delta[idx];
                end
              end
            end
          end
        end

        // Pop stored element and free space in buffer
        buffer_pop = buffer_rvalid & {NrMemPorts{vrf_wvalid_i}};
      end

      for (int unsigned i = 0; i < NrMemPorts; i++) begin
        // Write the load result to the buffer
        buffer_wdata[i] = x_mem_result_i[i].rdata;
        buffer_wid[i] = x_mem_result_i[i].id;
        buffer_push[i] = x_mem_result_valid_i[i];

        // Request a new id and and execute memory request
        if (~buffer_full[i] && mem_operation_valid[i]) begin
          buffer_req_id[i] = x_mem_ready_i[i] & x_mem_valid_o[i];
          mem_req_lvalid[i] = 1'b1;
          mem_req_id[i] = buffer_id[i];
          mem_req_last[i] = mem_operation_last[i];
        end
      end
    // Store operation
    end else begin
      // Read new element from the register file and store
      // it to the buffer
      if (~(|buffer_full) & |vreg_operation_valid) begin
        vrf_raddr_o = vreg_addr;
        vrf_re_o = 1'b1;

        // Push element to buffer if read from vregfile
        for (int unsigned i = 0; i < NrMemPorts; i++) begin
          for (int unsigned j = 0; j < NrIPUsPerMemPort; j++) begin
            automatic int unsigned idx = i*NrIPUsPerMemPort + j;
            if (vreg_counter_en[idx]) begin
              buffer_wdata[i] = vrf_rdata_i[ELEN*idx +: ELEN];
            end
          end

          buffer_wid[i]    = buffer_id[i];
          buffer_req_id[i] = vrf_rvalid_i;
          buffer_push[i]   = vrf_rvalid_i;
        end
      end

      for (int unsigned i = 0; i < NrMemPorts; i++) begin
        // Read element from buffer and execute memory request
        if (mem_operation_valid[i]) begin
          automatic logic [MemDataWidth-1:0] data = buffer_rdata[i];

          // Shift data to lsb if we have a strided memory access
          if (is_strided) begin
            unique case (mem_counter_value[i][1:0])
             2'b00: data = data;
             2'b01: data = {data[7:0], data[31:8]};
             2'b10: data = {data[15:0], data[31:16]};
             2'b11: data = {data[23:0], data[31:24]};
            endcase
          end

          // Shift data to correct position if we have an unaligned memory request
          unique case (is_strided ? mem_req_addr_offset[i] : spatz_req_q.rs1[1:0])
           2'b00: mem_req_data[i] = data;
           2'b01: mem_req_data[i] = {data[23:0], data[31:24]};
           2'b10: mem_req_data[i] = {data[15:0], data[31:16]};
           2'b11: mem_req_data[i] = {data[7:0], data[31:8]};
          endcase

          mem_req_svalid[i] = buffer_rvalid[i];
          mem_req_id[i] = buffer_rid[i];
          mem_req_last[i] = mem_operation_last[i];
          buffer_pop[i] = x_mem_ready_i[i] & x_mem_valid_o[i];

          // Create byte enable signal for memory request
          if (is_single_element_operation) begin
            automatic logic [$clog2(ELENB)-1:0] shift = is_strided ? mem_req_addr_offset[i] : mem_counter_value[i][$clog2(ELENB)-1:0] + spatz_req_q.rs1[1:0];
            automatic logic [MemDataWidthB-1:0]  mask = spatz_req_q.vtype.vsew == rvv_pkg::EW_8  ? MemDataWidthB'(1'b1) :
                                                        spatz_req_q.vtype.vsew == rvv_pkg::EW_16 ? MemDataWidthB'(2'b11) : MemDataWidthB'(4'b1111);
            mem_req_strb[i] = mask << shift;
          end else begin
            for (int unsigned k = 0; k < ELENB; k++) begin
              mem_req_strb[i][k +: 'd1] = k < mem_counter_delta[i];
            end
          end
        end else begin
          // Clear empty buffer id requests
          if (!buffer_empty[i]) begin
            buffer_pop[i] = 1'b1;
          end
        end
      end
    end
  end
  /* verilator lint_on LATCH */

  // Create memory requests
  for (genvar i = 0; i < NrMemPorts; i++) begin : gen_mem_req
    assign x_mem_req_o[i].id    = mem_req_id[i];
    assign x_mem_req_o[i].addr  = mem_req_addr[i];
    assign x_mem_req_o[i].mode  = '0; // Request always uses user privilege level
    assign x_mem_req_o[i].size  = spatz_req_q.vtype.vsew[1:0];
    assign x_mem_req_o[i].we    = ~is_load;
    assign x_mem_req_o[i].strb  = mem_req_strb[i];
    assign x_mem_req_o[i].wdata = mem_req_data[i];
    assign x_mem_req_o[i].last  = mem_req_last[i];
    assign x_mem_req_o[i].spec  = 1'b0; // Request is never speculative
    assign x_mem_valid_o[i]     = mem_req_svalid[i] | mem_req_lvalid[i];
  end

  ////////////////
  // Assertions //
  ////////////////

  if (MemDataWidth != ELEN)
    $error("[spatz_vlsu] The memory data width needs to be equal to %d.", ELEN);

  if (NrMemPorts > N_IPU)
    $error("[spatz_vlsu] The number of memory ports needs to be smaller or equal to the number of IPUs.");

  if (NrMemPorts != 2**$clog2(NrMemPorts))
    $error("[spatz_vlsu] The NrMemPorts parameter needs to be a power of two");

endmodule : spatz_vlsu
