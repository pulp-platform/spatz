// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich
//
// The vector load/store unit is used to load vectors from memory
// and to the vector register file and store them back again.

module spatz_vlsu
  import spatz_pkg::*;
  import rvv_pkg::*;
  import cf_math_pkg::idx_width; #(
    parameter int unsigned   NrMemPorts         = 1,
    parameter int unsigned   NrOutstandingLoads = 8,
    // Memory request
    parameter  type          spatz_mem_req_t    = logic,
    parameter  type          spatz_mem_rsp_t    = logic,
    // Dependant parameters. DO NOT CHANGE!
    localparam int  unsigned IdWidth            = idx_width(NrOutstandingLoads)
  ) (
    input  logic                            clk_i,
    input  logic                            rst_ni,
    // Spatz request
    input  spatz_req_t                      spatz_req_i,
    input  logic                            spatz_req_valid_i,
    output logic                            spatz_req_ready_o,
    // VLSU response
    output logic                            vlsu_rsp_valid_o,
    output vlsu_rsp_t                       vlsu_rsp_o,
    // Interface with the VRF
    output vrf_addr_t                       vrf_waddr_o,
    output vrf_data_t                       vrf_wdata_o,
    output logic                            vrf_we_o,
    output vrf_be_t                         vrf_wbe_o,
    input  logic                            vrf_wvalid_i,
    output spatz_id_t      [2:0]            vrf_id_o,
    output vrf_addr_t      [1:0]            vrf_raddr_o,
    output logic           [1:0]            vrf_re_o,
    input  vrf_data_t      [1:0]            vrf_rdata_i,
    input  logic           [1:0]            vrf_rvalid_i,
    // Memory Request
    output spatz_mem_req_t [NrMemPorts-1:0] spatz_mem_req_o,
    output logic           [NrMemPorts-1:0] spatz_mem_req_valid_o,
    input  logic           [NrMemPorts-1:0] spatz_mem_req_ready_i,
    //  Memory Response
    input  spatz_mem_rsp_t [NrMemPorts-1:0] spatz_mem_rsp_i,
    input  logic           [NrMemPorts-1:0] spatz_mem_rsp_valid_i,
    // Memory Finished
    output logic                            spatz_mem_finished_o,
    output logic                            spatz_mem_str_finished_o
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

  spatz_req_t mem_spatz_req;
  logic       mem_spatz_req_valid;
  logic       mem_spatz_req_ready;

  spill_register #(
    .T(spatz_req_t)
  ) i_operation_queue (
    .clk_i  (clk_i                                          ),
    .rst_ni (rst_ni                                         ),
    .data_i (spatz_req_d                                    ),
    .valid_i(spatz_req_valid_i && spatz_req_i.ex_unit == LSU),
    .ready_o(spatz_req_ready_o                              ),
    .data_o (mem_spatz_req                                  ),
    .valid_o(mem_spatz_req_valid                            ),
    .ready_i(mem_spatz_req_ready                            )
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

  // Do we have a strided memory access
  logic mem_is_strided;
  assign mem_is_strided = (mem_spatz_req.op == VLSE) || (mem_spatz_req.op == VSSE);

  // Do we have an indexed memory access
  logic mem_is_indexed;
  assign mem_is_indexed = (mem_spatz_req.op == VLXE) || (mem_spatz_req.op == VSXE);

  /////////////
  //  State  //
  /////////////

  typedef enum logic {
    VLSU_RunningLoad, VLSU_RunningStore
  } state_t;
  state_t state_d, state_q;
  `FF(state_q, state_d, VLSU_RunningLoad)


  id_t [NrMemPorts-1:0] store_count_q;
  id_t [NrMemPorts-1:0] store_count_d;

  for (genvar port = 0; port < NrMemPorts; port++) begin: gen_store_count_q
    `FF(store_count_q[port], store_count_d[port], '0)
  end: gen_store_count_q

  always_comb begin: proc_store_count
    // Maintain state
    store_count_d = store_count_q;

    for (int port = 0; port < NrMemPorts; port++) begin
      if (spatz_mem_req_o[port].write && spatz_mem_req_valid_o[port] && spatz_mem_req_ready_i[port])
        // Did we send a store?
        store_count_d[port]++;

      // Did we get the ack of a store?
  `ifdef MEMPOOL_SPATZ
      if (store_count_q[port] != '0 && spatz_mem_rsp_valid_i[port] && spatz_mem_rsp_i[port].write)
        store_count_d[port]--;
  `else
      if (store_count_q[port] != '0 && spatz_mem_rsp_valid_i[port])
        store_count_d[port]--;
  `endif
    end
  end: proc_store_count

  //////////////////////
  //  Reorder Buffer  //
  //////////////////////

  typedef logic [int'(MAXEW)-1:0] addr_offset_t;

  elen_t [NrMemPorts-1:0] rob_wdata;
  id_t   [NrMemPorts-1:0] rob_wid;
  logic  [NrMemPorts-1:0] rob_push;
  logic  [NrMemPorts-1:0] rob_rvalid;
  elen_t [NrMemPorts-1:0] rob_rdata;
  logic  [NrMemPorts-1:0] rob_pop;
  id_t   [NrMemPorts-1:0] rob_rid;
  logic  [NrMemPorts-1:0] rob_req_id;
  id_t   [NrMemPorts-1:0] rob_id;
  logic  [NrMemPorts-1:0] rob_full;
  logic  [NrMemPorts-1:0] rob_empty;

  // The reorder buffer decouples the memory side from the register file side.
  // All elements from one side to the other go through it.
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_rob
`ifdef MEMPOOL_SPATZ
    reorder_buffer #(
      .DataWidth(ELEN              ),
      .NumWords (NrOutstandingLoads)
    ) i_reorder_buffer (
      .clk_i    (clk_i           ),
      .rst_ni   (rst_ni          ),
      .data_i   (rob_wdata[port] ),
      .id_i     (rob_wid[port]   ),
      .push_i   (rob_push[port]  ),
      .data_o   (rob_rdata[port] ),
      .valid_o  (rob_rvalid[port]),
      .id_read_o(rob_rid[port]   ),
      .pop_i    (rob_pop[port]   ),
      .id_req_i (rob_req_id[port]),
      .id_o     (rob_id[port]    ),
      .full_o   (rob_full[port]  ),
      .empty_o  (rob_empty[port] )
    );
`else
    fifo_v3 #(
      .DATA_WIDTH(ELEN              ),
      .DEPTH     (NrOutstandingLoads)
    ) i_reorder_buffer (
      .clk_i     (clk_i           ),
      .rst_ni    (rst_ni          ),
      .flush_i   (1'b0            ),
      .testmode_i(1'b0            ),
      .data_i    (rob_wdata[port] ),
      .push_i    (rob_push[port]  ),
      .data_o    (rob_rdata[port] ),
      .pop_i     (rob_pop[port]   ),
      .full_o    (rob_full[port]  ),
      .empty_o   (rob_empty[port] ),
      .usage_o   (/* Unused */    )
    );
    assign rob_rvalid[port] = !rob_empty[port];
`endif
  end: gen_rob

  //////////////////////
  //  Memory request  //
  //////////////////////

  // Is the memory operation valid and are we at the last one?
  logic [NrMemPorts-1:0] mem_operation_valid;
  logic [NrMemPorts-1:0] mem_operation_last;

  // For each memory port we count how many elements we have already loaded/stored.
  // Multiple counters are needed all memory ports can work independent of each other.
  vlen_t [N_FU-1:0]       mem_counter_max;
  logic  [NrMemPorts-1:0] mem_counter_en;
  logic  [NrMemPorts-1:0] mem_counter_load;
  vlen_t [NrMemPorts-1:0] mem_counter_delta;
  vlen_t [NrMemPorts-1:0] mem_counter_d;
  vlen_t [NrMemPorts-1:0] mem_counter_q;
  logic  [NrMemPorts-1:0] mem_port_finished_q;

  vlen_t [NrMemPorts-1:0] mem_idx_counter_delta;
  vlen_t [NrMemPorts-1:0] mem_idx_counter_d;
  vlen_t [NrMemPorts-1:0] mem_idx_counter_q;

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

    delta_counter #(
      .WIDTH($bits(vlen_t))
    ) i_delta_counter_mem_idx (
      .clk_i     (clk_i                      ),
      .rst_ni    (rst_ni                     ),
      .clear_i   (1'b0                       ),
      .en_i      (mem_counter_en[port]       ),
      .load_i    (mem_counter_load[port]     ),
      .down_i    (1'b0                       ), // We always count up
      .delta_i   (mem_idx_counter_delta[port]),
      .d_i       (mem_idx_counter_d[port]    ),
      .q_o       (mem_idx_counter_q[port]    ),
      .overflow_o(/* Unused */               )
    );

    assign mem_port_finished_q[port] = mem_spatz_req_valid && (mem_counter_q[port] == mem_counter_max[port]);
  end: gen_mem_counters

  // Did the current instruction finished the memory requests?
  logic [NrParallelInstructions-1:0] mem_insn_finished_q, mem_insn_finished_d;
  `FF(mem_insn_finished_q, mem_insn_finished_d, '0)

  // Is the current instruction pending?
  logic [NrParallelInstructions-1:0] mem_insn_pending_q, mem_insn_pending_d;
  `FF(mem_insn_pending_q, mem_insn_pending_d, '0)

  ///////////////////
  //  VRF request  //
  ///////////////////

  typedef struct packed {
    spatz_id_t id;

    vreg_t vd;
    vew_e vsew;

    vlen_t vl;
    vlen_t vstart;
    logic [2:0] rs1;

    logic is_load;
    logic is_strided;
    logic is_indexed;
  } commit_metadata_t;

  commit_metadata_t commit_insn_d;
  logic             commit_insn_push;
  commit_metadata_t commit_insn_q;
  logic             commit_insn_pop;
  logic             commit_insn_empty;
  logic             commit_insn_valid;

  fifo_v3 #(
    .DEPTH       (NrParallelInstructions),
    .FALL_THROUGH(1'b1                  ),
    .dtype       (commit_metadata_t     )
  ) i_fifo_commit_insn (
    .clk_i     (clk_i            ),
    .rst_ni    (rst_ni           ),
    .flush_i   (1'b0             ),
    .testmode_i(1'b0             ),
    .data_i    (commit_insn_d    ),
    .push_i    (commit_insn_push ),
    .full_o    (/* Unused */     ),
    .data_o    (commit_insn_q    ),
    .empty_o   (commit_insn_empty),
    .pop_i     (commit_insn_pop  ),
    .usage_o   (/* Unused */     )
  );

  assign commit_insn_valid = !commit_insn_empty;
  assign commit_insn_d     = '{
      id        : mem_spatz_req.id,
      vd        : mem_spatz_req.vd,
      vsew      : mem_spatz_req.vtype.vsew,
      vl        : mem_spatz_req.vl,
      vstart    : mem_spatz_req.vstart,
      rs1       : mem_spatz_req.rs1[2:0],
      is_load   : mem_spatz_req.op_mem.is_load,
      is_strided: mem_is_strided,
      is_indexed: mem_is_indexed
  };

  always_comb begin: queue_control
    // Maintain state
    mem_insn_finished_d = mem_insn_finished_q;
    mem_insn_pending_d  = mem_insn_pending_q;

    // Do not ack anything
    mem_spatz_req_ready = 1'b0;

    // Do not push anything to the metadata queue
    commit_insn_push = 1'b0;

    // Did we start a new instruction?
    if (mem_spatz_req_valid && !mem_insn_pending_q[mem_spatz_req.id]) begin
      mem_insn_pending_d[mem_spatz_req.id] = 1'b1;
      commit_insn_push                     = 1'b1;
    end

    // Did an instruction finished its requests?
    if (&mem_port_finished_q) begin
      mem_insn_finished_d[mem_spatz_req.id] = 1'b1;
      mem_spatz_req_ready                   = 1'b1;
    end
    // Did we acknowledge the end of an instruction?
    if (vlsu_rsp_valid_o) begin
      mem_insn_finished_d[vlsu_rsp_o.id] = 1'b0;
      mem_insn_pending_d[vlsu_rsp_o.id]  = 1'b0;
    end
  end

  // For each FU that we have, count how many elements we have already loaded/stored.
  // Multiple counters are necessary for the case where not every single FU will
  // receive the same number of elements to work through.
  vlen_t [N_FU-1:0]       commit_counter_max;
  logic  [N_FU-1:0]       commit_counter_en;
  logic  [N_FU-1:0]       commit_counter_load;
  vlen_t [N_FU-1:0]       commit_counter_delta;
  vlen_t [N_FU-1:0]       commit_counter_d;
  vlen_t [N_FU-1:0]       commit_counter_q;
  logic  [NrMemPorts-1:0] commit_finished_q;
  logic  [NrMemPorts-1:0] commit_finished_d;

  for (genvar fu = 0; fu < N_FU; fu++) begin: gen_vreg_counters
    delta_counter #(
      .WIDTH($bits(vlen_t))
    ) i_delta_counter_vreg (
      .clk_i     (clk_i                   ),
      .rst_ni    (rst_ni                  ),
      .clear_i   (1'b0                    ),
      .en_i      (commit_counter_en[fu]   ),
      .load_i    (commit_counter_load[fu] ),
      .down_i    (1'b0                    ), // We always count up
      .delta_i   (commit_counter_delta[fu]),
      .d_i       (commit_counter_d[fu]    ),
      .q_o       (commit_counter_q[fu]    ),
      .overflow_o(/* Unused */            )
    );

    assign commit_finished_q[fu] = commit_insn_valid && (commit_counter_q[fu] == commit_counter_max[fu]);
    assign commit_finished_d[fu] = commit_insn_valid && ((commit_counter_q[fu] + commit_counter_delta[fu]) == commit_counter_max[fu]);
  end: gen_vreg_counters

  ////////////////////////
  // Address Generation //
  ////////////////////////

  elen_t [NrMemPorts-1:0] mem_req_addr;

  vrf_addr_t vd_vreg_addr;
  vrf_addr_t vs2_vreg_addr;

  // Current element index and byte index that are being accessed at the register file
  vreg_elem_t vd_elem_id;
  vreg_elem_t vs2_elem_id_d, vs2_elem_id_q;
  `FF(vs2_elem_id_q, vs2_elem_id_d, '0)

  // Pending indexes
  logic [NrMemPorts-1:0] pending_index;

  // Calculate the memory address for each memory port
  addr_offset_t [NrMemPorts-1:0] mem_req_addr_offset;
  for (genvar port = 0; port < NrMemPorts; port++) begin: gen_mem_req_addr
    logic [31:0] addr;
    logic [31:0] stride;
    logic [31:0] offset;

    // Pre-shuffling index offset
    typedef logic [int'(MAXEW)-1:0] maxew_t;
    maxew_t idx_offset;
    assign idx_offset = mem_idx_counter_q[port];

    always_comb begin
      stride = mem_is_strided ? mem_spatz_req.rs2 >> mem_spatz_req.vtype.vsew : 'd1;

      if (mem_is_indexed) begin
        // What is the relationship between data and index width?
        automatic logic [1:0] data_index_width_diff = int'(mem_spatz_req.vtype.vsew) - int'(mem_spatz_req.op_mem.ew);

        // Pointer to index
        automatic logic [idx_width(N_FU*ELENB)-1:0] word_index = (port << (MAXEW - data_index_width_diff)) + (maxew_t'(idx_offset << data_index_width_diff) >> data_index_width_diff) + (maxew_t'(idx_offset >> (MAXEW - data_index_width_diff)) << (MAXEW - data_index_width_diff)) * NrMemPorts;

        // Index
        unique case (mem_spatz_req.op_mem.ew)
          EW_8 : offset   = $signed(vrf_rdata_i[1][8 * word_index +: 8]);
          EW_16: offset   = $signed(vrf_rdata_i[1][8 * word_index +: 16]);
          default: offset = $signed(vrf_rdata_i[1][8 * word_index +: 32]);
        endcase
      end else begin
        offset = ({mem_counter_q[port][$bits(vlen_t)-1:MAXEW] << $clog2(NrMemPorts), mem_counter_q[port][int'(MAXEW)-1:0]} + (port << MAXEW)) * stride;
      end

      addr                      = mem_spatz_req.rs1 + offset;
      mem_req_addr[port]        = (addr >> MAXEW) << MAXEW;
      mem_req_addr_offset[port] = addr[int'(MAXEW)-1:0];

      pending_index[port] = (mem_idx_counter_q[port][$clog2(NrWordsPerVector*ELENB)-1:0] >> MAXEW) != vs2_vreg_addr[$clog2(NrWordsPerVector)-1:0];
    end
  end: gen_mem_req_addr

  // Calculate the register file address
  always_comb begin : gen_vreg_addr
    vd_vreg_addr  = (commit_insn_q.vd << $clog2(NrWordsPerVector)) + $unsigned(vd_elem_id);
    vs2_vreg_addr = (mem_spatz_req.vs2 << $clog2(NrWordsPerVector)) + $unsigned(vs2_elem_id_q);
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

    // Do not pop anything
    commit_insn_pop = 1'b0;

    // Do not ack anything
    vlsu_finished_req = 1'b0;

    // Finished the execution!
    if (commit_insn_valid && &commit_finished_q && mem_insn_finished_q[commit_insn_q.id]) begin
      commit_insn_pop = 1'b1;
      busy_d          = 1'b0;

      // Acknowledge response when the last load commits to the VRF, or when the store finishes
      vlsu_finished_req = 1'b1;
    end
    // Do we have a new instruction?
    else if (commit_insn_valid && !busy_d)
      busy_d = 1'b1;
  end: control_proc

  // Is the VRF operation valid and are we at the last one?
  logic [N_FU-1:0] commit_operation_valid;
  logic [N_FU-1:0] commit_operation_last;

  // Is instruction a load?
  logic mem_is_load;
  assign mem_is_load = mem_spatz_req.op_mem.is_load;

  // Signal when we are finished with with accessing the memory (necessary
  // for the case with more than one memory port)
  assign spatz_mem_finished_o     = commit_insn_valid && &commit_finished_q && mem_insn_finished_q[commit_insn_q.id];
  assign spatz_mem_str_finished_o = commit_insn_valid && &commit_finished_q && mem_insn_finished_q[commit_insn_q.id] && !commit_insn_q.is_load;

  // Do we start at the very fist element
  logic mem_is_vstart_zero;
  assign mem_is_vstart_zero = mem_spatz_req.vstart == 'd0;

  // Is the memory address unaligned
  logic mem_is_addr_unaligned;
  assign mem_is_addr_unaligned = mem_spatz_req.rs1[int'(MAXEW)-1:0] != '0;

  // Do we have to access every single element on its own
  logic mem_is_single_element_operation;
  assign mem_is_single_element_operation = mem_is_addr_unaligned || mem_is_strided || mem_is_indexed || !mem_is_vstart_zero;

  // How large is a single element (in bytes)
  logic [3:0] mem_single_element_size;
  assign mem_single_element_size = 1'b1 << mem_spatz_req.vtype.vsew;

  // How large is an index element (in bytes)
  logic [3:0] mem_idx_single_element_size;
  assign mem_idx_single_element_size = 1'b1 << mem_spatz_req.op_mem.ew;

  // Is the memory address unaligned
  logic commit_is_addr_unaligned;
  assign commit_is_addr_unaligned = commit_insn_q.rs1[int'(MAXEW)-1:0] != '0;

  // Do we have to access every single element on its own
  logic commit_is_single_element_operation;
  assign commit_is_single_element_operation = commit_is_addr_unaligned || commit_insn_q.is_strided || commit_insn_q.is_indexed || (commit_insn_q.vstart != '0);

  // Size of an element in the VRF
  logic [3:0] commit_single_element_size;
  assign commit_single_element_size = 1'b1 << commit_insn_q.vsew;

  ////////////////////
  //  Offset Queue  //
  ////////////////////

  // Store the offsets of all loads, for realigning
  addr_offset_t [NrMemPorts-1:0] vreg_addr_offset;
  logic [NrMemPorts-1:0] offset_queue_full;
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_offset_queue
    fifo_v3 #(
      .DATA_WIDTH(int'(MAXEW)       ),
      .DEPTH     (NrOutstandingLoads)
    ) i_offset_queue (
      .clk_i     (clk_i                                                                ),
      .rst_ni    (rst_ni                                                               ),
      .flush_i   (1'b0                                                                 ),
      .testmode_i(1'b0                                                                 ),
      .empty_o   (/* Unused */                                                         ),
      .full_o    (offset_queue_full[port]                                              ),
      .push_i    (spatz_mem_req_valid[port] && spatz_mem_req_ready[port] && mem_is_load),
      .data_i    (mem_req_addr_offset[port]                                            ),
      .data_o    (vreg_addr_offset[port]                                               ),
      .pop_i     (rob_pop[port] && commit_insn_q.is_load                               ),
      .usage_o   (/* Unused */                                                         )
    );
  end: gen_offset_queue

  ///////////////////////
  //  Output Register  //
  ///////////////////////

  typedef struct packed {
    vrf_addr_t waddr;
    vrf_data_t wdata;
    vrf_be_t wbe;

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
  assign vrf_id_o        = {vrf_req_q.rsp.id, mem_spatz_req.id, commit_insn_q.id};
  assign vrf_req_ready_q = vrf_wvalid_i;

  // Ack when the vector store finishes, or when the vector load commits to the VRF
  assign vlsu_rsp_o       = vrf_req_q.rsp_valid && vrf_req_valid_q ? vrf_req_q.rsp   : '{id: commit_insn_q.id, default: '0};
  assign vlsu_rsp_valid_o = vrf_req_q.rsp_valid && vrf_req_valid_q ? vrf_req_ready_q : vlsu_finished_req && !commit_insn_q.is_load;

  //////////////
  // Counters //
  //////////////

  // Do we need to catch up to reach element idx parity? (Because of non-zero vstart)
  vlen_t vreg_start_0;
  assign vreg_start_0 = vlen_t'(commit_insn_q.vstart[$clog2(ELENB)-1:0]);
  logic [N_FU-1:0] catchup;
  for (genvar i = 0; i < N_FU; i++) begin: gen_catchup
    assign catchup[i] = (commit_counter_q[i] < vreg_start_0) & (commit_counter_max[i] != commit_counter_q[i]);
  end: gen_catchup

  for (genvar fu = 0; fu < N_FU; fu++) begin: gen_vreg_counter_proc
    // The total amount of elements we have to work through
    vlen_t max_elements;

    always_comb begin
      // Default value
      max_elements = (commit_insn_q.vl >> $clog2(N_FU*ELENB)) << $clog2(ELENB);

      // Full transfer
      if (commit_insn_q.vl[$clog2(ELENB) +: $clog2(N_FU)] > fu)
        max_elements += ELENB;
      else if (commit_insn_q.vl[$clog2(N_FU*ELENB)-1:$clog2(ELENB)] == fu)
        max_elements += commit_insn_q.vl[$clog2(ELENB)-1:0];

      commit_counter_load[fu] = commit_insn_pop;
      commit_counter_d[fu]    = (commit_insn_q.vstart >> $clog2(N_FU*ELENB)) << $clog2(ELENB);
      if (commit_insn_q.vstart[$clog2(N_FU*ELENB)-1:$clog2(ELENB)] > fu)
        commit_counter_d[fu] += ELENB;
      else if (commit_insn_q.vstart[idx_width(N_FU*ELENB)-1:$clog2(ELENB)] == fu)
        commit_counter_d[fu] += commit_insn_q.vstart[$clog2(ELENB)-1:0];
      commit_operation_valid[fu] = commit_insn_valid && (commit_counter_q[fu] != max_elements) && (catchup[fu] || (!catchup[fu] && ~|catchup));
      commit_operation_last[fu]  = commit_operation_valid[fu] && ((max_elements - commit_counter_q[fu]) <= (commit_is_single_element_operation ? commit_single_element_size : ELENB));
      commit_counter_delta[fu]   = !commit_operation_valid[fu] ? vlen_t'('d0) : commit_is_single_element_operation ? vlen_t'(commit_single_element_size) : commit_operation_last[fu] ? (max_elements - commit_counter_q[fu]) : vlen_t'(ELENB);
      commit_counter_en[fu]      = commit_operation_valid[fu] && (commit_insn_q.is_load && vrf_req_valid_d && vrf_req_ready_d) || (!commit_insn_q.is_load && vrf_rvalid_i[0] && vrf_re_o[0] && (!mem_is_indexed || vrf_rvalid_i[1]));
      commit_counter_max[fu]     = max_elements;
    end
  end

  assign vd_elem_id = (commit_counter_q[0] > vreg_start_0) ? commit_counter_q[0] >> $clog2(ELENB) : commit_counter_q[N_FU-1] >> $clog2(ELENB);

  for (genvar port = 0; port < NrMemPorts; port++) begin: gen_mem_counter_proc
    // The total amount of elements we have to work through
    vlen_t max_elements;

    always_comb begin
      // Default value
      max_elements = (mem_spatz_req.vl >> $clog2(NrMemPorts*MemDataWidthB)) << $clog2(MemDataWidthB);

      if (NrMemPorts == 1)
        max_elements = mem_spatz_req.vl;
      else
        if (mem_spatz_req.vl[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] > port)
          max_elements += MemDataWidthB;
        else if (mem_spatz_req.vl[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] == port)
          max_elements += mem_spatz_req.vl[$clog2(MemDataWidthB)-1:0];

      mem_operation_valid[port] = mem_spatz_req_valid && (max_elements != mem_counter_q[port]);
      mem_operation_last[port]  = mem_operation_valid[port] && ((max_elements - mem_counter_q[port]) <= (mem_is_single_element_operation ? mem_single_element_size : MemDataWidthB));
      mem_counter_load[port]    = mem_spatz_req_ready;
      mem_counter_d[port]       = (mem_spatz_req.vstart >> $clog2(NrMemPorts*MemDataWidthB)) << $clog2(MemDataWidthB);
      if (NrMemPorts == 1)
        mem_counter_d[port] = mem_spatz_req.vstart;
      else
        if (mem_spatz_req.vstart[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] > port)
          mem_counter_d[port] += MemDataWidthB;
        else if (mem_spatz_req.vstart[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] == port)
          mem_counter_d[port] += mem_spatz_req.vstart[$clog2(MemDataWidthB)-1:0];
      mem_counter_delta[port] = !mem_operation_valid[port] ? 'd0 : mem_is_single_element_operation ? mem_single_element_size : mem_operation_last[port] ? (max_elements - mem_counter_q[port]) : MemDataWidthB;
      mem_counter_en[port]    = spatz_mem_req_ready[port] && spatz_mem_req_valid[port];
      mem_counter_max[port]   = max_elements;

      // Index counter
      mem_idx_counter_d[port]     = mem_counter_d[port];
      mem_idx_counter_delta[port] = !mem_operation_valid[port] ? 'd0 : mem_idx_single_element_size;
    end
  end

  ///////////
  // State //
  ///////////

  always_comb begin: p_state
    // Maintain state
    state_d = state_q;

    unique case (state_q)
      VLSU_RunningLoad: begin
        if (commit_insn_valid && !commit_insn_q.is_load)
          if (&rob_empty)
            state_d = VLSU_RunningStore;
      end

      VLSU_RunningStore: begin
        if (commit_insn_valid && commit_insn_q.is_load)
          if (&rob_empty)
            state_d = VLSU_RunningLoad;
      end

      default:;
    endcase
  end: p_state

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
  logic [NrMemPorts-1:0][idx_width(NrOutstandingLoads):0] mem_pending_d, mem_pending_q;
  logic [NrMemPorts-1:0] mem_pending;
  `FF(mem_pending_q, mem_pending_d, '{default: '0})
  always_comb begin
    // Maintain state
    mem_pending_d = mem_pending_q;

    for (int port = 0; port < NrMemPorts; port++) begin
      mem_pending[port] = mem_pending_q[port] != '0;

      // New request sent
      if (mem_is_load && spatz_mem_req_valid[port] && spatz_mem_req_ready[port])
        mem_pending_d[port]++;

      // Response used
      if (commit_insn_q.is_load && rob_rvalid[port] && rob_pop[port])
        mem_pending_d[port]--;
    end
  end

  // verilator lint_off LATCH
  always_comb begin
    vrf_raddr_o     = {vs2_vreg_addr, vd_vreg_addr};
    vrf_re_o        = '0;
    vrf_req_d       = '0;
    vrf_req_valid_d = 1'b0;

    rob_wdata = '0;
    rob_wid   = '0;
    rob_push  = '0;
    rob_pop   = '0;
    rob_req_id = '0;

    mem_req_id     = '0;
    mem_req_data   = '0;
    mem_req_strb   = '0;
    mem_req_svalid = '0;
    mem_req_lvalid = '0;
    mem_req_last   = '0;

    // Propagate request ID
    vrf_req_d.rsp.id    = commit_insn_q.id;
    vrf_req_d.rsp_valid = commit_insn_valid && &commit_finished_d && mem_insn_finished_d[commit_insn_q.id];

    // Request indexes
    vrf_re_o[1] = mem_is_indexed;

    // Count which vs2 element we should load (indexed loads)
    vs2_elem_id_d = vs2_elem_id_q;
    if (&(pending_index ^ ~mem_operation_valid) && mem_is_indexed)
      vs2_elem_id_d = vs2_elem_id_q + 1;
    if (mem_spatz_req_ready)
      vs2_elem_id_d = '0;

    if (commit_insn_valid && commit_insn_q.is_load) begin
      // If we have a valid element in the buffer, store it back to the register file
      if (state_q == VLSU_RunningLoad && |commit_operation_valid) begin
        // Enable write back to the VRF if we have a valid element in all buffers that still have to write something back.
        vrf_req_d.waddr = vd_vreg_addr;
        vrf_req_valid_d = &(rob_rvalid | ~mem_pending) && |mem_pending;

        for (int unsigned port = 0; port < NrMemPorts; port++) begin
          automatic logic [63:0] data = rob_rdata[port];

          // Shift data to correct position if we have an unaligned memory request
          if (MAXEW == EW_32)
            unique case ((commit_insn_q.is_strided || commit_insn_q.is_indexed) ? vreg_addr_offset[port] : commit_insn_q.rs1[1:0])
              2'b01: data   = {data[7:0], data[31:8]};
              2'b10: data   = {data[15:0], data[31:16]};
              2'b11: data   = {data[23:0], data[31:24]};
              default: data = data;
            endcase
          else
            unique case ((commit_insn_q.is_strided || commit_insn_q.is_indexed) ? vreg_addr_offset[port] : commit_insn_q.rs1[2:0])
              3'b001: data  = {data[7:0], data[63:8]};
              3'b010: data  = {data[15:0], data[63:16]};
              3'b011: data  = {data[23:0], data[63:24]};
              3'b100: data  = {data[31:0], data[63:32]};
              3'b101: data  = {data[39:0], data[63:40]};
              3'b110: data  = {data[47:0], data[63:48]};
              3'b111: data  = {data[55:0], data[63:56]};
              default: data = data;
            endcase

          // Pop stored element and free space in buffer
          rob_pop[port] = rob_rvalid[port] && vrf_req_valid_d && vrf_req_ready_d && commit_counter_en[port];

          // Shift data to correct position if we have a strided memory access
          if (commit_insn_q.is_strided || commit_insn_q.is_indexed)
            if (MAXEW == EW_32)
              unique case (commit_counter_q[port][1:0])
                2'b01: data   = {data[23:0], data[31:24]};
                2'b10: data   = {data[15:0], data[31:16]};
                2'b11: data   = {data[7:0], data[31:8]};
                default: data = data;
              endcase
            else
              unique case (commit_counter_q[port][2:0])
                3'b001: data  = {data[55:0], data[63:56]};
                3'b010: data  = {data[47:0], data[63:48]};
                3'b011: data  = {data[39:0], data[63:40]};
                3'b100: data  = {data[31:0], data[63:32]};
                3'b101: data  = {data[23:0], data[63:24]};
                3'b110: data  = {data[15:0], data[63:16]};
                3'b111: data  = {data[7:0], data[63:8]};
                default: data = data;
              endcase
          vrf_req_d.wdata[ELEN*port +: ELEN] = data;

          // Create write byte enable mask for register file
          if (commit_counter_en[port])
            if (commit_is_single_element_operation) begin
              automatic logic [$clog2(ELENB)-1:0] shift = commit_counter_q[port][$clog2(ELENB)-1:0];
              automatic logic [ELENB-1:0] mask          = '1;
              case (commit_insn_q.vsew)
                EW_8 : mask   = 1;
                EW_16: mask   = 3;
                EW_32: mask   = 15;
                default: mask = '1;
              endcase
              vrf_req_d.wbe[ELENB*port +: ELENB] = mask << shift;
            end else
              for (int unsigned k = 0; k < ELENB; k++)
                vrf_req_d.wbe[ELENB*port+k] = k < commit_counter_delta[port];
        end
      end

      for (int unsigned port = 0; port < NrMemPorts; port++) begin
        // Write the load result to the buffer
        rob_wdata[port] = spatz_mem_rsp_i[port].data;
`ifdef MEMPOOL_SPATZ
        rob_wid[port]   = spatz_mem_rsp_i[port].id;
        // Need to consider out-of-order memory response
        rob_push[port]  = spatz_mem_rsp_valid_i[port] && (state_q == VLSU_RunningLoad) && spatz_mem_rsp_i[port].write == '0;
`else
        rob_push[port]  = spatz_mem_rsp_valid_i[port] && (state_q == VLSU_RunningLoad) && store_count_q[port] == '0;
`endif
        if (!rob_full[port] && !offset_queue_full[port] && mem_operation_valid[port]) begin
          rob_req_id[port]     = spatz_mem_req_ready[port] & spatz_mem_req_valid[port];
          mem_req_lvalid[port] = (!mem_is_indexed || (vrf_rvalid_i[1] && !pending_index[port])) && mem_spatz_req.op_mem.is_load;
          mem_req_id[port]     = rob_id[port];
          mem_req_last[port]   = mem_operation_last[port];
        end
      end
    // Store operation
    end else begin
      // Read new element from the register file and store it to the buffer
      if (state_q == VLSU_RunningStore && !(|rob_full) && |commit_operation_valid) begin
        vrf_re_o[0] = 1'b1;

        for (int unsigned port = 0; port < NrMemPorts; port++) begin
          rob_wdata[port]  = vrf_rdata_i[0][ELEN*port +: ELEN];
          rob_wid[port]    = rob_id[port];
          rob_req_id[port] = vrf_rvalid_i[0] && (!mem_is_indexed || vrf_rvalid_i[1]);
          rob_push[port]   = rob_req_id[port];
        end
      end

      for (int unsigned port = 0; port < NrMemPorts; port++) begin
        // Read element from buffer and execute memory request
        if (mem_operation_valid[port]) begin
          automatic logic [63:0] data = rob_rdata[port];

          // Shift data to lsb if we have a strided or indexed memory access
          if (mem_is_strided || mem_is_indexed)
            if (MAXEW == EW_32)
              unique case (mem_counter_q[port][1:0])
                2'b01: data = {data[7:0], data[31:8]};
                2'b10: data = {data[15:0], data[31:16]};
                2'b11: data = {data[23:0], data[31:24]};
                default:; // Do nothing
              endcase
            else
              unique case (mem_counter_q[port][2:0])
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
            unique case ((mem_is_strided || mem_is_indexed) ? mem_req_addr_offset[port] : mem_spatz_req.rs1[1:0])
              2'b01: mem_req_data[port]   = {data[23:0], data[31:24]};
              2'b10: mem_req_data[port]   = {data[15:0], data[31:16]};
              2'b11: mem_req_data[port]   = {data[7:0], data[31:8]};
              default: mem_req_data[port] = data;
            endcase
          else
            unique case ((mem_is_strided || mem_is_indexed) ? mem_req_addr_offset[port] : mem_spatz_req.rs1[2:0])
              3'b001: mem_req_data[port]  = {data[55:0], data[63:56]};
              3'b010: mem_req_data[port]  = {data[47:0], data[63:48]};
              3'b011: mem_req_data[port]  = {data[39:0], data[63:40]};
              3'b100: mem_req_data[port]  = {data[31:0], data[63:32]};
              3'b101: mem_req_data[port]  = {data[23:0], data[63:24]};
              3'b110: mem_req_data[port]  = {data[15:0], data[63:16]};
              3'b111: mem_req_data[port]  = {data[7:0], data[63:8]};
              default: mem_req_data[port] = data;
            endcase

          mem_req_svalid[port] = rob_rvalid[port] && (!mem_is_indexed || (vrf_rvalid_i[1] && !pending_index[port])) && !mem_spatz_req.op_mem.is_load;
          mem_req_id[port]     = rob_rid[port];
          mem_req_last[port]   = mem_operation_last[port];
          rob_pop[port]        = spatz_mem_req_valid[port] && spatz_mem_req_ready[port];

          // Create byte enable signal for memory request
          if (mem_is_single_element_operation) begin
            automatic logic [$clog2(ELENB)-1:0] shift = (mem_is_strided || mem_is_indexed) ? mem_req_addr_offset[port] : mem_counter_q[port][$clog2(ELENB)-1:0] + commit_insn_q.rs1[int'(MAXEW)-1:0];
            automatic logic [MemDataWidthB-1:0] mask  = '1;
            case (mem_spatz_req.vtype.vsew)
              EW_8 : mask   = 1;
              EW_16: mask   = 3;
              EW_32: mask   = 15;
              default: mask = '1;
            endcase
            mem_req_strb[port] = mask << shift;
          end else
            for (int unsigned k = 0; k < ELENB; k++)
              mem_req_strb[port][k] = k < mem_counter_delta[port];
        end else begin
          // Clear empty buffer id requests
          if (!rob_empty[port])
            rob_pop[port] = 1'b1;
        end
      end
    end
  end
  // verilator lint_on LATCH

  // Create memory requests
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_mem_req
    spill_register #(
      .T(spatz_mem_req_t)
    ) i_spatz_mem_req_register (
      .clk_i   (clk_i                      ),
      .rst_ni  (rst_ni                     ),
      .data_i  (spatz_mem_req[port]        ),
      .valid_i (spatz_mem_req_valid[port]  ),
      .ready_o (spatz_mem_req_ready[port]  ),
      .data_o  (spatz_mem_req_o[port]      ),
      .valid_o (spatz_mem_req_valid_o[port]),
      .ready_i (spatz_mem_req_ready_i[port])
    );
`ifdef MEMPOOL_SPATZ
    // ID is required in Mempool-Spatz
    assign spatz_mem_req[port].id    = mem_req_id[port];
    assign spatz_mem_req[port].addr  = mem_req_addr[port];
    assign spatz_mem_req[port].mode  = '0; // Request always uses user privilege level
    assign spatz_mem_req[port].size  = mem_spatz_req.vtype.vsew[1:0];
    assign spatz_mem_req[port].write = !mem_is_load;
    assign spatz_mem_req[port].strb  = mem_req_strb[port];
    assign spatz_mem_req[port].data  = mem_req_data[port];
    assign spatz_mem_req[port].last  = mem_req_last[port];
    assign spatz_mem_req[port].spec  = 1'b0; // Request is never speculative
    assign spatz_mem_req_valid[port] = mem_req_svalid[port] || mem_req_lvalid[port];
`else
    assign spatz_mem_req[port].addr  = mem_req_addr[port];
    assign spatz_mem_req[port].write = !mem_is_load;
    assign spatz_mem_req[port].amo   = reqrsp_pkg::AMONone;
    assign spatz_mem_req[port].data  = mem_req_data[port];
    assign spatz_mem_req[port].strb  = mem_req_strb[port];
    assign spatz_mem_req[port].user  = '0;
    assign spatz_mem_req_valid[port] = mem_req_svalid[port] || mem_req_lvalid[port];
`endif
  end

  ////////////////
  // Assertions //
  ////////////////

  if (MemDataWidth != ELEN)
    $error("[spatz_vlsu] The memory data width needs to be equal to %d.", ELEN);

  if (NrMemPorts != N_FU)
    $error("[spatz_vlsu] The number of memory ports needs to be equal to the number of FUs.");

  if (NrMemPorts != 2**$clog2(NrMemPorts))
    $error("[spatz_vlsu] The NrMemPorts parameter needs to be a power of two");

endmodule : spatz_vlsu
