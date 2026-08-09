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
`ifdef VENTAGLIO
  import vtl_pkg::*;
`endif
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
    output logic [N_FU*(ELEN+7)-1:0]        vrf_wdata_ecc_o,
    output logic                            vrf_we_o,
    output vrf_be_t                         vrf_wbe_o,
    input  logic                            vrf_wvalid_i,
    output spatz_id_t      [2:0]            vrf_id_o,
    output vrf_addr_t      [1:0]            vrf_raddr_o,
    output logic           [1:0]            vrf_re_o,
    input  logic [1:0][N_FU*(ELEN+7)-1:0]   vrf_rdata_ecc,
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
    output logic                            spatz_mem_str_finished_o,
    // ECC error outputs
    output logic [N_FU-1:0]                 vlsu_vs2_sec_err_o,   // per-cut SEC on VS2 index read
    output logic [N_FU-1:0]                 vlsu_vs2_ded_err_o,   // per-cut DED on VS2 index read
    output logic                            vlsu_ld_sec_err_o,    // any-port SEC on load response
    output logic                            vlsu_ld_ded_err_o     // any-port DED on load response
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
`ifdef VENTAGLIO
  typedef logic [$clog2(NrWordsPerVector*8)+1:0] vreg_elem_t;
`else
  typedef logic [$clog2(NrWordsPerVector*8)-1:0] vreg_elem_t;
`endif

  ///////////////////////
  //  Operation queue  //
  ///////////////////////

  spatz_req_t spatz_req_d;

  spatz_req_t mem_spatz_req;
  logic       mem_spatz_req_valid;
  logic       mem_spatz_req_ready;

  logic spatz_req_ready;

  spill_register #(
    .T(spatz_req_t)
  ) i_operation_queue (
    .clk_i  (clk_i                                          ),
    .rst_ni (rst_ni                                         ),
    .data_i (spatz_req_d                                    ),
    .valid_i(spatz_req_valid_i && spatz_req_i.ex_unit == LSU),
    .ready_o(spatz_req_ready                                ),
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

`ifdef VENTAGLIO
    // For VLX (Ventaglio indexed load), the effective number of memory beats
    // is reduced by the index width: VL elements packed into an index vector
    // of width `sp_cfg_index_width`. Recompute the byte-VL we feed to the
    // memory stage accordingly.
    if (spatz_req_d.op_vtl.is_load_idx) begin
      unique case (spatz_req_d.op_vtl.sp_cfg.sp_cfg_index_width)
        IDXW_1  : spatz_req_d.vl = spatz_req_i.vl >> 4;
        IDXW_2  : spatz_req_d.vl = spatz_req_i.vl >> 2;
        IDXW_4  : spatz_req_d.vl = spatz_req_i.vl >> 1;
        IDXW_8  : spatz_req_d.vl = spatz_req_i.vl;
        default : spatz_req_d.vl = spatz_req_i.vl;
      endcase
    end
`endif
  end: proc_spatz_req

  // Only do the judgement when we have a valid instruction
  // This is used to protect false triggeering the counter in some corner cases
  // Do we have a strided memory access
  logic mem_is_strided;
  assign mem_is_strided = mem_spatz_req_valid &&
                          ((mem_spatz_req.op == VLSE) || (mem_spatz_req.op == VSSE));

  // Do we have an indexed memory access
  logic mem_is_indexed;
  assign mem_is_indexed = mem_spatz_req_valid &&
                          ((mem_spatz_req.op == VLXE) || (mem_spatz_req.op == VSXE));

  /////////////
  //  State  //
  /////////////

  typedef enum logic [1:0] {
    VLSU_RunningLoad, VLSU_RunningStore, VLSU_ReadingV0_t
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

  logic  [NrMemPorts-1:0][ELEN+7-1:0] rob_wdata;
  id_t   [NrMemPorts-1:0]             rob_wid;
  logic  [NrMemPorts-1:0]             rob_push;
  logic  [NrMemPorts-1:0]             rob_rvalid;
  logic  [NrMemPorts-1:0][ELEN+7-1:0] rob_rdata;
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
      .DataWidth(ELEN+7            ),
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
      .DATA_WIDTH(ELEN+7            ),
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

  // Is there are pending write request to be sent to the memory
  logic write_pending;

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

    logic vm;
    logic is_load;
    logic is_strided;
    logic is_indexed;
  } commit_metadata_t;

  commit_metadata_t commit_insn_d;
  logic             commit_insn_push;
  commit_metadata_t commit_insn_q;
  logic             commit_insn_pop;
  logic             commit_insn_empty, commit_insn_full;
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
    .full_o    (commit_insn_full ),
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
      vm        : mem_spatz_req.op_mem.vm,
      is_load   : mem_spatz_req.op_mem.is_load,
      is_strided: mem_is_strided,
      is_indexed: mem_is_indexed
  };

  assign spatz_req_ready_o = spatz_req_ready & !commit_insn_full;

  always_comb begin: queue_control
    // Maintain state
    mem_insn_finished_d = mem_insn_finished_q;
    mem_insn_pending_d  = mem_insn_pending_q;

    // Do not ack anything
    mem_spatz_req_ready = 1'b0;

    // Do not push anything to the metadata queue
    commit_insn_push = 1'b0;

    // Did we start a new instruction?
    if (mem_spatz_req_valid && !mem_insn_pending_q[mem_spatz_req.id] && !commit_insn_full) begin
      mem_insn_pending_d[mem_spatz_req.id] = 1'b1;
      commit_insn_push                     = 1'b1;
    end

    // Did an instruction finished its requests?
    if (&mem_port_finished_q & !write_pending) begin
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

  // Decoded VS2 word for indexed-load address computation (N_FU lanes decoded in parallel)
  logic [N_FU*ELEN-1:0] vs2_decoded;
  logic [N_FU-1:0] vs2_idx_sec_err, vs2_idx_ded_err;
  for (genvar cut = 0; cut < N_FU; cut++) begin : gen_vs2_idx_dec
    hsiao_ecc_dec #(
      .DataWidth(ELEN),
      .ProtWidth(7)
    ) i_vs2_idx_dec (
      .in        (vrf_rdata_ecc[1][(ELEN+7)*cut +: (ELEN+7)]),
      .out       (vs2_decoded[ELEN*cut +: ELEN]),
      .syndrome_o(),
      .err_o     ({vs2_idx_ded_err[cut], vs2_idx_sec_err[cut]})
    );
  end : gen_vs2_idx_dec
  assign vlsu_vs2_sec_err_o = vs2_idx_sec_err;
  assign vlsu_vs2_ded_err_o = vs2_idx_ded_err;

  // Decoded VD/V0.t-low word (port 0), needed since vrf_rdata_ecc carries the raw ECC codeword
  logic [N_FU*ELEN-1:0] vd_rdata_decoded;
  for (genvar cut = 0; cut < N_FU; cut++) begin : gen_vd_rdata_dec
    hsiao_ecc_dec #(
      .DataWidth(ELEN),
      .ProtWidth(7)
    ) i_vd_rdata_dec (
      .in        (vrf_rdata_ecc[0][(ELEN+7)*cut +: (ELEN+7)]),
      .out       (vd_rdata_decoded[ELEN*cut +: ELEN]),
      .syndrome_o(),
      .err_o     ()
    );
  end : gen_vd_rdata_dec

  elen_t [NrMemPorts-1:0] mem_req_addr;

  vrf_addr_t vd_vreg_addr;
  vrf_addr_t vs2_vreg_addr;
  vrf_addr_t v0_t_vreg_addr_lo, v0_t_vreg_addr_hi;

  // Current element index and byte index that are being accessed at the register file
  vreg_elem_t vd_elem_id;
  vreg_elem_t vs2_elem_id_d, vs2_elem_id_q;
  `FF(vs2_elem_id_q, vs2_elem_id_d, '0)

  // Total bytes of indexes consumed since instruction start
  vlen_t total_idx_bytes_q, total_idx_bytes_d;
  `FF(total_idx_bytes_q, total_idx_bytes_d, '0)

  logic fetch_next_idx_global;

  always_comb begin
    total_idx_bytes_d = total_idx_bytes_q;

    // Reset on new instruction
    if (mem_spatz_req_ready) begin
      total_idx_bytes_d = '0;
    end else begin
      for (int unsigned p = 0; p < NrMemPorts; p++) begin
        if (mem_counter_en[p])
          total_idx_bytes_d = total_idx_bytes_d + (vlen_t'(1) << mem_spatz_req.op_mem.ew);
      end
    end

    // Advance vs2_elem_id when we cross a VRF word boundary
    fetch_next_idx_global = mem_is_indexed && ((total_idx_bytes_d >> $clog2(VRFWordBWidth)) != (total_idx_bytes_q >> $clog2(VRFWordBWidth)));
  end


  // Calculate the memory address for each memory port
  addr_offset_t [NrMemPorts-1:0] mem_req_addr_offset;
  for (genvar port = 0; port < NrMemPorts; port++) begin: gen_mem_req_addr
    logic [31:0] addr;
    logic [31:0] stride;
    logic [31:0] offset;

    // Pre-shuffling index offset
    logic [$clog2(8*8):0] idx_offset; // Max index offset (in B) when 8 x 8B (num elements in one MAXEW x index width in bytes for 1 element)
    assign idx_offset = mem_idx_counter_q[port];

    logic signed [2:0] data_index_width_diff;
    logic [idx_width(N_FU*ELENB)-1:0] word_index;

    // Calculate shift amount for address normalization
    logic [$bits(vew_e)-1:0] log2_num_el_maxew;
    logic [$bits(vew_e)  :0] log2_num_idx_maxew_bytes;
    logic [2 * MAXEW     :0] num_idx_maxew_bytes;

    assign log2_num_el_maxew = MAXEW - mem_spatz_req.vtype.vsew;                       // Number of elements in MAXEW
    assign log2_num_idx_maxew_bytes = log2_num_el_maxew + mem_spatz_req.op_mem.ew;
    assign num_idx_maxew_bytes = 1'b1 << log2_num_idx_maxew_bytes;                     // Number of indices for MAXEW/SEW elements in bytes

    always_comb begin
      word_index = '0;
      addr = '0;
      stride ='0;
      offset ='0;
      stride = mem_is_strided ? mem_spatz_req.rs2 >> mem_spatz_req.vtype.vsew : 'd1;

      if (mem_is_indexed) begin
        // Compute word index from port offset, normalized index, and wrapped indices
        word_index = (port << log2_num_idx_maxew_bytes) +
                      (idx_offset & (num_idx_maxew_bytes - 1)) +
                      ((idx_offset >> log2_num_idx_maxew_bytes) << log2_num_idx_maxew_bytes) * NrMemPorts;

        // Index — use decoded VS2 word (N_FU decoders above)
        unique case (mem_spatz_req.op_mem.ew)
          EW_8 : offset   = $signed(vs2_decoded[8 * word_index +: 8]);
          EW_16: offset   = $signed(vs2_decoded[8 * word_index +: 16]);
          default: offset = $signed(vs2_decoded[8 * word_index +: 32]);
        endcase
      end else begin
        offset = ({mem_counter_q[port][$bits(vlen_t)-1:MAXEW] << $clog2(NrMemPorts), mem_counter_q[port][int'(MAXEW)-1:0]} + (port << MAXEW)) * stride;
      end

      addr                      = mem_spatz_req.rs1 + offset;
      mem_req_addr[port]        = (addr >> MAXEW) << MAXEW;
      mem_req_addr_offset[port] = addr[int'(MAXEW)-1:0];
    end
  end: gen_mem_req_addr

  logic v0_t_is_ready;
  // Reuse vrf_read[1] for V0 reading
  assign v0_t_is_ready   = (state_q == VLSU_ReadingV0_t) && (&vrf_rvalid_i);

  // v0 should be read from vrf
  logic [VLEN-1:0]  operand_v0_t,operand_v0_t_q;
  assign operand_v0_t = (state_q == VLSU_ReadingV0_t)? {vs2_decoded,vd_rdata_decoded}:'0;

  // Backup v0.t
  `FFL(operand_v0_t_q, operand_v0_t, v0_t_is_ready, '0)


  // Calculate the register file address
  always_comb begin : gen_vreg_addr
    vd_vreg_addr  = (commit_insn_q.vd << $clog2(NrWordsPerVector)) + $unsigned(vd_elem_id);
    vs2_vreg_addr = (mem_spatz_req.vs2 << $clog2(NrWordsPerVector)) + $unsigned(vs2_elem_id_q);

    v0_t_vreg_addr_lo = vrf_addr_t'(0);   // v0 word 0
    v0_t_vreg_addr_hi = vrf_addr_t'(1);   // v0 word 1
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
    logic [N_FU*(ELEN+7)-1:0] wdata;
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

  //////////////////////////
  //  VRF Write Interface  //
  //////////////////////////

  // Drive the VRF write port directly from the spill-register output. Each
  // partial (possibly sub-word) write is forwarded as-is, still encoded as a
  // 39-bit ECC codeword per FU lane — it is never decoded here. Merging of
  // successive partial writes that target the same VRF word (byte-level
  // read-modify-write across cycles) happens downstream, in spatz.sv's
  // gen_vlsu_vrf_ecc/vlsu_prev_merged_q logic, which is the only place the
  // codeword is decoded, merged with the accumulated bytes, and re-encoded
  // before reaching the VRF.
  assign vrf_waddr_o     = vrf_req_q.waddr;
  assign vrf_wdata_ecc_o = vrf_req_q.wdata;
  assign vrf_wbe_o       = vrf_req_q.wbe;
  assign vrf_we_o        = vrf_req_valid_q;
  assign vrf_id_o        = {vrf_req_q.rsp.id, mem_spatz_req.id, commit_insn_q.id};
  assign vrf_req_ready_q = vrf_wvalid_i;

  // Ack when the vector store finishes, or when the load's write commits to the VRF
  assign vlsu_rsp_o       = vrf_req_q.rsp_valid && vrf_req_valid_q ? vrf_req_q.rsp   : '{id: commit_insn_q.id, default: '0};
  assign vlsu_rsp_valid_o = vrf_req_q.rsp_valid && vrf_req_valid_q ? vrf_req_ready_q : vlsu_finished_req && !commit_insn_q.is_load;

  ///////////////////////////
  // VD Read-Modify-Write  //
  ///////////////////////////
  //
  // The VRF is ECC-protected at cut granularity: a write to a cut writes the
  // full codeword whenever any byte-enable bit in that cut is set (see
  // spatz_vrf_ecc.sv). A masked/narrow-element load's first write to a fresh
  // destination word can therefore clobber bytes it doesn't actually touch,
  // unless those bytes are explicitly merged in from the true old VRF value.
  // This section reads back the old VD word (idle port 0 during loads) and
  // merges it in, mirroring spatz_vfu.sv's VFU_RMW mechanism but scoped so it
  // only ever engages for instructions that actually need it.

  // Tracks whether the CURRENT destination word (vd_vreg_addr) has already
  // been committed to once by this instruction. Mirrors spatz.sv's own
  // vlsu_prev_waddr_q one stage earlier; since VLSU writes reach spatz.sv
  // strictly in order (through the spill register below), the two always
  // agree on "is this a new word" despite the time skew.
  vrf_addr_t vd_prev_commit_waddr_q, vd_prev_commit_waddr_d;
  logic      vd_prev_commit_valid_q, vd_prev_commit_valid_d;
  `FF(vd_prev_commit_waddr_q, vd_prev_commit_waddr_d, '0)
  `FF(vd_prev_commit_valid_q, vd_prev_commit_valid_d, 1'b0)

  logic vd_is_new_word;
  assign vd_is_new_word = !vd_prev_commit_valid_q || (vd_prev_commit_waddr_q != vd_vreg_addr);

  always_comb begin
    vd_prev_commit_waddr_d = vd_prev_commit_waddr_q;
    vd_prev_commit_valid_d = vd_prev_commit_valid_q;
    if (commit_insn_pop)
      vd_prev_commit_valid_d = 1'b0; // next instruction's first word is "new"
    else if (vrf_req_valid_d && vrf_req_ready_d && commit_insn_q.is_load) begin
      vd_prev_commit_waddr_d = vd_vreg_addr;
      vd_prev_commit_valid_d = 1'b1;
    end
  end

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
      commit_operation_valid[fu] = (state_q == VLSU_RunningLoad || state_q == VLSU_RunningStore)&& commit_insn_valid && (commit_counter_q[fu] != max_elements) && (catchup[fu] || (!catchup[fu] && ~|catchup));
      commit_operation_last[fu]  = commit_operation_valid[fu] && ((max_elements - commit_counter_q[fu]) <= (commit_is_single_element_operation ? commit_single_element_size : ELENB));
      commit_counter_delta[fu]   = !commit_operation_valid[fu] ? vlen_t'('d0) : commit_is_single_element_operation ? vlen_t'(commit_single_element_size) : commit_operation_last[fu] ? (max_elements - commit_counter_q[fu]) : vlen_t'(ELENB);
      commit_counter_en[fu]      = commit_operation_valid[fu] && (commit_insn_q.is_load && vrf_req_valid_d && vrf_req_ready_d) || (!commit_insn_q.is_load && vrf_rvalid_i[0] && vrf_re_o[0] && (!mem_is_indexed || vrf_rvalid_i[1]));
      commit_counter_max[fu]     = max_elements;
    end
  end

  assign vd_elem_id = (commit_counter_q[0] > vreg_start_0) ? commit_counter_q[0] >> $clog2(ELENB) : commit_counter_q[N_FU-1] >> $clog2(ELENB);

  // Does the current commit need an RMW merge against the true old VD word?
  // Conservative superset of VFU's exact per-cut wbe!=0&&wbe!=all1 check:
  // masked, OR single-element/narrow/strided/indexed/unaligned/vstart!=0, OR
  // a genuinely-partial tail (fewer than ELENB bytes left in this cut).
  // Computed only from signals already known before per-cut wbe is finalized,
  // so it doesn't require restructuring the load/store commit always_comb.
  logic [N_FU-1:0] commit_tail_partial;
  for (genvar fu = 0; fu < N_FU; fu++) begin : gen_commit_tail_partial
    assign commit_tail_partial[fu] = commit_operation_last[fu] && (commit_counter_delta[fu] != vlen_t'(ELENB));
  end : gen_commit_tail_partial

  logic vlsu_rmw_needed;
  assign vlsu_rmw_needed = commit_insn_valid && commit_insn_q.is_load && (state_q == VLSU_RunningLoad) &&
                            |commit_operation_valid && vd_is_new_word &&
                            (!commit_insn_q.vm || commit_is_single_element_operation || |commit_tail_partial);

  // Wait/capture sub-FSM: read back the old VD word via the otherwise-idle
  // port 0 (VLSU_VD_RD) when RMW is needed, then hold the decoded result
  // until this word's write has actually RETIRED (vrf_wvalid_i), not merely
  // been accepted into the elastic spill register below — this prevents a
  // second RMW-needing word's capture from clobbering the data still in use
  // by the first word's in-flight write.
  typedef enum logic { VLSU_LD_NORMAL, VLSU_LD_RMW } vlsu_ld_rmw_e;
  vlsu_ld_rmw_e vlsu_ld_rmw_q, vlsu_ld_rmw_d;
  `FF(vlsu_ld_rmw_q, vlsu_ld_rmw_d, VLSU_LD_NORMAL)

  logic                 vd_rmw_valid_q,    vd_rmw_valid_d;
  vrf_addr_t            vd_rmw_addr_q,     vd_rmw_addr_d;
  logic [N_FU*ELEN-1:0] vd_rmw_data_q,     vd_rmw_data_d;
  logic                 vd_rmw_inflight_q, vd_rmw_inflight_d;
  `FF(vd_rmw_valid_q,    vd_rmw_valid_d,    1'b0)
  `FF(vd_rmw_addr_q,     vd_rmw_addr_d,     '0)
  `FF(vd_rmw_data_q,     vd_rmw_data_d,     '0)
  `FF(vd_rmw_inflight_q, vd_rmw_inflight_d, 1'b0)

  logic vlsu_rmw_have_data;
  assign vlsu_rmw_have_data = vd_rmw_valid_q && (vd_rmw_addr_q == vd_vreg_addr);
  logic vlsu_rmw_commit_now;
  assign vlsu_rmw_commit_now = vlsu_rmw_needed && vlsu_rmw_have_data && !vd_rmw_inflight_q;

  always_comb begin : vlsu_ld_rmw_fsm
    vlsu_ld_rmw_d     = vlsu_ld_rmw_q;
    vd_rmw_valid_d    = vd_rmw_valid_q;
    vd_rmw_addr_d     = vd_rmw_addr_q;
    vd_rmw_data_d     = vd_rmw_data_q;
    vd_rmw_inflight_d = vd_rmw_inflight_q;

    unique case (vlsu_ld_rmw_q)
      VLSU_LD_NORMAL:
        if (vlsu_rmw_needed && !vd_rmw_inflight_q && !vlsu_rmw_have_data)
          vlsu_ld_rmw_d = VLSU_LD_RMW;
      VLSU_LD_RMW:
        // A masked instruction can flip state_q to VLSU_ReadingV0_t mid-wait
        // (right after being recognized, before its first element commits),
        // which redirects port 0's read address to the v0.t mask word — any
        // vrf_rvalid_i[0] pulse seen then belongs to THAT read, not ours.
        // Require state_q == VLSU_RunningLoad so we only ever accept a
        // response that was actually addressed at vd_vreg_addr; otherwise
        // just keep waiting (vrf_re_o[0] keeps re-requesting once the state
        // returns to VLSU_RunningLoad).
        if (vrf_rvalid_i[0] && (state_q == VLSU_RunningLoad)) begin
          vd_rmw_valid_d = 1'b1;
          vd_rmw_addr_d  = vd_vreg_addr;
          vd_rmw_data_d  = vd_rdata_decoded;
          vlsu_ld_rmw_d  = VLSU_LD_NORMAL;
        end
      default: vlsu_ld_rmw_d = VLSU_LD_NORMAL;
    endcase

    if (vlsu_rmw_commit_now && vrf_req_valid_d && vrf_req_ready_d)
      vd_rmw_inflight_d = 1'b1;
    if (vrf_wvalid_i && vd_rmw_inflight_q) begin
      vd_rmw_inflight_d = 1'b0;
      vd_rmw_valid_d    = 1'b0;
    end
  end : vlsu_ld_rmw_fsm

`ifndef SYNTHESIS
  logic [15:0] vlsu_rmw_wait_cycles_q;
  `FFL(vlsu_rmw_wait_cycles_q, (vlsu_ld_rmw_q == VLSU_LD_RMW) ? (vlsu_rmw_wait_cycles_q + 16'd1) : 16'd0,
       (vlsu_ld_rmw_q == VLSU_LD_RMW), 16'd0)
  // Liveness canary: a repeat of the earlier read-port-starvation deadlock
  // should show up here as an early, attributable warning instead of a
  // silent full-sim hang.
  always_ff @(posedge clk_i) begin
    if (vlsu_rmw_wait_cycles_q == 16'd400)
      $warning("[spatz_vlsu] VD RMW read-back has been waiting %0d cycles on VLSU_VD_RD \
- possible read-port starvation against a higher-priority port", vlsu_rmw_wait_cycles_q);
  end
`endif

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

  logic vlsu_rsp_valid_q; // register the instruction finish signal
  logic v0_t_is_ready_q;
  logic v0_t_read_done;
  `FFLARNC(v0_t_read_done,1'b1,v0_t_is_ready,vlsu_rsp_valid_o,1'b0,clk_i,rst_ni);
  `FF(v0_t_is_ready_q,v0_t_is_ready,'0);

  always_comb begin: p_state
    // Maintain state
    state_d = state_q;
    write_pending = 1'b0;

    for (int port = 0; port < NrMemPorts; port++) begin
      write_pending |= (spatz_mem_req_o[port].write & spatz_mem_req_valid_o[port]);
    end

    unique case (state_q)
      VLSU_RunningLoad: begin
        if(commit_insn_valid && !commit_insn_q.vm && !v0_t_read_done)
          state_d = VLSU_ReadingV0_t;
        if (commit_insn_valid && !commit_insn_q.is_load)
          if (&rob_empty)
            state_d = VLSU_RunningStore;
      end

      VLSU_ReadingV0_t:
        if(v0_t_is_ready & ~v0_t_is_ready_q) begin
          state_d = VLSU_RunningLoad;
          if (commit_insn_valid && !commit_insn_q.is_load)
            state_d = VLSU_RunningStore;
        end
        else state_d = state_q;

      VLSU_RunningStore: begin
        if(commit_insn_valid && !commit_insn_q.vm && !v0_t_read_done)
          state_d = VLSU_ReadingV0_t;
        if (commit_insn_valid && commit_insn_q.is_load)
          if (&rob_empty)
            if (!write_pending)
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
  logic [NrMemPorts-1:0][ELEN+7-1:0]      mem_req_data;
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

  // Generate masking based on v0.t
  logic [VLEN-1:0] vm_masking;

  always_comb begin
    vm_masking = '1;
    if(!commit_insn_q.vm) begin
      case (commit_insn_q.vsew)
        // i < (VLEN/vsew)*8 where 8 --> max lmul
        EW_8:for(int i=0;i<VLEN;i=i+1)begin
          vm_masking[i*1+:1] = {1{operand_v0_t_q[i]}};
        end
        EW_16:for(int i=0;i<(VLEN/2);i=i+1)begin
          vm_masking[i*2+:2] = {2{operand_v0_t_q[i]}};
        end
        EW_32: for(int i=0;i<(VLEN/4);i=i+1)begin
          vm_masking[i*4+:4] = {4{operand_v0_t_q[i]}};
        end
        default: if (MAXEW == EW_64) for(int i=0;i<(VLEN/8);i=i+1)begin
          vm_masking[i*8+:8] = {8{operand_v0_t_q[i]}};
        end
      endcase
    end
  end

  vlen_t commit_counter_sum,mem_counter_sum;
  vlen_t commit_slice_base, mem_slice_base;

  always_comb begin
     commit_counter_sum = '0;
     mem_counter_sum = '0;
    for (int unsigned port = 0; port < NrMemPorts; port++) begin
      commit_counter_sum += commit_counter_q[port];
      mem_counter_sum += mem_counter_q[port];
    end
    commit_slice_base = (commit_counter_sum >> $clog2(VRFWordBWidth)) << $clog2(VRFWordBWidth);
    mem_slice_base    = (mem_counter_sum    >> $clog2(VRFWordBWidth)) << $clog2(VRFWordBWidth);
  end


  // Intermediate wbe, before vm_masking.
  vrf_be_t load_wbe;

  // Monitor the vm_wbe selected from vm_masking
  vrf_be_t vm_wbe;

  // Final load-commit wbe before the RMW override (see vlsu_rmw_commit_now below)
  vrf_be_t vrf_wbe_pre;

  // Select 8-bit masking for each port, before reordering according to rs1
  logic [NrMemPorts-1:0][ELEN/8-1:0] vm_wbe_store;

  // Intermediate strb, before vm_masking.
  logic [NrMemPorts-1:0][ELEN/8-1:0] store_strb;

  // To monitor the vm_masking on each port.
  logic [NrMemPorts-1:0][ELEN/8-1:0] vm_strb;

  always_comb begin
    for (int port = 0; port < NrMemPorts; port++) begin
      vm_wbe_store[port] = commit_insn_q.vm ? {ELENB{1'b1}} : vm_masking[mem_slice_base + port*ELENB +: ELENB];
    end
  end

  // Load-side ECC decode/re-encode for byte-level data manipulation.
  //
  // rob_rdata stores a 39-bit ECC codeword. When the load commit path needs
  // byte-level realignment, the codeword must not be rotated directly.
  // Instead, decode to the 32-bit payload, realign the payload, and re-encode.
  logic [NrMemPorts-1:0][ELEN-1:0]   load_rsp_data_decoded;
  logic [NrMemPorts-1:0][ELEN-1:0]   load_rsp_data_aligned;
  logic [NrMemPorts-1:0][ELEN+7-1:0] load_rsp_data_encoded;
  logic [NrMemPorts-1:0]              load_rsp_sec_err, load_rsp_ded_err;

  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_load_rsp_ecc
    hsiao_ecc_dec #(
      .DataWidth(ELEN),
      .ProtWidth(7)
    ) i_load_rsp_ecc_dec (
      .in        (rob_rdata[port]),
      .out       (load_rsp_data_decoded[port]),
      .syndrome_o(),
      .err_o     ({load_rsp_ded_err[port], load_rsp_sec_err[port]})
    );

    hsiao_ecc_enc #(
      .DataWidth(ELEN),
      .ProtWidth(7)
    ) i_load_rsp_ecc_enc (
      .in  (load_rsp_data_aligned[port]),
      .out (load_rsp_data_encoded[port])
    );
  end
  assign vlsu_ld_sec_err_o = |load_rsp_sec_err;
  assign vlsu_ld_ded_err_o = |load_rsp_ded_err;

  // RMW merge: on the first write to a not-yet-touched word that needs it
  // (masked / narrow-element / partial-tail commit), merge this write's new
  // bytes with the captured pre-existing decoded VD codeword so bytes not
  // covered by this write's byte-enable keep their true old value (RVV
  // mask-undisturbed / tail-undisturbed) instead of being zeroed.
  logic [NrMemPorts-1:0][ELEN-1:0]   vlsu_rmw_merged_payload;
  logic [NrMemPorts-1:0][ELEN+7-1:0] vlsu_rmw_merged_enc;
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_vlsu_rmw_merge_ecc
    for (genvar b = 0; b < ELENB; b++) begin : gen_vlsu_rmw_merge_byte
      assign vlsu_rmw_merged_payload[port][8*b +: 8] =
        vrf_wbe_pre[ELENB*port + b] ? load_rsp_data_aligned[port][8*b +: 8]
                                     : vd_rmw_data_q[ELEN*port + 8*b +: 8];
    end : gen_vlsu_rmw_merge_byte
    hsiao_ecc_enc #(
      .DataWidth(ELEN),
      .ProtWidth(7)
    ) i_vlsu_rmw_merge_ecc_enc (
      .in  (vlsu_rmw_merged_payload[port]),
      .out (vlsu_rmw_merged_enc[port])
    );
  end : gen_vlsu_rmw_merge_ecc

  // verilator lint_off LATCH
  always_comb begin
    load_wbe = '0;

    vrf_raddr_o     = (state_q == VLSU_ReadingV0_t)? {v0_t_vreg_addr_hi, v0_t_vreg_addr_lo}:{vs2_vreg_addr, vd_vreg_addr};
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

    load_rsp_data_aligned = load_rsp_data_decoded;

    // Propagate request ID
    vrf_req_d.rsp.id    = commit_insn_q.id;
    vrf_req_d.rsp_valid = commit_insn_valid && &commit_finished_d && mem_insn_finished_d[commit_insn_q.id];

    // Request indexes
    vrf_re_o[1] = (state_q == VLSU_ReadingV0_t)? 1'b1:mem_is_indexed; // for indexed load/store we need to read vs2
    if (state_q == VLSU_ReadingV0_t)
      vrf_re_o[0] = 1'b1;
    // RMW read-back of the old VD word (see vlsu_ld_rmw_fsm above); mutually
    // exclusive with the V0.t/store uses of port 0 by construction, since
    // vlsu_rmw_needed requires state_q == VLSU_RunningLoad.
    if (vlsu_ld_rmw_q == VLSU_LD_RMW)
      vrf_re_o[0] = 1'b1;

    // Count which vs2 element we should load (indexed loads)
    vs2_elem_id_d = vs2_elem_id_q;
    if (fetch_next_idx_global)
      vs2_elem_id_d = vs2_elem_id_q + 1;
    if (mem_spatz_req_ready) // finish one instruction
      vs2_elem_id_d = '0;

    if (commit_insn_valid && commit_insn_q.is_load) begin
      // If we have a valid element in the buffer, put it back to the register file
      if (state_q == VLSU_RunningLoad && |commit_operation_valid) begin
        // Enable write back to the VRF if we have a valid element in all buffers that still have to write something back.
        vrf_req_d.waddr = vd_vreg_addr;
        // rob_rvalid: data is in the rob ready to be written back to VRF
        // vlsu_rmw_needed: stall this word's commit until the old VD word has
        // been read back and captured (vlsu_rmw_commit_now), so the RMW merge
        // below has real data instead of corrupting untouched bytes.
        vrf_req_valid_d = &(rob_rvalid | ~mem_pending) && |mem_pending && (commit_insn_q.vm || v0_t_read_done) &&
                          (!vlsu_rmw_needed || vlsu_rmw_commit_now);
        for (int unsigned port = 0; port < NrMemPorts; port++) begin
          automatic logic [63:0] data;
          data = '0;
          data[ELEN-1:0] = load_rsp_data_decoded[port];

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
          // vrf_req_d.wdata[(ELEN+7)*port +: (ELEN+7)] = data[ELEN+7-1:0];
          load_rsp_data_aligned[port] = data[ELEN-1:0];
          vrf_req_d.wdata[(ELEN+7)*port +: (ELEN+7)] =
            vlsu_rmw_commit_now ? vlsu_rmw_merged_enc[port] : load_rsp_data_encoded[port];

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

              load_wbe[ELENB*port +: ELENB] = (mask << shift);
              vm_wbe = vm_masking[commit_slice_base +: VRFWordBWidth];
              vrf_wbe_pre = load_wbe & vm_wbe;
            end
            else begin
              for (int unsigned k = 0; k < ELENB; k++) begin
                load_wbe[ELENB*port+k] = (k < commit_counter_delta[port]);
                vrf_wbe_pre = load_wbe & (commit_insn_q.vm ? {VRFWordBWidth{1'b1}} : vm_masking[commit_slice_base +: VRFWordBWidth]);
              end
            end
        end
        // RMW override: force a full-word write when this commit merges in
        // the old VD word, so spatz_vrf_ecc.sv's cut-granular write doesn't
        // leave any byte only partially updated.
        vrf_req_d.wbe = vlsu_rmw_commit_now ? {VRFWordBWidth{1'b1}} : vrf_wbe_pre;
      end

      for (int unsigned port = 0; port < NrMemPorts; port++) begin
        // Write the load result to the buffer
        rob_wdata[port] = spatz_mem_rsp_i[port].data;
`ifdef MEMPOOL_SPATZ
        rob_wid[port]   = spatz_mem_rsp_i[port].id;
        // Need to consider out-of-order memory response
        rob_push[port]  = spatz_mem_rsp_valid_i[port] && (state_q == VLSU_RunningLoad || state_q == VLSU_ReadingV0_t) && store_count_q[port] == '0;
`else
        rob_push[port]  = spatz_mem_rsp_valid_i[port] && (state_q == VLSU_RunningLoad || state_q == VLSU_ReadingV0_t) && store_count_q[port] == '0;
`endif
        if (!rob_full[port] && !offset_queue_full[port] && mem_operation_valid[port]) begin
          rob_req_id[port]     = spatz_mem_req_ready[port] & spatz_mem_req_valid[port];
          mem_req_lvalid[port] = (!mem_is_indexed || vrf_rvalid_i[1]) && mem_spatz_req.op_mem.is_load;
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
          rob_wdata[port]  = vrf_rdata_ecc[0][(ELEN+7)*port +: (ELEN+7)];
          rob_wid[port]    = rob_id[port];
          rob_req_id[port] = vrf_rvalid_i[0] && (!mem_is_indexed || vrf_rvalid_i[1]);
          rob_push[port]   = rob_req_id[port];
        end
      end

      for (int unsigned port = 0; port < NrMemPorts; port++) begin
        vm_strb[port] = vm_wbe_store[port];
        // Read element from buffer and execute memory request
        if (mem_operation_valid[port]) begin
          automatic logic [63:0] data = rob_rdata[port];

          // Shift data to lsb if we have a strided or indexed memory access
          if (mem_is_strided || mem_is_indexed)
            if (MAXEW == EW_32)
              unique case (mem_counter_q[port][1:0])
                2'b01: begin
                  data          = {data[7:0], data[31:8]};
                  vm_strb[port] = {vm_wbe_store[port][0],   vm_wbe_store[port][3:1]};
                end
                2'b10: begin
                  data          = {data[15:0], data[31:16]};
                  vm_strb[port] = {vm_wbe_store[port][1:0], vm_wbe_store[port][3:2]};
                end
                2'b11: begin
                  data          = {data[23:0], data[31:24]};
                  vm_strb[port] = {vm_wbe_store[port][2:0], vm_wbe_store[port][3]};
                end
                default: vm_strb[port] = vm_wbe_store[port];
              endcase
            else
              // Shift vm_masking along with data
              unique case (mem_counter_q[port][2:0])
                3'b001: begin
                  data = {data[7:0], data[63:8]};
                  vm_strb[port] = {vm_wbe_store[port][0],vm_wbe_store[port][7:1]};
                end
                3'b010: begin
                  data = {data[15:0], data[63:16]};
                  vm_strb[port] = {vm_wbe_store[port][1:0],vm_wbe_store[port][7:2]};
                end
                3'b011: begin
                  data = {data[23:0], data[63:24]};
                  vm_strb[port] = {vm_wbe_store[port][2:0],vm_wbe_store[port][7:3]};
                end
                3'b100: begin
                  data = {data[31:0], data[63:32]};
                  vm_strb[port] = {vm_wbe_store[port][3:0],vm_wbe_store[port][7:4]};
                end
                3'b101: begin
                  data = {data[39:0], data[63:40]};
                  vm_strb[port] = {vm_wbe_store[port][4:0],vm_wbe_store[port][7:5]};
                end
                3'b110: begin
                  data = {data[47:0], data[63:48]};
                  vm_strb[port] = {vm_wbe_store[port][5:0],vm_wbe_store[port][7:6]};
                end
                3'b111: begin
                  data = {data[55:0], data[63:56]};
                  vm_strb[port] = {vm_wbe_store[port][6:0],vm_wbe_store[port][7]};
                end
                default: vm_strb[port] = vm_wbe_store[port]; // Do nothing
              endcase

          // Shift data to correct position if we have an unaligned memory request
          if (MAXEW == EW_32)
              unique case ((mem_is_strided || mem_is_indexed) ? mem_req_addr_offset[port] : mem_spatz_req.rs1[1:0])
                2'b01: begin
                  mem_req_data[port] = {data[23:0], data[31:24]};
                  vm_strb[port]      = {vm_strb[port][2:0], vm_strb[port][3]};
                end
                2'b10: begin
                  mem_req_data[port] = {data[15:0], data[31:16]};
                  vm_strb[port]      = {vm_strb[port][1:0], vm_strb[port][3:2]};
                end
                2'b11: begin
                  mem_req_data[port] = {data[7:0], data[31:8]};
                  vm_strb[port]      = {vm_strb[port][0], vm_strb[port][3:1]};
                end
                default: mem_req_data[port] = data;
              endcase
          else
            unique case ((mem_is_strided || mem_is_indexed) ? mem_req_addr_offset[port] : mem_spatz_req.rs1[2:0])
              3'b001: begin
                // Reoreder vm_masking along with data
                mem_req_data[port]  = {data[55:0], data[63:56]};
                vm_strb[port] = {vm_strb[port][6:0],vm_strb[port][7]};
              end
              3'b010: begin
                mem_req_data[port]  = {data[47:0], data[63:48]};
                vm_strb[port] = {vm_strb[port][5:0],vm_strb[port][7:6]};
              end
              3'b011: begin
                mem_req_data[port]  = {data[39:0], data[63:40]};
                vm_strb[port] = {vm_strb[port][4:0],vm_strb[port][7:5]};
              end
              3'b100: begin
                mem_req_data[port]  = {data[31:0], data[63:32]};
                vm_strb[port] = {vm_strb[port][3:0],vm_strb[port][7:4]};
              end
              3'b101: begin
                mem_req_data[port]  = {data[23:0], data[63:24]};
                vm_strb[port] = {vm_strb[port][2:0],vm_strb[port][7:3]};
              end
              3'b110: begin
                mem_req_data[port]  = {data[15:0], data[63:16]};
                vm_strb[port] = {vm_strb[port][1:0],vm_strb[port][7:2]};
              end
              3'b111: begin
                mem_req_data[port]  = {data[7:0], data[63:8]};
                vm_strb[port] = {vm_strb[port][0],vm_strb[port][7:1]};
              end
              default: begin
                mem_req_data[port] = data;
                vm_strb[port] = vm_strb[port];
              end
            endcase

          mem_req_svalid[port] = rob_rvalid[port] && (!mem_is_indexed || vrf_rvalid_i[1]) && !mem_spatz_req.op_mem.is_load && (commit_insn_q.vm || v0_t_read_done);
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
            store_strb[port] = mask << shift;
            mem_req_strb[port] = store_strb[port] & vm_strb[port];
          end
          else begin
            for (int unsigned k = 0; k < ELENB; k++) begin
              store_strb[port][k] = k < mem_counter_delta[port];
            end
            mem_req_strb[port] = store_strb[port] & (commit_insn_q.vm ? {ELENB{1'b1}} : vm_masking[mem_slice_base + port*ELENB +: ELENB]);
          end
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
    assign spatz_mem_req_valid[port] = (state_q == VLSU_RunningLoad || state_q == VLSU_RunningStore)&&(mem_req_svalid[port] || mem_req_lvalid[port]);
`endif
  end

  ////////////////
  // Assertions //
  ////////////////

  if (MemDataWidth != ELEN)
    $error("[spatz_vlsu] The memory data width needs to be equal to %d.", ELEN);

  if (NrMemPorts != N_FU)
    $error("[spatz_vlsu] The number of memory ports needs to be equal to the number of FUs. NrMemPorts=%0d, N_FU=%0d",
         NrMemPorts, N_FU);

  if (NrMemPorts != 2**$clog2(NrMemPorts))
    $error("[spatz_vlsu] The NrMemPorts parameter needs to be a power of two");

endmodule : spatz_vlsu
