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
    parameter int unsigned   NrOutstandingLoads = 16,
    // Memory request
    parameter  type          spatz_mem_req_t    = logic,
    parameter  type          spatz_mem_rsp_t    = logic,
    // Dependant parameters. DO NOT CHANGE!
    localparam int  unsigned NrInterfaces       = NrMemPorts / spatz_pkg::N_FU,
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
    output vrf_addr_t      [NrInterfaces-1:0] vrf_waddr_o,
    output vrf_data_t      [NrInterfaces-1:0] vrf_wdata_o,
    output logic           [NrInterfaces-1:0] vrf_we_o,
    output vrf_be_t        [NrInterfaces-1:0] vrf_wbe_o,
    input  logic           [NrInterfaces-1:0] vrf_wvalid_i,
    output spatz_id_t      [NrInterfaces-1:0] [2:0] vrf_id_o,
    output vrf_addr_t      [NrInterfaces-1:0] [1:0] vrf_raddr_o,
    output logic           [NrInterfaces-1:0] [1:0] vrf_re_o,
    input  vrf_data_t      [NrInterfaces-1:0] [1:0] vrf_rdata_i,
    input  logic           [NrInterfaces-1:0] [1:0] vrf_rvalid_i,
    // Memory Request
    output spatz_mem_req_t [NrMemPorts-1:0] spatz_mem_req_o,
    output logic           [NrMemPorts-1:0] spatz_mem_req_valid_o,
    input  logic           [NrMemPorts-1:0] spatz_mem_req_ready_i,
    //  Memory Response
    input  spatz_mem_rsp_t [NrMemPorts-1:0] spatz_mem_rsp_i,
    input  logic           [NrMemPorts-1:0] spatz_mem_rsp_valid_i,
    output logic           [NrMemPorts-1:0] spatz_mem_rsp_ready_o,
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
        // MXU (32 bit)
        if (spatz_req_i.op_arith.is_mx) begin
          // The following code works if M, N, K are either 4 or 8, m*k = vl
          unique case(spatz_req_i.matrix)
            // Tile A is m*k, but since k = vl/m, then m*k == vl
            TILE_A: spatz_req_d.vl = spatz_req_i.vl << 2;
            // Tile B is k*n, and k = vl/m. Therefore, tile B size is vl * n/m
            TILE_B: spatz_req_d.vl = spatz_req_i.vl << (
              spatz_req_i.tile_M == spatz_req_i.tile_N
              ? 2
              : spatz_req_i.tile_M == 4
                ? 3
                : 1);
            // Tile C is m*n. Since vl = m*k and m*n should fit a vreg,
            // n <= k
            TILE_C: spatz_req_d.vl = spatz_req_i.vl << (
              spatz_req_i.tile_N == 4 && spatz_req_i.tile_K == 8 ? 1 : 2);
            default: spatz_req_d.vl = spatz_req_i.vl << 2;
          endcase
        end
      end
      default: begin
        spatz_req_d.vl     = spatz_req_i.vl << MAXEW;
        spatz_req_d.vstart = spatz_req_i.vstart << MAXEW;
        // MXU (64 bit)
        if (spatz_req_i.op_arith.is_mx) begin
          unique case(spatz_req_i.matrix)
            TILE_A: spatz_req_d.vl = spatz_req_i.vl << MAXEW;
            TILE_B: spatz_req_d.vl = spatz_req_i.vl << (
              spatz_req_i.tile_M == spatz_req_i.tile_N
              ? MAXEW
              : spatz_req_i.tile_M == 4
                ? MAXEW + 1
                : MAXEW - 1);
            TILE_C: spatz_req_d.vl = spatz_req_i.vl << (
              spatz_req_i.tile_N == 4 && spatz_req_i.tile_K == 8 ? MAXEW - 1 : MAXEW);
            default: spatz_req_d.vl = spatz_req_i.vl << MAXEW;
          endcase
        end
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

  state_t [NrInterfaces-1:0] [N_FU-1:0] port_state_d, port_state_q;
  for (genvar intf = 0; intf < NrInterfaces; intf++) begin : gen_state_intf
    for (genvar fu = 0; fu < N_FU; fu++) begin : gen_state_fu
      `FF(port_state_q[intf][fu], port_state_d[intf][fu], VLSU_RunningLoad)
    end : gen_state_fu
  end : gen_state_intf

  id_t [NrInterfaces-1:0] [N_FU-1:0] store_count_q;
  id_t [NrInterfaces-1:0] [N_FU-1:0] store_count_d;

  // Memory requests
  spatz_mem_req_t [NrInterfaces-1:0] [N_FU-1:0] spatz_mem_req;
  logic           [NrInterfaces-1:0] [N_FU-1:0] spatz_mem_req_valid;
  logic           [NrInterfaces-1:0] [N_FU-1:0] spatz_mem_req_ready;

  `FF(store_count_q, store_count_d, '0)

  // debugging flags
  logic [NrMemPorts-1:0] st_counter_up, st_counter_dw;

  always_comb begin: proc_store_count
    // Maintain state
    store_count_d = store_count_q;

    for (int intf = 0; intf < NrInterfaces; intf++) begin
      for (int fu = 0; fu < N_FU; fu++) begin
        automatic int unsigned port = intf * N_FU + fu;
        st_counter_up[port] = 1'b0;
        st_counter_dw[port] = 1'b0;
        // We need to count a store once it is sent to reg for correct handling
        if (spatz_mem_req[intf][fu].write && spatz_mem_req_valid[intf][fu] && spatz_mem_req_ready[intf][fu]) begin
          // Did we send a store?
          store_count_d[intf][fu]++;
          st_counter_up[port] = 1'b1;
        end
        // Did we get the ack of a store?
        if (store_count_q[intf][fu] != '0 && spatz_mem_rsp_valid_i[port] && spatz_mem_rsp_i[port].write) begin
          store_count_d[intf][fu]--;
          st_counter_dw[port] = 1'b1;
        end
      end
    end
  end: proc_store_count

  //////////////////////
  //  Reorder Buffer  //
  //////////////////////

  typedef logic [int'(MAXEW)-1:0] addr_offset_t;

  elen_t [NrInterfaces-1:0] [N_FU-1:0] rob_wdata;
  id_t   [NrInterfaces-1:0] [N_FU-1:0] rob_wid;
  logic  [NrInterfaces-1:0] [N_FU-1:0] rob_push;
  logic  [NrInterfaces-1:0] [N_FU-1:0] rob_rvalid;
  elen_t [NrInterfaces-1:0] [N_FU-1:0] rob_rdata;
  logic  [NrInterfaces-1:0] [N_FU-1:0] rob_pop;
  id_t   [NrInterfaces-1:0] [N_FU-1:0] rob_rid;
  logic  [NrInterfaces-1:0] [N_FU-1:0] rob_req_id;
  id_t   [NrInterfaces-1:0] [N_FU-1:0] rob_id;
  logic  [NrInterfaces-1:0] [N_FU-1:0] rob_full;
  logic  [NrInterfaces-1:0] [N_FU-1:0] rob_empty;

  // The reorder buffer decouples the memory side from the register file side.
  // All elements from one side to the other go through it.
  for (genvar intf = 0; intf < NrInterfaces; intf++) begin : gen_rob_intf
    for (genvar fu = 0; fu < N_FU; fu++) begin : gen_rob_intf_fu
      reorder_buffer #(
        .DataWidth  (ELEN                 ),
        .NumWords   (NrOutstandingLoads   )
      ) i_reorder_buffer (
        .clk_i      (clk_i                ),
        .rst_ni     (rst_ni               ),
        .data_i     (rob_wdata[intf][fu]  ),
        .id_i       (rob_wid[intf][fu]    ),
        .push_i     (rob_push[intf][fu]   ),
        .data_o     (rob_rdata[intf][fu]  ),
        .valid_o    (rob_rvalid[intf][fu] ),
        .id_read_o  (rob_rid[intf][fu]    ),
        .pop_i      (rob_pop[intf][fu]    ),
        .id_req_i   (rob_req_id[intf][fu] ),
        .id_o       (rob_id[intf][fu]     ),
        .id_valid_o (/* not used */       ),
        .full_o     (rob_full[intf][fu]   ),
        .empty_o    (rob_empty[intf][fu]  )
      );
    end: gen_rob_intf_fu
  end: gen_rob_intf

  //////////////////////
  //  Memory request  //
  //////////////////////

  // Is the memory operation valid and are we at the last one?
  logic [NrInterfaces-1:0] [N_FU-1:0] mem_operation_valid;
  logic [NrInterfaces-1:0] [N_FU-1:0] mem_operation_last;

  // For each memory port we count how many bytes we have already loaded/stored (VLSU <-> MEM).
  // Multiple counters are needed all memory ports can work independent of each other.
  vlen_t [NrInterfaces-1:0] [N_FU-1:0] mem_counter_max;
  logic  [NrInterfaces-1:0] [N_FU-1:0] mem_counter_en;
  logic  [NrInterfaces-1:0] [N_FU-1:0] mem_counter_load;
  vlen_t [NrInterfaces-1:0] [N_FU-1:0] mem_counter_delta;
  vlen_t [NrInterfaces-1:0] [N_FU-1:0] mem_counter_d;
  vlen_t [NrInterfaces-1:0] [N_FU-1:0] mem_counter_q;
  logic  [NrInterfaces-1:0] [N_FU-1:0] mem_port_finished_d, mem_port_finished_q;

  vlen_t [NrInterfaces-1:0] [N_FU-1:0] mem_idx_counter_delta;
  vlen_t [NrInterfaces-1:0] [N_FU-1:0] mem_idx_counter_d;
  vlen_t [NrInterfaces-1:0] [N_FU-1:0] mem_idx_counter_q;

  // MXU
  // Counters to count rows (m dim) and columns (n dim (stores) or k dim (loads)) for each memory port
  // When loading, the column dimension is k (in memory, A mtx is transposed)
  // The impact of this code is 2*NrPorts counters with $clog2(MAX_TILE) bits
  // This hardware works for TILE_M, TILE_N, TILE_K > NrMemPorts
  localparam int unsigned MAX_TILE_ROW = MAX_TILE_M;
  localparam int unsigned MAX_TILE_COL = MAX_TILE_N;
  // Counters to parametrically handle memory loads/stores
  logic [NrMemPorts-1:0] [$clog2(MAX_TILE_M)-1:0] mx_cnt_row_q;
  logic [NrMemPorts-1:0] [$clog2(MAX_TILE_N)-1:0] mx_cnt_col_q;
  logic [NrMemPorts-1:0] mx_cnt_en_row, mx_cnt_en_col;
  logic [NrMemPorts-1:0] mx_cnt_clr_row, mx_cnt_clr_col;
  logic [$clog2(MAX_TILE_N)-1:0] mx_cnt_max_row, mx_cnt_max_col;
  // Max counter for m and n counters
  logic [$clog2(MAX_TILE_M)-1:0] mx_max_row;
  logic [$clog2(MAX_TILE_N)-1:0] mx_max_col;
  assign mx_max_row = commit_insn_q.is_load
                    ? mem_spatz_req.tile_K[$clog2(MAX_TILE_K)-1:0] - 1
                    : mem_spatz_req.tile_M[$clog2(MAX_TILE_M)-1:0] - 1;
  // Columns on the N dimension for mtx C store, otherwise on the K dimension
  assign mx_max_col = commit_insn_q.is_load && mem_spatz_req.matrix == TILE_A
                    ? mem_spatz_req.tile_M[$clog2(MAX_TILE_M)-1:0] - 1
                    : mem_spatz_req.tile_N[$clog2(MAX_TILE_N)-1:0] - 1;
  assign mx_cnt_max_row = mx_max_row; // >> ( commit_insn_q.is_load ? $clog2(NrMemPorts) : 0);
  assign mx_cnt_max_col = mx_max_col >> $clog2(NrMemPorts); //(!commit_insn_q.is_load ? $clog2(NrMemPorts) : 0);

  for (genvar intf = 0; intf < NrInterfaces; intf++) begin: gen_mem_counters_intf
    for (genvar fu = 0; fu < N_FU; fu++) begin: gen_mem_counters_intf_fu
      delta_counter #(
        .WIDTH($bits(vlen_t))
      ) i_delta_counter_mem (
        .clk_i     (clk_i                  ),
        .rst_ni    (rst_ni                 ),
        .clear_i   (1'b0                   ),
        .en_i      (mem_counter_en[intf][fu]   ),
        .load_i    (mem_counter_load[intf][fu] ),
        .down_i    (1'b0                   ), // We always count up
        .delta_i   (mem_counter_delta[intf][fu]),
        .d_i       (mem_counter_d[intf][fu]    ),
        .q_o       (mem_counter_q[intf][fu]    ),
        .overflow_o(/* Unused */           )
      );

      delta_counter #(
        .WIDTH($bits(vlen_t))
      ) i_delta_counter_mem_idx (
        .clk_i     (clk_i                      ),
        .rst_ni    (rst_ni                     ),
        .clear_i   (1'b0                       ),
        .en_i      (mem_counter_en[intf][fu]       ),
        .load_i    (mem_counter_load[intf][fu]     ),
        .down_i    (1'b0                       ), // We always count up
        .delta_i   (mem_idx_counter_delta[intf][fu]),
        .d_i       (mem_idx_counter_d[intf][fu]    ),
        .q_o       (mem_idx_counter_q[intf][fu]    ),
        .overflow_o(/* Unused */               )
      );

      assign mem_port_finished_d[intf][fu] = mem_spatz_req_valid && (mem_counter_q[intf][fu] == mem_counter_max[intf][fu] - mem_counter_delta[intf][fu]);
      assign mem_port_finished_q[intf][fu] = mem_spatz_req_valid && (mem_counter_q[intf][fu] == mem_counter_max[intf][fu]);

      ////////////////////
      //  MXU counters  //
      ////////////////////

      // We store row-wise
      // Go to a new row when we finished the previous one
      // Load instructions: we load column-wise
      // Store instructions: we store row-wise
      // Both loads and stores are unit strides to exploit the coalescer of the L1 cache
      // A is assumed to be Transposed in memory while B is in its original form
      // For loads, column counter is enabled by default
      // For stores, row counter is enabled by default
      localparam int unsigned port = intf * N_FU + fu;
      assign mx_cnt_en_row[port] = mem_spatz_req.op_arith.is_mx & mem_counter_en[port] & (mx_cnt_col_q[port] == mx_cnt_max_col);
      assign mx_cnt_en_col[port] = mem_spatz_req.op_arith.is_mx & mem_counter_en[port];
      // Count up to (tile_size - 1), and tile_size is power of 2
      assign mx_cnt_clr_row[port] = mx_cnt_en_row[port] & (mx_cnt_row_q[port] == mx_cnt_max_row);
      assign mx_cnt_clr_col[port] = mx_cnt_en_col[port] & (mx_cnt_col_q[port] == mx_cnt_max_col);

      counter #(
        .WIDTH($clog2(MAX_TILE_ROW))
      ) i_mx_cnt_row (
        .clk_i     (clk_i               ),
        .rst_ni    (rst_ni              ),
        .clear_i   (mx_cnt_clr_row[port]),
        .en_i      (mx_cnt_en_row[port] ),
        .load_i    ('0                  ),
        .down_i    (1'b0                ), // We always count up
        .d_i       ('0                  ),
        .q_o       (mx_cnt_row_q[port]  ),
        .overflow_o(/* Unused */        )
      );

      counter #(
        .WIDTH($clog2(MAX_TILE_COL))
      ) i_mx_cnt_col (
        .clk_i     (clk_i               ),
        .rst_ni    (rst_ni              ),
        .clear_i   (mx_cnt_clr_col[port]),
        .en_i      (mx_cnt_en_col[port] ),
        .load_i    ('0                  ),
        .down_i    (1'b0                ), // We always count up
        .d_i       ('0                  ),
        .q_o       (mx_cnt_col_q[port]  ),
        .overflow_o(/* Unused */        )
      );

    end: gen_mem_counters_intf_fu
  end: gen_mem_counters_intf

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
    .DEPTH       (4                ),
    .FALL_THROUGH(1'b1             ),
    .dtype       (commit_metadata_t)
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
    if (&(mem_port_finished_q | (mem_port_finished_d & mem_counter_en))) begin
      mem_insn_finished_d[mem_spatz_req.id] = 1'b1;
      mem_spatz_req_ready                   = 1'b1;
    end
    // Did we acknowledge the end of an instruction?
    if (vlsu_rsp_valid_o) begin
      mem_insn_finished_d[vlsu_rsp_o.id] = 1'b0;
      mem_insn_pending_d[vlsu_rsp_o.id]  = 1'b0;
    end
  end

  // For each memory port that we have, count how many elements we have already loaded/stored (VRF <-> VLSU).
  // Multiple counters are necessary for the case where not every single port will
  // receive the same number of elements to work through.
  vlen_t [NrInterfaces-1:0] [N_FU-1:0] commit_counter_max;
  logic  [NrInterfaces-1:0] [N_FU-1:0] commit_counter_en;
  logic  [NrInterfaces-1:0] [N_FU-1:0] commit_counter_load;
  vlen_t [NrInterfaces-1:0] [N_FU-1:0] commit_counter_delta;
  vlen_t [NrInterfaces-1:0] [N_FU-1:0] commit_counter_d;
  vlen_t [NrInterfaces-1:0] [N_FU-1:0] commit_counter_q;

  logic  [NrInterfaces-1:0] [N_FU-1:0] commit_finished_q;
  logic  [NrInterfaces-1:0] [N_FU-1:0] commit_finished_d;

  for (genvar intf = 0; intf < NrInterfaces; intf++) begin : gen_vreg_counters_intf
    for (genvar fu = 0; fu < N_FU; fu++) begin : gen_vreg_counters_intf_fu
      delta_counter #(
        .WIDTH($bits(vlen_t))
      ) i_delta_counter_vreg (
        .clk_i     (clk_i                         ),
        .rst_ni    (rst_ni                        ),
        .clear_i   (1'b0                          ),
        .en_i      (commit_counter_en[intf][fu]   ),
        .load_i    (commit_counter_load[intf][fu] ),
        .down_i    (1'b0                          ), // We always count up
        .delta_i   (commit_counter_delta[intf][fu]),
        .d_i       (commit_counter_d[intf][fu]    ),
        .q_o       (commit_counter_q[intf][fu]    ),
        .overflow_o(/* Unused */                  )
      );

      assign commit_finished_q[intf][fu] = commit_insn_valid && (commit_counter_q[intf][fu] == commit_counter_max[intf][fu]);
      assign commit_finished_d[intf][fu] = commit_insn_valid &&
                                           ((commit_counter_q[intf][fu] + commit_counter_delta[intf][fu]) == commit_counter_max[intf][fu]);
    end: gen_vreg_counters_intf_fu
  end: gen_vreg_counters_intf

  ////////////////////////
  // Address Generation //
  ////////////////////////

  elen_t [NrInterfaces-1:0] [N_FU-1:0] mem_req_addr;

  vrf_addr_t [NrInterfaces-1:0] vd_vreg_addr;
  vrf_addr_t [NrInterfaces-1:0] vs2_vreg_addr, vs2_vreg_idx_addr;

  // MXU
  elen_t [NrMemPorts-1:0] mx_offset_addr_d, mx_offset_addr_q;
  `FF(mx_offset_addr_q, mx_offset_addr_d, '0)

  // Current element index and byte index that are being accessed at the register file
  vreg_elem_t [NrInterfaces-1:0] vd_elem_id;
  vreg_elem_t [NrInterfaces-1:0] vs2_elem_id_d, vs2_elem_id_q;
  `FF(vs2_elem_id_q, vs2_elem_id_d, '0)

  // Pending indexes
  logic [NrInterfaces-1:0] [N_FU-1:0] pending_index;

  // Calculate the memory address for each memory port
   // Calculate the memory address for each memory port
  addr_offset_t [NrInterfaces-1:0] [N_FU-1:0] mem_req_addr_offset;
  for (genvar intf = 0; intf < NrInterfaces; intf++) begin: gen_mem_req_addr_intf
    for (genvar fu = 0; fu < N_FU; fu++) begin: gen_mem_req_addr_intf_fu
      localparam int unsigned port = intf * N_FU + fu;
      logic [31:0] addr;
      logic [31:0] stride;
      logic [31:0] offset;
      // MXU
      logic [31:0] mx_offset_load;
      logic [31:0] mx_offset_store;
      logic [31:0] mx_offset;

      // Pre-shuffling index offset
      typedef logic [int'(MAXEW)-1:0] maxew_t;
      maxew_t idx_offset;
      assign idx_offset = mem_idx_counter_q[intf][fu];

      always_comb begin
        stride = mem_is_strided ? mem_spatz_req.rs2 >> mem_spatz_req.vtype.vsew : 'd1;

        if (mem_is_indexed) begin
          // What is the relationship between data and index width?
          automatic logic [1:0] data_index_width_diff = int'(mem_spatz_req.vtype.vsew) - int'(mem_spatz_req.op_mem.ew);

          // Pointer to index
          automatic logic [idx_width(N_FU*ELENB)-1:0] word_index = (fu << (MAXEW - data_index_width_diff)) +
                                                                   (maxew_t'(idx_offset >> (MAXEW - data_index_width_diff)) << (MAXEW - data_index_width_diff)) * N_FU +
                                                                   (maxew_t'(idx_offset << data_index_width_diff) >> data_index_width_diff);

          // Index
          unique case (mem_spatz_req.op_mem.ew)
            EW_8 : offset   = $signed(vrf_rdata_i[intf][1][8 * word_index +: 8]);
            EW_16: offset   = $signed(vrf_rdata_i[intf][1][8 * word_index +: 16]);
            default: offset = $signed(vrf_rdata_i[intf][1][8 * word_index +: 32]);
          endcase
        // MXU
        end else if(mem_spatz_req.op_arith.is_mx) begin
          // Ports finish storing one matrix column before passing to a new one
          // If mem_spatz_req.rs2 is a power of 2, this can be further optimized
          // mx_offset_load = (mem_spatz_req.rs2 * (port << 3)) + ((mx_cnt_row_q[port] * mem_spatz_req.rs2) << $clog2(NrMemPorts * MemDataWidthB)) + (mx_cnt_col_q[port] << $clog2(MemDataWidthB));
          mx_offset_load = (1 * (port << 3)) + ((mx_cnt_col_q[port]) << $clog2(NrMemPorts * MemDataWidthB)) + ((mx_cnt_row_q[port]* mem_spatz_req.rs2) << $clog2(MemDataWidthB));
          // Ports finish storing one matrix row before passing to a new one
          // If mem_spatz_req.rs2 is a power of 2, this can be further optimized
          mx_offset_store = (port << 3) + ((mx_cnt_row_q[port] * mem_spatz_req.rs2) << $clog2(MemDataWidthB)) + (mx_cnt_col_q[port] << ($clog2(NrMemPorts * MemDataWidthB)));
          offset = commit_insn_q.is_load ? mx_offset_load : mx_offset_store;
        end else begin
          offset = ({mem_counter_q[intf][fu][$bits(vlen_t)-1:MAXEW] << $clog2(N_FU), mem_counter_q[intf][fu][int'(MAXEW)-1:0]} + (fu << MAXEW)) * stride;
        end

        // The second interface starts from half of the vector to straighten the write-back VRF access pattern
        // To ensure that the 2 interfaces do not also conflict at the TCDM, there is HW scrambling of addresses to TCDM
        // such that they access different superbanks.
        if (!mem_is_indexed && !mem_is_strided && intf == 1) offset += (mem_spatz_req.vl / 2);

        addr                      = mem_spatz_req.rs1 + offset;
        mem_req_addr[intf][fu]        = (addr >> MAXEW) << MAXEW;
        mem_req_addr_offset[intf][fu] = addr[int'(MAXEW)-1:0];

        pending_index[intf][fu] = (mem_idx_counter_q[intf][fu][$clog2(NrWordsPerVector*ELENB)-1:0] >> MAXEW) != vs2_vreg_addr[intf][$clog2(NrWordsPerVector)-1:0];
      end
    end: gen_mem_req_addr_intf_fu
  end: gen_mem_req_addr_intf

  // Calculate the register file addresses
  always_comb begin : gen_vreg_addr
    for (int intf = 0; intf < NrInterfaces; intf++) begin : gen_vreg_addr_intf
      vd_vreg_addr[intf]  = (commit_insn_q.vd << $clog2(NrWordsPerVector)) + $unsigned(vd_elem_id[intf]);
      
      // For indices for indexed operations
      vs2_vreg_addr[intf] = (mem_spatz_req.vs2 << $clog2(NrWordsPerVector)) + $unsigned(vs2_elem_id_q[intf]);
      vs2_vreg_idx_addr[intf] = vs2_vreg_addr[intf];
      
      // The second interface starts from half of the vector to straighten the write-back VRF access pattern   
      if (intf == 1) begin
        vd_vreg_addr[intf] += commit_insn_q.vl / (2 * N_FU * ELENB);
        vs2_vreg_idx_addr[intf] += ((mem_spatz_req.vl >> (mem_spatz_req.vtype.vsew - int'(mem_spatz_req.op_mem.ew))) / (2 * N_FU * ELENB));
      end
    end    
  end

  ///////////////
  //  Control  //
  ///////////////

  // Are we busy?
  logic busy_q, busy_d;
  `FF(busy_q, busy_d, 1'b0)

  // Did we finish an instruction?
  logic vlsu_finished_req;

  always_comb begin: control_proc
    // Maintain state
    busy_d = busy_q;

    // Do not pop anything
    commit_insn_pop = 1'b0;

    // Do not ack anything
    vlsu_finished_req = 1'b0;

    // Finished the execution!
    if (commit_insn_valid && &(commit_finished_q | (commit_finished_d & commit_counter_en)) && mem_insn_finished_q[commit_insn_q.id]) begin
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
  logic [NrInterfaces-1:0] [N_FU-1:0] commit_operation_valid;
  logic [NrInterfaces-1:0] [N_FU-1:0] commit_operation_last;

  // Is instruction a load?
  logic mem_is_load;
  assign mem_is_load = mem_spatz_req.op_mem.is_load;

  // Signal when we are finished with with accessing the memory (necessary
  // for the case with more than one memory port)
  assign spatz_mem_finished_o     = commit_insn_valid && &(commit_finished_q | (commit_finished_d & commit_counter_en)) && mem_insn_finished_q[commit_insn_q.id];
  assign spatz_mem_str_finished_o = commit_insn_valid && &(commit_finished_q | (commit_finished_d & commit_counter_en)) && mem_insn_finished_q[commit_insn_q.id] && !commit_insn_q.is_load;

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
  addr_offset_t [NrInterfaces-1:0] [N_FU-1:0] vreg_addr_offset;
  logic [NrInterfaces-1:0] [N_FU-1:0] offset_queue_full;
  for (genvar intf = 0; intf < NrInterfaces; intf++) begin : gen_offset_queue_intf
    for (genvar fu = 0; fu < N_FU; fu++) begin : gen_offset_queue_intf_fu
      fifo_v3 #(
        .DATA_WIDTH(int'(MAXEW)       ),
        .DEPTH     (NrOutstandingLoads)
      ) i_offset_queue (
        .clk_i     (clk_i                                                                    ),
        .rst_ni    (rst_ni                                                                   ),
        .flush_i   (1'b0                                                                     ),
        .testmode_i(1'b0                                                                     ),
        .empty_o   (/* Unused */                                                             ),
        .full_o    (offset_queue_full[intf][fu]                                              ),
        .push_i    (spatz_mem_req_valid[intf][fu] && spatz_mem_req_ready[intf][fu] && mem_is_load),
        .data_i    (mem_req_addr_offset[intf][fu]                                            ),
        .data_o    (vreg_addr_offset[intf][fu]                                               ),
        .pop_i     (rob_pop[intf][fu] && commit_insn_q.is_load                               ),
        .usage_o   (/* Unused */                                                             )
      );
    end: gen_offset_queue_intf_fu
  end: gen_offset_queue_intf

  ///////////////////////
  //  Output Register  //
  ///////////////////////

  typedef struct packed {
    vrf_addr_t waddr;
    vrf_data_t wdata;
    vrf_be_t wbe;

    vlsu_rsp_t rsp;
    logic rsp_valid;

    // Is this the last vrf write from this interface?
    logic last;
  } vrf_req_t;

  vrf_req_t [NrInterfaces-1:0] vrf_req_d, vrf_req_q;
  logic     [NrInterfaces-1:0] vrf_req_valid_d, vrf_req_ready_d;
  logic     [NrInterfaces-1:0] vrf_req_valid_q, vrf_req_ready_q;
  logic     [NrInterfaces-1:0] vrf_valid_rsp_q, vrf_valid_rsp;
  logic     [NrInterfaces-1:0] vrf_commit_intf_valid, vrf_commit_intf_valid_q;
  logic     [NrInterfaces-1:0] resp_overlap;

  logic     [NrInterfaces-1:0] vrf_commit_done_d, vrf_commit_done_q;
  `FF(vrf_commit_done_q, vrf_commit_done_d, '0);
  always_comb begin
    vrf_commit_done_d = vrf_commit_done_q;

    // Here, we check if each interface is committing the last vrf
    // If yes, we record the commit is done for this interface
    for (int intf = 0; intf < NrInterfaces; intf++) begin
      if (vrf_req_q[intf].last & vrf_req_valid_q[intf]) begin
        vrf_commit_done_d[intf] = 1'b1;
      end
    end

    // Clear the commit recording if all ports have done it
    if (&vrf_commit_done_d) begin
      vrf_commit_done_d = '0;
    end

    // Clear if the insn is retired
    if (vlsu_rsp_valid_o) begin
      vrf_commit_done_d = '0;
    end
  end

  for (genvar intf = 0; intf < NrInterfaces; intf++) begin : gen_vrf_req_register_intf
    spill_register #(
      .T(vrf_req_t)
    ) i_vrf_req_register (
      .clk_i  (clk_i                ),
      .rst_ni (rst_ni               ),
      .data_i (vrf_req_d[intf]      ),
      .valid_i(vrf_req_valid_d[intf]),
      .ready_o(vrf_req_ready_d[intf]),
      .data_o (vrf_req_q[intf]      ),
      .valid_o(vrf_req_valid_q[intf]),
      .ready_i(vrf_req_ready_q[intf])
    );

    assign vrf_waddr_o[intf]     = vrf_req_q[intf].waddr;
    assign vrf_wdata_o[intf]     = vrf_req_q[intf].wdata;
    assign vrf_wbe_o[intf]       = vrf_req_q[intf].wbe;
    assign vrf_we_o[intf]        = vrf_req_valid_q[intf];
    assign vrf_id_o[intf]        = {vrf_req_q[intf].rsp.id, mem_spatz_req.id, commit_insn_q.id};
    assign vrf_req_ready_q[intf] = vrf_wvalid_i[intf];

    // Remember if the interfaces finished writing back to the VRF.
    // Clear this notebook once the memory instruction is over.

    // To check if one interface is ahead and has another response to be written to the VRF
    // In this case check if there is a response overlap
    // If there is a response overlap, do not clear the FF
    // If one interface has finished the commit earlier, we should also consider it as finish VRF writing.
    assign vrf_valid_rsp[intf] = vrf_req_q[intf].rsp_valid & vrf_req_valid_q[intf];
    assign resp_overlap[intf] = vrf_valid_rsp[intf] & vrf_valid_rsp_q[intf];

    // To track a valid response on an interface until both interfaces finish and can send to the VRF
    // When this happens the FF is cleared
    `FFLARNC(vrf_valid_rsp_q[intf], 1'b1, vrf_valid_rsp[intf], vlsu_rsp_valid_o & ~resp_overlap[intf], 1'b0, clk_i, rst_ni)

    // Check if either a previously tracked response or there is a response in the current cycle
    assign vrf_commit_intf_valid[intf] = vrf_valid_rsp[intf] | vrf_valid_rsp_q[intf] | vrf_commit_done_q[intf];
    `FF(vrf_commit_intf_valid_q[intf], vrf_commit_intf_valid[intf], 1'b0);
  end

  ////////////////////////////
  // Response to Controller //
  ////////////////////////////

  // Ack when the vector store finishes, or when the vector load commits to the VRF.
  // With more than an interface, we need to wait until all the interfaces commit to the VRF.

  // Check if interface 1 is the interface trying to commit, if so take resp information from interface 1
  // If both interfaces in sync, interface 1 is given priority
  assign resp_intf = vrf_commit_intf_valid_q [1] == 1'b0 ? 1'b1 : 1'b0;
  assign vlsu_rsp_o = &vrf_commit_intf_valid && |vrf_req_valid_q ? vrf_req_q[resp_intf].rsp   : '{id: commit_insn_q.id, default: '0};

  // Send response back to the controller to indicate end of request
  // Check if both the interfaces have completed request and have a valid response to send
  // Check if atleast one interface has a valid (interfaces can send responses asynchronously, but they finish the request together)
  // Set reponse high if one of the interfaces has a ready indicating the response has been written to the VRF
  assign vlsu_rsp_valid_o = &vrf_commit_intf_valid && |vrf_req_valid_q ? |vrf_req_ready_q : vlsu_finished_req && !commit_insn_q.is_load;

  //////////////
  // Counters //
  //////////////

  // Do we need to catch up to reach element idx parity? (Because of non-zero vstart)
  vlen_t vreg_start_0;
  assign vreg_start_0 = vlen_t'(commit_insn_q.vstart[$clog2(ELENB)-1:0]);

  logic [NrInterfaces-1:0] [N_FU-1:0] catchup;
  for (genvar intf = 0; intf < NrInterfaces; intf++) begin: gen_catchup_intf
    for (genvar fu = 0; fu < N_FU; fu++) begin: gen_catchup_intf_fu
      assign catchup[intf][fu] = (commit_counter_q[intf][fu] < vreg_start_0) & (commit_counter_max[intf][fu] != commit_counter_q[intf][fu]);
    end: gen_catchup_intf_fu
  end: gen_catchup_intf

  for (genvar intf = 0; intf < NrInterfaces; intf++) begin: gen_vreg_counter_proc
    for (genvar fu = 0; fu < N_FU; fu++) begin: gen_vreg_counter_proc
      localparam int unsigned port = intf * N_FU + fu;

      // The total amount of vector bytes we have to work through
      vlen_t max_bytes;

      always_comb begin
        // Default value
        max_bytes = (commit_insn_q.vl >> $clog2(NrMemPorts*ELENB)) << $clog2(ELENB);

        // Full transfer
        if (commit_insn_q.vl[$clog2(ELENB) +: $clog2(NrMemPorts)] > port)
          max_bytes += ELENB;
        else if (commit_insn_q.vl[$clog2(NrMemPorts*ELENB)-1:$clog2(ELENB)] == port)
          max_bytes += commit_insn_q.vl[$clog2(ELENB)-1:0];

        commit_counter_load[intf][fu] = commit_insn_pop;
        commit_counter_d[intf][fu]    = (commit_insn_q.vstart >> $clog2(NrMemPorts*ELENB)) << $clog2(ELENB);
        if (commit_insn_q.vstart[$clog2(NrMemPorts*ELENB)-1:$clog2(ELENB)] > port)
          commit_counter_d[intf][fu] += ELENB;
        else if (commit_insn_q.vstart[idx_width(NrMemPorts*ELENB)-1:$clog2(ELENB)] == port)
          commit_counter_d[intf][fu] += commit_insn_q.vstart[$clog2(ELENB)-1:0];
        commit_operation_valid[intf][fu] = commit_insn_valid && (commit_counter_q[intf][fu] != max_bytes) && (catchup[intf][fu] || (!catchup[intf][fu] && ~|catchup));
        commit_operation_last[intf][fu]  = commit_operation_valid[intf][fu] && ((max_bytes - commit_counter_q[intf][fu]) <= (commit_is_single_element_operation ? commit_single_element_size : ELENB));
        commit_counter_delta[intf][fu]   = !commit_operation_valid[intf][fu] ? vlen_t'('d0) : commit_is_single_element_operation ? vlen_t'(commit_single_element_size) : commit_operation_last[intf][fu] ? (max_bytes - commit_counter_q[intf][fu]) : vlen_t'(ELENB);
        commit_counter_en[intf][fu]      = commit_operation_valid[intf][fu] && (commit_insn_q.is_load && vrf_req_valid_d[intf] && vrf_req_ready_d[intf]) || (!commit_insn_q.is_load && vrf_rvalid_i[intf][0] && vrf_re_o[intf][0] && (!mem_is_indexed || vrf_rvalid_i[intf][1]));

        commit_counter_max[intf][fu]     = max_bytes;
      end
    end
  end


  for (genvar intf = 0; intf < NrInterfaces; intf++) begin: gen_vd_elem_id
    assign vd_elem_id[intf] = (commit_counter_q[intf][0] > vreg_start_0)
                            ? commit_counter_q[intf][0] >> $clog2(ELENB)
                            : commit_counter_q[intf][3] >> $clog2(ELENB);
  end

  for (genvar intf = 0; intf < NrInterfaces; intf++) begin: gen_mem_counter_proc_intf
    for (genvar fu = 0; fu < N_FU; fu++) begin: gen_mem_counter_proc_intf_fu
      localparam int unsigned port = intf * N_FU + fu;

      // The total amount of vector bytes we have to work through
      vlen_t max_bytes;

      always_comb begin
        // Default value
        max_bytes = (mem_spatz_req.vl >> $clog2(NrMemPorts*MemDataWidthB)) << $clog2(MemDataWidthB);

        if (NrMemPorts == 1)
          max_bytes = mem_spatz_req.vl;
        else
          if (mem_spatz_req.vl[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] > port)
            max_bytes += MemDataWidthB;
          else if (mem_spatz_req.vl[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] == port)
            max_bytes += mem_spatz_req.vl[$clog2(MemDataWidthB)-1:0];

        mem_operation_valid[intf][fu] = mem_spatz_req_valid && (max_bytes != mem_counter_q[intf][fu]);
        mem_operation_last[intf][fu]  = mem_operation_valid[intf][fu] && ((max_bytes - mem_counter_q[intf][fu]) <= (mem_is_single_element_operation ? mem_single_element_size : MemDataWidthB));
        mem_counter_load[intf][fu]    = mem_spatz_req_ready;
        mem_counter_d[intf][fu]       = (mem_spatz_req.vstart >> $clog2(NrMemPorts*MemDataWidthB)) << $clog2(MemDataWidthB);
        if (NrMemPorts == 1)
          mem_counter_d[intf][fu] = mem_spatz_req.vstart;
        else
          if (mem_spatz_req.vstart[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] > port)
            mem_counter_d[intf][fu] += MemDataWidthB;
          else if (mem_spatz_req.vstart[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] == port)
            mem_counter_d[intf][fu] += mem_spatz_req.vstart[$clog2(MemDataWidthB)-1:0];
        mem_counter_delta[intf][fu] = !mem_operation_valid[intf][fu] ? 'd0 : mem_is_single_element_operation ? mem_single_element_size : mem_operation_last[intf][fu] ? (max_bytes - mem_counter_q[intf][fu]) : MemDataWidthB;
        mem_counter_en[intf][fu]    = spatz_mem_req_ready[intf][fu] && spatz_mem_req_valid[intf][fu];
        mem_counter_max[intf][fu]   = max_bytes;

        // Index counter
        mem_idx_counter_d[intf][fu]     = mem_counter_d[intf][fu];
        mem_idx_counter_delta[intf][fu] = !mem_operation_valid[intf][fu] ? 'd0 : mem_idx_single_element_size;
      end
    end
  end

  ///////////
  // State //
  ///////////

  always_comb begin: p_state
    // Maintain state
    state_d = state_q;
    for (int intf = 0; intf < NrInterfaces; intf++) begin
      for (int fu = 0; fu < N_FU; fu++) begin
        automatic int port = intf * N_FU + fu;

        port_state_d[intf][fu] = port_state_q[intf][fu];
        unique case (port_state_q[intf][fu])
          VLSU_RunningLoad: begin
            if (commit_insn_valid && !commit_insn_q.is_load)
              if (rob_empty[intf][fu])
                port_state_d[intf][fu] = VLSU_RunningStore;
          end

          // Do not switch mode until all store requests are actually sent
          VLSU_RunningStore: begin
            if (spatz_mem_req_valid_o[port])
              port_state_d[intf][fu] = spatz_mem_req_o[port].write ? VLSU_RunningStore : VLSU_RunningLoad;
          end

          default:;
        endcase
      end
    end

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
  id_t  [NrInterfaces-1:0] [N_FU-1:0]                   mem_req_id;
  logic [NrInterfaces-1:0] [N_FU-1:0][MemDataWidth-1:0] mem_req_data;
  logic [NrInterfaces-1:0] [N_FU-1:0]                   mem_req_svalid;
  logic [NrInterfaces-1:0] [N_FU-1:0][ELEN/8-1:0]       mem_req_strb;
  logic [NrInterfaces-1:0] [N_FU-1:0]                   mem_req_lvalid;
  logic [NrInterfaces-1:0] [N_FU-1:0]                   mem_req_last;

  // Number of pending requests
  logic [NrInterfaces-1:0] [N_FU-1:0][idx_width(NrOutstandingLoads):0] mem_pending_d, mem_pending_q;
  logic [NrInterfaces-1:0] [N_FU-1:0] mem_pending;
  `FF(mem_pending_q, mem_pending_d, '{default: '0})
  always_comb begin
    // Maintain state
    mem_pending_d = mem_pending_q;

    for (int intf = 0; intf < NrInterfaces; intf++) begin
      for (int fu = 0; fu < N_FU; fu++) begin
        mem_pending[intf][fu] = mem_pending_q[intf][fu] != '0;

        // New request sent
        if (mem_is_load && spatz_mem_req_valid[intf][fu] && spatz_mem_req_ready[intf][fu])
          mem_pending_d[intf][fu]++;

        // Response used
        if (commit_insn_q.is_load && rob_rvalid[intf][fu] && rob_pop[intf][fu])
          mem_pending_d[intf][fu]--;
      end
    end
  end

  // Debug flags
  logic [NrInterfaces-1:0][N_FU-1:0] load_flag, store_flag, clear_flag;
  // Indicate if each port is in loading or storing mode
  logic [NrInterfaces-1:0][N_FU-1:0] port_state_load;
  for (genvar intf = 0; intf < NrInterfaces; intf++) begin
    for (genvar fu = 0; fu < N_FU; fu++) begin
      assign port_state_load[intf][fu] = (port_state_q[intf][fu] == VLSU_RunningLoad);
    end
  end

  // verilator lint_off LATCH
  always_comb begin
    for (int intf = 0; intf < NrInterfaces; intf++) begin
      vrf_raddr_o[intf] = {vs2_vreg_idx_addr[intf], vd_vreg_addr[intf]};
      vrf_re_o[intf]        = '0;
      vrf_req_d[intf]       = '0;
      vrf_req_valid_d[intf] = '0;

      rob_wdata[intf]  = '0;
      rob_wid[intf]    = '0;
      rob_push[intf]   = '0;
      rob_pop[intf]    = '0;
      rob_req_id[intf] = '0;

      mem_req_id[intf]     = '0;
      mem_req_data[intf]   = '0;
      mem_req_strb[intf]   = '0;
      mem_req_svalid[intf] = '0;
      mem_req_lvalid[intf] = '0;
      mem_req_last[intf]   = '0;

      spatz_mem_rsp_ready_o[intf*N_FU +: N_FU] = {N_FU{1'b1}};

      // Debugging flags
      load_flag[intf]  = '0;
      store_flag[intf] = '0;
      clear_flag[intf] = '0;

      // Propagate request ID
      vrf_req_d[intf].rsp.id    = commit_insn_q.id;
      vrf_req_d[intf].rsp.intf_id = intf;
      vrf_req_d[intf].rsp_valid = commit_insn_valid && &commit_finished_d[intf] && mem_insn_finished_d[commit_insn_q.id];
      // For last commit on the current interface, we do not check mem_insn_finished, because it checks both interface.
      vrf_req_d[intf].last      = commit_insn_valid && &commit_finished_d[intf];

      // Request indexes
      vrf_re_o[intf][1] = mem_is_indexed;

      // Count which vs2 element we should load (indexed loads)
      vs2_elem_id_d = vs2_elem_id_q;
      for (int intf = 0; intf < NrInterfaces; intf++) begin
        if (&(pending_index[intf] ^ ~mem_operation_valid[intf]) && mem_is_indexed)
          vs2_elem_id_d[intf] = vs2_elem_id_q[intf] + 1;
      end
      if (mem_spatz_req_ready)
        vs2_elem_id_d = '0;

      if (commit_insn_valid && commit_insn_q.is_load) begin
        // If we have a valid element in the buffer, store it back to the register file
        if (state_q == VLSU_RunningLoad && |commit_operation_valid[intf]) begin
          // Enable write back from an interface to the VRF if we have a valid element in all
          // the interface buffers that still have to write something back.
          vrf_req_d[intf].waddr = vd_vreg_addr[intf];
          // Here, we do not enable the writing to ROB unless all ports are in loading mode (avoid racing in ports)
          vrf_req_valid_d[intf] = &(rob_rvalid[intf] | (~mem_pending[intf] && port_state_load[intf])) && |mem_pending[intf];

          for (int unsigned fu = 0; fu < N_FU; fu++) begin
            automatic int unsigned port = intf * N_FU + fu;
            automatic logic [63:0] data = rob_rdata[intf][fu];

            // Shift data to correct position if we have an unaligned memory request
            if (MAXEW == EW_32) begin
              unique case ((commit_insn_q.is_strided || commit_insn_q.is_indexed) ? vreg_addr_offset[intf][fu] : commit_insn_q.rs1[1:0])
                2'b01: data   = {data[7:0], data[31:8]};
                2'b10: data   = {data[15:0], data[31:16]};
                2'b11: data   = {data[23:0], data[31:24]};
                default: data = data;
              endcase
            end else begin
              unique case ((commit_insn_q.is_strided || commit_insn_q.is_indexed) ? vreg_addr_offset[intf][fu] : commit_insn_q.rs1[2:0])

                3'b001: data  = {data[7:0], data[63:8]};
                3'b010: data  = {data[15:0], data[63:16]};
                3'b011: data  = {data[23:0], data[63:24]};
                3'b100: data  = {data[31:0], data[63:32]};
                3'b101: data  = {data[39:0], data[63:40]};

                3'b110: data  = {data[47:0], data[63:48]};
                3'b111: data  = {data[55:0], data[63:56]};
                default: data = data;
              endcase
            end

            // Pop stored element and free space in buffer
            rob_pop[intf][fu] = rob_rvalid[intf][fu] && vrf_req_valid_d[intf] && vrf_req_ready_d[intf] && commit_counter_en[intf][fu];
            load_flag[intf][fu] = 1'b1;

            // Shift data to correct position if we have a strided memory access
            if (commit_insn_q.is_strided || commit_insn_q.is_indexed)
              if (MAXEW == EW_32)
                unique case (commit_counter_q[intf][fu][1:0])
                  2'b01: data   = {data[23:0], data[31:24]};
                  2'b10: data   = {data[15:0], data[31:16]};
                  2'b11: data   = {data[7:0], data[31:8]};
                  default: data = data;
                endcase
              else
                unique case (commit_counter_q[intf][fu][2:0])
                  3'b001: data  = {data[55:0], data[63:56]};
                  3'b010: data  = {data[47:0], data[63:48]};
                  3'b011: data  = {data[39:0], data[63:40]};
                  3'b100: data  = {data[31:0], data[63:32]};
                  3'b101: data  = {data[23:0], data[63:24]};
                  3'b110: data  = {data[15:0], data[63:16]};
                  3'b111: data  = {data[7:0], data[63:8]};
                  default: data = data;
                endcase
            vrf_req_d[intf].wdata[ELEN*fu +: ELEN] = data;

            // Create write byte enable mask for register file
            if (commit_counter_en[intf][fu])
              if (commit_is_single_element_operation) begin
                automatic logic [$clog2(ELENB)-1:0] shift = commit_counter_q[intf][fu][$clog2(ELENB)-1:0];
                automatic logic [ELENB-1:0] mask          = '1;
                case (commit_insn_q.vsew)
                  EW_8 : mask   = 1;
                  EW_16: mask   = 3;
                  EW_32: mask   = 15;
                  default: mask = '1;
                endcase
                vrf_req_d[intf].wbe[ELENB*fu +: ELENB] = mask << shift;
              end else
                for (int unsigned k = 0; k < ELENB; k++)
                  vrf_req_d[intf].wbe[ELENB*fu+k] = k < commit_counter_delta[intf][fu];
          end
        end

        for (int unsigned fu = 0; fu < N_FU; fu++) begin
          automatic int unsigned port = intf * N_FU + fu;

          // Write the load result to the buffer
          rob_wdata[intf][fu] = spatz_mem_rsp_i[port].data;
`ifdef MEMPOOL_SPATZ
          rob_wid[intf][fu]   = spatz_mem_rsp_i[port].id;
          // Need to consider out-of-order memory response
          rob_push[intf][fu]  = spatz_mem_rsp_valid_i[port] && (state_q == VLSU_RunningLoad) && spatz_mem_rsp_i[port].write == '0;
`else
          rob_wid[intf][fu]   = spatz_mem_rsp_i[port].user.req_id;
          rob_push[intf][fu]  = spatz_mem_rsp_valid_i[port] && (state_q == VLSU_RunningLoad) && store_count_q[intf][fu] == '0;
`endif
          if (!rob_full[intf][fu] && !offset_queue_full[intf][fu] && mem_operation_valid[intf][fu]) begin
            rob_req_id[intf][fu]     = spatz_mem_req_ready[intf][fu] & spatz_mem_req_valid[intf][fu];
            mem_req_lvalid[intf][fu] = (!mem_is_indexed || (vrf_rvalid_i[intf][1] && !pending_index[intf][fu])) && mem_spatz_req.op_mem.is_load;
            mem_req_id[intf][fu]     = rob_id[intf][fu];
            mem_req_last[intf][fu]   = mem_operation_last[intf][fu];
          end
        end
      // Store operation
      end else begin
        // Read new element from the register file and store it to the buffer
        if (state_q == VLSU_RunningStore && !(|rob_full) && |commit_operation_valid[intf]) begin
          vrf_re_o[intf][0] = 1'b1;

          for (int unsigned fu = 0; fu < N_FU; fu++) begin
            automatic int unsigned port = intf * N_FU + fu;

            rob_wdata[intf][fu]  = vrf_rdata_i[intf][0][ELEN*fu +: ELEN];
            rob_wid[intf][fu]    = rob_id[intf][fu];
            rob_req_id[intf][fu] = vrf_rvalid_i[intf][0] && (!mem_is_indexed || vrf_rvalid_i[intf][1]);
            rob_push[intf][fu]   = rob_req_id[intf][fu];
          end
        end

        for (int unsigned fu = 0; fu < N_FU; fu++) begin
          // Read element from buffer and execute memory request
          if (mem_operation_valid[intf][fu]) begin
            automatic logic [63:0] data = rob_rdata[intf][fu];

            // Shift data to lsb if we have a strided or indexed memory access
            if (mem_is_strided || mem_is_indexed)
              if (MAXEW == EW_32)
                unique case (mem_counter_q[intf][fu][1:0])
                  2'b01: data = {data[7:0], data[31:8]};
                  2'b10: data = {data[15:0], data[31:16]};
                  2'b11: data = {data[23:0], data[31:24]};
                  default:; // Do nothing
                endcase
              else
                unique case (mem_counter_q[intf][fu][2:0])
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
              unique case ((mem_is_strided || mem_is_indexed) ? mem_req_addr_offset[intf][fu] : mem_spatz_req.rs1[1:0])
                2'b01: mem_req_data[intf][fu]   = {data[23:0], data[31:24]};
                2'b10: mem_req_data[intf][fu]   = {data[15:0], data[31:16]};
                2'b11: mem_req_data[intf][fu]   = {data[7:0], data[31:8]};
                default: mem_req_data[intf][fu] = data;
              endcase
            else
              unique case ((mem_is_strided || mem_is_indexed) ? mem_req_addr_offset[intf][fu] : mem_spatz_req.rs1[2:0])
                3'b001: mem_req_data[intf][fu]  = {data[55:0], data[63:56]};
                3'b010: mem_req_data[intf][fu]  = {data[47:0], data[63:48]};
                3'b011: mem_req_data[intf][fu]  = {data[39:0], data[63:40]};
                3'b100: mem_req_data[intf][fu]  = {data[31:0], data[63:32]};
                3'b101: mem_req_data[intf][fu]  = {data[23:0], data[63:24]};
                3'b110: mem_req_data[intf][fu]  = {data[15:0], data[63:16]};
                3'b111: mem_req_data[intf][fu]  = {data[7:0], data[63:8]};
                default: mem_req_data[intf][fu] = data;
              endcase

            mem_req_svalid[intf][fu] = rob_rvalid[intf][fu] && (!mem_is_indexed || (vrf_rvalid_i[intf][1] && !pending_index[intf][fu])) && !mem_spatz_req.op_mem.is_load;
            mem_req_id[intf][fu]     = rob_rid[intf][fu];
            mem_req_last[intf][fu]   = mem_operation_last[intf][fu];
            rob_pop[intf][fu]        = spatz_mem_req_valid[intf][fu] && spatz_mem_req_ready[intf][fu];

            // Create byte enable signal for memory request
            if (mem_is_single_element_operation) begin
              automatic logic [$clog2(ELENB)-1:0] shift = (mem_is_strided || mem_is_indexed) ? mem_req_addr_offset[intf][fu] : mem_counter_q[intf][fu][$clog2(ELENB)-1:0] + commit_insn_q.rs1[int'(MAXEW)-1:0];
              automatic logic [MemDataWidthB-1:0] mask  = '1;
              case (mem_spatz_req.vtype.vsew)
                EW_8 : mask   = 1;
                EW_16: mask   = 3;
                EW_32: mask   = 15;
                default: mask = '1;
              endcase
              mem_req_strb[intf][fu] = mask << shift;
            end else
              for (int unsigned k = 0; k < ELENB; k++)
                mem_req_strb[intf][fu][k] = k < mem_counter_delta[intf][fu];
          end else begin
            // We are not ready for new reponses
            automatic int port = intf * N_FU + fu;
            spatz_mem_rsp_ready_o[port] = 1'b0;
          end
        end
      end
    end
  end
  // verilator lint_on LATCH

  // Create memory requests
  for (genvar intf = 0; intf < NrInterfaces; intf++) begin : gen_mem_req
    for (genvar fu = 0; fu < N_FU; fu++) begin : gen_mem_req
      localparam int unsigned port = intf * N_FU + fu;

      spill_register #(
        .T(spatz_mem_req_t)
      ) i_spatz_mem_req_register (
        .clk_i   (clk_i                          ),
        .rst_ni  (rst_ni                         ),
        .data_i  (spatz_mem_req[intf][fu]        ),
        .valid_i (spatz_mem_req_valid[intf][fu]  ),
        .ready_o (spatz_mem_req_ready[intf][fu]  ),
        .data_o  (spatz_mem_req_o[port]          ),
        .valid_o (spatz_mem_req_valid_o[port]    ),
        .ready_i (spatz_mem_req_ready_i[port]    )
      );
`ifdef MEMPOOL_SPATZ
      // ID is required in Mempool-Spatz
      assign spatz_mem_req[intf][fu].id    = mem_req_id[intf][fu];
      assign spatz_mem_req[intf][fu].addr  = mem_req_addr[intf][fu];
      assign spatz_mem_req[intf][fu].mode  = '0; // Request always uses user privilege level
      assign spatz_mem_req[intf][fu].size  = mem_spatz_req.vtype.vsew[1:0];
      assign spatz_mem_req[intf][fu].write = !mem_is_load;
      assign spatz_mem_req[intf][fu].strb  = mem_req_strb[intf][fu];
      assign spatz_mem_req[intf][fu].data  = mem_req_data[intf][fu];
      assign spatz_mem_req[intf][fu].last  = mem_req_last[intf][fu];
      assign spatz_mem_req[intf][fu].spec  = 1'b0; // Request is never speculative
      assign spatz_mem_req_valid[intf][fu] = mem_req_svalid[intf][fu] || mem_req_lvalid[intf][fu];
`else
      assign spatz_mem_req[intf][fu].addr  = mem_req_addr[intf][fu];
      assign spatz_mem_req[intf][fu].write = !mem_is_load;
      assign spatz_mem_req[intf][fu].amo   = reqrsp_pkg::AMONone;
      assign spatz_mem_req[intf][fu].data  = mem_req_data[intf][fu];
      assign spatz_mem_req[intf][fu].strb  = mem_req_strb[intf][fu];
      assign spatz_mem_req[intf][fu].user.req_id   = mem_req_id[intf][fu];
      assign spatz_mem_req[intf][fu].user.core_id  = '0;
      assign spatz_mem_req[intf][fu].user.is_core  = '0;
      assign spatz_mem_req_valid[intf][fu] = mem_req_svalid[intf][fu] || mem_req_lvalid[intf][fu];
`endif
    end
  end

  ////////////////
  // Assertions //
  ////////////////

  if (MemDataWidth != ELEN)
    $error("[spatz_vlsu] The memory data width needs to be equal to %d.", ELEN);

  if (NrMemPorts != 2**$clog2(NrMemPorts))
    $error("[spatz_vlsu] The NrMemPorts parameter needs to be a power of two");

endmodule : spatz_vlsu
