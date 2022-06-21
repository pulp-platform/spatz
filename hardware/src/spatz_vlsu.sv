// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// The vector load/store unit is used to load vectors from memory
// and to the vector register file and store them back again.

module spatz_vlsu import spatz_pkg::*; import rvv_pkg::*; import cf_math_pkg::idx_width; #(
    parameter                NrMemPorts         = 1,
    parameter                NrOutstandingLoads = 8,
    parameter  type          x_mem_req_t        = logic,
    parameter  type          x_mem_resp_t       = logic,
    parameter  type          x_mem_result_t     = logic,
    // Dependant parameters. DO NOT CHANGE!
    localparam int  unsigned IdWidth            = idx_width(NrOutstandingLoads)
  ) (
    input  logic                           clk_i,
    input  logic                           rst_ni,
    // Spatz request
    input  spatz_req_t                     spatz_req_i,
    input  logic                           spatz_req_valid_i,
    output logic                           spatz_req_ready_o,
    // VLSU response
    output logic                           vlsu_rsp_valid_o,
    output vlsu_rsp_t                      vlsu_rsp_o,
    // Interface with the VRF
    output vreg_addr_t                     vrf_waddr_o,
    output vreg_data_t                     vrf_wdata_o,
    output logic                           vrf_we_o,
    output vreg_be_t                       vrf_wbe_o,
    input  logic                           vrf_wvalid_i,
    output vreg_addr_t                     vrf_raddr_o,
    output logic                           vrf_re_o,
    input  vreg_data_t                     vrf_rdata_i,
    input  logic                           vrf_rvalid_i,
    // X-Interface Memory Request
    output logic          [NrMemPorts-1:0] x_mem_valid_o,
    input  logic          [NrMemPorts-1:0] x_mem_ready_i,
    output x_mem_req_t    [NrMemPorts-1:0] x_mem_req_o,
    input  x_mem_resp_t   [NrMemPorts-1:0] x_mem_resp_i,
    // X-Interface Memory Result
    input  logic          [NrMemPorts-1:0] x_mem_result_valid_i,
    input  x_mem_result_t [NrMemPorts-1:0] x_mem_result_i,
    // X-Interface Memory Finished
    output logic                           x_mem_finished_o
  );

// Include FF
`include "common_cells/registers.svh"

  ////////////////
  // Parameters //
  ////////////////

  localparam int unsigned MemDataWidth  = $bits(x_mem_req_o[0].wdata);
  localparam int unsigned MemDataWidthB = MemDataWidth/8;

  //////////////
  // Typedefs //
  //////////////

  typedef logic [IdWidth-1:0] id_t;
  typedef logic [$clog2(NrWordsPerVector*8)-1:0] vreg_elem_t;

  /////////////
  // Signals //
  /////////////

  // Spatz request
  spatz_req_t spatz_req_d, spatz_req_q;
  `FF(spatz_req_q, spatz_req_d, '0)

  // Is the memory operation valid and are we at the last one?
  logic [NrMemPorts-1:0] mem_operation_valid;
  logic [NrMemPorts-1:0] mem_operation_last;
  logic [NrMemPorts-1:0] mem_operations_finished;

  // For each memory port we count how many elements we have already loaded/stored.
  // Multiple counters are needed all memory ports can work independent of each other.
  logic  [NrMemPorts-1:0] mem_counter_en;
  logic  [NrMemPorts-1:0] mem_counter_load;
  vlen_t [NrMemPorts-1:0] mem_counter_delta;
  vlen_t [NrMemPorts-1:0] mem_counter_load_value;
  vlen_t [NrMemPorts-1:0] mem_counter_value;

  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_mem_counters
    delta_counter #(
      .WIDTH($bits(vlen_t))
    ) i_delta_counter_mem (
      .clk_i     (clk_i                       ),
      .rst_ni    (rst_ni                      ),
      .clear_i   (1'b0                        ),
      .en_i      (mem_counter_en[port]        ),
      .load_i    (mem_counter_load[port]      ),
      .down_i    (1'b0                        ), // We always count up
      .delta_i   (mem_counter_delta[port]     ),
      .d_i       (mem_counter_load_value[port]),
      .q_o       (mem_counter_value[port]     ),
      .overflow_o(/* Unused */                )
    );
  end

  // Is the VRF operation valid and are we at the last one?
  logic [N_IPU-1:0] vreg_operation_valid;
  logic [N_IPU-1:0] vreg_operation_last;
  logic [N_IPU-1:0] vreg_operations_finished;

  // For each IPU that we have, count how many elements we have already loaded/stored.
  // Multiple counters are necessary for the case where not every signle IPU will
  // receive the same number of elements to work through.
  vlen_t [N_IPU-1:0] vreg_counter_max;
  logic  [N_IPU-1:0] vreg_counter_en;
  logic  [N_IPU-1:0] vreg_counter_load;
  vlen_t [N_IPU-1:0] vreg_counter_delta;
  vlen_t [N_IPU-1:0] vreg_counter_load_value;
  vlen_t [N_IPU-1:0] vreg_counter_value;

  for (genvar ipu = 0; ipu < N_IPU; ipu++) begin : gen_vreg_counters
    delta_counter #(
      .WIDTH($bits(vlen_t))
    ) i_delta_counter_vreg (
      .clk_i     (clk_i                       ),
      .rst_ni    (rst_ni                      ),
      .clear_i   (1'b0                        ),
      .en_i      (vreg_counter_en[ipu]        ),
      .load_i    (vreg_counter_load[ipu]      ),
      .down_i    (1'b0                        ), // We always count up
      .delta_i   (vreg_counter_delta[ipu]     ),
      .d_i       (vreg_counter_load_value[ipu]),
      .q_o       (vreg_counter_value[ipu]     ),
      .overflow_o(/* Unused */                )
    );
  end

  // Are we ready to accept a new instruction from the controller?
  logic vlsu_is_ready;
  assign vlsu_is_ready = (&vreg_operations_finished) & (&mem_operations_finished);

  // Did a new VLSU request arrive?
  logic new_vlsu_request;
  assign new_vlsu_request = spatz_req_valid_i && vlsu_is_ready && (spatz_req_i.ex_unit == LSU);

  // Is instruction a load?
  logic is_load;
  assign is_load = spatz_req_q.op_mem.is_load;

  // Is the vector length zero (no active instruction)
  logic is_vl_zero;
  assign is_vl_zero = spatz_req_q.vl == 'd0;

  // Do we start at the very fist element
  logic is_vstart_zero;
  assign is_vstart_zero = spatz_req_q.vstart == 'd0;

  // Is the memory address unaligned
  logic is_addr_unaligned;
  assign is_addr_unaligned = spatz_req_q.rs1[1:0] != 2'b00;

  // Do we have a strided memory access
  logic is_strided;
  assign is_strided = (spatz_req_q.op == VLSE) | (spatz_req_q.op == VSSE);

  // Do we have to access every single element on its own
  logic is_single_element_operation;
  assign is_single_element_operation = is_addr_unaligned || is_strided || !is_vstart_zero;

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

  // The reorder buffer decouples the memory side from the register file side.
  // All elements from one side to the other go through it.
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_buffer
    reorder_buffer #(
      .DataWidth  (ELEN              ),
      .NumWords   (NrOutstandingLoads),
      .FallThrough(1'b1              )
    ) i_reorder_buffer (
      .clk_i    (clk_i              ),
      .rst_ni   (rst_ni             ),
      .data_i   (buffer_wdata[port] ),
      .id_i     (buffer_wid[port]   ),
      .push_i   (buffer_push[port]  ),
      .data_o   (buffer_rdata[port] ),
      .valid_o  (buffer_rvalid[port]),
      .id_read_o(buffer_rid[port]   ),
      .pop_i    (buffer_pop[port]   ),
      .id_req_i (buffer_req_id[port]),
      .id_o     (buffer_id[port]    ),
      .full_o   (buffer_full[port]  ),
      .empty_o  (buffer_empty[port] )
    );
  end

  // Current element id and byte id that are being accessed at the register file
  vreg_elem_t                     vreg_elem_id;
  logic       [$clog2(ELENB)-1:0] vreg_byte_id;

  ///////////////////////
  //  Output Register  //
  ///////////////////////

  typedef struct packed {
    vreg_addr_t waddr;
    vreg_data_t wdata;
    vreg_be_t wbe;
  } vrf_req_t;

  vrf_req_t vrf_req_d, vrf_req_q;
  logic     vrf_req_valid_d, vrf_req_ready_d;
  logic     vrf_req_valid_q, vrf_req_ready_q;

  spill_register #(
    .T(vrf_req_t)
  ) i_vrf_req_register (
    .clk_i  (clk_i          ),
    .rst_ni (rst_ni         ),
    .data_i (vrf_req_d      ),
    .valid_i(vrf_req_valid_d),
    .ready_o(vrf_req_ready_d),
    .data_o (vrf_req_q      ),
    .valid_o(vrf_req_valid_q),
    .ready_i(vrf_req_ready_q)
  );

  assign vrf_waddr_o     = vrf_req_q.waddr;
  assign vrf_wdata_o     = vrf_req_q.wdata;
  assign vrf_wbe_o       = vrf_req_q.wbe;
  assign vrf_we_o        = vrf_req_valid_q;
  assign vrf_req_ready_q = vrf_wvalid_i;

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
        EW_8: begin
          spatz_req_d.vl     = spatz_req_i.vl;
          spatz_req_d.vstart = spatz_req_i.vstart;
        end
        EW_16: begin
          spatz_req_d.vl     = spatz_req_i.vl << 1;
          spatz_req_d.vstart = spatz_req_i.vstart << 1;
        end
        default: begin
          spatz_req_d.vl     = spatz_req_i.vl << 2;
          spatz_req_d.vstart = spatz_req_i.vstart << 2;
        end
      endcase
    end else if (!is_vl_zero && vlsu_is_ready && !new_vlsu_request &&
        !vrf_req_valid_d && (!vrf_req_valid_q || (vrf_req_valid_q && vrf_req_ready_q))) begin
      // If we are ready for a new instruction but there is none, clear the register
      spatz_req_d = '0;
    end
  end

  // Respond to controller if we are finished executing
  always_comb begin : vlsu_rsp
    vlsu_rsp_valid_o = 1'b0;
    vlsu_rsp_o       = '0;

    // Write back accessed register file vector to clear scoreboard entry
    if (vlsu_is_ready && !is_vl_zero && !vrf_req_valid_d && (!vrf_req_valid_q || (vrf_req_valid_q && vrf_req_ready_q))) begin
      vlsu_rsp_o.id    = spatz_req_q.id;
      vlsu_rsp_o.vd    = spatz_req_q.vd;
      vlsu_rsp_valid_o = 1'b1;
    end
  end

  // Signal when we are finished with with accessing the memory (necessary
  // for the case with more than one memory port)
  assign x_mem_finished_o = vlsu_is_ready && (!is_vl_zero || (new_vlsu_request && spatz_req_i.vl == 'd0)) && !vrf_req_valid_d;

  ////////////////////////
  // Address Generation //
  ////////////////////////

  elen_t [NrMemPorts-1:0]      mem_req_addr;
  logic  [NrMemPorts-1:0][1:0] mem_req_addr_offset;

  vreg_addr_t       vreg_addr;
  logic       [1:0] vreg_addr_offset;

  // Calculate the memory address for each memory port
  always_comb begin : gen_mem_req_addr
    for (int unsigned port = 0; port < NrMemPorts; port++) begin
      automatic logic [31:0] stride = is_strided ? spatz_req_q.rs2 >> spatz_req_q.vtype.vsew : 'd1;
      automatic logic [31:0] addr   = spatz_req_q.rs1 + ({mem_counter_value[port][$bits(vlen_t)-1:2] << $clog2(NrMemPorts), mem_counter_value[port][1:0]} + (port << 2)) * stride;

      mem_req_addr[port]        = {addr[31:2], 2'b00};
      mem_req_addr_offset[port] = addr[1:0];
    end
  end

  // Calculate the register file address
  always_comb begin : gen_vreg_addr
    vreg_addr        = (spatz_req_q.vd << $clog2(NrWordsPerVector)) + $unsigned(vreg_elem_id);
    vreg_addr_offset = vreg_byte_id * spatz_req_q.rs2 + spatz_req_q.rs1;
  end

  //////////////
  // Counters //
  //////////////

  // Do we need to catch up to reach element idx parity? (Because of non-zero vstart)
  vlen_t vreg_start_0;
  assign vreg_start_0 = ((spatz_req_q.vstart >> ($clog2(N_IPU*ELENB))) << $clog2(ELENB)) +
                        (spatz_req_q.vstart[idx_width(N_IPU*ELENB)-1:$clog2(ELENB)] > 'd0 ?
                            ELENB :
                            (spatz_req_q.vstart[idx_width(N_IPU*ELENB)-1:$clog2(ELENB)] == 'd0 ? spatz_req_q.vstart[$clog2(ELENB)-1:0] : 'd0));
  logic [N_IPU-1:0] catchup;
  for (genvar i = 0; i < N_IPU; i++) begin: gen_catchup
    assign catchup[i] = (vreg_counter_value[i] < vreg_start_0) & (vreg_counter_max[i] != vreg_counter_value[i]);
  end

  for (genvar ipu = 0; ipu < N_IPU; ipu++) begin: gen_vreg_counter_proc
    always_comb begin
      // The total amount of elements we have to work through
      automatic int unsigned max   = ((spatz_req_q.vl >> ($clog2(N_IPU*ELENB))) << $clog2(ELENB)) + (spatz_req_q.vl[idx_width(N_IPU*ELENB)-1:$clog2(ELENB)] > ipu ? ELENB : (spatz_req_q.vl[idx_width(N_IPU*ELENB)-1:$clog2(ELENB)] == ipu ? spatz_req_q.vl[$clog2(ELENB)-1:0] : 'd0));
      // How many elements are left to do
      automatic int unsigned delta = max - vreg_counter_value[ipu];

      vreg_counter_load[ipu]        = new_vlsu_request;
      vreg_counter_load_value[ipu]  = ((spatz_req_d.vstart >> ($clog2(N_IPU*ELENB))) << $clog2(ELENB)) + (spatz_req_d.vstart[idx_width(N_IPU*ELENB)-1:$clog2(ELENB)] > ipu ? ELENB : (spatz_req_d.vstart[idx_width(N_IPU*ELENB)-1:$clog2(ELENB)] == ipu ? spatz_req_d.vstart[$clog2(ELENB)-1:0] : 'd0));
      vreg_operation_valid[ipu]     = (delta != 'd0) & ~is_vl_zero & (catchup[ipu] | ~catchup[ipu] & ~|catchup);
      vreg_operation_last[ipu]      = vreg_operation_valid[ipu] && (delta <= (is_single_element_operation ? single_element_size : ELENB));
      vreg_counter_delta[ipu]       = !vreg_operation_valid[ipu] ? 'd0 : is_single_element_operation ? single_element_size : vreg_operation_last[ipu] ? delta : ELENB;
      vreg_counter_en[ipu]          = vreg_operation_valid[ipu] && (is_load && vrf_req_valid_d && vrf_req_ready_d) || (!is_load && vrf_rvalid_i && vrf_re_o);
      vreg_operations_finished[ipu] = (delta == 0 || is_vl_zero) || (vreg_operation_last[ipu] && vreg_counter_en[ipu]) && !vrf_req_valid_d && (!vrf_req_valid_q || (vrf_req_valid_q && vrf_req_ready_q));
      vreg_counter_max[ipu]         = max;
    end
  end

  assign vreg_elem_id = (vreg_counter_value[0] > vreg_start_0) ? vreg_counter_value[0] >> $clog2(ELENB)   : vreg_counter_value[N_IPU-1] >> $clog2(ELENB);
  assign vreg_byte_id = (vreg_counter_value[0] > vreg_start_0) ? vreg_counter_value[0][$clog2(ELENB)-1:0] : vreg_counter_value[N_IPU-1][$clog2(ELENB)-1:0];

  for (genvar port = 0; port < NrMemPorts; port++) begin: gen_mem_counter_proc
    always_comb begin
      // The total amount of elements we have to work through
      automatic int unsigned max   = NrMemPorts == 'd1 ? spatz_req_q.vl : ((spatz_req_q.vl >> ($clog2(NrMemPorts) + $clog2(MemDataWidthB))) << $clog2(MemDataWidthB)) + (spatz_req_q.vl[idx_width(NrMemPorts)+$clog2(MemDataWidthB)-1:$clog2(MemDataWidthB)] > port ? MemDataWidthB : spatz_req_q.vl[idx_width(NrMemPorts)+$clog2(MemDataWidthB)-1:$clog2(MemDataWidthB)] == port ? spatz_req_q.vl[$clog2(MemDataWidthB)-1:0] : 'd0);
      // How many elements are left to do
      automatic int unsigned delta = max - mem_counter_value[port];

      mem_operation_valid[port]     = (delta != 'd0) & ~is_vl_zero;
      mem_operation_last[port]      = mem_operation_valid[port] & (delta <= (is_single_element_operation ? single_element_size : 'd4));
      mem_counter_load[port]        = new_vlsu_request;
      mem_counter_load_value[port]  = NrMemPorts == 'd1 ? spatz_req_d.vstart : ((spatz_req_d.vstart >> ($clog2(NrMemPorts) + $clog2(MemDataWidthB))) << $clog2(MemDataWidthB)) + (spatz_req_d.vstart[idx_width(NrMemPorts)+$clog2(MemDataWidthB)-1:$clog2(MemDataWidthB)] > port ? MemDataWidthB : spatz_req_d.vstart[idx_width(NrMemPorts)+$clog2(MemDataWidthB)-1:$clog2(MemDataWidthB)] == port ? spatz_req_d.vstart[$clog2(MemDataWidthB)-1:0] : 'd0);
      mem_counter_delta[port]       = !mem_operation_valid[port] ? 'd0 : is_single_element_operation ? single_element_size : mem_operation_last[port] ? delta : 'd4;
      mem_counter_en[port]          = x_mem_ready_i[port] & x_mem_valid_o[port];
      mem_operations_finished[port] = ~mem_operation_valid[port]; // | (mem_operation_last[i] & mem_counter_en[i]);
    end
  end

  //////////////////////////
  // Memory/VRF Interface //
  //////////////////////////

  // Memory request signals
  id_t  [NrMemPorts-1:0]                   mem_req_id;
  logic [NrMemPorts-1:0][MemDataWidth-1:0] mem_req_data;
  logic [NrMemPorts-1:0]                   mem_req_svalid;
  logic [NrMemPorts-1:0][ELEN/8-1:0]       mem_req_strb;
  logic [NrMemPorts-1:0]                   mem_req_lvalid;
  logic [NrMemPorts-1:0]                   mem_req_last;

  // verilator lint_off LATCH
  always_comb begin
    vrf_raddr_o     = '0;
    vrf_re_o        = 1'b0;
    vrf_req_d       = '0;
    vrf_req_valid_d = 1'b0;

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
      // If we have a valid element in the buffer, store it back to the register file
      if (|vreg_operation_valid) begin
        // Enable write back to the VRF if we have a valid element in all buffers that still have to write something back.
        vrf_req_d.waddr = vreg_addr;
        vrf_req_valid_d = &buffer_rvalid || (&(buffer_rvalid | buffer_empty) && ~(|mem_operation_valid));

        for (int unsigned port = 0; port < NrMemPorts; port++) begin
          automatic elen_t data = buffer_rdata[port];

          // Shift data to correct position if we have an unaligned memory request
          unique case (is_strided ? vreg_addr_offset : spatz_req_q.rs1[1:0])
            2'b00: data = data;
            2'b01: data = {data[7:0], data[31:8]};
            2'b10: data = {data[15:0], data[31:16]};
            2'b11: data = {data[23:0], data[31:24]};
          endcase

          // Pop stored element and free space in buffer
          buffer_pop[port] = buffer_rvalid[port] & vrf_req_valid_d & vrf_req_ready_d & vreg_counter_en[port];

          // Shift data to correct position if we have a strided memory access
          if (is_strided) begin
            unique case (vreg_counter_value[port][1:0])
              2'b00: data = data;
              2'b01: data = {data[23:0], data[31:24]};
              2'b10: data = {data[15:0], data[31:16]};
              2'b11: data = {data[7:0], data[31:8]};
            endcase
          end
          vrf_req_d.wdata[ELEN*port +: ELEN] = data;

          // Create write byte enable mask for register file
          if (vreg_counter_en[port]) begin
            if (is_single_element_operation) begin
              automatic logic [$clog2(ELENB)-1:0] shift = vreg_counter_value[port][$clog2(ELENB)-1:0];
              automatic logic [ELENB-1:0] mask          = '0;
              unique case (spatz_req_q.vtype.vsew)
                rvv_pkg::EW_8 : mask = 4'b0001;
                rvv_pkg::EW_16: mask = 4'b0011;
                default:        mask = 4'b1111;
              endcase
              vrf_req_d.wbe[ELENB*port +: ELENB] = mask << shift;
            end else begin
              for (int unsigned k = 0; k < ELENB; k++)
                vrf_req_d.wbe[ELENB*port+k] = k < vreg_counter_delta[port];
            end
          end
        end
      end

      for (int unsigned port = 0; port < NrMemPorts; port++) begin
        // Write the load result to the buffer
        buffer_wdata[port] = x_mem_result_i[port].rdata;
        buffer_wid[port]   = x_mem_result_i[port].id;
        buffer_push[port]  = x_mem_result_valid_i[port];

        // Request a new id and and execute memory request
        if (!buffer_full[port] && mem_operation_valid[port]) begin
          buffer_req_id[port]  = x_mem_ready_i[port] & x_mem_valid_o[port];
          mem_req_lvalid[port] = 1'b1;
          mem_req_id[port]     = buffer_id[port];
          mem_req_last[port]   = mem_operation_last[port];
        end
      end
    // Store operation
    end else begin
      // Read new element from the register file and store
      // it to the buffer
      if (!(|buffer_full) && |vreg_operation_valid) begin
        vrf_raddr_o = vreg_addr;
        vrf_re_o    = 1'b1;

        // Push element to buffer if read from vregfile
        for (int unsigned port = 0; port < NrMemPorts; port++) begin
          if (vreg_counter_en[port])
            buffer_wdata[port] = vrf_rdata_i[ELEN*port +: ELEN];
          buffer_wid[port]    = buffer_id[port];
          buffer_req_id[port] = vrf_rvalid_i;
          buffer_push[port]   = buffer_req_id[port];
        end
      end

      for (int unsigned port = 0; port < NrMemPorts; port++) begin
        // Read element from buffer and execute memory request
        if (mem_operation_valid[port]) begin
          automatic logic [MemDataWidth-1:0] data = buffer_rdata[port];

          // Shift data to lsb if we have a strided memory access
          if (is_strided) begin
            unique case (mem_counter_value[port][1:0])
              2'b00: data = data;
              2'b01: data = {data[7:0], data[31:8]};
              2'b10: data = {data[15:0], data[31:16]};
              2'b11: data = {data[23:0], data[31:24]};
            endcase
          end

          // Shift data to correct position if we have an unaligned memory request
          unique case (is_strided ? mem_req_addr_offset[port] : spatz_req_q.rs1[1:0])
            2'b00: mem_req_data[port] = data;
            2'b01: mem_req_data[port] = {data[23:0], data[31:24]};
            2'b10: mem_req_data[port] = {data[15:0], data[31:16]};
            2'b11: mem_req_data[port] = {data[7:0], data[31:8]};
          endcase

          mem_req_svalid[port] = buffer_rvalid[port];
          mem_req_id[port]     = buffer_rid[port];
          mem_req_last[port]   = mem_operation_last[port];
          buffer_pop[port]     = x_mem_ready_i[port] & x_mem_valid_o[port];

          // Create byte enable signal for memory request
          if (is_single_element_operation) begin
            automatic logic [$clog2(ELENB)-1:0] shift = is_strided ? mem_req_addr_offset[port] : mem_counter_value[port][$clog2(ELENB)-1:0] + spatz_req_q.rs1[1:0];
            automatic logic [MemDataWidthB-1:0] mask  = '0;
            unique case (spatz_req_q.vtype.vsew)
              rvv_pkg::EW_8 : mask = 4'b0001;
              rvv_pkg::EW_16: mask = 4'b0011;
              default:        mask = 4'b1111;
            endcase

            mem_req_strb[port] = mask << shift;
          end else begin
            for (int unsigned k = 0; k < ELENB; k++)
              mem_req_strb[port][k +: 'd1] = k < mem_counter_delta[port];
          end
        end else begin
          // Clear empty buffer id requests
          if (!buffer_empty[port]) begin
            buffer_pop[port] = 1'b1;
          end
        end
      end
    end
  end
  // verilator lint_on LATCH

  // Create memory requests
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_mem_req
    assign x_mem_req_o[port].id    = mem_req_id[port];
    assign x_mem_req_o[port].addr  = mem_req_addr[port];
    assign x_mem_req_o[port].mode  = '0; // Request always uses user privilege level
    assign x_mem_req_o[port].size  = spatz_req_q.vtype.vsew[1:0];
    assign x_mem_req_o[port].we    = ~is_load;
    assign x_mem_req_o[port].strb  = mem_req_strb[port];
    assign x_mem_req_o[port].wdata = mem_req_data[port];
    assign x_mem_req_o[port].last  = mem_req_last[port];
    assign x_mem_req_o[port].spec  = 1'b0; // Request is never speculative
    assign x_mem_valid_o[port]     = mem_req_svalid[port] | mem_req_lvalid[port];
  end

  ////////////////
  // Assertions //
  ////////////////

  if (MemDataWidth != ELEN)
    $error("[spatz_vlsu] The memory data width needs to be equal to %d.", ELEN);

  if (NrMemPorts != N_IPU)
    $error("[spatz_vlsu] The number of memory ports needs to be equal to the number of IPUs.");

  if (NrMemPorts != 2**$clog2(NrMemPorts))
    $error("[spatz_vlsu] The NrMemPorts parameter needs to be a power of two");

endmodule : spatz_vlsu
