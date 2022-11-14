// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// The vector load/store unit is used to load vectors from memory
// and to the vector register file and store them back again.

module spatz_vlsu import spatz_pkg::*; import rvv_pkg::*; import cf_math_pkg::idx_width; #(
    parameter               NrMemPorts         = 1,
    parameter               NrOutstandingLoads = 4,
    // Dependant parameters. DO NOT CHANGE!
    localparam int unsigned IdWidth            = idx_width(NrOutstandingLoads)
  ) (
    input  logic                             clk_i,
    input  logic                             rst_ni,
    // Spatz request
    input  spatz_req_t                       spatz_req_i,
    input  logic                             spatz_req_valid_i,
    output logic                             spatz_req_ready_o,
    // VLSU response
    output logic                             vlsu_rsp_valid_o,
    output vlsu_rsp_t                        vlsu_rsp_o,
    // Interface with the VRF
    output vreg_addr_t                       vrf_waddr_o,
    output vreg_data_t                       vrf_wdata_o,
    output logic                             vrf_we_o,
    output vreg_be_t                         vrf_wbe_o,
    input  logic                             vrf_wvalid_i,
    output spatz_id_t       [1:0]            vrf_id_o,
    output vreg_addr_t                       vrf_raddr_o,
    output logic                             vrf_re_o,
    input  vreg_data_t                       vrf_rdata_i,
    input  logic                             vrf_rvalid_i,
    // Memory Request
    output spatz_mem_req_t  [NrMemPorts-1:0] spatz_mem_req_o,
    output logic            [NrMemPorts-1:0] spatz_mem_req_valid_o,
    input  logic            [NrMemPorts-1:0] spatz_mem_req_ready_i,
    //  Memory Response
    input  spatz_mem_resp_t [NrMemPorts-1:0] spatz_mem_resp_i,
    input  logic            [NrMemPorts-1:0] spatz_mem_resp_valid_i,
    output logic            [NrMemPorts-1:0] spatz_mem_resp_ready_o,
    // Memory Finished
    output logic                             spatz_mem_finished_o,
    output logic                             spatz_mem_str_finished_o
  );

// Include FF
`include "common_cells/registers.svh"


  ////////////////
  // Parameters //
  ////////////////

  localparam int unsigned MemDataWidth  = ELEN;
  localparam int unsigned MemDataWidthB = MemDataWidth/8;

  //////////////
  // Typedefs //
  //////////////

  typedef logic [IdWidth-1:0] id_t;
  typedef logic [$clog2(NrWordsPerVector*8)-1:0] vreg_elem_t;

  ///////////////////////
  //  Operation queue  //
  ///////////////////////

  spatz_req_t spatz_req_d;

  spatz_req_t spatz_req;
  logic       spatz_req_valid;
  logic       spatz_req_ready;

  spill_register #(
    .T(spatz_req_t)
  ) i_operation_queue (
    .clk_i  (clk_i                                          ),
    .rst_ni (rst_ni                                         ),
    .data_i (spatz_req_d                                    ),
    .valid_i(spatz_req_valid_i && spatz_req_i.ex_unit == LSU),
    .ready_o(spatz_req_ready_o                              ),
    .data_o (spatz_req                                      ),
    .valid_o(spatz_req_valid                                ),
    .ready_i(spatz_req_ready                                )
  );

  // Convert the vl to number of bytes for all element widths
  always_comb begin: proc_spatz_req
    spatz_req_d = spatz_req_i;

    unique case (spatz_req_i.vtype.vsew)
      EW_8: begin
        spatz_req_d.vl     = spatz_req_i.vl;
        spatz_req_d.vstart = spatz_req_i.vstart;
      end
      EW_16: begin
        spatz_req_d.vl     = spatz_req_i.vl << 1;
        spatz_req_d.vstart = spatz_req_i.vstart << 1;
      end
      EW_32: begin
        spatz_req_d.vl     = spatz_req_i.vl << 2;
        spatz_req_d.vstart = spatz_req_i.vstart << 2;
      end
      default: begin
        spatz_req_d.vl     = spatz_req_i.vl << MAXEW;
        spatz_req_d.vstart = spatz_req_i.vstart << MAXEW;
      end
    endcase
  end: proc_spatz_req

  //////////////////////
  //  Reorder Buffer  //
  //////////////////////

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
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_reorder_buffer
    reorder_buffer #(
      .DataWidth(ELEN              ),
      .NumWords (NrOutstandingLoads)
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
  end: gen_reorder_buffer

  ////////////////
  //  Counters  //
  ////////////////

  // Is the memory operation valid and are we at the last one?
  logic [NrMemPorts-1:0] mem_operation_valid;
  logic [NrMemPorts-1:0] mem_operation_last;

  // For each memory port we count how many elements we have already loaded/stored.
  // Multiple counters are needed all memory ports can work independent of each other.
  vlen_t [N_IPU-1:0]      mem_counter_max;
  logic  [NrMemPorts-1:0] mem_counter_en;
  logic  [NrMemPorts-1:0] mem_counter_load;
  vlen_t [NrMemPorts-1:0] mem_counter_delta;
  vlen_t [NrMemPorts-1:0] mem_counter_d;
  vlen_t [NrMemPorts-1:0] mem_counter_q;
  logic  [NrMemPorts-1:0] mem_finished_q;
  logic  [NrMemPorts-1:0] mem_finished_d;

  for (genvar port = 0; port < NrMemPorts; port++) begin: gen_mem_counters
    delta_counter #(
      .WIDTH($bits(vlen_t))
    ) i_delta_counter_mem (
      .clk_i     (clk_i                  ),
      .rst_ni    (rst_ni                 ),
      .clear_i   (1'b0                   ),
      .en_i      (mem_counter_en[port]   ),
      .load_i    (mem_counter_load[port] ),
      .down_i    (1'b0                   ), // We always count up
      .delta_i   (mem_counter_delta[port]),
      .d_i       (mem_counter_d[port]    ),
      .q_o       (mem_counter_q[port]    ),
      .overflow_o(/* Unused */           )
    );

    assign mem_finished_q[port] = spatz_req_valid && (mem_counter_q[port] == mem_counter_max[port]);
    assign mem_finished_d[port] = spatz_req_valid && ((mem_counter_q[port] + mem_counter_delta[port]) == mem_counter_max[port]);
  end: gen_mem_counters

  // For each IPU that we have, count how many elements we have already loaded/stored.
  // Multiple counters are necessary for the case where not every single IPU will
  // receive the same number of elements to work through.
  vlen_t [N_IPU-1:0]      vreg_counter_max;
  logic  [N_IPU-1:0]      vreg_counter_en;
  logic  [N_IPU-1:0]      vreg_counter_load;
  vlen_t [N_IPU-1:0]      vreg_counter_delta;
  vlen_t [N_IPU-1:0]      vreg_counter_d;
  vlen_t [N_IPU-1:0]      vreg_counter_q;
  logic  [NrMemPorts-1:0] vreg_finished_q;
  logic  [NrMemPorts-1:0] vreg_finished_d;

  for (genvar ipu = 0; ipu < N_IPU; ipu++) begin: gen_vreg_counters
    delta_counter #(
      .WIDTH($bits(vlen_t))
    ) i_delta_counter_vreg (
      .clk_i     (clk_i                  ),
      .rst_ni    (rst_ni                 ),
      .clear_i   (1'b0                   ),
      .en_i      (vreg_counter_en[ipu]   ),
      .load_i    (vreg_counter_load[ipu] ),
      .down_i    (1'b0                   ), // We always count up
      .delta_i   (vreg_counter_delta[ipu]),
      .d_i       (vreg_counter_d[ipu]    ),
      .q_o       (vreg_counter_q[ipu]    ),
      .overflow_o(/* Unused */           )
    );

    assign vreg_finished_q[ipu] = spatz_req_valid && (vreg_counter_q[ipu] == vreg_counter_max[ipu]);
    assign vreg_finished_d[ipu] = spatz_req_valid && ((vreg_counter_q[ipu] + vreg_counter_delta[ipu]) == vreg_counter_max[ipu]);
  end: gen_vreg_counters

  ////////////////////////
  // Address Generation //
  ////////////////////////

  elen_t [NrMemPorts-1:0]      mem_req_addr;
  logic  [NrMemPorts-1:0][1:0] mem_req_addr_offset;

  vreg_addr_t                   vreg_addr;
  logic       [int'(MAXEW)-1:0] vreg_addr_offset;

  // Current element index and byte index that are being accessed at the register file
  vreg_elem_t                     vreg_elem_id;
  logic       [$clog2(ELENB)-1:0] vreg_byte_id;

  // Do we have a strided memory access
  logic is_strided;
  assign is_strided = (spatz_req.op == VLSE) || (spatz_req.op == VSSE);

  // Calculate the memory address for each memory port
  always_comb begin : gen_mem_req_addr
    for (int unsigned port = 0; port < NrMemPorts; port++) begin
      automatic logic [31:0] stride = is_strided ? spatz_req.rs2 >> spatz_req.vtype.vsew : 'd1;
      automatic logic [31:0] addr   = spatz_req.rs1 + ({mem_counter_q[port][$bits(vlen_t)-1:MAXEW] << $clog2(NrMemPorts), mem_counter_q[port][int'(MAXEW)-1:0]} + (port << MAXEW)) * stride;

      mem_req_addr[port]        = (addr >> MAXEW) << MAXEW;
      mem_req_addr_offset[port] = addr[int'(MAXEW)-1:0];
    end
  end

  // Calculate the register file address
  always_comb begin : gen_vreg_addr
    vreg_addr        = (spatz_req.vd << $clog2(NrWordsPerVector)) + $unsigned(vreg_elem_id);
    vreg_addr_offset = (vreg_byte_id >> int'(spatz_req.vtype.vsew)) * spatz_req.rs2[31:0] + spatz_req.rs1[31:0];
  end

  ///////////////
  //  Control  //
  ///////////////

  // Are we busy?
  logic busy_q, busy_d;
  `FF(busy_q, busy_d, 1'b0)

  // Did we finish an instruction?
  logic vlsu_finished_req;

  // Memory requests
  spatz_mem_req_t [NrMemPorts-1:0] spatz_mem_req;
  logic           [NrMemPorts-1:0] spatz_mem_req_valid;
  logic           [NrMemPorts-1:0] spatz_mem_req_ready;

  always_comb begin: control_proc
    // Maintain state
    busy_d = busy_q;

    // We are handling an instruction
    spatz_req_ready = 1'b0;

    // Do not ack anything
    vlsu_finished_req = 1'b0;

    // Finished the execution!
    if (spatz_req_valid && &vreg_finished_q && &mem_finished_q) begin
      spatz_req_ready = spatz_req_valid;
      busy_d          = 1'b0;

      // Acknowledge response when the last load commits to the VRF, or when the store finishes
      vlsu_finished_req = 1'b1;
    end
    // Do we have a new instruction?
    else if (spatz_req_valid && !busy_d)
      busy_d = 1'b1;
  end: control_proc

  // Is the VRF operation valid and are we at the last one?
  logic [N_IPU-1:0] vreg_operation_valid;
  logic [N_IPU-1:0] vreg_operation_last;

  // Did a new VLSU request arrive?
  logic new_vlsu_request;
  assign new_vlsu_request = spatz_req_valid && spatz_req_ready && !busy_d;

  // Is instruction a load?
  logic is_load;
  assign is_load = spatz_req.op_mem.is_load;

  // Signal when we are finished with with accessing the memory (necessary
  // for the case with more than one memory port)
  assign spatz_mem_finished_o     = spatz_req_valid && &vreg_finished_q && &mem_finished_q;
  assign spatz_mem_str_finished_o = spatz_req_valid && &vreg_finished_q && &mem_finished_q && !is_load;

  // Do we start at the very fist element
  logic is_vstart_zero;
  assign is_vstart_zero = spatz_req.vstart == 'd0;

  // Is the memory address unaligned
  logic is_addr_unaligned;
  assign is_addr_unaligned = spatz_req.rs1[int'(MAXEW)-1:0] != '0;

  // Do we have to access every single element on its own
  logic is_single_element_operation;
  assign is_single_element_operation = is_addr_unaligned || is_strided || !is_vstart_zero;

  // How large is a single element (in bytes)
  logic [3:0] single_element_size;
  assign single_element_size = 1'b1 << spatz_req.vtype.vsew;

  ///////////////////////
  //  Output Register  //
  ///////////////////////

  typedef struct packed {
    vreg_addr_t waddr;
    vreg_data_t wdata;
    vreg_be_t wbe;

    vlsu_rsp_t rsp;
    logic rsp_valid;
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
  assign vrf_id_o        = {vrf_req_q.rsp.id, spatz_req.id};
  assign vrf_req_ready_q = vrf_wvalid_i;

  // Ack when the vector store finishes, or when the vector load commits to the VRF
  assign vlsu_rsp_o       = vrf_req_q.rsp_valid && vrf_req_valid_q ? vrf_req_q.rsp   : '{id: spatz_req.id, default: '0};
  assign vlsu_rsp_valid_o = vrf_req_q.rsp_valid && vrf_req_valid_q ? vrf_req_ready_q : vlsu_finished_req && !is_load;

  //////////////
  // Counters //
  //////////////

  // Do we need to catch up to reach element idx parity? (Because of non-zero vstart)
  vlen_t vreg_start_0;
  assign vreg_start_0 = ((spatz_req.vstart >> ($clog2(N_IPU*ELENB))) << $clog2(ELENB)) +
                        (spatz_req.vstart[idx_width(N_IPU*ELENB)-1:$clog2(ELENB)] > 'd0 ?
                            ELENB :
                            (spatz_req.vstart[idx_width(N_IPU*ELENB)-1:$clog2(ELENB)] == 'd0 ? spatz_req.vstart[$clog2(ELENB)-1:0] : 'd0));
  logic [N_IPU-1:0] catchup;
  for (genvar i = 0; i < N_IPU; i++) begin: gen_catchup
    assign catchup[i] = (vreg_counter_q[i] < vreg_start_0) & (vreg_counter_max[i] != vreg_counter_q[i]);
  end: gen_catchup

  for (genvar ipu = 0; ipu < N_IPU; ipu++) begin: gen_vreg_counter_proc
    // The total amount of elements we have to work through
    vlen_t max_elements;
    always_comb begin
      // Default value
      max_elements = (spatz_req.vl >> $clog2(N_IPU*ELENB)) << $clog2(ELENB);

      // Full transfer
      if (spatz_req.vl[$clog2(ELENB) +: $clog2(N_IPU)] > ipu)
        max_elements += ELENB;
      else if (spatz_req.vl[$clog2(N_IPU*ELENB)-1:$clog2(ELENB)] == ipu)
        max_elements += spatz_req.vl[$clog2(ELENB)-1:0];

      vreg_counter_load[ipu] = new_vlsu_request;
      vreg_counter_d[ipu]    = (spatz_req.vstart >> $clog2(N_IPU*ELENB)) << $clog2(ELENB);
      if (spatz_req.vstart[$clog2(N_IPU*ELENB)-1:$clog2(ELENB)] > ipu)
        vreg_counter_d[ipu] += ELENB;
      else if (spatz_req.vstart[idx_width(N_IPU*ELENB)-1:$clog2(ELENB)] == ipu)
        vreg_counter_d[ipu] += spatz_req.vstart[$clog2(ELENB)-1:0];
      vreg_operation_valid[ipu] = spatz_req_valid && (vreg_counter_q[ipu] != max_elements) && (catchup[ipu] || (!catchup[ipu] && ~|catchup));
      vreg_operation_last[ipu]  = vreg_operation_valid[ipu] && ((max_elements - vreg_counter_q[ipu]) <= (is_single_element_operation ? single_element_size : ELENB));
      vreg_counter_delta[ipu]   = !vreg_operation_valid[ipu] ? 'd0 : is_single_element_operation ? single_element_size : vreg_operation_last[ipu] ? (max_elements - vreg_counter_q[ipu]) : ELENB;
      vreg_counter_en[ipu]      = vreg_operation_valid[ipu] && (is_load && vrf_req_valid_d && vrf_req_ready_d) || (!is_load && vrf_rvalid_i && vrf_re_o);
      vreg_counter_max[ipu]     = max_elements;
    end
  end

  assign vreg_elem_id = (vreg_counter_q[0] > vreg_start_0) ? vreg_counter_q[0] >> $clog2(ELENB)   : vreg_counter_q[N_IPU-1] >> $clog2(ELENB);
  assign vreg_byte_id = (vreg_counter_q[0] > vreg_start_0) ? vreg_counter_q[0][$clog2(ELENB)-1:0] : vreg_counter_q[N_IPU-1][$clog2(ELENB)-1:0];

  for (genvar port = 0; port < NrMemPorts; port++) begin: gen_mem_counter_proc
    // The total amount of elements we have to work through
    vlen_t max_elements;

    always_comb begin
      // Default value
      max_elements = (spatz_req.vl >> $clog2(NrMemPorts*MemDataWidthB)) << $clog2(MemDataWidthB);

      if (NrMemPorts == 1)
        max_elements = spatz_req.vl;
      else
        if (spatz_req.vl[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] > port)
          max_elements += MemDataWidthB;
        else if (spatz_req.vl[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] == port)
          max_elements += spatz_req.vl[$clog2(MemDataWidthB)-1:0];

      mem_operation_valid[port] = spatz_req_valid && (max_elements != mem_counter_q[port]);
      mem_operation_last[port]  = mem_operation_valid[port] && ((max_elements - mem_counter_q[port]) <= (is_single_element_operation ? single_element_size : MemDataWidthB));
      mem_counter_load[port]    = new_vlsu_request;
      mem_counter_d[port]       = (spatz_req.vstart >> $clog2(NrMemPorts*MemDataWidthB)) << $clog2(MemDataWidthB);
      if (NrMemPorts == 1)
        mem_counter_d[port] = spatz_req.vstart;
      else
        if (spatz_req.vstart[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] > port)
          mem_counter_d[port] += MemDataWidthB;
        else if (spatz_req.vstart[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] == port)
          mem_counter_d[port] += spatz_req.vstart[$clog2(MemDataWidthB)-1:0];
      mem_counter_delta[port] = !mem_operation_valid[port] ? 'd0 : is_single_element_operation ? single_element_size : mem_operation_last[port] ? (max_elements - mem_counter_q[port]) : MemDataWidthB;
      mem_counter_en[port]    = spatz_mem_req_ready_i[port] && spatz_mem_req_valid_o[port];
      mem_counter_max[port]   = max_elements;
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

  // Number of pending requests
  id_t  [NrMemPorts-1:0]                   mem_pending_d, mem_pending_q;
  logic [NrMemPorts-1:0]                   mem_pending;
  `FF(mem_pending_q, mem_pending_d, '{default: '0})
  always_comb begin
    // Maintain state
    mem_pending_d  = mem_pending_q;

    for (int port = 0; port < NrMemPorts; port++) begin
      mem_pending[port] = mem_pending_q[port] != '0;

      // New request sent
      if (is_load && spatz_mem_req_valid[port] && spatz_mem_req_ready[port])
        mem_pending_d[port]++;

      // Response used
      if (buffer_rvalid[port] && buffer_pop[port] && mem_pending_q[port] != '0)
        mem_pending_d[port]--;
    end
  end

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

    // Always ready
    spatz_mem_resp_ready_o = '1;

    // Propagate request ID
    vrf_req_d.rsp.id    = spatz_req.id;
    vrf_req_d.rsp_valid = spatz_req_valid && &vreg_finished_d && &mem_finished_d;

    if (is_load) begin
      // If we have a valid element in the buffer, store it back to the register file
      if (|vreg_operation_valid) begin
        // Enable write back to the VRF if we have a valid element in all buffers that still have to write something back.
        vrf_req_d.waddr = vreg_addr;
        vrf_req_valid_d = &(buffer_rvalid | ~mem_pending) && |mem_pending;

        for (int unsigned port = 0; port < NrMemPorts; port++) begin
          automatic elen_t data = buffer_rdata[port];

          // Shift data to correct position if we have an unaligned memory request
          if (MAXEW == EW_32)
            unique case (is_strided ? vreg_addr_offset : spatz_req.rs1[1:0])
              2'b00: data = data;
              2'b01: data = {data[7:0], data[31:8]};
              2'b10: data = {data[15:0], data[31:16]};
              2'b11: data = {data[23:0], data[31:24]};
            endcase
          else
            unique case (is_strided ? vreg_addr_offset : spatz_req.rs1[2:0])
              3'b000: data = data;
              3'b001: data = {data[7:0], data[63:8]};
              3'b010: data = {data[15:0], data[63:16]};
              3'b011: data = {data[23:0], data[63:24]};
              3'b100: data = {data[31:0], data[63:32]};
              3'b101: data = {data[39:0], data[63:40]};
              3'b110: data = {data[47:0], data[63:48]};
              3'b111: data = {data[55:0], data[63:56]};
            endcase

          // Pop stored element and free space in buffer
          buffer_pop[port] = buffer_rvalid[port] && vrf_req_valid_d && vrf_req_ready_d && vreg_counter_en[port];

          // Shift data to correct position if we have a strided memory access
          if (is_strided)
            if (MAXEW == EW_32)
              unique case (vreg_counter_q[port][1:0])
                2'b00: data = data;
                2'b01: data = {data[23:0], data[31:24]};
                2'b10: data = {data[15:0], data[31:16]};
                2'b11: data = {data[7:0], data[31:8]};
              endcase
            else
              unique case (vreg_counter_q[port][2:0])
                3'b000: data = data;
                3'b001: data = {data[55:0], data[63:56]};
                3'b010: data = {data[47:0], data[63:48]};
                3'b011: data = {data[39:0], data[63:40]};
                3'b100: data = {data[31:0], data[63:32]};
                3'b101: data = {data[23:0], data[63:24]};
                3'b110: data = {data[15:0], data[63:16]};
                3'b111: data = {data[7:0], data[63:8]};
              endcase
          vrf_req_d.wdata[ELEN*port +: ELEN] = data;

          // Create write byte enable mask for register file
          if (vreg_counter_en[port])
            if (is_single_element_operation) begin
              automatic logic [$clog2(ELENB)-1:0] shift = vreg_counter_q[port][$clog2(ELENB)-1:0];
              automatic logic [ELENB-1:0] mask          = '1;
              case (spatz_req.vtype.vsew)
                EW_8 : mask = 1;
                EW_16: mask = 3;
                EW_32: mask = 15;
              endcase
              vrf_req_d.wbe[ELENB*port +: ELENB] = mask << shift;
            end else
              for (int unsigned k = 0; k < ELENB; k++)
                vrf_req_d.wbe[ELENB*port+k] = k < vreg_counter_delta[port];
        end
      end

      for (int unsigned port = 0; port < NrMemPorts; port++) begin
        // Write the load result to the buffer
        buffer_wdata[port] = spatz_mem_resp_i[port].rdata;
        buffer_wid[port]   = spatz_mem_resp_i[port].id;
        buffer_push[port]  = spatz_mem_resp_valid_i[port];

        // Request a new id and and execute memory request
        if (!buffer_full[port] && mem_operation_valid[port]) begin
          buffer_req_id[port]  = spatz_mem_req_ready[port] & spatz_mem_req_valid[port];
          mem_req_lvalid[port] = 1'b1;
          mem_req_id[port]     = buffer_id[port];
          mem_req_last[port]   = mem_operation_last[port];
        end
      end
    // Store operation
    end else begin
      // Read new element from the register file and store it to the buffer
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
          if (is_strided)
            if (MAXEW == EW_32)
              unique case (mem_counter_q[port][1:0])
                2'b01: data = {data[7:0], data[31:8]};
                2'b10: data = {data[15:0], data[31:16]};
                2'b11: data = {data[23:0], data[31:24]};
                default:; // Do nothing
              endcase
            else
              unique case (vreg_counter_q[port][2:0])
                3'b001: data = {data[7:0], data[63:8]};
                3'b010: data = {data[15:0], data[63:16]};
                3'b011: data = {data[23:0], data[63:24]};
                3'b100: data = {data[31:0], data[63:32]};
                3'b101: data = {data[39:0], data[63:40]};
                3'b110: data = {data[47:0], data[63:48]};
                3'b111: data = {data[55:0], data[63:56]};
                default:; // Do nothing
              endcase

          // Shift data to correct position if we have an unaligned memory request
          if (MAXEW == EW_32)
            unique case (is_strided ? mem_req_addr_offset[port] : spatz_req.rs1[1:0])
              2'b00: mem_req_data[port] = data;
              2'b01: mem_req_data[port] = {data[23:0], data[31:24]};
              2'b10: mem_req_data[port] = {data[15:0], data[31:16]};
              2'b11: mem_req_data[port] = {data[7:0], data[31:8]};
            endcase
          else
            unique case (is_strided ? vreg_addr_offset : spatz_req.rs1[2:0])
              3'b000: mem_req_data[port] = data;
              3'b001: mem_req_data[port] = {data[55:0], data[63:56]};
              3'b010: mem_req_data[port] = {data[47:0], data[63:48]};
              3'b011: mem_req_data[port] = {data[39:0], data[63:40]};
              3'b100: mem_req_data[port] = {data[31:0], data[63:32]};
              3'b101: mem_req_data[port] = {data[23:0], data[63:24]};
              3'b110: mem_req_data[port] = {data[15:0], data[63:16]};
              3'b111: mem_req_data[port] = {data[7:0], data[63:8]};
            endcase

          mem_req_svalid[port] = buffer_rvalid[port];
          mem_req_id[port]     = buffer_rid[port];
          mem_req_last[port]   = mem_operation_last[port];
          buffer_pop[port]     = spatz_mem_req_ready[port] && spatz_mem_req_valid[port];

          // Create byte enable signal for memory request
          if (is_single_element_operation) begin
            automatic logic [$clog2(ELENB)-1:0] shift = is_strided ? mem_req_addr_offset[port] : mem_counter_q[port][$clog2(ELENB)-1:0] + spatz_req.rs1[int'(MAXEW)-1:0];
            automatic logic [MemDataWidthB-1:0] mask  = '1;
            case (spatz_req.vtype.vsew)
              EW_8 : mask = 1;
              EW_16: mask = 3;
              EW_32: mask = 15;
            endcase
            mem_req_strb[port] = mask << shift;
          end else
            for (int unsigned k = 0; k < ELENB; k++)
              mem_req_strb[port][k] = k < mem_counter_delta[port];
        end else begin
          // Clear empty buffer id requests
          if (!buffer_empty[port])
            buffer_pop[port] = 1'b1;
        end
      end
    end
  end
  // verilator lint_on LATCH

  // Create memory requests
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_mem_req
    stream_register #(
      .T(spatz_mem_req_t)
    ) i_spatz_mem_req_register (
      .clk_i     (clk_i                      ),
      .rst_ni    (rst_ni                     ),
      .clr_i     (1'b0                       ),
      .testmode_i(1'b0                       ),
      .data_i    (spatz_mem_req[port]        ),
      .valid_i   (spatz_mem_req_valid[port]  ),
      .ready_o   (spatz_mem_req_ready[port]  ),
      .data_o    (spatz_mem_req_o[port]      ),
      .valid_o   (spatz_mem_req_valid_o[port]),
      .ready_i   (spatz_mem_req_ready_i[port])
    );

    assign spatz_mem_req[port].id    = mem_req_id[port];
    assign spatz_mem_req[port].addr  = mem_req_addr[port];
    assign spatz_mem_req[port].mode  = '0; // Request always uses user privilege level
    assign spatz_mem_req[port].size  = spatz_req.vtype.vsew[1:0];
    assign spatz_mem_req[port].we    = !is_load;
    assign spatz_mem_req[port].strb  = mem_req_strb[port];
    assign spatz_mem_req[port].wdata = mem_req_data[port];
    assign spatz_mem_req[port].last  = mem_req_last[port];
    assign spatz_mem_req[port].spec  = 1'b0; // Request is never speculative
    assign spatz_mem_req_valid[port] = mem_req_svalid[port] || mem_req_lvalid[port];
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
