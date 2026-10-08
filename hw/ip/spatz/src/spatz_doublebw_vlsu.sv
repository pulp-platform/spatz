// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich
//         Matteo Perotti, ETH Zurich
//         Navaneeth Kunhi Purayil, ETH Zurich
//
// Vector and tile load/store unit with a configurable number of TCDM interfaces.

module spatz_doublebw_vlsu
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
    localparam int  unsigned IdWidth            = idx_width(NrOutstandingLoads),
    localparam int  unsigned SpatzMemBytes      = NrMemPorts * ELENB
  ) (
    input  logic                            clk_i,
    input  logic                            rst_ni,
    // Spatz request
    input  spatz_req_t                      spatz_req_i,
    input  logic                            spatz_req_valid_i,
    output logic                            spatz_req_ready_o,
    input  logic [NrParallelInstructions-1:0] matrix_enable_i,
    // VLSU response
    output logic                            vlsu_rsp_valid_o,
    output vlsu_rsp_t                       vlsu_rsp_o,
    input  logic                            vlsu_buf_empty_i,
    input  logic                            vlsu_buf_full_i,
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
    // Interface with the Tile
    output logic        tile_wvalid_o,
    output tile_w_req_t tile_w_req_o,
    input  logic        tile_wready_i,

    output logic        tile_rvalid_o,
    output tile_r_req_t tile_r_req_o,
    input  tile_row_t   tile_rdata_i,
    input  logic        tile_rready_i,
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
  localparam int unsigned MemDataWidthB = ELENB;

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

  logic spatz_req_ready, tile_req_ready;

  // For indexed operations op_mem.ew is the index EEW and the data EEW is
  // VSEW.  For all other memory operations op_mem.ew is the data EEW decoded
  // from the instruction.
  vew_e req_data_ew;
  assign req_data_ew = (spatz_req_i.op inside {VLXE, VSXE}) ? spatz_req_i.vtype.vsew : spatz_req_i.op_mem.ew;

  stream_fifo #(
    .FALL_THROUGH(1'b0       ),
    .DEPTH       (4          ),
    .T           (spatz_req_t)
  ) i_operation_queue (
    .clk_i     (clk_i                                          ),
    .rst_ni    (rst_ni                                         ),
    .flush_i   (1'b0                                           ),
    .testmode_i(1'b0                                           ),
    .usage_o   (/* Unused */                                   ),
    .data_i    (spatz_req_d                                    ),
    .valid_i   (spatz_req_valid_i && spatz_req_i.ex_unit == LSU),
    .ready_o   (spatz_req_ready                                ),
    .data_o    (mem_spatz_req                                  ),
    .valid_o   (mem_spatz_req_valid                            ),
    .ready_i   (mem_spatz_req_ready                            )
  );

  // Convert vl to bytes for address generation using the memory data EEW.
  always_comb begin: proc_spatz_req
    spatz_req_d = spatz_req_i;
    spatz_req_d.vl     = spatz_req_i.vl << req_data_ew;
    spatz_req_d.vstart = spatz_req_i.vstart << req_data_ew;
  end: proc_spatz_req

  // Tile requests use the tile datapath instead of the VRF datapath.
  logic mem_is_tile_mem;
  assign mem_is_tile_mem = mem_spatz_req_valid && mem_spatz_req.op_ope.is_mem;

  // Strided accesses use the scalar stride operand.
  logic mem_is_strided;
  assign mem_is_strided = mem_spatz_req_valid && !mem_is_tile_mem && ((mem_spatz_req.op == VLSE) || (mem_spatz_req.op == VSSE));

  // Indexed accesses use the vector index operand.
  logic mem_is_indexed;
  assign mem_is_indexed = mem_spatz_req_valid && !mem_is_tile_mem && ((mem_spatz_req.op == VLXE) || (mem_spatz_req.op == VSXE));

  vew_e mem_data_ew;
  assign mem_data_ew = mem_is_indexed ? mem_spatz_req.vtype.vsew : mem_spatz_req.op_mem.ew;

  /////////////
  //  State  //
  /////////////

  typedef enum logic {
    VLSU_RunningLoad, VLSU_RunningStore
  } state_t;
  state_t state_d, state_q;
  `FF(state_q, state_d, VLSU_RunningLoad)

  // Memory requests
  spatz_mem_req_t [NrInterfaces-1:0] [N_FU-1:0] spatz_mem_req;
  logic           [NrInterfaces-1:0] [N_FU-1:0] spatz_mem_req_valid;
  logic           [NrInterfaces-1:0] [N_FU-1:0] spatz_mem_req_ready;

  id_t [NrInterfaces-1:0] [N_FU-1:0] store_count_q;
  id_t [NrInterfaces-1:0] [N_FU-1:0] store_count_d;

  /*------------------------------------------------------------*/
  /*                Tile Memory Types and State                 */
  /*------------------------------------------------------------*/

  localparam int unsigned TileRowBytes = TileEdge * AccElemBytes;
  localparam int unsigned TileMaxChunks = (TileRowBytes + 2 * MemDataWidthB - 2) / MemDataWidthB;
  typedef logic [$clog2(TileRowBytes+1)-1:0] tile_byte_cnt_t;
  typedef logic [$clog2(TileMaxChunks+1)-1:0] tile_chunk_cnt_t;
  typedef logic [$clog2(TileMaxChunks+NrMemPorts+1)-1:0] tile_chunk_idx_t;
  typedef logic signed [$clog2(TileRowBytes+(NrMemPorts+2)*MemDataWidthB+1):0]
      tile_stream_offset_t;

  typedef struct packed {
    spatz_req_t              req;
    tile_r_req_t             access;
    tile_dim_t               row_idx;
    tile_count_t             elems;
    logic                    is_col;
    tile_byte_cnt_t          bytes;
    tile_chunk_cnt_t         chunk_sent;
    tile_row_t               packed_data;
    tile_row_t               row_buf;
  } tile_ctx_t;

  typedef struct packed {
    logic                     active;
    logic [31:0]              addr;
    logic [MemDataWidth-1:0]  data;
    logic [MemDataWidthB-1:0] strb;
    logic                     last;
  } tile_mem_req_t;

  // Retain the chunk index because TCDM responses carry no request metadata.
  typedef struct packed {
    logic            tile_owned;
    logic            write;
    spatz_id_t       id;
    tile_chunk_cnt_t chunk;
  } mem_req_tag_t;

  typedef struct packed {
    logic            valid;
    tile_chunk_cnt_t remaining;
  } tile_store_completion_t;

  localparam int unsigned MemByteOffW = $clog2(MemDataWidthB);

  typedef enum logic [3:0] {
    Tile_Idle,
    Tile_GatherRead,
    Tile_MemReq,
    Tile_MemWaitRsp,
    Tile_CommitRead,
    Tile_CommitWrite,
    Tile_Done
  } tile_state_e;

  tile_state_e tile_state_d, tile_state_q;
  tile_ctx_t tile_ctx_d, tile_ctx_q;
  tile_store_completion_t [NrParallelInstructions-1:0] tile_store_completion_d;
  tile_store_completion_t [NrParallelInstructions-1:0] tile_store_completion_q;
  tile_r_req_t tile_tss_req;
  logic [3:0] tile_elem_bytes;
  logic [NrMemPorts-1:0] tile_load_pending_d, tile_load_pending_q;
  tile_chunk_cnt_t tile_store_req_count_d, tile_store_req_count_q;
  logic tile_mem_busy;
  logic tile_req_accept;
  logic tile_load_rsp_valid, tile_store_rsp_valid, vrf_rsp_valid;
  logic vrf_mem_finished, vrf_store_finished;
  spatz_id_t tile_store_rsp_id;

  `FF(tile_state_q, tile_state_d, Tile_Idle)
  `FF(tile_ctx_q, tile_ctx_d, '0)
  `FF(tile_load_pending_q, tile_load_pending_d, '0)
  `FF(tile_store_req_count_q, tile_store_req_count_d, '0)
  `FF(tile_store_completion_q, tile_store_completion_d, '0)

  assign tile_elem_bytes = 4'(1 << ((tile_state_q == Tile_Idle) ? mem_spatz_req.op_ope.tew : tile_ctx_q.access.tew));
  assign tile_mem_busy = (tile_state_q != Tile_Idle);

  /*------------------------------------------------------------*/
  /*                   Tile Request Geometry                    */
  /*------------------------------------------------------------*/

  // Map an architectural tile to its first physical accumulator.
  assign tile_tss_req.idx = mem_spatz_req.op_ope.tss.tile_id;
  assign tile_tss_req.row = mem_spatz_req.op_ope.tss.index;
  assign tile_tss_req.tew = mem_spatz_req.op_ope.tew;

  logic [31:0]            tile_mem_base_aligned;
  logic [MemByteOffW-1:0] tile_mem_head;
  tile_chunk_cnt_t        tile_num_chunks;

  assign tile_mem_head         = tile_ctx_q.req.rs1[MemByteOffW-1:0];
  assign tile_mem_base_aligned = {tile_ctx_q.req.rs1[31:MemByteOffW], {MemByteOffW{1'b0}}};
  assign tile_num_chunks = tile_chunk_cnt_t'((tile_ctx_q.bytes + tile_mem_head + MemDataWidthB - 1) / MemDataWidthB);

  /*------------------------------------------------------------*/
  /*                Tile TCDM Request Generation                */
  /*------------------------------------------------------------*/

  tile_mem_req_t [NrMemPorts-1:0] tile_mem_req;
  logic [NrMemPorts-1:0] tile_mem_req_mask;
  logic tile_mem_req_ready, tile_load_req_fire, tile_store_req_fire;
  tile_chunk_cnt_t tile_mem_req_count;
  logic vrf_mem_req_valid;

  always_comb begin : proc_tile_mem_req
    tile_mem_req = tile_mem_req_t'(0);
    tile_mem_req_mask = {NrMemPorts{1'b0}};
    tile_mem_req_ready = 1'b1;
    tile_mem_req_count = tile_chunk_cnt_t'(0);

    for (int unsigned p = 0; p < NrMemPorts; p++) begin
      tile_chunk_idx_t chunk_idx;
      logic [31:0] aligned_addr;
      chunk_idx    = tile_chunk_idx_t'(tile_ctx_q.chunk_sent + p);
      aligned_addr = tile_mem_base_aligned + (chunk_idx * MemDataWidthB);

      if (chunk_idx < tile_num_chunks) begin
        tile_mem_req[p].active = 1'b1;
        tile_mem_req_mask[p] = 1'b1;
        tile_mem_req[p].addr = aligned_addr;
        tile_mem_req[p].last = (chunk_idx == tile_chunk_idx_t'(tile_num_chunks - 1'b1));
        tile_mem_req_count = tile_mem_req_count + 1'b1;
        if (!spatz_mem_req_ready[p / N_FU][p % N_FU]) tile_mem_req_ready = 1'b0;
        for (int unsigned b = 0; b < MemDataWidthB; b++) begin
          tile_stream_offset_t stream_byte_idx;
          stream_byte_idx = tile_stream_offset_t'(chunk_idx * MemDataWidthB + b) -
                            tile_stream_offset_t'(tile_mem_head);
          if ((stream_byte_idx >= 0) && (stream_byte_idx < tile_stream_offset_t'(tile_ctx_q.bytes))) begin
            tile_mem_req[p].data[b*8 +: 8] = tile_ctx_q.packed_data[stream_byte_idx*8 +: 8];
            tile_mem_req[p].strb[b] = 1'b1;
          end
        end
      end
    end
  end

  logic tile_store_req_last;
  assign tile_store_req_last = (tile_ctx_q.chunk_sent + tile_mem_req_count) >= tile_num_chunks;
  assign tile_load_req_fire = (tile_state_q == Tile_MemReq) && tile_ctx_q.req.op_mem.is_load &&
      (tile_ctx_q.chunk_sent < tile_num_chunks) && tile_mem_req_ready && !vrf_mem_req_valid;
  assign tile_store_req_fire = (tile_state_q == Tile_MemReq) && !tile_ctx_q.req.op_mem.is_load &&
      (tile_ctx_q.chunk_sent < tile_num_chunks) && tile_mem_req_ready;

  // Hold a dependent request in the operation queue. In particular, a younger
  // VTSE must not assert tile_rvalid_o and block an older MAC before it retires.
  assign tile_req_accept = mem_spatz_req_valid && mem_is_tile_mem && tile_req_ready &&
      matrix_enable_i[mem_spatz_req.id] && mem_spatz_req.op_ope.tss.tile_valid;

  logic [NrMemPorts-1:0] tile_mem_req_valid;
  logic [NrMemPorts-1:0] tile_mem_port_busy;

  always_comb begin : proc_tile_port_masks
    for (int unsigned p = 0; p < NrMemPorts; p++) begin
      tile_mem_req_valid[p] = (tile_load_req_fire || tile_store_req_fire) && tile_mem_req[p].active;
      tile_mem_port_busy[p] = tile_mem_req_valid[p] || tile_load_pending_q[p] ||
          ((tile_state_q == Tile_MemReq) && !tile_ctx_q.req.op_mem.is_load && tile_mem_req[p].active);
    end
  end

  /*------------------------------------------------------------*/
  /*               Tile and VRF Response Metadata               */
  /*------------------------------------------------------------*/

  // Tag in-order TCDM responses with their owner and access type.
  logic [NrMemPorts-1:0] tag_empty, tag_full;
  mem_req_tag_t [NrMemPorts-1:0] tag_head;
  logic [NrMemPorts-1:0] rsp_tile_mem, rsp_vrf_load, rsp_vrf_store;
  mem_req_tag_t [NrMemPorts-1:0] spatz_mem_req_tag_o;
  logic [NrMemPorts-1:0] mem_out_fire;
  for (genvar p = 0; p < NrMemPorts; p++) begin : gen_mem_tag
    mem_req_tag_t tag_q;
    logic tag_valid_q;

    always_ff @(posedge clk_i or negedge rst_ni) begin
      if (!rst_ni) begin
        tag_q       <= mem_req_tag_t'(0);
        tag_valid_q <= 1'b0;
      end else begin
        if (mem_out_fire[p]) begin
          tag_q       <= spatz_mem_req_tag_o[p];
          tag_valid_q <= 1'b1;
        end else if (spatz_mem_rsp_valid_i[p]) begin
          tag_valid_q <= 1'b0;
        end
      end
    end

    assign tag_head[p]  = tag_q;
    assign tag_empty[p] = !tag_valid_q;
    assign tag_full[p]  = tag_valid_q;

    // Empty means no outstanding request; keep all decodes low.
    assign rsp_tile_mem[p] = !tag_empty[p] && tag_head[p].tile_owned;
    assign rsp_vrf_load[p] = !tag_empty[p] && !tag_head[p].tile_owned && !tag_head[p].write;
    assign rsp_vrf_store[p] = !tag_empty[p] && !tag_head[p].tile_owned && tag_head[p].write;
  end

  /*------------------------------------------------------------*/
  /*               Tile Accumulator Data Shaping                */
  /*------------------------------------------------------------*/

  // One tile-port handshake transfers the whole selected row. A column
  // request reads full physical rows so its fixed column index is present.
  tile_count_t tile_req_elems;
  assign tile_req_elems = tile_ctx_q.is_col ? tile_count_t'(TileEdge) : tile_ctx_q.elems;

  tile_row_t tile_col_write_row;
  always_comb begin
    tile_byte_cnt_t dst_byte;

    tile_col_write_row = tile_ctx_q.row_buf;
    dst_byte = tile_byte_cnt_t'(tile_ctx_q.access.row * tile_elem_bytes);
    for (int unsigned byte_idx = 0; byte_idx < AccElemBytes; byte_idx++) begin
      if (byte_idx < tile_elem_bytes) begin
        tile_col_write_row[(dst_byte+byte_idx)*8 +: 8] = tile_ctx_q.packed_data[(tile_ctx_q.row_idx*tile_elem_bytes+byte_idx)*8 +: 8];
      end
    end
  end

  tile_count_t head_tile_elems;
  tile_byte_cnt_t head_tile_bytes;
  always_comb begin : proc_head_tile_elems
    elen_t active_dim;

    active_dim = mem_spatz_req.op_ope.tss.is_row ? mem_spatz_req.op_ope.tn : mem_spatz_req.op_ope.tm;
    head_tile_elems = (active_dim < TileEdge) ? tile_count_t'(active_dim) : tile_count_t'(TileEdge);
  end
  assign tile_tss_req.elems = head_tile_elems;
  assign head_tile_bytes = tile_byte_cnt_t'(head_tile_elems * tile_elem_bytes);

  /*------------------------------------------------------------*/
  /*           Tile Accumulator and TCDM Transfer FSM           */
  /*------------------------------------------------------------*/

  always_comb begin : tile_fsm
    tile_stream_offset_t stream_byte_idx;

    tile_state_d = tile_state_q;
    tile_ctx_d   = tile_ctx_q;
    tile_load_pending_d = tile_load_pending_q;

    tile_wvalid_o    = 1'b0;
    tile_w_req_o.idx = tile_ctx_q.access.idx;
    tile_w_req_o.row = tile_ctx_q.is_col ? tile_ctx_q.row_idx : tile_ctx_q.access.row;
    tile_w_req_o.elems = tile_req_elems;
    tile_w_req_o.tew = tile_ctx_q.access.tew;
    tile_w_req_o.data = tile_ctx_q.is_col ? tile_col_write_row : tile_ctx_q.packed_data;

    tile_rvalid_o    = 1'b0;
    tile_r_req_o.idx = tile_ctx_q.access.idx;
    tile_r_req_o.row = tile_ctx_q.is_col ? tile_ctx_q.row_idx : tile_ctx_q.access.row;
    tile_r_req_o.elems = tile_req_elems;
    tile_r_req_o.tew = tile_ctx_q.access.tew;

    // Snapshot a row before issuing its memory requests.
    if (tile_req_accept && !mem_spatz_req.op_mem.is_load && mem_spatz_req.op_ope.tss.is_row) begin
      tile_rvalid_o    = 1'b1;
      tile_r_req_o = tile_tss_req;
    end

    tile_load_rsp_valid = 1'b0;

    if (tile_store_req_fire) begin
      if (tile_store_req_last)
        tile_ctx_d.chunk_sent = tile_chunk_idx_t'(0);
      else
        tile_ctx_d.chunk_sent = tile_ctx_q.chunk_sent + tile_mem_req_count;
    end

    if (tile_load_req_fire) begin
      tile_ctx_d.chunk_sent = tile_ctx_q.chunk_sent + tile_mem_req_count;
      tile_load_pending_d   = tile_mem_req_mask;
    end

    unique case (tile_state_q)

      Tile_Idle: begin
        if (tile_req_accept) begin
          tile_ctx_d.req     = mem_spatz_req;
          tile_ctx_d.access  = tile_tss_req;
          // TSS selects one row or column. Only a column walks physical rows.
          tile_ctx_d.row_idx = tile_dim_t'(0);
          tile_ctx_d.elems   = head_tile_elems;
          tile_ctx_d.is_col     = !mem_spatz_req.op_ope.tss.is_row;
          tile_ctx_d.bytes   = tile_byte_cnt_t'(head_tile_bytes);
          tile_ctx_d.chunk_sent   = tile_chunk_idx_t'(0);
          tile_ctx_d.packed_data  = tile_row_t'(0);
          tile_ctx_d.row_buf      = tile_row_t'(0);
          tile_load_pending_d     = {NrMemPorts{1'b0}};
          if (head_tile_elems == tile_count_t'(0)) begin
            tile_state_d = mem_spatz_req.op_mem.is_load ? Tile_Done : Tile_Idle;
          end else if (mem_spatz_req.op_mem.is_load) begin
            tile_state_d = Tile_MemReq;
          end else if (mem_spatz_req.op_ope.tss.is_row && tile_rready_i) begin
            tile_ctx_d.packed_data = tile_rdata_i;
            tile_state_d = Tile_MemReq;
          end else begin
            tile_state_d = Tile_GatherRead;
          end
        end
      end

      Tile_GatherRead: begin
        tile_rvalid_o = 1'b1;
        if (tile_rready_i) begin
          if (tile_ctx_q.is_col) begin
            for (int unsigned byte_idx = 0; byte_idx < AccElemBytes; byte_idx++) begin
              if (byte_idx < tile_elem_bytes) begin
                tile_ctx_d.packed_data[
                    (tile_ctx_q.row_idx*tile_elem_bytes+byte_idx)*8 +: 8] =
                    tile_rdata_i[(tile_ctx_q.access.row*tile_elem_bytes+byte_idx)*8 +: 8];
              end
            end

            if ((tile_count_t'(tile_ctx_q.row_idx) + 1'b1) >= tile_ctx_q.elems)
              tile_state_d = Tile_MemReq;
            else
              tile_ctx_d.row_idx = tile_ctx_q.row_idx + 1'b1;
          end else begin
            tile_ctx_d.packed_data = tile_rdata_i;
            tile_state_d = Tile_MemReq;
          end
        end
      end

      Tile_MemReq: begin
        if (tile_ctx_q.req.op_mem.is_load) begin
          if (tile_load_req_fire)
            tile_state_d = Tile_MemWaitRsp;
        end else begin
          if (tile_store_req_fire && tile_store_req_last) begin
            // Wait for acknowledgments after the final request wave.
            tile_ctx_d.chunk_sent = tile_chunk_cnt_t'(0);
            tile_state_d          = Tile_MemWaitRsp;
          end
        end
      end

      Tile_MemWaitRsp: begin
        if (tile_ctx_q.req.op_mem.is_load) begin
          for (int unsigned p = 0; p < NrMemPorts; p++) begin
            if (spatz_mem_rsp_valid_i[p] && rsp_tile_mem[p] && !tag_head[p].write) begin
              tile_load_pending_d[p] = 1'b0;
              for (int unsigned b = 0; b < MemDataWidthB; b++) begin
                stream_byte_idx = tile_stream_offset_t'(
                    tag_head[p].chunk * MemDataWidthB + b - tile_mem_head);

                if ((stream_byte_idx >= 0) && (stream_byte_idx < tile_stream_offset_t'(tile_ctx_q.bytes))) begin
                  tile_ctx_d.packed_data[stream_byte_idx*8 +: 8] = spatz_mem_rsp_i[p].data[b*8 +: 8];
                end
              end
            end
          end

          if ((tile_load_pending_q != '0) && (tile_load_pending_d == '0)) begin
            if (tile_ctx_q.chunk_sent >= tile_num_chunks) begin
              tile_ctx_d.row_idx = tile_dim_t'(0);
              tile_state_d = tile_ctx_q.is_col ? Tile_CommitRead : Tile_CommitWrite;
            end else begin
              tile_state_d = Tile_MemReq;
            end
          end
        end else begin
          // Release row data after every request leaves the output registers.
          if (tile_store_req_count_d == tile_num_chunks)
            tile_state_d = Tile_Idle;
        end
      end

      Tile_CommitRead: begin
        tile_rvalid_o = 1'b1;
        if (tile_rready_i) begin
          tile_ctx_d.row_buf = tile_rdata_i;
          tile_state_d   = Tile_CommitWrite;
        end
      end

      Tile_CommitWrite: begin
        tile_wvalid_o = 1'b1;
        if (tile_wready_i) begin
          if (tile_ctx_q.is_col && ((tile_count_t'(tile_ctx_q.row_idx) + 1'b1) < tile_ctx_q.elems)) begin
            tile_ctx_d.row_idx = tile_ctx_q.row_idx + 1'b1;
            tile_state_d = Tile_CommitRead;
          end else if (!vrf_mem_finished && !vrf_rsp_valid) begin
            tile_load_rsp_valid = 1'b1;
            tile_state_d = Tile_Idle;
          end else begin
            tile_state_d = Tile_Done;
          end
        end
      end

      Tile_Done: begin
        if (!vrf_mem_finished && !vrf_rsp_valid) begin
          tile_load_rsp_valid = 1'b1;
          tile_state_d       = Tile_Idle;
        end
      end

      default: tile_state_d = Tile_Idle;
    endcase
  end

  /*------------------------------------------------------------*/
  /*            Tile Store Completion and Retirement            */
  /*------------------------------------------------------------*/

  always_comb begin : proc_tile_store_completion
    tile_store_completion_d = tile_store_completion_q;
    tile_store_rsp_valid = 1'b0;
    tile_store_rsp_id = spatz_id_t'(0);

    if (tile_req_accept && !mem_spatz_req.op_mem.is_load) begin
      tile_store_completion_d[mem_spatz_req.id].valid = 1'b1;
      tile_store_completion_d[mem_spatz_req.id].remaining = tile_chunk_cnt_t'(
          (head_tile_bytes + mem_spatz_req.rs1[MemByteOffW-1:0] +
           MemDataWidthB - 1) / MemDataWidthB);
    end

    for (int unsigned p = 0; p < NrMemPorts; p++) begin
      if (spatz_mem_rsp_valid_i[p] && rsp_tile_mem[p] && tag_head[p].write) begin
        tile_store_completion_d[tag_head[p].id].remaining = tile_store_completion_d[tag_head[p].id].remaining - 1'b1;
      end
    end

    for (int unsigned id = 0; id < NrParallelInstructions; id++) begin
      if (!tile_store_rsp_valid && tile_store_completion_q[id].valid &&
          (tile_store_completion_d[id].remaining == '0) && !vrf_mem_finished && !vrf_rsp_valid && !tile_load_rsp_valid) begin
        tile_store_rsp_valid = 1'b1;
        tile_store_rsp_id = spatz_id_t'(id);
        tile_store_completion_d[id].valid = 1'b0;
      end
    end
  end

  always_comb begin : proc_tile_store_count
    tile_store_req_count_d = tile_store_req_count_q;
    for (int unsigned p = 0; p < NrMemPorts; p++) begin
      if (mem_out_fire[p] && spatz_mem_req_tag_o[p].tile_owned && spatz_mem_req_tag_o[p].write)
        tile_store_req_count_d = tile_store_req_count_d + 1'b1;
    end
    if (tile_req_accept) tile_store_req_count_d = tile_chunk_cnt_t'(0);
  end

  for (genvar intf = 0; intf < NrInterfaces; intf++) begin : gen_store_count_q_intf
    for (genvar fu = 0; fu < N_FU; fu++) begin : gen_store_count_q_intf_fu
      `FF(store_count_q[intf][fu], store_count_d[intf][fu], '0)
    end: gen_store_count_q_intf_fu
  end: gen_store_count_q_intf

  always_comb begin: proc_store_count
    // Maintain state
    store_count_d = store_count_q;

    for (int intf = 0; intf < NrInterfaces; intf++) begin
      for (int fu = 0; fu < N_FU; fu++) begin
        int unsigned port;
        port = intf * N_FU + fu;

        if (!tile_mem_port_busy[port] && spatz_mem_req[intf][fu].write && spatz_mem_req_valid[intf][fu] && spatz_mem_req_ready[intf][fu])
          // Did we send a store?
          store_count_d[intf][fu]++;

        // Drain VRF-store responses even while another port serves a tile request.
`ifdef MEMPOOL_SPATZ
        if (store_count_q[intf][fu] != '0 && spatz_mem_rsp_valid_i[port] && spatz_mem_rsp_i[port].write && rsp_vrf_store[port])
          store_count_d[intf][fu]--;
`else
        if (store_count_q[intf][fu] != '0 && spatz_mem_rsp_valid_i[port] && rsp_vrf_store[port])
          store_count_d[intf][fu]--;
`endif
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
  logic  [NrInterfaces-1:0] [N_FU-1:0] rob_commit_valid;
  elen_t [NrInterfaces-1:0] [N_FU-1:0] rob_rdata;
  logic  [NrInterfaces-1:0] [N_FU-1:0] rob_pop;
  id_t   [NrInterfaces-1:0] [N_FU-1:0] rob_rid;
  logic  [NrInterfaces-1:0] [N_FU-1:0] rob_req_id;
  id_t   [NrInterfaces-1:0] [N_FU-1:0] rob_id;
  logic  [NrInterfaces-1:0] [N_FU-1:0] rob_full;
  logic  [NrInterfaces-1:0] [N_FU-1:0] rob_empty;

  // Per-port ROBs decouple TCDM responses from VRF writes.
  for (genvar intf = 0; intf < NrInterfaces; intf++) begin : gen_rob_intf
    for (genvar fu = 0; fu < N_FU; fu++) begin : gen_rob_intf_fu
`ifdef MEMPOOL_SPATZ
      reorder_buffer #(
        .DataWidth(ELEN              ),
        .NumWords (NrOutstandingLoads)
      ) i_reorder_buffer (
        .clk_i    (clk_i               ),
        .rst_ni   (rst_ni              ),
        .data_i   (rob_wdata[intf][fu] ),
        .id_i     (rob_wid[intf][fu]   ),
        .push_i   (rob_push[intf][fu]  ),
        .data_o   (rob_rdata[intf][fu] ),
        .valid_o  (rob_rvalid[intf][fu]),
        .id_read_o(rob_rid[intf][fu]   ),
        .pop_i    (rob_pop[intf][fu]   ),
        .id_req_i (rob_req_id[intf][fu]),
        .id_o     (rob_id[intf][fu]    ),
        .full_o   (rob_full[intf][fu]  ),
        .empty_o  (rob_empty[intf][fu] )
      );
      assign rob_commit_valid[intf][fu] = rob_rvalid[intf][fu];
`else
      fifo_v3 #(
        .DATA_WIDTH(ELEN              ),
        .DEPTH     (NrOutstandingLoads)
      ) i_reorder_buffer (
        .clk_i     (clk_i               ),
        .rst_ni    (rst_ni              ),
        .flush_i   (1'b0                ),
        .testmode_i(1'b0                ),
        .data_i    (rob_wdata[intf][fu] ),
        .push_i    (rob_push[intf][fu]  ),
        .data_o    (rob_rdata[intf][fu] ),
        .pop_i     (rob_pop[intf][fu]   ),
        .full_o    (rob_full[intf][fu]  ),
        .empty_o   (rob_empty[intf][fu] ),
        .usage_o   (/* Unused */        )
      );
      assign rob_rvalid[intf][fu] = !rob_empty[intf][fu];
      assign rob_commit_valid[intf][fu] = rob_rvalid[intf][fu];
`endif
    end: gen_rob_intf_fu
  end: gen_rob_intf

  //////////////////////
  //  Memory request  //
  //////////////////////

  // Is the memory operation valid and are we at the last one?
  logic [NrInterfaces-1:0] [N_FU-1:0] mem_operation_valid;
  logic [NrInterfaces-1:0] [N_FU-1:0] mem_operation_last;

  // Per-port counters allow independent TCDM progress.
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

  logic [NrInterfaces-1:0] [N_FU-1:0] mem_port_active;

  for (genvar intf = 0; intf < NrInterfaces; intf++) begin: gen_mem_port_active_intf
    for (genvar fu = 0; fu < N_FU; fu++) begin: gen_mem_port_active_intf_fu
      assign mem_port_active[intf][fu] = 1'b1;
    end
  end

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

      assign mem_port_finished_d[intf][fu] = mem_spatz_req_valid &&
          (!mem_port_active[intf][fu] || (mem_counter_q[intf][fu] == mem_counter_max[intf][fu] - mem_counter_delta[intf][fu]));
      assign mem_port_finished_q[intf][fu] = mem_spatz_req_valid &&
          (!mem_port_active[intf][fu] || (mem_counter_q[intf][fu] == mem_counter_max[intf][fu]));
    end: gen_mem_counters_intf_fu
  end: gen_mem_counters_intf

  // Did the current instruction finished the memory requests?
  logic [NrParallelInstructions-1:0] mem_insn_finished_q, mem_insn_finished_d;
  `FF(mem_insn_finished_q, mem_insn_finished_d, '0)

  // Is the current instruction pending?
  logic [NrParallelInstructions-1:0] mem_insn_pending_q, mem_insn_pending_d;
  `FF(mem_insn_pending_q, mem_insn_pending_d, '0)

  // A VRF memory instruction may issue requests only after its commit
  // metadata has been queued. Otherwise a full commit FIFO can leave orphan
  // responses in the per-port ROBs with no matching commit instruction.
  logic mem_vrf_insn_tracked;

  // Keep stores pending until TCDM accepts them.
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
    .DEPTH       (3                ),
    .FALL_THROUGH(1'b1             ),
    .dtype       (commit_metadata_t)
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
      vsew      : mem_data_ew,
      vl        : mem_spatz_req.vl,
      vstart    : mem_spatz_req.vstart,
      rs1       : mem_spatz_req.rs1[2:0],
      is_load   : mem_spatz_req.op_mem.is_load,
      is_strided: mem_is_strided,
      is_indexed: mem_is_indexed
  };

  assign mem_vrf_insn_tracked = mem_spatz_req_valid && !mem_is_tile_mem && (mem_insn_pending_q[mem_spatz_req.id] || commit_insn_push);

  always_comb begin: queue_control
    // Maintain state
    mem_insn_finished_d = mem_insn_finished_q;
    mem_insn_pending_d  = mem_insn_pending_q;

    // Do not ack anything
    mem_spatz_req_ready = 1'b0;

    // Do not push anything to the metadata queue
    commit_insn_push = 1'b0;

    // Did we start a new instruction?
    if (mem_spatz_req_valid && !mem_is_tile_mem && !commit_insn_full &&
        !(tile_mem_busy && tile_ctx_q.req.op_mem.is_load) && !mem_insn_pending_q[mem_spatz_req.id]) begin
      mem_insn_pending_d[mem_spatz_req.id] = 1'b1;
      commit_insn_push                     = 1'b1;
    end

    // Did an instruction finished its requests?
    if (mem_vrf_insn_tracked && &(mem_port_finished_q | (mem_port_finished_d & mem_counter_en)) && !write_pending) begin
      mem_insn_finished_d[mem_spatz_req.id] = 1'b1;
      mem_spatz_req_ready                   = 1'b1;
    end
    // Did we acknowledge the end of an instruction?
    if (vrf_rsp_valid) begin
      mem_insn_finished_d[vlsu_rsp_o.id] = 1'b0;
      mem_insn_pending_d[vlsu_rsp_o.id]  = 1'b0;
    end
    // Clear stale bookkeeping when all memory paths are idle.
    if (!mem_spatz_req_valid && commit_insn_empty && (&rob_empty) && !write_pending && !tile_mem_busy) begin
      for (int unsigned id_idx = 0; id_idx < NrParallelInstructions; id_idx++) begin
        mem_insn_finished_d[id_idx] = 1'b0;
        mem_insn_pending_d[id_idx]  = 1'b0;
      end
    end
    // Tile requests leave the queue when accepted by the tile FSM.
    if (tile_req_accept) begin
      mem_spatz_req_ready = 1'b1;
    end
  end

  // Per-port counters handle uneven VRF work distribution.
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

  // Current element index and byte index that are being accessed at the register file
  vreg_elem_t [NrInterfaces-1:0] vd_elem_id;
  vreg_elem_t [NrInterfaces-1:0] vs2_elem_id_d, vs2_elem_id_q;
  `FF(vs2_elem_id_q, vs2_elem_id_d, '0)

  // Pending indexes
  logic [NrInterfaces-1:0] [N_FU-1:0] fetch_next_idx;

  // Calculate the memory address for each memory port
  addr_offset_t [NrInterfaces-1:0] [N_FU-1:0] mem_req_addr_offset;
  for (genvar intf = 0; intf < NrInterfaces; intf++) begin: gen_mem_req_addr_intf
    for (genvar fu = 0; fu < N_FU; fu++) begin: gen_mem_req_addr_intf_fu
      localparam int unsigned port = intf * N_FU + fu;

      logic [31:0] addr;
      logic [31:0] stride;
      logic [31:0] offset;

      // Pre-shuffling index offset
      // Maximum byte offset for eight MAXEW elements with an eight-byte index width.
      logic [$clog2(8*8):0] idx_offset;
      assign idx_offset = mem_idx_counter_q[intf][fu];

      // Calculate shift amount for address normalization
      logic [$bits(vew_e)-1:0] log2_num_el_maxew;
      logic [$bits(vew_e)  :0] log2_num_idx_maxew_bytes;
      logic [2 * MAXEW     :0] num_idx_maxew_bytes;

      // Number of selected elements contained in one MAXEW element.
      assign log2_num_el_maxew = MAXEW - mem_data_ew;
      assign log2_num_idx_maxew_bytes = log2_num_el_maxew + mem_spatz_req.op_mem.ew;
      // Byte footprint of the indices associated with one MAXEW data element.
      assign num_idx_maxew_bytes = 1'b1 << log2_num_idx_maxew_bytes;

      always_comb begin
        stride = mem_is_strided ? mem_spatz_req.rs2 >> mem_data_ew : 'd1;

        if (mem_is_indexed) begin
          // What is the relationship between data and index width?
          automatic logic [1:0] data_index_width_diff = int'(mem_spatz_req.vtype.vsew) - int'(mem_spatz_req.op_mem.ew);

          // Pointer to index
          automatic logic [idx_width(N_FU*ELENB)-1:0] word_index =
              (fu << log2_num_idx_maxew_bytes) +
              (idx_offset & (num_idx_maxew_bytes - 1)) +
              ((idx_offset >> log2_num_idx_maxew_bytes) << log2_num_idx_maxew_bytes) *
              N_FU;

          // Index
          unique case (mem_spatz_req.op_mem.ew)
            EW_8 : offset   = $signed(vrf_rdata_i[intf][1][8 * word_index +: 8]);
            EW_16: offset   = $signed(vrf_rdata_i[intf][1][8 * word_index +: 16]);
            default: offset = $signed(vrf_rdata_i[intf][1][8 * word_index +: 32]);
          endcase
        end else begin
          offset = {
              mem_counter_q[intf][fu][$bits(vlen_t)-1:MAXEW] << $clog2(N_FU),
              mem_counter_q[intf][fu][int'(MAXEW)-1:0]
          } + (fu << MAXEW);
        end

        // Split interfaces across vector halves and TCDM superbanks to reduce conflicts.
        if (!mem_is_indexed && intf == 1) begin
          // Align the vector length with SpatzMemBytes bytes
          offset +=
              ((mem_spatz_req.vl + (SpatzMemBytes / 2)) >> $clog2(SpatzMemBytes) <<
               $clog2(SpatzMemBytes)) / 2;
        end
        offset *= stride;

        addr                          = mem_spatz_req.rs1 + offset;
        mem_req_addr[intf][fu]        = (addr >> MAXEW) << MAXEW;
        mem_req_addr_offset[intf][fu] = addr[int'(MAXEW)-1:0];

        fetch_next_idx[intf][fu] =
            (mem_idx_counter_q[intf][fu][$clog2(NrWordsPerVector*ELENB)-1:0] ==
             (num_idx_maxew_bytes - (1'b1 << mem_spatz_req.op_mem.ew))) &&
            mem_counter_en[intf][fu];
      end
    end: gen_mem_req_addr_intf_fu
  end: gen_mem_req_addr_intf

  // Calculate the register file addresses
  always_comb begin : gen_vreg_addr
    for (int intf = 0; intf < NrInterfaces; intf++) begin : gen_vreg_addr_intf
      vd_vreg_addr[intf] = (commit_insn_q.vd << $clog2(NrWordsPerVector)) + $unsigned(vd_elem_id[intf]);

      // For indices for indexed operations
      vs2_vreg_addr[intf] = (mem_spatz_req.vs2 << $clog2(NrWordsPerVector)) + $unsigned(vs2_elem_id_q[intf]);
      vs2_vreg_idx_addr[intf] = vs2_vreg_addr[intf];

	      // Start the second interface at the upper vector half for balanced VRF writes.
	      if (intf == 1) begin
	        vd_vreg_addr[intf] += (commit_insn_q.vl + (SpatzMemBytes / 2)) >> $clog2(SpatzMemBytes);
	        vs2_vreg_idx_addr[intf] +=
              (mem_spatz_req.vl >>
               (mem_spatz_req.vtype.vsew - int'(mem_spatz_req.op_mem.ew))) /
              SpatzMemBytes;
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
  logic load_rsp_ready;

  always_comb begin: control_proc
    // Maintain state
    busy_d = busy_q;

    // Do not pop anything
    commit_insn_pop = 1'b0;

    // Do not ack anything
    vlsu_finished_req = 1'b0;

    // A store may retire as soon as memory finishes.  A load must keep its
    // commit metadata until the final writes from all active VRF interfaces
    // have joined the memory-completion event in vrf_rsp_valid.
    if (commit_insn_valid &&
        (commit_insn_q.is_load ? load_rsp_ready : vrf_store_finished)) begin
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
  assign vrf_mem_finished = commit_insn_valid && &(commit_finished_q | (commit_finished_d & commit_counter_en)) &&
      mem_insn_finished_q[commit_insn_q.id];
  assign vrf_store_finished = commit_insn_valid && &(commit_finished_q | (commit_finished_d & commit_counter_en)) &&
      mem_insn_finished_q[commit_insn_q.id] && !commit_insn_q.is_load;

  // Snitch and the FPU sequencer consume these as one-cycle completion events,
  // not as levels.  A load can keep vrf_mem_finished asserted while waiting
  // for the final VRF interface, so report it only when the joined operation
  // retires from the commit queue.
  assign spatz_mem_finished_o = vlsu_finished_req | tile_load_rsp_valid | tile_store_rsp_valid;
  assign spatz_mem_str_finished_o = (vlsu_finished_req && commit_insn_valid && !commit_insn_q.is_load) |
      tile_store_rsp_valid;

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
  assign mem_single_element_size = 1'b1 << mem_data_ew;

  // How large is an index element (in bytes)
  logic [3:0] mem_idx_single_element_size;
  assign mem_idx_single_element_size = 1'b1 << mem_spatz_req.op_mem.ew;

  // Is the memory address unaligned
  logic commit_is_addr_unaligned;
  assign commit_is_addr_unaligned = commit_insn_q.rs1[int'(MAXEW)-1:0] != '0;

  // Do we have to access every single element on its own
  logic commit_is_single_element_operation;
  assign commit_is_single_element_operation = commit_is_addr_unaligned || commit_insn_q.is_strided ||
      commit_insn_q.is_indexed || (commit_insn_q.vstart != '0);

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
        // Do not enqueue tile beats in the VRF load offset queue.
        .push_i    (spatz_mem_req_valid[intf][fu] && spatz_mem_req_ready[intf][fu] && mem_is_load && !tile_mem_req_valid[intf * N_FU + fu]),
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

    spatz_id_t id;
    logic last;
  } vrf_req_t;

  vrf_req_t [NrInterfaces-1:0] vrf_req_d, vrf_req_q;
  logic     [NrInterfaces-1:0] vrf_req_valid_d, vrf_req_ready_d;
  logic     [NrInterfaces-1:0] vrf_req_valid_q, vrf_req_ready_q;
  logic     [NrInterfaces-1:0] load_vrf_done_d, load_vrf_done_q;
  logic     [NrInterfaces-1:0] load_vrf_done;
  logic     [NrInterfaces-1:0] load_vrf_required;
  logic     [NrInterfaces-1:0] vrf_write_fire;

	  logic vrf_path_idle, vrf_store_active, vrf_full_mode_active;
  logic [NrParallelInstructions-1:0] vrf_full_mode_pending;
  assign vrf_path_idle = commit_insn_empty && (&rob_empty) && !busy_q && !write_pending && !(|vrf_req_valid_q);
  assign vrf_store_active = (state_q == VLSU_RunningStore) || write_pending ||
	      (commit_insn_valid && !commit_insn_q.is_load) || (mem_spatz_req_valid && !mem_is_tile_mem && !mem_spatz_req.op_mem.is_load);
  assign vrf_full_mode_pending = mem_insn_pending_q;
  assign vrf_full_mode_active = (|vrf_full_mode_pending) || (commit_insn_valid && commit_insn_q.is_load) ||
      vrf_req_valid_q[0] || vrf_req_valid_q[1];
  assign tile_req_ready = !mem_spatz_req.op_mem.is_load ?
      ((tile_state_q == Tile_Idle) && !tile_store_completion_q[mem_spatz_req.id].valid) : (vrf_path_idle && !tile_mem_busy);
  assign spatz_req_ready_o = spatz_req_ready;

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
    // Each interface is an independent ready/valid channel. Interface 1 may
    // be accepted into the external conflict FIFO, while interface 0 writes
    // the VRF directly; coupling them here can deadlock when DMA contention
    // separates their completion timing.
    assign vrf_we_o[intf] = vrf_req_valid_q[intf] && ((intf == 1) ? !vlsu_buf_full_i : 1'b1);
    assign vrf_id_o[intf]        = {vrf_req_q[intf].id, mem_spatz_req.id, commit_insn_q.id};
    assign vrf_req_ready_q[intf] = vrf_we_o[intf] && vrf_wvalid_i[intf];
  end

  for (genvar intf = 0; intf < NrInterfaces; intf++) begin
    assign vrf_write_fire[intf] = vrf_req_ready_q[intf];
  end

  // A completion event from either interface may arrive many cycles before
  // the other one. Keep both events until the load at the commit head retires.
  always_comb begin
    load_vrf_done_d = load_vrf_done_q;
    for (int intf = 0; intf < NrInterfaces; intf++) begin
      if (vrf_write_fire[intf] && vrf_req_q[intf].last)
        load_vrf_done_d[intf] = 1'b1;
    end
    if (commit_insn_pop)
      load_vrf_done_d = '0;
  end
  `FF(load_vrf_done_q, load_vrf_done_d, '0)

  always_comb begin : proc_load_vrf_done
    load_vrf_done = load_vrf_done_q;
    for (int intf = 0; intf < NrInterfaces; intf++)
      load_vrf_done[intf] |= vrf_write_fire[intf] && vrf_req_q[intf].last;
  end

  // Short vectors use only interface 0. This mask is derived from stable
  // commit metadata rather than the instantaneous output-valid pattern.
  assign load_vrf_required[0] = commit_insn_valid && commit_insn_q.is_load && (commit_insn_q.vl != '0);
  assign load_vrf_required[1] = commit_insn_valid && commit_insn_q.is_load && (commit_insn_q.vl > (SpatzMemBytes / 2));

  ////////////////////////////
  // Response to Controller //
  ////////////////////////////

  vlsu_rsp_t vrf_rsp;
  assign vrf_rsp = '{id: commit_insn_q.id, default: '0};

  assign load_rsp_ready = commit_insn_valid && commit_insn_q.is_load && vrf_mem_finished &&
      &((~load_vrf_required) | load_vrf_done) && vlsu_buf_empty_i;
  assign vrf_rsp_valid = load_rsp_ready || vrf_store_finished;

  always_comb begin : proc_vlsu_response
    vlsu_rsp_o       = '0;
    vlsu_rsp_valid_o = 1'b0;

    if (vrf_rsp_valid) begin
      vlsu_rsp_o       = vrf_rsp;
      vlsu_rsp_valid_o = 1'b1;
    end else if (tile_store_rsp_valid) begin
      vlsu_rsp_o       = '{id: tile_store_rsp_id, default: '0};
      vlsu_rsp_valid_o = 1'b1;
    end else if (tile_load_rsp_valid) begin
      vlsu_rsp_o       = '{id: tile_ctx_q.req.id, default: '0};
      vlsu_rsp_valid_o = 1'b1;
    end
  end

  //////////////
  // Counters //
  //////////////

  // Do we need to catch up to reach element idx parity? (Because of non-zero vstart)
  vlen_t vreg_start_0;
  assign vreg_start_0 = vlen_t'(commit_insn_q.vstart[$clog2(ELENB)-1:0]);
  logic [NrInterfaces-1:0] [N_FU-1:0] catchup;
  for (genvar intf = 0; intf < NrInterfaces; intf++) begin: gen_catchup_intf
    for (genvar fu = 0; fu < N_FU; fu++) begin: gen_catchup_intf_fu
      assign catchup[intf][fu] =
          (commit_counter_q[intf][fu] < vreg_start_0) &
          (commit_counter_max[intf][fu] != commit_counter_q[intf][fu]);
    end: gen_catchup_intf_fu
  end: gen_catchup_intf

  for (genvar intf = 0; intf < NrInterfaces; intf++) begin: gen_vreg_counter_proc
    for (genvar fu = 0; fu < N_FU; fu++) begin: gen_vreg_counter_proc
      localparam int unsigned port = intf * N_FU + fu;

      // The total amount of vector bytes we have to work through
      vlen_t max_bytes;

	      always_comb begin
	        // Default value
	        max_bytes = '0;
	        commit_counter_load[intf][fu] = commit_insn_pop;
	        commit_counter_d[intf][fu]    = '0;

	        max_bytes = (commit_insn_q.vl >> $clog2(SpatzMemBytes)) << $clog2(ELENB);
	        if (commit_insn_q.vl[$clog2(ELENB) +: $clog2(NrMemPorts)] > port)
	          max_bytes += ELENB;
	        else if (commit_insn_q.vl[$clog2(SpatzMemBytes)-1:$clog2(ELENB)] == port)
	          max_bytes += commit_insn_q.vl[$clog2(ELENB)-1:0];

	        commit_counter_d[intf][fu] = (commit_insn_q.vstart >> $clog2(SpatzMemBytes)) << $clog2(ELENB);
	        if (commit_insn_q.vstart[$clog2(SpatzMemBytes)-1:$clog2(ELENB)] > port)
	          commit_counter_d[intf][fu] += ELENB;
	        else if (commit_insn_q.vstart[idx_width(SpatzMemBytes)-1:$clog2(ELENB)] == port)
	          commit_counter_d[intf][fu] += commit_insn_q.vstart[$clog2(ELENB)-1:0];
	        commit_operation_valid[intf][fu] = commit_insn_valid && (commit_counter_q[intf][fu] != max_bytes) &&
	                                           (catchup[intf][fu] || (!catchup[intf][fu] && ~|catchup));
        commit_operation_last[intf][fu] = commit_operation_valid[intf][fu] &&
            ((max_bytes - commit_counter_q[intf][fu]) <=
             (commit_is_single_element_operation ? commit_single_element_size : ELENB));
        commit_counter_delta[intf][fu] = !commit_operation_valid[intf][fu] ? vlen_t'('d0) :
            commit_is_single_element_operation ? vlen_t'(commit_single_element_size) :
            commit_operation_last[intf][fu] ? (max_bytes - commit_counter_q[intf][fu]) : vlen_t'(ELENB);
        commit_counter_en[intf][fu] =
            (commit_operation_valid[intf][fu] && commit_insn_q.is_load && vrf_req_valid_d[intf] && vrf_req_ready_d[intf]) ||
            (!commit_insn_q.is_load && vrf_rvalid_i[intf][0] && vrf_re_o[intf][0] && (!mem_is_indexed || vrf_rvalid_i[intf][1]));
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

        mem_operation_valid[intf][fu] = mem_vrf_insn_tracked && mem_port_active[intf][fu] && (max_bytes != mem_counter_q[intf][fu]);
	        mem_operation_last[intf][fu] =
              mem_operation_valid[intf][fu] &&
              ((max_bytes - mem_counter_q[intf][fu]) <=
               (mem_is_single_element_operation ? mem_single_element_size : MemDataWidthB));
	        mem_counter_load[intf][fu]    = mem_spatz_req_ready;
	        mem_counter_d[intf][fu]       = '0;
	        mem_counter_d[intf][fu] =
              (mem_spatz_req.vstart >> $clog2(NrMemPorts*MemDataWidthB)) <<
              $clog2(MemDataWidthB);
	        if (NrMemPorts == 1)
	          mem_counter_d[intf][fu] = mem_spatz_req.vstart;
	        else
	          if (mem_spatz_req.vstart[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] > port)
	            mem_counter_d[intf][fu] += MemDataWidthB;
	          else if (mem_spatz_req.vstart[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] == port)
	            mem_counter_d[intf][fu] += mem_spatz_req.vstart[$clog2(MemDataWidthB)-1:0];
        mem_counter_delta[intf][fu] =
            !mem_operation_valid[intf][fu] ? 'd0 :
            mem_is_single_element_operation ? mem_single_element_size :
            mem_operation_last[intf][fu] ?
                (max_bytes - mem_counter_q[intf][fu]) : MemDataWidthB;
        mem_counter_en[intf][fu] = spatz_mem_req_ready[intf][fu] && spatz_mem_req_valid[intf][fu] && !tile_mem_req_valid[port];
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
    write_pending = 1'b0;

    for (int intf = 0; intf < NrInterfaces; intf++) begin
      for (int fu = 0; fu < N_FU; fu++) begin
        int unsigned port;
        port = intf * N_FU + fu;
        write_pending |= (store_count_d[intf][fu] != '0);
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
  id_t  [NrInterfaces-1:0] [N_FU-1:0]                   mem_req_id;
  logic [NrInterfaces-1:0] [N_FU-1:0][MemDataWidth-1:0] mem_req_data;
  logic [NrInterfaces-1:0] [N_FU-1:0]                   mem_req_svalid;
  logic [NrInterfaces-1:0] [N_FU-1:0][ELEN/8-1:0]       mem_req_strb;
  logic [NrInterfaces-1:0] [N_FU-1:0]                   mem_req_lvalid;
  logic [NrInterfaces-1:0] [N_FU-1:0]                   mem_req_last;

  assign vrf_mem_req_valid = |mem_req_svalid || |mem_req_lvalid;

  // Number of pending requests
  logic [NrInterfaces-1:0] [N_FU-1:0][idx_width(NrOutstandingLoads):0] mem_pending_d, mem_pending_q;
  logic [NrInterfaces-1:0] [N_FU-1:0] mem_pending;
  `FF(mem_pending_q, mem_pending_d, '{default: '0})

  always_comb begin
    // Maintain state
    mem_pending_d = mem_pending_q;

    for (int intf = 0; intf < NrInterfaces; intf++) begin
      for (int fu = 0; fu < N_FU; fu++) begin
        int unsigned port;
        port = intf * N_FU + fu;
        mem_pending[intf][fu] = mem_pending_q[intf][fu] != '0;

        // Count only requests accepted by TCDM.
        if (spatz_mem_req_valid_o[port] && spatz_mem_req_ready_i[port] &&
            !spatz_mem_req_tag_o[port].tile_owned && !spatz_mem_req_tag_o[port].write)
          mem_pending_d[intf][fu]++;

        // Drain VRF-load responses independently of tile traffic.
        if (commit_insn_q.is_load && rob_rvalid[intf][fu] && rob_pop[intf][fu])
          mem_pending_d[intf][fu]--;

        // Drop a pending bit when no request or response can clear it later.
        if (commit_insn_valid && commit_insn_q.is_load && (mem_pending_d[intf][fu] != '0) && tag_empty[port] &&
            !spatz_mem_req_valid_o[port] && !spatz_mem_rsp_valid_i[port] && rob_empty[intf][fu]) begin
          mem_pending_d[intf][fu] = '0;
        end
      end
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

      // Propagate request ID
      vrf_req_d[intf].id   = commit_insn_q.id;
      vrf_req_d[intf].last = commit_insn_valid && &commit_finished_d[intf];

      // Request indexes
      vrf_re_o[intf][1] = mem_is_indexed;

      // Count which vs2 element we should load (indexed loads)
      vs2_elem_id_d = vs2_elem_id_q;
      for (int intf = 0; intf < NrInterfaces; intf++) begin
        if (&(fetch_next_idx[intf] ^ ~mem_operation_valid[intf]) && mem_is_indexed)
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
          vrf_req_valid_d[intf] = &(rob_commit_valid[intf] | ~commit_operation_valid[intf]) && |commit_operation_valid[intf];

	          for (int unsigned fu = 0; fu < N_FU; fu++) begin
	            int unsigned port;
	            logic [63:0] data;
	            logic [$clog2(ELENB)-1:0] shift;
	            logic [ELENB-1:0] mask;

	            port = intf * N_FU + fu;
	            data = rob_rdata[intf][fu];

            // Shift data to correct position if we have an unaligned memory request
            if (MAXEW == EW_32)
              unique case (
                  (commit_insn_q.is_strided || commit_insn_q.is_indexed) ?
                  vreg_addr_offset[intf][fu] : commit_insn_q.rs1[1:0])
                2'b01: data   = {data[7:0], data[31:8]};
                2'b10: data   = {data[15:0], data[31:16]};
                2'b11: data   = {data[23:0], data[31:24]};
                default: data = data;
              endcase
            else
              unique case (
                  (commit_insn_q.is_strided || commit_insn_q.is_indexed) ?
                  vreg_addr_offset[intf][fu] : commit_insn_q.rs1[2:0])
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
            rob_pop[intf][fu] = rob_commit_valid[intf][fu] && vrf_req_valid_d[intf] && vrf_req_ready_d[intf] && commit_counter_en[intf][fu];

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
          int unsigned port;
          port = intf * N_FU + fu;

          // Write the load result to the buffer
          rob_wdata[intf][fu] = spatz_mem_rsp_i[port].data;
          // Route only VRF-load responses into the ROB.
`ifdef MEMPOOL_SPATZ
          rob_wid[intf][fu]   = spatz_mem_rsp_i[port].id;
          // Need to consider out-of-order memory response
          rob_push[intf][fu] = rsp_vrf_load[port] && spatz_mem_rsp_valid_i[port] &&
                               (state_q == VLSU_RunningLoad) && spatz_mem_rsp_i[port].write == '0;
`else
          rob_push[intf][fu] = rsp_vrf_load[port] && spatz_mem_rsp_valid_i[port] && (state_q == VLSU_RunningLoad);
`endif
          if (!rob_full[intf][fu] && !offset_queue_full[intf][fu] && mem_operation_valid[intf][fu]) begin
            rob_req_id[intf][fu]     = spatz_mem_req_ready[intf][fu] &
                                       spatz_mem_req_valid[intf][fu] &
                                       ~tile_mem_req_valid[port];
            mem_req_lvalid[intf][fu] = (!mem_is_indexed || vrf_rvalid_i[intf][1]) && mem_spatz_req.op_mem.is_load;
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
            int unsigned port;
            port = intf * N_FU + fu;

            rob_wdata[intf][fu]  = vrf_rdata_i[intf][0][ELEN*fu +: ELEN];
            rob_wid[intf][fu]    = rob_id[intf][fu];
            rob_req_id[intf][fu] = vrf_rvalid_i[intf][0] && (!mem_is_indexed || vrf_rvalid_i[intf][1]);
            rob_push[intf][fu]   = rob_req_id[intf][fu];
          end
	        end

	        for (int unsigned fu = 0; fu < N_FU; fu++) begin
	          logic [63:0] data;
	          logic [$clog2(ELENB)-1:0] shift;
	          logic [MemDataWidthB-1:0] mask;

	          // Read element from buffer and execute memory request
	          if (mem_operation_valid[intf][fu]) begin
	            data = rob_rdata[intf][fu];

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

            mem_req_svalid[intf][fu] = rob_rvalid[intf][fu] && (!mem_is_indexed || vrf_rvalid_i[intf][1]) && !mem_spatz_req.op_mem.is_load;
            mem_req_id[intf][fu]     = rob_rid[intf][fu];
            mem_req_last[intf][fu]   = mem_operation_last[intf][fu];
            rob_pop[intf][fu] = spatz_mem_req_valid[intf][fu] && spatz_mem_req_ready[intf][fu] && !tile_mem_req_valid[intf * N_FU + fu];

	            // Create byte enable signal for memory request
	            if (mem_is_single_element_operation) begin
	              shift = (mem_is_strided || mem_is_indexed) ? mem_req_addr_offset[intf][fu] :
                      mem_counter_q[intf][fu][$clog2(ELENB)-1:0] + commit_insn_q.rs1[int'(MAXEW)-1:0];
	              mask  = '1;
	              case (mem_data_ew)
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
            // Clear empty buffer id requests
            if (!rob_empty[intf][fu])
              rob_pop[intf][fu] = 1'b1;
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
      logic mem_drv_ready;
      logic mem_drv_valid;
      logic tag_drv_ready;
      logic tag_drv_valid;
      mem_req_tag_t tag_drv;

      assign tag_drv = '{tile_owned: tile_mem_req_valid[port],
                         write:      spatz_mem_req[intf][fu].write,
                         id:         tile_mem_req_valid[port] ? tile_ctx_q.req.id : mem_spatz_req.id,
                         chunk:      tile_chunk_cnt_t'(tile_ctx_q.chunk_sent + port)};

      spill_register #(
        .T(spatz_mem_req_t)
      ) i_spatz_mem_req_register (
        .clk_i   (clk_i                    ),
        .rst_ni  (rst_ni                   ),
        .data_i  (spatz_mem_req[intf][fu]  ),
        .valid_i (spatz_mem_req_valid[intf][fu] && tag_drv_ready),
        .ready_o (mem_drv_ready            ),
        .data_o  (spatz_mem_req_o[port]    ),
        .valid_o (mem_drv_valid            ),
        .ready_i (spatz_mem_req_ready_i[port] && tag_drv_valid && !tag_full[port])
      );

      spill_register #(
        .T(mem_req_tag_t)
      ) i_spatz_mem_req_tag_register (
        .clk_i   (clk_i                    ),
        .rst_ni  (rst_ni                   ),
        .data_i  (tag_drv                  ),
        .valid_i (spatz_mem_req_valid[intf][fu] && mem_drv_ready),
        .ready_o (tag_drv_ready            ),
        .data_o  (spatz_mem_req_tag_o[port]),
        .valid_o (tag_drv_valid            ),
        .ready_i (spatz_mem_req_ready_i[port] && mem_drv_valid && !tag_full[port])
      );

      assign spatz_mem_req_ready[intf][fu] = mem_drv_ready && tag_drv_ready;
      assign spatz_mem_req_valid_o[port] = mem_drv_valid && tag_drv_valid && !tag_full[port];
      assign mem_out_fire[port] = spatz_mem_req_valid_o[port] && spatz_mem_req_ready_i[port];

`ifdef MEMPOOL_SPATZ
      // ID is required in Mempool-Spatz
      assign spatz_mem_req[intf][fu].id    = tile_mem_req_valid[port] ? '0 :  mem_req_id[intf][fu];
      assign spatz_mem_req[intf][fu].addr = tile_mem_req_valid[port] ? tile_mem_req[port].addr : mem_req_addr[intf][fu];
      assign spatz_mem_req[intf][fu].mode  = '0; // Request always uses user privilege level
      assign spatz_mem_req[intf][fu].size = tile_mem_req_valid[port] ?
          tile_ctx_q.req.op_mem.ew[1:0] : mem_data_ew[1:0];
      assign spatz_mem_req[intf][fu].write = tile_mem_req_valid[port] ? !tile_ctx_q.req.op_mem.is_load : !mem_is_load;
      assign spatz_mem_req[intf][fu].strb  = tile_mem_req_valid[port] ?
          (tile_ctx_q.req.op_mem.is_load ? '0 : tile_mem_req[port].strb) : mem_req_strb[intf][fu];
      assign spatz_mem_req[intf][fu].data = tile_mem_req_valid[port] ? tile_mem_req[port].data : mem_req_data[intf][fu];
      assign spatz_mem_req[intf][fu].last = tile_mem_req_valid[port] ? tile_mem_req[port].last : mem_req_last[intf][fu];
      assign spatz_mem_req[intf][fu].spec  = 1'b0; // Request is never speculative
      assign spatz_mem_req_valid[intf][fu] = tile_mem_req_valid[port] ? 1'b1 :
          (!tile_mem_port_busy[port] && (mem_req_svalid[intf][fu] || mem_req_lvalid[intf][fu]));
`else
      assign spatz_mem_req[intf][fu].addr = tile_mem_req_valid[port] ? tile_mem_req[port].addr : mem_req_addr[intf][fu];
      assign spatz_mem_req[intf][fu].write = tile_mem_req_valid[port] ? !tile_ctx_q.req.op_mem.is_load : !mem_is_load;
      assign spatz_mem_req[intf][fu].amo   = reqrsp_pkg::AMONone;
      assign spatz_mem_req[intf][fu].data = tile_mem_req_valid[port] ? tile_mem_req[port].data : mem_req_data[intf][fu];
      assign spatz_mem_req[intf][fu].strb = tile_mem_req_valid[port] ?
          (tile_ctx_q.req.op_mem.is_load ? '0 : tile_mem_req[port].strb) : mem_req_strb[intf][fu];
      assign spatz_mem_req[intf][fu].user  = '0;
      assign spatz_mem_req_valid[intf][fu] = tile_mem_req_valid[port] ? 1'b1 :
          (!tile_mem_port_busy[port] && (mem_req_svalid[intf][fu] || mem_req_lvalid[intf][fu]));
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

endmodule : spatz_doublebw_vlsu
