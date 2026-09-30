// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Pei-Yu Lin <peilin@ethz.ch>
//
// Outer Product Engine wrapper

module spatz_ope
  import spatz_pkg::*;
  import rvv_pkg::*;
  import fpnew_pkg::*;
#(
  parameter int unsigned CE = OPEComputeEdge,
  parameter int unsigned TE = TileEdge
) ( 
  input  logic             clk_i              ,
  input  logic             rst_ni             ,

  // interface with Controller
  input  spatz_req_t       spatz_req_i        ,
  input  logic             spatz_req_valid_i  ,
  output logic             spatz_req_ready_o  ,
  input  logic [NrParallelInstructions-1:0] matrix_enable_i,

  output logic             ope_rsp_valid_o    ,
  input  logic             ope_rsp_ready_i    ,
  output ope_rsp_t         ope_rsp_o          ,

  // interface with VLSU
  output vrf_addr_t        vrf_waddr_o        ,
  output vrf_data_t        vrf_wdata_o        ,
  output logic             vrf_we_o           ,
  output vrf_be_t          vrf_wbe_o          ,
  input  logic             vrf_wvalid_i       ,
  output spatz_id_t  [3:0] vrf_id_o           ,

  output vrf_addr_t  [2:0] vrf_raddr_o        ,
  output logic       [2:0] vrf_re_o           ,
  input  vrf_data_t  [2:0] vrf_rdata_i        ,
  input  logic       [2:0] vrf_rvalid_i       ,

  input  logic             tile_wvalid_i      ,
  input  tile_w_req_t      tile_w_req_i       ,
  output logic             tile_wready_o      ,
  input  logic             tile_rvalid_i      ,
  input  tile_r_req_t      tile_r_req_i       ,
  output tile_row_t        tile_rdata_o       ,
  output logic             tile_rready_o
);

`include "common_cells/registers.svh"

  localparam NumPipeRegs = 4;
  localparam GroupsPerEdge = TE / CE; // TE & CE should be power of 2
  localparam SpatialBeats = GroupsPerEdge * GroupsPerEdge;
  localparam GroupIdxW = (GroupsPerEdge > 1) ? $clog2(GroupsPerEdge) : 1;
  localparam SpatialBeatW = (SpatialBeats > 1) ? $clog2(SpatialBeats) : 1;
  localparam ReductionIdxW = (KMAX > 1) ? $clog2(KMAX) : 1;
  localparam NrAccumulatorTiles = NrPhysicalTile / AccElemBytes;
  localparam AccDepth = NrAccumulatorTiles * SpatialBeats;
  localparam AccAddrW = (AccDepth > 1) ? $clog2(AccDepth) : 1;
  localparam AccTileIdxW = (NrAccumulatorTiles > 1) ? $clog2(NrAccumulatorTiles) : 1;
  localparam AccByteIdxW = (AccElemBytes > 1) ? $clog2(AccElemBytes) : 1;
  localparam logic [AccElemBytes-1:0] AccHalfMask = {{(AccElemBytes-2){1'b0}}, 2'b11};
  localparam logic [AccElemBytes-1:0] AccFullMask = {AccElemBytes{1'b1}};
  // Capacity bound for TEW8/TEW16/TEW32; runtime TEW selects the active element count.
  localparam VtMaxElemsPerWord = VRFWordWidth / 8;
  // Capacity bound for maximum LMUL=8; the number of VRF words is independent of TEW.
  localparam MaxVtGroupWords = 8 * NrWordsPerVector;

  ////////////////////////////////////////////////////////////////////////
  ////                  Shared Types and Signals                      ////
  ////////////////////////////////////////////////////////////////////////

  typedef logic [$clog2(GroupsPerEdge+1)-1:0] group_count_t;
  typedef logic [$clog2(MaxVtGroupWords+1)-1:0] vt_word_count_t;
  typedef logic [$clog2(VtMaxElemsPerWord+1)-1:0] vrf_elem_count_t;
  typedef logic [3:0] vreg_group_count_t;
  typedef logic [$clog2((TE<<1)+1)-1:0] tile_sum_t;
  typedef logic [5:0] operand_width_t;
  typedef logic [$clog2(VRFWordWidth)-1:0] vrf_bit_offset_t;
  typedef struct packed {
    spatz_id_t     id;
    vreg_t         vs1;
    vreg_t         vs2;
    vew_e          ew;
    vew_e          tew;
    mt_t           tile;
    logic          is_alt;
    tile_count_t   tm;
    tile_count_t   tn;
    logic [2:0]    tk;
  } mac_op_t;
  typedef struct packed {
    mac_op_t       op;
    logic          valid;
  } mac_op_slot_t;
  typedef struct packed {
    logic                    valid;
    mt_t  tile;
    vew_e tew;
    logic [SpatialBeatW-1:0] beat;
    logic [CE-1:0][CE-1:0]   lane_active;
    logic                    write_acc;
  } fma_pipe_tag_t;

  // Request queues and arbitration.
  spatz_req_t spatz_req_mac ;
  logic       mac_req_valid ;
  logic       mac_req_ready ;
  spatz_req_t spatz_req_tv ;
  logic       tv_req_valid ;
  logic       tv_req_ready ;
  spatz_req_t spatz_req_vt ;
  logic       vt_req_valid ;
  logic       vt_req_ready ;
  spatz_req_t spatz_req_clean ;
  logic       clean_req_valid ;
  logic       clean_req_ready ;
  logic mac_in_ready, tv_in_ready, vt_in_ready, clean_in_ready;
  logic mac_queue_valid, tv_queue_valid, vt_queue_valid, clean_queue_valid;

  // MAC scheduler and active operation.
  mac_op_slot_t mac_op_current_d, mac_op_current_q;
  mac_op_slot_t mac_op_next_d, mac_op_next_q;
  logic [SpatialBeatW-1:0] mac_beat_d, mac_beat_q;
  logic [ReductionIdxW-1:0] mac_reduction_d, mac_reduction_q;
  logic         mac_op_queue_ready;
  logic         mac_op_exec_valid;
  logic         mac_op_bypass;
  logic mac_req_empty, mac_current_empty, mac_op_empty;
  logic mac_op_has_more_spatial, mac_op_has_more_reductions, mac_op_has_more_beats;
  mac_op_t mac_op_exec;
  logic [SpatialBeatW-1:0] mac_op_beat;
  logic [ReductionIdxW-1:0] mac_reduction_idx;
  logic [GroupIdxW-1:0] mac_group_row, mac_group_col;
  group_count_t mac_m_groups, mac_n_groups;
  tile_count_t mac_active_tm, mac_active_tn;
  logic [SpatialBeatW-1:0] mac_next_spatial_beat;
  logic [CE-1:0][CE-1:0] mac_lane_active;
  logic mac_fire, mac_step_fire;
  logic mac_nonfma_ready;
  logic mac_commit_valid, mac_commit_ready;
  logic mac_idle;

  // FMA pipeline and MAC completion.
  logic [CE-1:0][CE-1:0] fma16_ready;
  logic [CE-1:0][CE-1:0] fma32_ready;
  logic                  fma_pipe_ready;
  logic fma_pipe_result_valid;
  logic fma_pipe_result_fire;
  logic fma_pipe_result_forward;
  logic fma_pipe_result_ready;
  logic fma_pipe_continue_ready;
  logic [NrPhysicalTile-1:0][SpatialBeats-1:0] fma_pipe_tile_busy, fma_pipe_tile_busy_next;
  logic [NumPipeRegs-1:0] fma_pipe_valid, fma_pipe_valid_next;
  fma_pipe_tag_t [NumPipeRegs-1:0] fma_pipe_tag_d, fma_pipe_tag_q;
  fma_pipe_tag_t                   fma_pipe_tag_in;
  logic                            fma_pipe_advance;
  logic                            fma_pipe_drain;
  logic                            fma_pipe_write_acc;
  ope_rsp_t mac_commit_rsp, mac_done_rsp;
  logic     mac_done_valid, mac_done_ready;

  // Tile -> VRF move.
  spatz_req_t  vt_req_q                       ;
  logic        vt_busy_d    , vt_busy_q       ;
  logic        vt_commit_valid, vt_commit_ready;
  ope_rsp_t    vt_commit_rsp, vt_done_rsp;
  logic        vt_done_valid , vt_done_ready  ;
  logic [$clog2(MaxVtGroupWords)-1:0] vt_word_idx_d, vt_word_idx_q;
  logic        vt_word_fire;
  vt_word_count_t vt_active_words;
  vlen_t          vt_active_elems;
  tile_count_t    vt_active_lines;
  tile_count_t    vt_line_elems;
  tile_count_t    vt_line_words;
  vrf_elem_count_t vt_elems_per_word;

  // VRF -> tile move.
  spatz_req_t  tv_req_q                       ;
  logic        tv_busy_d    , tv_busy_q       ;
  logic        tv_commit_valid, tv_commit_ready;
  ope_rsp_t    tv_commit_rsp, tv_done_rsp      ;
  logic        tv_done_valid , tv_done_ready  ;
  logic [$clog2(MaxVtGroupWords)-1:0] tv_word_idx_d, tv_word_idx_q;
  vt_word_count_t tv_active_words;
  vlen_t          tv_active_elems;
  tile_count_t    tv_active_lines;
  tile_count_t    tv_line_elems;
  tile_count_t    tv_line_words;
  vrf_elem_count_t tv_elems_per_word;
  vrf_data_t tv_data_q;
  logic      tv_data_latched_q;
  vrf_data_t tv_vrf_data;
  logic      tv_vrf_avail;
  logic tv_acc_wen;

  // Tile clean and busy tracking.
  ope_rsp_t clean_commit_rsp, clean_done_rsp;
  logic     clean_done_valid, clean_done_ready ;
  logic     clean_commit_ready                  ;
  logic     clean_acc_ready                     ;
  logic [NrPhysicalTile-1:0] mac_tile_busy;

  // Resident tile and drain state.
  logic            resident_valid_d, resident_valid_q;
  mt_t             resident_tile_d, resident_tile_q;
  mt_t             resident_drain_tile_d, resident_drain_tile_q;
  logic            resident_drain_valid_d, resident_drain_valid_q;
  logic            resident_drain_writeback_d, resident_drain_writeback_q;
  logic            resident_start_fire;
  logic            resident_switch_req, resident_switch_fire;
  logic            resident_beat_mismatch;

  // Accumulator datapath and access arbitration.
  logic [CE-1:0][CE-1:0][AccElemWidth-1:0] fma_addend;
  logic [CE-1:0][CE-1:0][AccElemWidth-1:0] fma_result;
  logic [CE-1:0][CE-1:0] fma_result_valid;
  logic [CE-1:0][CE-1:0][AccElemWidth-1:0] acc_fma_rdata, acc_fma_wdata;
  logic [CE-1:0][CE-1:0][AccElemBytes-1:0] acc_fma_wen;
  logic [CE-1:0][CE-1:0][GroupsPerEdge-1:0][AccElemWidth-1:0] acc_vrf_rdata, acc_vrf_wdata;
  logic [CE-1:0][CE-1:0][AccElemBytes-1:0] acc_vrf_wen;
  logic [CE-1:0][CE-1:0][GroupsPerEdge-1:0][AccElemWidth-1:0] acc_vlsu_rdata, acc_vlsu_wdata;
  logic [CE-1:0][CE-1:0][AccElemBytes-1:0] acc_vlsu_wen;
  logic [AccAddrW-1:0] acc_fma_raddr, acc_fma_waddr;
  logic [AccAddrW-1:0] acc_vrf_raddr, acc_vrf_waddr;
  logic [AccAddrW-1:0] acc_vlsu_raddr, acc_vlsu_waddr;
  logic acc_flush;
  logic [NrAccumulatorTiles-1:0][SpatialBeats-1:0][AccElemBytes-1:0] acc_zero_d, acc_zero_q;
  logic tile_access_blocked;
  logic [NrPhysicalTile-1:0] tile_read_pipe_conflict, tile_write_pipe_conflict;
  logic tile_write_mac_conflict, tile_write_zero_conflict;

  ////////////////////////////////////////////////////////////////////////
  ////                 Request Queues and Arbitration                 ////
  ////////////////////////////////////////////////////////////////////////

  // A blocked queue head must neither execute nor win arbitration over an
  // older operation in another queue. Matrix grants describe older IDs only.
  assign mac_req_valid = mac_queue_valid && matrix_enable_i[spatz_req_mac.id];
  assign tv_req_valid = tv_queue_valid && matrix_enable_i[spatz_req_tv.id];
  assign vt_req_valid = vt_queue_valid && matrix_enable_i[spatz_req_vt.id];
  assign clean_req_valid = clean_queue_valid && matrix_enable_i[spatz_req_clean.id];

  stream_fifo #( .FALL_THROUGH(1'b0       ),
    .DEPTH       (4          ),
    .T           (spatz_req_t)
  ) i_op_mac_queue ( 
    .clk_i     (clk_i                                                   ),
    .rst_ni    (rst_ni                                                  ),
    .flush_i   (1'b0                                                    ),
    .testmode_i(1'b0                                                    ),
    .usage_o   (/* Unused */                                            ),
    .data_i    (spatz_req_i                                             ),
    .valid_i   (spatz_req_valid_i && (spatz_req_i.ex_unit == OPE) && spatz_req_i.op_ope.is_mac),
    .ready_o   (mac_in_ready                                             ),
    .data_o    (spatz_req_mac                                            ),
    .valid_o   (mac_queue_valid                                          ),
    .ready_i   (mac_req_ready                                            )
  );

  spill_register #(.T(spatz_req_t)) i_op_tv_queue ( 
    .clk_i  (clk_i                                                   ),
    .rst_ni (rst_ni                                                  ),
    .data_i (spatz_req_i                                             ),
    .valid_i(spatz_req_valid_i && (spatz_req_i.ex_unit == OPE) && spatz_req_i.op_ope.is_tv),
    .ready_o(tv_in_ready                                                ),
    .data_o (spatz_req_tv                                               ),
    .valid_o(tv_queue_valid                                             ),
    .ready_i(tv_req_ready                                               )
  );

  spill_register #(.T(spatz_req_t)) i_op_vt_queue ( 
    .clk_i  (clk_i                                                   ),
    .rst_ni (rst_ni                                                  ),
    .data_i (spatz_req_i                                             ),
    .valid_i(spatz_req_valid_i && (spatz_req_i.ex_unit == OPE) && spatz_req_i.op_ope.is_vt),
    .ready_o(vt_in_ready                                       ),
    .data_o (spatz_req_vt                                               ),
    .valid_o(vt_queue_valid                                             ),
    .ready_i(vt_req_ready                                               )
  );

  spill_register #(.T(spatz_req_t)) i_op_clean_queue ( 
    .clk_i  (clk_i                                                   ),
    .rst_ni (rst_ni                                                  ),
    .data_i (spatz_req_i                                             ),
    .valid_i(spatz_req_valid_i && (spatz_req_i.ex_unit == OPE) &&
             (spatz_req_i.op_ope.is_zero_tile || spatz_req_i.op_ope.is_discard)),
    .ready_o(clean_in_ready                                                ),
    .data_o (spatz_req_clean                                               ),
    .valid_o(clean_queue_valid                                             ),
    .ready_i(clean_req_ready                                               )
  );

  // Shared Request Arbitration
  always_comb begin : req_ready_proc

    mac_req_ready    = 1'b0;
    vt_req_ready     = 1'b0;
    tv_req_ready     = 1'b0;
    clean_req_ready = 1'b0;

    /*
    * Preserve OPE command ordering.
    * Simple commands such as VTZERO/VTDISCARD must complete
    * before a younger MAC may start.
    */
    if (clean_req_valid)
      clean_req_ready = clean_commit_ready && clean_acc_ready;
    else if (vt_req_valid)
      vt_req_ready = !vt_busy_q && !resident_drain_valid_q && (!mac_tile_busy[spatz_req_vt.op_ope.tss.tile_id]) &&
                  (!resident_valid_q || (spatz_req_vt.op_ope.tss.tile_id != resident_tile_q));
    else if (tv_req_valid)
      tv_req_ready = !tv_busy_q && !resident_drain_valid_q && (!mac_tile_busy[spatz_req_tv.op_ope.tss.tile_id]) &&
                  (!resident_valid_q || (spatz_req_tv.op_ope.tss.tile_id != resident_tile_q));
    else if (mac_req_valid)
      mac_req_ready = mac_op_queue_ready && !resident_drain_valid_q && !(tile_rvalid_i &&
            (tile_r_req_i.idx == spatz_req_mac.mtd));
  end : req_ready_proc

  assign spatz_req_ready_o = (~mac_in_ready || ~tv_in_ready || ~vt_in_ready || ~clean_in_ready) ? 1'b0 : 1'b1;

  ////////////////////////////////////////////////////////////////////////
  ////                         MAC Operation                          ////
  ////////////////////////////////////////////////////////////////////////
  // MAC Operation: Scheduler and Active State

  // The MAC scheduler only executes instructions in issue order.  Keep the
  // active instruction and one look-ahead instruction instead of indexing a
  // context slot by every architectural tile.

  assign mac_group_row = GroupIdxW'(mac_op_beat / GroupsPerEdge);
  assign mac_group_col = GroupIdxW'(mac_op_beat % GroupsPerEdge);

  always_comb begin : mac_active_shape
    mac_active_tm = (mac_op_exec.tm < TE) ? mac_op_exec.tm : tile_count_t'(TE);
    mac_active_tn = (mac_op_exec.tn < TE) ? mac_op_exec.tn : tile_count_t'(TE);
    mac_m_groups = (mac_active_tm + CE - 1) / CE;
    mac_n_groups = (mac_active_tn + CE - 1) / CE;
    mac_next_spatial_beat = mac_op_beat;

    if (({1'b0, mac_group_col} + 1'b1) < mac_n_groups)
      mac_next_spatial_beat = mac_op_beat + 1'b1;
    else
      mac_next_spatial_beat = SpatialBeatW'(({1'b0, mac_group_row} + 1'b1) * GroupsPerEdge);

    mac_lane_active = $bits(mac_lane_active)'(0);
    for (int unsigned row = 0; row < CE; row++) begin
      for (int unsigned col = 0; col < CE; col++) begin
        mac_lane_active[row][col] = ((mac_group_row * CE + row) < mac_active_tm) 
                                  && ((mac_group_col * CE + col) < mac_active_tn);
      end
    end
  end : mac_active_shape

  always_comb begin : mac_op_scheduler
    logic result_match, result_switch;
    logic tile_hazard;
    logic current_ready;

    if (fma_pipe_result_valid && !fma_pipe_tag_q[NumPipeRegs-1].write_acc) begin
      result_match  = (fma_pipe_tag_q[NumPipeRegs-1].tile == mac_op_current_q.op.tile) &&
                      (fma_pipe_tag_q[NumPipeRegs-1].beat == mac_beat_q);
      result_switch =  resident_valid_q && !resident_drain_valid_q && (fma_pipe_tag_q[NumPipeRegs-1].tile == resident_tile_q) &&
                      (mac_op_current_q.op.tile != resident_tile_q);
    end
    tile_hazard = fma_pipe_tile_busy[mac_op_current_q.op.tile][mac_beat_q] && !result_match;
    current_ready = mac_op_current_q.valid && (mac_current_empty || (!resident_drain_valid_q && !tile_hazard &&
          !(tile_rvalid_i && (tile_r_req_i.idx == mac_op_current_q.op.tile)) &&
          ((!(&fma_pipe_valid)) || result_match || result_switch || fma_pipe_result_fire)));
    mac_op_bypass = !mac_op_current_q.valid && mac_req_valid && mac_op_queue_ready && mac_req_ready && !mac_req_empty;
    mac_op_exec_valid = current_ready || mac_op_bypass;
  end : mac_op_scheduler

  assign mac_op_beat = mac_op_bypass ? SpatialBeatW'(0) : mac_beat_q;
  assign mac_reduction_idx = mac_op_bypass ? ReductionIdxW'(0) : mac_reduction_q;

  always_comb begin : mac_op_select
    mac_op_exec = mac_op_current_q.op;
    if (mac_op_bypass) begin
      mac_op_exec = '{
        tile     : spatz_req_mac.mtd,
        id       : spatz_req_mac.id,
        vs1      : spatz_req_mac.vs1,
        vs2      : spatz_req_mac.vs2,
        ew       : spatz_req_mac.vtype.vsew,
        tew      : spatz_req_mac.op_ope.tew,
        is_alt   : (spatz_req_mac.op == VTFMM_ALT),
        tk       : spatz_req_mac.op_ope.tk,
        tm       : (spatz_req_mac.op_ope.tm > TE) ? tile_count_t'(TE) : tile_count_t'(spatz_req_mac.op_ope.tm),
        tn       : (spatz_req_mac.op_ope.tn > TE) ? tile_count_t'(TE) : tile_count_t'(spatz_req_mac.op_ope.tn)
      };
    end
  end : mac_op_select

  // Empty requests are rare control cases.  Keep their detection off the
  // active-shape/group arithmetic and never send them through combinational
  // bypass, so normal MAC enables only depend on registered dimensions.
  always_comb begin : mac_op_empty_dimension
    mac_req_empty = (spatz_req_mac.op_ope.tm == elen_t'(0)) || (spatz_req_mac.op_ope.tn == elen_t'(0)) ||
                    (spatz_req_mac.op_ope.tk == elen_t'(0));
    mac_current_empty = (mac_op_current_q.op.tm == tile_count_t'(0)) || (mac_op_current_q.op.tn == tile_count_t'(0)) ||
                        (mac_op_current_q.op.tk == 3'd0);
    mac_op_empty = mac_op_exec_valid && !mac_op_bypass && mac_current_empty;
  end : mac_op_empty_dimension

  always_comb begin : mac_op_has_more
    mac_op_has_more_spatial = (({1'b0, mac_group_col} + 1'b1) < mac_n_groups) || (({1'b0, mac_group_row} + 1'b1) < mac_m_groups);
    mac_op_has_more_reductions = (({1'b0, mac_reduction_idx} + 1'b1) < mac_op_exec.tk);
    mac_op_has_more_beats = mac_op_has_more_spatial || mac_op_has_more_reductions;
  end : mac_op_has_more

  always_comb begin : mac_op_fire
    mac_nonfma_ready  = &vrf_rvalid_i[1:0] && (mac_op_has_more_beats || mac_commit_ready);
    mac_step_fire = (mac_op_empty && mac_commit_ready) ||
                    (mac_op_exec_valid && !mac_op_empty && fma_pipe_ready && mac_nonfma_ready);
    mac_fire          = mac_op_exec_valid && !mac_op_empty && fma_pipe_ready && mac_nonfma_ready;
  end : mac_op_fire

  assign mac_commit_valid = mac_op_empty ||
      (mac_op_exec_valid && !mac_op_empty && !mac_op_has_more_beats && fma_pipe_ready && &vrf_rvalid_i[1:0]);
  assign mac_idle = !mac_op_current_q.valid && !mac_op_next_q.valid && !(|fma_pipe_valid);
  assign mac_op_queue_ready = !mac_op_next_q.valid || (mac_commit_valid && mac_commit_ready);

  always_comb begin : mac_op_update
    mac_op_current_d = mac_op_current_q;
    mac_op_next_d    = mac_op_next_q;
    mac_beat_d       = mac_beat_q;
    mac_reduction_d  = mac_reduction_q;

    if (mac_step_fire && !mac_op_bypass) begin
      if (mac_op_empty) begin
        mac_op_current_d = mac_op_next_q;
        mac_op_next_d = mac_op_slot_t'(0);
        mac_beat_d = $bits(mac_beat_d)'(0);
        mac_reduction_d = $bits(mac_reduction_d)'(0);
      end else if (mac_op_has_more_spatial) begin
        mac_beat_d = mac_next_spatial_beat;
      end else if (mac_op_has_more_reductions) begin
        mac_beat_d = $bits(mac_beat_d)'(0);
        mac_reduction_d = mac_reduction_idx + 1'b1;
      end else begin
        mac_op_current_d = mac_op_next_q;
        mac_op_next_d = mac_op_slot_t'(0);
        mac_beat_d = $bits(mac_beat_d)'(0);
        mac_reduction_d = $bits(mac_reduction_d)'(0);
      end
    end

    if (mac_req_valid && mac_req_ready) begin
      mac_op_t new_op;
      new_op = '{
        tile   : spatz_req_mac.mtd,
        id     : spatz_req_mac.id,
        vs1    : spatz_req_mac.vs1,
        vs2    : spatz_req_mac.vs2,
        ew     : spatz_req_mac.vtype.vsew,
        tew    : spatz_req_mac.op_ope.tew,
        is_alt : (spatz_req_mac.op == VTFMM_ALT),
        tk     : spatz_req_mac.op_ope.tk,
        tm     : (spatz_req_mac.op_ope.tm > TE) ? tile_count_t'(TE) : tile_count_t'(spatz_req_mac.op_ope.tm),
        tn     : (spatz_req_mac.op_ope.tn > TE) ? tile_count_t'(TE) : tile_count_t'(spatz_req_mac.op_ope.tn)
      };

      if (!mac_op_current_q.valid) begin
        mac_op_current_d.op    = new_op;
        mac_op_current_d.valid = !(mac_op_bypass && mac_commit_valid && mac_commit_ready);

        if (mac_op_bypass && mac_step_fire) begin
          mac_beat_d = (mac_op_has_more_spatial) ? mac_next_spatial_beat : SpatialBeatW'(0);
          mac_reduction_d = (!mac_op_has_more_spatial && mac_op_has_more_reductions) ?
                            mac_reduction_idx + 1'b1 : ReductionIdxW'(0);
        end

      end else if (mac_commit_valid && mac_commit_ready && !mac_op_next_q.valid) begin
        mac_op_current_d.op    = new_op;
        mac_op_current_d.valid = 1'b1;
        mac_beat_d             = SpatialBeatW'(0);
        mac_reduction_d        = ReductionIdxW'(0);

      end else begin
        mac_op_next_d.op    = new_op;
        mac_op_next_d.valid = 1'b1;
      end
    end
  end : mac_op_update

   `FF(mac_op_current_q, mac_op_current_d, mac_op_slot_t'(0))
   `FF(mac_op_next_q, mac_op_next_d, mac_op_slot_t'(0))
   `FF(mac_beat_q, mac_beat_d, $bits(mac_beat_q)'(0))
   `FF(mac_reduction_q, mac_reduction_d, $bits(mac_reduction_q)'(0))

  ////----------------------------------------------------------------////
  ////                     Resident Tile State                        ////
  ////----------------------------------------------------------------////

  assign resident_start_fire = mac_fire && !resident_valid_q && (mac_op_beat == SpatialBeatW'(0));
  // A queued MAC must be able to drain a mismatching resident result before
  // it becomes executable.  Qualifying these conditions with exec_valid made
  // tile_hazard and result_ready wait on each other when the pipeline was full.
  assign resident_switch_req = (mac_op_current_q.valid || mac_op_bypass) && resident_valid_q &&
      !resident_drain_valid_q && (mac_op_exec.tile != resident_tile_q);
  assign resident_switch_fire = resident_switch_req && mac_fire;
  assign resident_beat_mismatch = (mac_op_current_q.valid || mac_op_bypass) && resident_valid_q &&
      !resident_drain_valid_q && fma_pipe_result_valid && (mac_op_exec.tile == resident_tile_q) &&
      (fma_pipe_tag_q[NumPipeRegs-1].tile == resident_tile_q) && (fma_pipe_tag_q[NumPipeRegs-1].beat != mac_op_beat);

  always_comb begin : resident_state_update
    logic start_drain;
    logic start_writeback;

    resident_valid_d       = resident_valid_q;
    resident_tile_d        = resident_tile_q;
    resident_drain_valid_d = resident_drain_valid_q;
    resident_drain_tile_d  = resident_drain_tile_q;
    resident_drain_writeback_d = resident_drain_writeback_q;

    start_drain = 1'b0;
    start_writeback = 1'b0;

    if (resident_valid_q && !resident_drain_valid_q && (|fma_pipe_valid_next)) begin
      // VTSE is already older than any MAC sitting in the OPE queues.  Drain
      // the resident FMA state even when a younger same-tile MAC is queued;
      // that MAC remains blocked by tile_rvalid_i until the read completes.
      if (tile_rvalid_i && (tile_r_req_i.idx == resident_tile_q)) begin
        start_drain = 1'b1;
        start_writeback = 1'b1;
      end else if (!mac_op_current_d.valid && !mac_op_next_d.valid &&
                   clean_req_valid && (spatz_req_clean.op_ope.is_discard)) begin
        start_drain = 1'b1;
      end else if (!mac_op_current_d.valid && !mac_op_next_d.valid && clean_req_valid && (spatz_req_clean.op_ope.is_zero_tile) &&
                   (spatz_req_clean.mtd == resident_tile_q)) begin
        start_drain = 1'b1;
      end else if (!mac_op_current_d.valid && !mac_op_next_d.valid &&
                   tile_wvalid_i && (tile_w_req_i.idx == resident_tile_q)) begin
        start_drain = 1'b1;
        start_writeback = 1'b1;
      end else if (!mac_op_current_d.valid && !mac_op_next_d.valid && vt_req_valid &&
                   (spatz_req_vt.op_ope.tss.tile_id == resident_tile_q)) begin
        start_drain = 1'b1;
        start_writeback = 1'b1;
      end else if (!mac_op_current_d.valid && !mac_op_next_d.valid && tv_req_valid &&
                   (spatz_req_tv.op_ope.tss.tile_id == resident_tile_q)) begin
        start_drain = 1'b1;
        start_writeback = 1'b1;
      end
    end

    if (start_drain) begin
      resident_drain_valid_d = 1'b1;
      resident_drain_tile_d = resident_tile_q;
      resident_drain_writeback_d = start_writeback;
    end

    if (resident_start_fire) begin
      resident_valid_d = 1'b1;
      resident_tile_d = mac_op_exec.tile;
    end
    if (resident_switch_fire) begin
      resident_valid_d = 1'b1;
      resident_tile_d = mac_op_exec.tile;
    end

    if (resident_drain_valid_q && !(|fma_pipe_tile_busy_next[resident_drain_tile_q])) begin
      if (resident_valid_q && (resident_tile_q == resident_drain_tile_q))
        resident_valid_d = 1'b0;
      resident_drain_valid_d = 1'b0;
      resident_drain_writeback_d = 1'b0;
    end
  end : resident_state_update

   `FF(resident_valid_q, resident_valid_d, 1'b0)
   `FF(resident_tile_q, resident_tile_d, mt_t'(0))
   `FF(resident_drain_valid_q, resident_drain_valid_d, 1'b0)
   `FF(resident_drain_tile_q, resident_drain_tile_d, mt_t'(0))
   `FF(resident_drain_writeback_q, resident_drain_writeback_d, 1'b0)

  ////----------------------------------------------------------------////
  ////                         FMA Pipeline                           ////
  ////----------------------------------------------------------------////

  assign fma_pipe_ready   = (&fma16_ready) && (&fma32_ready);
  assign fma_pipe_advance = (mac_fire || (|fma_pipe_valid)) && fma_pipe_ready;
  assign fma_pipe_tag_in = mac_fire ? '{valid: 1'b1, tile: mac_op_exec.tile, tew: mac_op_exec.tew, beat: mac_op_beat,
        lane_active: mac_lane_active, write_acc: 1'b0} : fma_pipe_tag_t'(0);

  always_comb begin : fma_pipe_tag_update
    fma_pipe_tag_d = fma_pipe_tag_q;
    if (resident_switch_fire) begin
      for (int unsigned stage = 0; stage < NumPipeRegs; stage++)
        fma_pipe_tag_d[stage].write_acc = 1'b1;
    end
    if (fma_pipe_advance) begin
      fma_pipe_tag_d = {fma_pipe_tag_d[NumPipeRegs-2:0], fma_pipe_tag_in};
    end
  end : fma_pipe_tag_update

  // Occupancy is already encoded by the tag pipeline. Derive empty/full and
  // the per-tile beat map instead of storing a second copy in flip-flops.
  always_comb begin : fma_pipe_occupancy
    fma_pipe_tile_busy      = '0;
    fma_pipe_tile_busy_next = '0;
    fma_pipe_valid          = '0;
    fma_pipe_valid_next     = '0;
    for (int unsigned stage = 0; stage < NumPipeRegs; stage++) begin
      fma_pipe_valid[stage] = fma_pipe_tag_q[stage].valid;
      fma_pipe_valid_next[stage] = fma_pipe_tag_d[stage].valid;
      if (fma_pipe_tag_q[stage].valid) begin
        fma_pipe_tile_busy[fma_pipe_tag_q[stage].tile][fma_pipe_tag_q[stage].beat] = 1'b1;
      end
      if (fma_pipe_tag_d[stage].valid) begin
        fma_pipe_tile_busy_next[fma_pipe_tag_d[stage].tile][fma_pipe_tag_d[stage].beat] = 1'b1;
      end
    end
  end : fma_pipe_occupancy

  // Result handshake, drain and forwarding.
  assign fma_pipe_result_valid = fma_result_valid[0][0];

  assign fma_pipe_drain = fma_pipe_tag_q[NumPipeRegs-1].write_acc || (fma_pipe_result_valid && ((resident_switch_req &&
         (fma_pipe_tag_q[NumPipeRegs-1].tile != mac_op_exec.tile)) || resident_beat_mismatch));
  assign fma_pipe_write_acc = fma_pipe_drain ||
      (resident_drain_valid_q && resident_drain_writeback_q &&
       (fma_pipe_tag_q[NumPipeRegs-1].tile == resident_drain_tile_q));

  assign fma_pipe_continue_ready = mac_op_exec_valid && &vrf_rvalid_i[1:0] &&
      (fma_pipe_tag_q[NumPipeRegs-1].tile == mac_op_exec.tile) && (fma_pipe_tag_q[NumPipeRegs-1].beat == mac_op_beat);

  assign fma_pipe_result_ready = ((resident_drain_valid_q && (fma_pipe_tag_q[NumPipeRegs-1].tile == resident_drain_tile_q)) ||
       fma_pipe_drain) ? 1'b1 : (fma_pipe_tag_q[NumPipeRegs-1].write_acc ? mac_commit_ready : fma_pipe_continue_ready);
  assign fma_pipe_result_fire = fma_pipe_result_valid && fma_pipe_result_ready;
  assign fma_pipe_result_forward = mac_fire && fma_pipe_result_fire && !resident_drain_valid_q && !fma_pipe_drain &&
      (fma_pipe_tag_q[NumPipeRegs-1].tile == mac_op_exec.tile) && (fma_pipe_tag_q[NumPipeRegs-1].beat == mac_op_beat) &&
      !fma_pipe_tag_q[NumPipeRegs-1].write_acc;

  `FF(fma_pipe_tag_q, fma_pipe_tag_d, '0)

  ////////////////////////////////////////////////////////////////////////
  ////                  Tile -> VRF Move Operation                    ////
  ////////////////////////////////////////////////////////////////////////

  always_comb begin : vt_move_shape
    tile_count_t remaining_lines;
    tile_count_t requested_lines;
    vreg_group_count_t group_regs;
    vt_word_count_t group_words;

    vt_active_words = vt_word_count_t'(0);
    vt_active_elems = vlen_t'(0);
    vt_active_lines = tile_count_t'(0);
    unique case (vt_req_q.op_ope.tew)
      EW_8:    vt_elems_per_word = vrf_elem_count_t'(VRFWordWidth / 8);
      EW_16:   vt_elems_per_word = vrf_elem_count_t'(VRFWordWidth / 16);
      default: vt_elems_per_word = vrf_elem_count_t'(VRFWordWidth / AccElemWidth);
    endcase
    vt_line_elems = (vt_req_q.op_ope.tss.is_row ? vt_req_q.op_ope.tn : vt_req_q.op_ope.tm) > TE ?
                    tile_count_t'(TE) : tile_count_t'(vt_req_q.op_ope.tss.is_row ? vt_req_q.op_ope.tn : vt_req_q.op_ope.tm);
    vt_line_words = (vt_line_elems + vt_elems_per_word - 1) / vt_elems_per_word;

    unique case (vt_req_q.vtype.vlmul)
      LMUL_2:  group_regs = 2;
      LMUL_4:  group_regs = 4;
      LMUL_8:  group_regs = 8;
      default: group_regs = vreg_group_count_t'(1);
    endcase
    group_words = vt_word_count_t'(group_regs) * NrWordsPerVector;
    if (group_words > ((NRVREG - vt_req_q.vd) * NrWordsPerVector))
      group_words = vt_word_count_t'((NRVREG - vt_req_q.vd) * NrWordsPerVector);

    remaining_lines = tile_count_t'(0);
    if (vt_req_q.op_ope.tss.is_row) begin
      if (vt_req_q.op_ope.tm > vt_req_q.op_ope.tss.index)
        remaining_lines = (vt_req_q.op_ope.tm - vt_req_q.op_ope.tss.index > TE) ? tile_count_t'(TE) :
            tile_count_t'(vt_req_q.op_ope.tm - vt_req_q.op_ope.tss.index);
    end else if (vt_req_q.op_ope.tn > vt_req_q.op_ope.tss.index) begin
      remaining_lines = (vt_req_q.op_ope.tn - vt_req_q.op_ope.tss.index > TE) ? tile_count_t'(TE) :
          tile_count_t'(vt_req_q.op_ope.tn - vt_req_q.op_ope.tss.index);
    end

    requested_lines = tile_count_t'(0);
    if (vt_line_elems != 0)
      requested_lines = (vt_req_q.vl > TE * vt_line_elems) ? tile_count_t'(TE) :
          tile_count_t'((vt_req_q.vl + vt_line_elems - 1'b1) / vt_line_elems);

    if ((vt_line_words != 0) && (requested_lines != 0)) begin
      vt_active_lines = (group_words / vt_line_words > TE) ? tile_count_t'(TE) : tile_count_t'(group_words / vt_line_words);
      if (vt_active_lines > requested_lines)
        vt_active_lines = requested_lines;
      if (vt_active_lines > remaining_lines)
        vt_active_lines = remaining_lines;
      vt_active_words = vt_word_count_t'(vt_active_lines) * vt_line_words;
      vt_active_elems = vlen_t'(vt_active_lines) * vt_line_elems;
      if (vt_active_elems > vt_req_q.vl)
        vt_active_elems = vt_req_q.vl;
    end
  end

  assign vt_word_fire = vt_busy_q && vt_req_q.use_vd && vrf_wvalid_i;

  always_comb begin : vt_handler
    vt_busy_d        = vt_busy_q;
    vt_word_idx_d    = vt_word_idx_q;
    vt_commit_valid  = 1'b0;
    if (vt_req_valid && vt_req_ready) begin
      vt_busy_d = 1'b1;
      vt_word_idx_d = $bits(vt_word_idx_d)'(0);
    end

    if (vt_busy_q && (vt_active_words == 0)) begin
      vt_commit_valid = 1'b1;
    end else if (vt_word_fire) begin
      if (({1'b0, vt_word_idx_q} + 1'b1) >= vt_active_words)
        vt_commit_valid = 1'b1;
      else
        vt_word_idx_d = vt_word_idx_q + 1'b1;
    end

    if (vt_commit_valid && vt_commit_ready) begin
      vt_busy_d = 1'b0;
      vt_word_idx_d = $bits(vt_word_idx_d)'(0);
    end
  end : vt_handler

  ////////////////////////////////////////////////////////////////////////
  ////                  VRF -> Tile Move Operation                    ////
  ////////////////////////////////////////////////////////////////////////

  always_comb begin : tv_move_shape
    tile_count_t remaining_lines;
    tile_count_t requested_lines;
    vreg_group_count_t group_regs;
    vt_word_count_t group_words;

    requested_lines = tile_count_t'(0);
    tv_active_words = vt_word_count_t'(0);
    tv_active_elems = vlen_t'(0);
    tv_active_lines = tile_count_t'(0);
    unique case (tv_req_q.op_ope.tew)
      EW_8:    tv_elems_per_word = vrf_elem_count_t'(VRFWordWidth / 8);
      EW_16:   tv_elems_per_word = vrf_elem_count_t'(VRFWordWidth / 16);
      default: tv_elems_per_word = vrf_elem_count_t'(VRFWordWidth / AccElemWidth);
    endcase
    tv_line_elems = (tv_req_q.op_ope.tss.is_row ? tv_req_q.op_ope.tn : tv_req_q.op_ope.tm) > TE ?
                    tile_count_t'(TE) : tile_count_t'(tv_req_q.op_ope.tss.is_row ? tv_req_q.op_ope.tn : tv_req_q.op_ope.tm);
    tv_line_words = (tv_line_elems + tv_elems_per_word - 1) / tv_elems_per_word;

    unique case (tv_req_q.vtype.vlmul)
      LMUL_2:  group_regs = 2;
      LMUL_4:  group_regs = 4;
      LMUL_8:  group_regs = 8;
      default: group_regs = vreg_group_count_t'(1);
    endcase
    group_words = vt_word_count_t'(group_regs) * NrWordsPerVector;
    if (group_words > ((NRVREG - tv_req_q.vs2) * NrWordsPerVector))
      group_words = vt_word_count_t'((NRVREG - tv_req_q.vs2) * NrWordsPerVector);

    remaining_lines = tile_count_t'(0);
    if (tv_req_q.op_ope.tss.is_row) begin
      if (tv_req_q.op_ope.tm > tv_req_q.op_ope.tss.index)
        remaining_lines = (tv_req_q.op_ope.tm - tv_req_q.op_ope.tss.index > TE) ? tile_count_t'(TE) :
            tile_count_t'(tv_req_q.op_ope.tm - tv_req_q.op_ope.tss.index);
    end else if (tv_req_q.op_ope.tn > tv_req_q.op_ope.tss.index) begin
      remaining_lines = (tv_req_q.op_ope.tn - tv_req_q.op_ope.tss.index > TE) ? tile_count_t'(TE) :
          tile_count_t'(tv_req_q.op_ope.tn - tv_req_q.op_ope.tss.index);
    end

    if (tv_line_elems != 0)
      requested_lines = (tv_req_q.vl > TE * tv_line_elems) ? tile_count_t'(TE) :
          tile_count_t'((tv_req_q.vl + tv_line_elems - 1'b1) / tv_line_elems);

    if ((tv_line_words != 0) && (requested_lines != 0)) begin
      tv_active_lines = (group_words / tv_line_words > TE) ? tile_count_t'(TE) : tile_count_t'(group_words / tv_line_words);
      if (tv_active_lines > requested_lines)
        tv_active_lines = requested_lines;
      if (tv_active_lines > remaining_lines)
        tv_active_lines = remaining_lines;
      tv_active_words = vt_word_count_t'(tv_active_lines) * tv_line_words;
      tv_active_elems = vlen_t'(tv_active_lines) * tv_line_elems;
      if (tv_active_elems > tv_req_q.vl)
        tv_active_elems = tv_req_q.vl;
    end
  end

  // TV owns the third OPE read port, independently of MAC vs2/vs1.
  assign tv_vrf_avail = tv_data_latched_q || (tv_busy_q && vrf_rvalid_i[2]);
  assign tv_vrf_data  = tv_data_latched_q ? tv_data_q : vrf_rdata_i[2];

  assign tv_acc_wen = tv_busy_q && tv_vrf_avail && !vt_busy_q;

  always_comb begin : tv_handler
    tv_busy_d        = tv_busy_q;
    tv_word_idx_d    = tv_word_idx_q;
    tv_commit_valid  = 1'b0;
    if (tv_req_valid && tv_req_ready) begin
      tv_busy_d = 1'b1;
      tv_word_idx_d = $bits(tv_word_idx_d)'(0);
    end

    if (tv_busy_q && (tv_active_words == vt_word_count_t'(0)))
      tv_commit_valid = 1'b1;
    else if (tv_acc_wen) begin
      if (({1'b0, tv_word_idx_q} + 1'b1) >= tv_active_words)
        tv_commit_valid = 1'b1;
      else
        tv_word_idx_d = tv_word_idx_q + 1'b1;
    end

    if (tv_commit_valid && tv_commit_ready) begin
      tv_busy_d = 1'b0;
      tv_word_idx_d = $bits(tv_word_idx_d)'(0);
    end
  end : tv_handler

  ////////////////////////////////////////////////////////////////////////
  ////                    Tile Clean Operation                        ////
  ////////////////////////////////////////////////////////////////////////
  
  always_comb begin : mac_tile_busy_proc
    mac_tile_busy = $bits(mac_tile_busy)'(0);

    if (mac_op_current_q.valid)
      mac_tile_busy[mac_op_current_q.op.tile] = 1'b1;
    if (mac_op_next_q.valid)
      mac_tile_busy[mac_op_next_q.op.tile] = 1'b1;
    for (int unsigned tile = 0; tile < NrPhysicalTile; tile++) begin
      if (|fma_pipe_tile_busy[tile])
        mac_tile_busy[tile] = 1'b1;
    end
  end : mac_tile_busy_proc

  always_comb begin : clean_acc_ready_proc
    clean_acc_ready = 1'b0;
    if (spatz_req_clean.op_ope.is_zero_tile)
      clean_acc_ready = !resident_drain_valid_q && !mac_tile_busy[spatz_req_clean.mtd] &&
          !(tile_rvalid_i && (tile_r_req_i.idx == spatz_req_clean.mtd)) && (!resident_valid_q ||
           (spatz_req_clean.mtd != resident_tile_q));
    else if (spatz_req_clean.op_ope.is_discard)
      clean_acc_ready = !resident_valid_q && !resident_drain_valid_q && mac_idle && !vt_busy_q && !tv_busy_q &&
                         !tile_rvalid_i && !tile_wvalid_i;
  end : clean_acc_ready_proc

  ////////////////////////////////////////////////////////////////////////
  ////                        VRF Port Access                         ////
  ////////////////////////////////////////////////////////////////////////

  // Read ports [0:1] feed MAC vs2/vs1; read port [2] feeds TV.
  // ID [3] accompanies the independent VT write port.
  always_comb begin : sb_ids
    vrf_id_o[0] = mac_op_exec.id;
    vrf_id_o[1] = mac_op_exec.id;
    vrf_id_o[2] = tv_req_q.id;
    vrf_id_o[3] = vt_req_q.id;
  end

  ////////////////////////////////////////////////////////////////////////
  ////                      VRF -> Tile Read   Ports                  ////
  ////////////////////////////////////////////////////////////////////////

  always_comb begin : vrf_re_proc
    logic [3:0] row_stride_regs;
    operand_width_t operand_bits;
    vrf_addr_t reduction_word_offset;
    vrf_addr_t row_word_offset;
    vrf_addr_t col_word_offset;

    vrf_re_o = $bits(vrf_re_o)'(0);
    vrf_raddr_o = $bits(vrf_raddr_o)'(0);
    row_stride_regs = 4'(1);
    operand_bits = operand_width_t'(0);
    reduction_word_offset = vrf_addr_t'(0);
    row_word_offset = vrf_addr_t'(0);
    col_word_offset = vrf_addr_t'(0);

    if (mac_op_exec_valid) begin
      unique case (mac_op_exec.ew)
        EW_8: begin
          operand_bits = 6'd8;
          row_stride_regs = 4'd2;
        end
        EW_16: begin
          operand_bits = 6'd16;
          row_stride_regs = 4'd4;
        end
        default: begin
          operand_bits = 6'd32;
          row_stride_regs = 4'd8;
        end
      endcase
      // vs1/vs2 name the first K row. mtype.tk selects subsequent rows;
      // tm/tn select the 256-bit spatial word. LMUL does not add MAC steps.
      reduction_word_offset = vrf_addr_t'(mac_reduction_idx) * row_stride_regs * NrWordsPerVector;
      row_word_offset = vrf_addr_t'((mac_group_row * CE * operand_bits) / VRFWordWidth);
      col_word_offset = vrf_addr_t'((mac_group_col * CE * operand_bits) / VRFWordWidth);
      vrf_re_o[1:0] = 2'b11;
      vrf_raddr_o[0] = vrf_addr_t'(mac_op_exec.vs2) * NrWordsPerVector + reduction_word_offset + row_word_offset;
      vrf_raddr_o[1] = vrf_addr_t'(mac_op_exec.vs1) * NrWordsPerVector + reduction_word_offset + col_word_offset;
    end

    if (tv_busy_q && !tv_data_latched_q) begin
      vrf_re_o[2]    = 1'b1;
      vrf_raddr_o[2] = (vrf_addr_t'(tv_req_q.vs2) << $clog2(NrWordsPerVector)) + vrf_addr_t'(tv_word_idx_q);
    end
  end

  ////////////////////////////////////////////////////////////////////////
  ////                      Tile -> VRF Write Port                    ////
  ////////////////////////////////////////////////////////////////////////

  always_comb begin : vrf_wr_proc
    tile_dim_t idx;
    tile_dim_t row_i;
    tile_dim_t col_i;
    vlen_t active_len;
    tile_sum_t elem_idx;
    vlen_t flat_elem_idx;
    tile_dim_t line_idx;
    tile_dim_t line_word_idx;
    vlen_t vstart_i;

    vrf_waddr_o = vrf_addr_t'(0);
    vrf_we_o    = 1'b0;
    vrf_wbe_o = vrf_be_t'(0);
    vrf_wdata_o = vrf_data_t'(0);

    row_i = tile_dim_t'(0);
    col_i = tile_dim_t'(0);
    elem_idx = tile_sum_t'(0);
    flat_elem_idx = vlen_t'(0);
    line_idx = tile_dim_t'(0);
    line_word_idx = tile_dim_t'(0);

    active_len = vt_active_elems;
    vstart_i   = vt_req_q.vstart;

    if (vt_busy_q) begin
      if (vt_line_words != 0) begin
        line_idx = tile_dim_t'(vt_word_idx_q / vt_line_words);
        line_word_idx = tile_dim_t'(vt_word_idx_q % vt_line_words);
      end
      // A flat word offset deliberately crosses physical-register
      // boundaries, so vd denotes the base of an LMUL register group.
      vrf_waddr_o =(vrf_addr_t'(vt_req_q.vd) << $clog2(NrWordsPerVector)) + vrf_addr_t'(vt_word_idx_q);
      vrf_we_o = vt_req_q.use_vd;

      for (int lane = 0; lane < VtMaxElemsPerWord; lane++) begin
        elem_idx = line_word_idx * vt_elems_per_word + lane;
        flat_elem_idx = vlen_t'(line_idx) * vt_line_elems + elem_idx;
        if ((flat_elem_idx >= vstart_i) && (flat_elem_idx < active_len) && (elem_idx < vt_line_elems) &&
            (lane < vt_elems_per_word)) begin
          logic [AccElemWidth-1:0] acc_word;

          if (vt_req_q.op_ope.tss.is_row) begin
            row_i = vt_req_q.op_ope.tss.index + line_idx;
            col_i = elem_idx[$clog2(TE)-1:0];
          end else begin
            row_i = elem_idx[$clog2(TE)-1:0];
            col_i = vt_req_q.op_ope.tss.index + line_idx;
          end

          acc_word = acc_vrf_rdata[row_i % CE][col_i % CE][col_i / CE];
          for (int unsigned byte_idx = 0; byte_idx < AccElemBytes; byte_idx++) begin
            if (acc_zero_q[vt_acc_sel.tile][(row_i / CE) * GroupsPerEdge + (col_i / CE)][byte_idx])
              acc_word[byte_idx*8 +: 8] = 8'd0;
          end
          unique case (vt_req_q.op_ope.tew)
            EW_8: begin
              vrf_wdata_o[lane*8 +: 8] = acc_word[vt_acc_sel.byte_offset*8 +: 8];
              vrf_wbe_o[lane] = 1'b1;
            end
            EW_16: begin
              vrf_wdata_o[lane*16 +: 16] = acc_word[vt_acc_sel.byte_offset*8 +: 16];
              vrf_wbe_o[lane*2 +: 2] = 2'b11;
            end
            default: begin
              vrf_wdata_o[lane*AccElemWidth +: AccElemWidth] = acc_word;
              vrf_wbe_o[lane*AccElemBytes +: AccElemBytes] = AccFullMask;
            end
          endcase
        end
      end
    end
  end

  ////----------------------------------------------------------------////
  ////       Shared Accumulator Access and Tile Slice Selection       ////
  ////----------------------------------------------------------------////

  typedef struct packed {
    logic [AccTileIdxW-1:0]  tile;
    logic [AccByteIdxW-1:0]  byte_offset;
    logic [AccElemBytes-1:0] byte_enable;
  } acc_sel_t;
  
  // Accumulator slice selection.
  acc_sel_t mac_acc_sel;
  acc_sel_t fma_acc_sel;
  acc_sel_t tv_acc_sel;
  acc_sel_t vt_acc_sel;
  acc_sel_t tile_r_acc_sel;
  acc_sel_t tile_w_acc_sel;
  acc_sel_t clean_acc_sel;
  
  // Decode each architectural tile exactly once. The physical accumulator is
  // 32 bits wide; TEW8 selects a byte, TEW16 a half-word, and TEW32 the full word.
  always_comb begin : acc_select_proc
    mac_acc_sel.tile = AccTileIdxW'(mac_op_exec.tile / AccElemBytes);
    mac_acc_sel.byte_offset = AccByteIdxW'(mac_op_exec.tile % AccElemBytes);
    unique case (mac_op_exec.tew)
      EW_8: mac_acc_sel.byte_enable = AccElemBytes'(1) << mac_acc_sel.byte_offset;
      EW_16: mac_acc_sel.byte_enable = AccHalfMask << mac_acc_sel.byte_offset;
      EW_32: mac_acc_sel.byte_enable = AccFullMask;
      default: mac_acc_sel.byte_enable = AccElemBytes'(0);
    endcase

    fma_acc_sel.tile = AccTileIdxW'(fma_pipe_tag_q[NumPipeRegs-1].tile / AccElemBytes);
    fma_acc_sel.byte_offset = AccByteIdxW'(fma_pipe_tag_q[NumPipeRegs-1].tile % AccElemBytes);
    unique case (fma_pipe_tag_q[NumPipeRegs-1].tew)
      EW_8: fma_acc_sel.byte_enable = AccElemBytes'(1) << fma_acc_sel.byte_offset;
      EW_16: fma_acc_sel.byte_enable = AccHalfMask << fma_acc_sel.byte_offset;
      EW_32: fma_acc_sel.byte_enable = AccFullMask;
      default: fma_acc_sel.byte_enable = AccElemBytes'(0);
    endcase

    tv_acc_sel.tile = AccTileIdxW'(tv_req_q.op_ope.tss.tile_id / AccElemBytes);
    tv_acc_sel.byte_offset = AccByteIdxW'(tv_req_q.op_ope.tss.tile_id % AccElemBytes);
    unique case (tv_req_q.op_ope.tew)
      EW_8: tv_acc_sel.byte_enable = AccElemBytes'(1) << tv_acc_sel.byte_offset;
      EW_16: tv_acc_sel.byte_enable = AccHalfMask << tv_acc_sel.byte_offset;
      EW_32: tv_acc_sel.byte_enable = AccFullMask;
      default: tv_acc_sel.byte_enable = AccElemBytes'(0);
    endcase

    vt_acc_sel.tile = AccTileIdxW'( vt_req_q.op_ope.tss.tile_id / AccElemBytes);
    vt_acc_sel.byte_offset = AccByteIdxW'( vt_req_q.op_ope.tss.tile_id % AccElemBytes);
    unique case (vt_req_q.op_ope.tew)
      EW_8: vt_acc_sel.byte_enable = AccElemBytes'(1) << vt_acc_sel.byte_offset;
      EW_16: vt_acc_sel.byte_enable = AccHalfMask << vt_acc_sel.byte_offset;
      EW_32: vt_acc_sel.byte_enable = AccFullMask;
      default: vt_acc_sel.byte_enable = AccElemBytes'(0);
    endcase

    tile_r_acc_sel.tile = AccTileIdxW'(tile_r_req_i.idx / AccElemBytes);
    tile_r_acc_sel.byte_offset = AccByteIdxW'(tile_r_req_i.idx % AccElemBytes);
    unique case (tile_r_req_i.tew)
      EW_8: tile_r_acc_sel.byte_enable = AccElemBytes'(1) << tile_r_acc_sel.byte_offset;
      EW_16: tile_r_acc_sel.byte_enable = AccHalfMask << tile_r_acc_sel.byte_offset;
      EW_32: tile_r_acc_sel.byte_enable = AccFullMask;
      default: tile_r_acc_sel.byte_enable = AccElemBytes'(0);
    endcase

    tile_w_acc_sel.tile = AccTileIdxW'(tile_w_req_i.idx / AccElemBytes);
    tile_w_acc_sel.byte_offset = AccByteIdxW'(tile_w_req_i.idx % AccElemBytes);
    unique case (tile_w_req_i.tew)
      EW_8: tile_w_acc_sel.byte_enable = AccElemBytes'(1) << tile_w_acc_sel.byte_offset;
      EW_16: tile_w_acc_sel.byte_enable = AccHalfMask << tile_w_acc_sel.byte_offset;
      EW_32: tile_w_acc_sel.byte_enable = AccFullMask;
      default: tile_w_acc_sel.byte_enable = AccElemBytes'(0);
    endcase

    clean_acc_sel.tile = AccTileIdxW'(spatz_req_clean.mtd / AccElemBytes);
    clean_acc_sel.byte_offset = AccByteIdxW'(spatz_req_clean.mtd % AccElemBytes);
    unique case (spatz_req_clean.op_ope.tew)
      EW_8: clean_acc_sel.byte_enable = AccElemBytes'(1) << clean_acc_sel.byte_offset;
      EW_16: clean_acc_sel.byte_enable = AccHalfMask << clean_acc_sel.byte_offset;
      EW_32: clean_acc_sel.byte_enable = AccFullMask;
      default: clean_acc_sel.byte_enable = AccElemBytes'(0);
    endcase
  end : acc_select_proc

  // Compare IDs in parallel with per-tile beat reduction, avoiding a wide beat-vector mux before reduction.
  for (genvar tile = 0; tile < NrPhysicalTile; tile++) begin : gen_tile_access_conflict
    assign tile_read_pipe_conflict[tile] = (tile_r_req_i.idx == mt_t'(tile)) && (|fma_pipe_tile_busy[tile]);
    assign tile_write_pipe_conflict[tile] = (tile_w_req_i.idx == mt_t'(tile)) && (|fma_pipe_tile_busy[tile]);
  end

  assign tile_access_blocked = resident_drain_valid_q;
  assign tile_write_mac_conflict =
      (mac_op_current_q.valid && (mac_op_current_q.op.tile == tile_w_req_i.idx)) ||
      (mac_op_next_q.valid && (mac_op_next_q.op.tile == tile_w_req_i.idx)) ||
      (mac_op_bypass && (spatz_req_mac.mtd == tile_w_req_i.idx));

  // When write-ready can be high, drain/resident/MAC conflicts and tile_rvalid_i are already excluded.
  // For a matching VTZERO these are precisely the clean_acc_ready conditions, so no ready feedback is needed.
  assign tile_write_zero_conflict = clean_req_valid && spatz_req_clean.op_ope.is_zero_tile &&
                                   clean_commit_ready && (spatz_req_clean.mtd == tile_w_req_i.idx);

  // A valid tile read already blocks matching MAC issue/bypass and VTZERO in their own arbitration.
  // Do not block on queued MACs here: an older tile read must be able to complete ahead of them.
  // The registered busy bits include the final FMA result until its retirement edge.
  assign tile_rready_o = !tile_access_blocked && !(|tile_read_pipe_conflict) &&
      !(resident_valid_q && (resident_tile_q == tile_r_req_i.idx)) &&
      !(tv_busy_q && tv_vrf_avail && (tv_req_q.op_ope.tss.tile_id == tile_r_req_i.idx));

  assign tile_wready_o = !tile_access_blocked && !tile_rvalid_i && !(|tile_write_pipe_conflict) &&
      !tile_write_mac_conflict && !tile_write_zero_conflict &&
      !(resident_valid_q && (resident_tile_q == tile_w_req_i.idx)) &&
      !(vt_busy_q && (vt_req_q.op_ope.tss.tile_id == tile_w_req_i.idx)) &&
      !(tv_busy_q && tv_vrf_avail && (tv_req_q.op_ope.tss.tile_id == tile_w_req_i.idx));

  // FMA, VRF moves and VLSU tile transfers have independent accumulator addresses.
  always_comb begin : acc_read_addr_proc
    logic [GroupIdxW-1:0] bank_row;
    tile_dim_t line_idx;
    tile_dim_t line_word_idx;

    acc_fma_raddr = AccAddrW'(mac_acc_sel.tile * SpatialBeats + mac_op_beat);
    acc_vrf_raddr = AccAddrW'(0);
    acc_vlsu_raddr = AccAddrW'(0);
    bank_row = GroupIdxW'(0);
    line_idx = tile_dim_t'(0);
    line_word_idx = tile_dim_t'(0);

    if (vt_busy_q) begin
      if (vt_line_words != 0) begin
        line_idx = tile_dim_t'(vt_word_idx_q / vt_line_words);
        line_word_idx = tile_dim_t'(vt_word_idx_q % vt_line_words);
      end
      bank_row = vt_req_q.op_ope.tss.is_row ? GroupIdxW'((vt_req_q.op_ope.tss.index + line_idx) / CE) :
          GroupIdxW'((line_word_idx * vt_elems_per_word) / CE);
      acc_vrf_raddr = AccAddrW'(vt_acc_sel.tile * SpatialBeats + bank_row * GroupsPerEdge);
    end else if (tv_busy_q) begin
      if (tv_line_words != 0) begin
        line_idx = tile_dim_t'(tv_word_idx_q / tv_line_words);
        line_word_idx = tile_dim_t'(tv_word_idx_q % tv_line_words);
      end
      bank_row = tv_req_q.op_ope.tss.is_row ? GroupIdxW'((tv_req_q.op_ope.tss.index + line_idx) / CE) :
          GroupIdxW'((line_word_idx * tv_elems_per_word) / CE);
      acc_vrf_raddr = AccAddrW'(tv_acc_sel.tile * SpatialBeats + bank_row * GroupsPerEdge);
    end

    if (tile_rvalid_i && tile_rready_o) begin
      bank_row = GroupIdxW'(tile_r_req_i.row / CE);
      acc_vlsu_raddr = AccAddrW'(tile_r_acc_sel.tile * SpatialBeats + bank_row * GroupsPerEdge);
    end else if (tile_wvalid_i && tile_wready_o) begin
      bank_row = GroupIdxW'(tile_w_req_i.row / CE);
      acc_vlsu_raddr = AccAddrW'(tile_w_acc_sel.tile * SpatialBeats + bank_row * GroupsPerEdge);
    end
  end : acc_read_addr_proc

  always_comb begin : acc_access_proc
    tile_dim_t idx;
    tile_dim_t row_i;
    tile_dim_t col_i;

    logic [GroupIdxW-1:0] bank_row;
    tile_sum_t elem_idx;
    vlen_t flat_elem_idx;
    tile_dim_t line_idx;
    tile_dim_t line_word_idx;

    acc_fma_wdata = $bits(acc_fma_wdata)'(0);
    acc_fma_wen = $bits(acc_fma_wen)'(0);
    acc_fma_waddr = AccAddrW'(0);
    acc_vrf_wdata = $bits(acc_vrf_wdata)'(0);
    acc_vrf_wen = $bits(acc_vrf_wen)'(0);
    acc_vrf_waddr = AccAddrW'(0);
    acc_vlsu_wdata = $bits(acc_vlsu_wdata)'(0);
    acc_vlsu_wen = $bits(acc_vlsu_wen)'(0);
    acc_vlsu_waddr = AccAddrW'(0);
    acc_flush = 1'b0;
    tile_rdata_o = tile_row_t'(0);

    idx = tile_dim_t'(0);
    row_i = tile_dim_t'(0);
    col_i = tile_dim_t'(0);
    bank_row = GroupIdxW'(0);
    elem_idx = tile_sum_t'(0);
    flat_elem_idx = vlen_t'(0);
    line_idx = tile_dim_t'(0);
    line_word_idx = tile_dim_t'(0);

    if (clean_req_valid && clean_req_ready && (spatz_req_clean.op_ope.is_discard)) begin
      acc_flush = 1'b1;
    end

    if (tv_acc_wen) begin
      if (tv_line_words != 0) begin
        line_idx = tile_dim_t'(tv_word_idx_q / tv_line_words);
        line_word_idx = tile_dim_t'(tv_word_idx_q % tv_line_words);
      end
      idx        = tv_req_q.op_ope.tss.index + line_idx;
      bank_row   = tv_req_q.op_ope.tss.is_row ? GroupIdxW'(idx / CE) : GroupIdxW'((line_word_idx * tv_elems_per_word) / CE);

      acc_vrf_waddr = acc_vrf_raddr;
      for (int row = 0; row < CE; row++) begin
        for (int col = 0; col < CE; col++) begin
          for (int beat = 0; beat < GroupsPerEdge; beat++) begin
            acc_vrf_wdata[row][col][beat] = acc_vrf_rdata[row][col][beat];
            for (int byte_idx = 0; byte_idx < AccElemBytes; byte_idx++) begin
              if (acc_zero_q[tv_acc_sel.tile][bank_row * GroupsPerEdge + beat][byte_idx])
                acc_vrf_wdata[row][col][beat][byte_idx*8 +: 8] = 8'd0;
            end
          end
          acc_vrf_wen[row][col] = tv_acc_sel.byte_enable;
        end
      end

      for (int lane = 0; lane < VtMaxElemsPerWord; lane++) begin
        elem_idx = line_word_idx * tv_elems_per_word + lane;
        flat_elem_idx = vlen_t'(line_idx) * tv_line_elems + elem_idx;
        if ((flat_elem_idx >= tv_req_q.vstart) && (flat_elem_idx < tv_active_elems) &&
            (elem_idx < tv_line_elems) && (lane < tv_elems_per_word)) begin
          if (tv_req_q.op_ope.tss.is_row) begin
            row_i = idx;
            col_i = elem_idx[$clog2(TE)-1:0];
          end else begin
            row_i = elem_idx[$clog2(TE)-1:0];
            col_i = idx;
          end
          acc_vrf_wen[row_i % CE][col_i % CE] = tv_acc_sel.byte_enable;
          if (tv_req_q.op_ope.tew == EW_8)
            acc_vrf_wdata[row_i % CE][col_i % CE][col_i / CE][tv_acc_sel.byte_offset*8 +: 8] = tv_vrf_data[lane*8 +: 8];
          else if (tv_req_q.op_ope.tew == EW_16)
            acc_vrf_wdata[row_i % CE][col_i % CE][col_i / CE][tv_acc_sel.byte_offset*8 +: 16] = tv_vrf_data[lane*16 +: 16];
          else
            acc_vrf_wdata[row_i % CE][col_i % CE][col_i / CE] = tv_vrf_data[lane*AccElemWidth +: AccElemWidth];
        end
      end
    end

    if (tile_wvalid_i && tile_wready_o) begin
      bank_row = GroupIdxW'(tile_w_req_i.row / CE);
      acc_vlsu_waddr = acc_vlsu_raddr;
      for (int row = 0; row < CE; row++) begin
        for (int col = 0; col < CE; col++) begin
          for (int beat = 0; beat < GroupsPerEdge; beat++) begin
            acc_vlsu_wdata[row][col][beat] = acc_vlsu_rdata[row][col][beat];
            for (int byte_idx = 0; byte_idx < AccElemBytes; byte_idx++) begin
              if (acc_zero_q[tile_w_acc_sel.tile][bank_row * GroupsPerEdge + beat][byte_idx])
                acc_vlsu_wdata[row][col][beat][byte_idx*8 +: 8] = 8'd0;
            end
          end
          acc_vlsu_wen[row][col] = tile_w_acc_sel.byte_enable;
        end
      end
      for (int i = 0; i < TE; i++) begin
        col_i = tile_dim_t'(i);
        if (i < tile_w_req_i.elems) begin
          acc_vlsu_wen[tile_w_req_i.row % CE][col_i % CE] = tile_w_acc_sel.byte_enable;
          if (tile_w_req_i.tew == EW_8)
            acc_vlsu_wdata[tile_w_req_i.row % CE][col_i % CE][col_i / CE][tile_w_acc_sel.byte_offset*8 +: 8] =
                tile_w_req_i.data[i*8 +: 8];
          else if (tile_w_req_i.tew == EW_16)
            acc_vlsu_wdata[tile_w_req_i.row % CE][col_i % CE][col_i / CE][tile_w_acc_sel.byte_offset*8 +: 16] =
                tile_w_req_i.data[i*16 +: 16];
          else
            acc_vlsu_wdata[tile_w_req_i.row % CE][col_i % CE][col_i / CE] = tile_w_req_i.data[i*AccElemWidth +: AccElemWidth];
        end
      end
    end

    if (fma_pipe_result_fire && fma_pipe_write_acc) begin
      acc_fma_waddr = AccAddrW'(fma_acc_sel.tile * SpatialBeats + fma_pipe_tag_q[NumPipeRegs-1].beat);
      for (int row = 0; row < CE; row++) begin
        for (int col = 0; col < CE; col++) begin
          acc_fma_wen[row][col] = fma_pipe_tag_q[NumPipeRegs-1].lane_active[row][col] ?
              fma_acc_sel.byte_enable : AccElemBytes'(0);
          if (fma_pipe_tag_q[NumPipeRegs-1].tew == EW_8)
            acc_fma_wdata[row][col][fma_acc_sel.byte_offset*8 +: 8] = fma_result[row][col][7:0];
          else if (fma_pipe_tag_q[NumPipeRegs-1].tew == EW_16)
            acc_fma_wdata[row][col][fma_acc_sel.byte_offset*8 +: 16] = fma_result[row][col][15:0];
          else
            acc_fma_wdata[row][col] = fma_result[row][col];
        end
      end
    end

    if (tile_rvalid_i && tile_rready_o) begin
      bank_row = GroupIdxW'(tile_r_req_i.row / CE);
      for (int i = 0; i < TE; i++) begin
        logic [$clog2(CE)-1:0] bank_col;
        logic [GroupIdxW-1:0] group_col;

        bank_col = $clog2(CE)'(i % CE);
        group_col = GroupIdxW'(i / CE);
        if (i < tile_r_req_i.elems) begin
          logic [AccElemWidth-1:0] acc_word;

          acc_word = acc_vlsu_rdata[tile_r_req_i.row % CE][bank_col][group_col];
          for (int unsigned byte_idx = 0; byte_idx < AccElemBytes; byte_idx++) begin
            if (acc_zero_q[tile_r_acc_sel.tile][bank_row * GroupsPerEdge + group_col][byte_idx])
              acc_word[byte_idx*8 +: 8] = 8'd0;
          end
          unique case (tile_r_req_i.tew)
            EW_8: tile_rdata_o[i*8 +: 8] = acc_word[tile_r_acc_sel.byte_offset*8 +: 8];
            EW_16: tile_rdata_o[i*16 +: 16] = acc_word[tile_r_acc_sel.byte_offset*8 +: 16];
            default: tile_rdata_o[i*AccElemWidth +: AccElemWidth] = acc_word;
          endcase
        end
      end
    end
  end

  always_comb begin : acc_zero_update
    logic [GroupIdxW-1:0] bank_row;
    tile_dim_t line_idx;
    tile_dim_t line_word_idx;

    acc_zero_d = acc_zero_q;
    bank_row = GroupIdxW'(0);
    line_idx = tile_dim_t'(0);
    line_word_idx = tile_dim_t'(0);

    if (clean_req_valid && clean_req_ready && (spatz_req_clean.op_ope.is_discard))
      acc_zero_d = $bits(acc_zero_d)'(0);
    else begin
      if (clean_req_valid && clean_req_ready && (spatz_req_clean.op_ope.is_zero_tile)) begin
        for (int beat = 0; beat < SpatialBeats; beat++) begin
          acc_zero_d[clean_acc_sel.tile][beat] |= clean_acc_sel.byte_enable;
        end
      end
      if (fma_pipe_result_fire && fma_pipe_write_acc)
        acc_zero_d[fma_acc_sel.tile][fma_pipe_tag_q[NumPipeRegs-1].beat] &= ~fma_acc_sel.byte_enable;
      if (tv_acc_wen) begin
        if (tv_line_words != 0) begin
          line_idx = tile_dim_t'(tv_word_idx_q / tv_line_words);
          line_word_idx = tile_dim_t'(tv_word_idx_q % tv_line_words);
        end
        bank_row = tv_req_q.op_ope.tss.is_row ? GroupIdxW'((tv_req_q.op_ope.tss.index + line_idx) / CE) :
            GroupIdxW'((line_word_idx * tv_elems_per_word) / CE);
        for (int beat = 0; beat < GroupsPerEdge; beat++)
          acc_zero_d[tv_acc_sel.tile][bank_row * GroupsPerEdge + beat] &= ~tv_acc_sel.byte_enable;
      end
      if (tile_wvalid_i && tile_wready_o) begin
        bank_row = GroupIdxW'(tile_w_req_i.row / CE);
        for (int beat = 0; beat < GroupsPerEdge; beat++)
          acc_zero_d[tile_w_acc_sel.tile][bank_row * GroupsPerEdge + beat] &= ~tile_w_acc_sel.byte_enable;
      end
    end
  end : acc_zero_update

  for (genvar row = 0; row < CE; row++) begin : gen_acc_row
    for (genvar col = 0; col < CE; col++) begin : gen_acc_col
      opope_accumulator #(
        .DATA_WIDTH(AccElemWidth         ),
        .DEPTH     (AccDepth    ),
        .RD_PORTS  (GroupsPerEdge),
        .WR_PORTS  (GroupsPerEdge)
      ) i_accumulator ( 
        .clk_i             (clk_i              ),
        .rst_ni            (rst_ni             ),
        .flush_i           (acc_flush          ),
        .iteration_change_i(1'b0               ),
        .fma_wdata_i       (acc_fma_wdata[row][col] ),
        .fma_wen_i         (acc_fma_wen[row][col]   ),
        .fma_waddr_i       (acc_fma_waddr            ),
        .fma_raddr_i       (acc_fma_raddr            ),
        .fma_rdata_o       (acc_fma_rdata[row][col]  ),
        .vrf_wdata_i       (acc_vrf_wdata[row][col] ),
        .vrf_wen_i         (acc_vrf_wen[row][col]   ),
        .vrf_waddr_i       (acc_vrf_waddr            ),
        .vrf_raddr_i       (acc_vrf_raddr            ),
        .vrf_rdata_o       (acc_vrf_rdata[row][col] ),
        .vlsu_wdata_i      (acc_vlsu_wdata[row][col]),
        .vlsu_wen_i        (acc_vlsu_wen[row][col]  ),
        .vlsu_waddr_i      (acc_vlsu_waddr           ),
        .vlsu_raddr_i      (acc_vlsu_raddr           ),
        .vlsu_rdata_o      (acc_vlsu_rdata[row][col])
      );
    end : gen_acc_col
  end : gen_acc_row

  ////////////////////////////////////////////////////////////////////////
  ////     FMA Datapath: Operands, Addend and Compute Array           ////
  ////////////////////////////////////////////////////////////////////////
  
  // FMA operand datapath and clock.
  logic fma_clk;
  logic [CE-1:0][AccElemWidth-1:0] fma_x_operand, fma_w_operand;
  fpnew_pkg::fp_format_e mac_input_format;

  tc_clk_gating i_fma_clk_gate ( 
    .clk_i     (clk_i       ),
    .en_i      (mac_fire || (|fma_pipe_valid)),
    .test_en_i (1'b0          ),
    .clk_o     (fma_clk     )
  );

  always_comb begin : proc_mac_input_format
    mac_input_format = fpnew_pkg::FP32;
    unique case (mac_op_exec.ew)
      EW_16: mac_input_format = mac_op_exec.is_alt ? fpnew_pkg::FP16ALT : fpnew_pkg::FP16;
      EW_8:  mac_input_format = mac_op_exec.is_alt ? fpnew_pkg::FP8ALT  : fpnew_pkg::FP8;
      default: mac_input_format = fpnew_pkg::FP32;
    endcase
  end

  always_comb begin : fma_operand_select
    operand_width_t operand_bits;
    vrf_bit_offset_t x_bit_offset;
    vrf_bit_offset_t w_bit_offset;

    fma_x_operand = $bits(fma_x_operand)'(0);
    fma_w_operand = $bits(fma_w_operand)'(0);

    unique case (mac_op_exec.ew)
      EW_8:    operand_bits = 6'd8;
      EW_16:   operand_bits = 6'd16;
      default: operand_bits = 6'd32;
    endcase
    x_bit_offset = vrf_bit_offset_t'((mac_group_row * CE * operand_bits) % VRFWordWidth);
    w_bit_offset = vrf_bit_offset_t'((mac_group_col * CE * operand_bits) % VRFWordWidth);
    unique case (mac_op_exec.ew)
      EW_16: begin
        for (int unsigned el = 0; el < CE; el++) begin
          fma_x_operand[el][15:0] = vrf_rdata_i[0][x_bit_offset + el*16 +: 16];
          fma_w_operand[el][15:0] = vrf_rdata_i[1][w_bit_offset + el*16 +: 16];
        end
      end
      EW_8: begin
        for (int unsigned el = 0; el < CE; el++) begin
          fma_x_operand[el][7:0] = vrf_rdata_i[0][x_bit_offset + el*8 +: 8];
          fma_w_operand[el][7:0] = vrf_rdata_i[1][w_bit_offset + el*8 +: 8];
        end
      end
      default: begin
        for (int unsigned el = 0; el < CE; el++) begin
          fma_x_operand[el] = vrf_rdata_i[0][x_bit_offset + el*AccElemWidth +: AccElemWidth];
          fma_w_operand[el] = vrf_rdata_i[1][w_bit_offset + el*AccElemWidth +: AccElemWidth];
        end
      end
    endcase
  end : fma_operand_select

  always_comb begin : fma_addend_proc
    logic [AccElemBytes-1:0] zero_bytes;

    fma_addend = $bits(fma_addend)'(0);
    zero_bytes = acc_zero_q[mac_acc_sel.tile][mac_op_beat];

    for (int unsigned row = 0; row < CE; row++) begin
      for (int unsigned col = 0; col < CE; col++) begin
        if (fma_pipe_result_forward) begin
          fma_addend[row][col] = fma_result[row][col];
        end else if ((zero_bytes & mac_acc_sel.byte_enable) == mac_acc_sel.byte_enable) begin
          fma_addend[row][col] = $bits(fma_addend[row][col])'(0);
        end else if (mac_op_exec.tew == EW_8) begin
          fma_addend[row][col][7:0] = acc_fma_rdata[row][col][mac_acc_sel.byte_offset*8 +: 8];
        end else if (mac_op_exec.tew == EW_16) begin
          fma_addend[row][col][15:0] = acc_fma_rdata[row][col][mac_acc_sel.byte_offset*8 +: 16];
        end else begin
          fma_addend[row][col] = acc_fma_rdata[row][col];
        end
      end
    end
  end

  for (genvar row = 0; row < CE; row++) begin : gen_fma_row
    for (genvar col = 0; col < CE; col++) begin : gen_fma_col

      logic        fma16_valid, fma32_valid;
      logic [15:0] fma16_result;
      logic [31:0] fma32_result;

      assign fma_result_valid[row][col] = fma16_valid || fma32_valid;
      assign fma_result[row][col] = fma16_valid ? {{(AccElemWidth-16){1'b0}}, fma16_result} : fma32_result;

      opope_fma #( 
        .FpFormat    (fpnew_pkg::FP16       ),
        .NumPipeRegs (NumPipeRegs           ),
        .PipeConfig  (fpnew_pkg::DISTRIBUTED),
        .Stallable   (1'b1                  )
      ) i_fma16 ( 
        .clk_i          (fma_clk                                              ),
        .rst_ni         (rst_ni                                               ),
        .operands_i     ({fma_w_operand[col][15:0], fma_x_operand[row][15:0]} ),
        .addend_i       (fma_addend[row][col][15:0]                           ),
        .input_format_i (fpnew_pkg::FP16                                      ),
        .valid_i        (mac_fire && mac_lane_active[row][col] && (mac_op_exec.tew == EW_16)),
        .ready_o        (fma16_ready[row][col]                                ),
        .reg_enable_i   (fma_pipe_advance                                     ),
        .result_valid_o (fma16_valid                                          ),
        .result_ready_i (fma_pipe_result_ready                                ),
        .result_o       (fma16_result                                         )
      );

      opope_fma #( 
        .FpFormat    (fpnew_pkg::FP32       ),
        .NumPipeRegs (NumPipeRegs           ),
        .PipeConfig  (fpnew_pkg::DISTRIBUTED),
        .Stallable   (1'b1                  )
      ) i_fma32 ( 
        .clk_i          (fma_clk                                              ),
        .rst_ni         (rst_ni                                               ),
        .operands_i     ({fma_w_operand[col], fma_x_operand[row]}             ),
        .addend_i       (fma_addend[row][col]                                 ),
        .input_format_i (mac_input_format                                     ),
        .valid_i        (mac_fire && mac_lane_active[row][col] && (mac_op_exec.tew == EW_32)),
        .ready_o        (fma32_ready[row][col]                                ),
        .reg_enable_i   (fma_pipe_advance                                     ),
        .result_valid_o (fma32_valid                                          ),
        .result_ready_i (fma_pipe_result_ready                                ),
        .result_o       (fma32_result                                         )
      );

    end : gen_fma_col
  end : gen_fma_row

  ////////////////////////////////////////////////////////////////////////
  ////                             Commit                             ////
  ////////////////////////////////////////////////////////////////////////

  // Response buffers retain IDs under backpressure; the output arbiter selects one completion.
  always_comb begin : mac_commit_rsp_proc
    mac_commit_rsp.id = mac_op_exec.id;
  end : mac_commit_rsp_proc

  spill_register #(.T(ope_rsp_t)) i_mac_commit ( 
    .clk_i  (clk_i             ),
    .rst_ni (rst_ni            ),
    .data_i (mac_commit_rsp    ),
    .valid_i(mac_commit_valid  ),
    .ready_o(mac_commit_ready  ),
    .data_o (mac_done_rsp      ),
    .valid_o(mac_done_valid    ),
    .ready_i(mac_done_ready    )
  );

  always_comb begin : vt_commit_rsp_proc
    vt_commit_rsp.id = vt_req_q.id;
  end : vt_commit_rsp_proc

  spill_register #(.T(ope_rsp_t)) i_vt_commit ( 
    .clk_i  (clk_i            ),
    .rst_ni (rst_ni           ),
    .data_i (vt_commit_rsp    ),
    .valid_i(vt_commit_valid  ),
    .ready_o(vt_commit_ready  ),
    .data_o (vt_done_rsp   ),
    .valid_o(vt_done_valid    ),
    .ready_i(vt_done_ready    )
  );

  always_comb begin : tv_commit_rsp_proc
    tv_commit_rsp.id = tv_req_q.id;
  end : tv_commit_rsp_proc

  spill_register #(.T(ope_rsp_t)) i_tv_commit ( 
    .clk_i  (clk_i            ),
    .rst_ni (rst_ni           ),
    .data_i (tv_commit_rsp    ),
    .valid_i(tv_commit_valid  ),
    .ready_o(tv_commit_ready  ),
    .data_o (tv_done_rsp      ),
    .valid_o(tv_done_valid    ),
    .ready_i(tv_done_ready    )
  );

  always_comb begin : clean_commit_rsp_proc
    clean_commit_rsp.id = spatz_req_clean.id;
  end : clean_commit_rsp_proc

  spill_register #(.T(ope_rsp_t)) i_clean_commit ( 
    .clk_i  (clk_i                                    ),
    .rst_ni (rst_ni                                   ),
    .data_i (clean_commit_rsp                        ),
    .valid_i(clean_req_valid && clean_acc_ready     ),
    .ready_o(clean_commit_ready                      ),
    .data_o (clean_done_rsp                          ),
    .valid_o(clean_done_valid                        ),
    .ready_i(clean_done_ready                        )
  );

  // Completion response arbitration.
  ope_rsp_t [3:0] arb_inp_data ;
  logic     [3:0] arb_inp_valid;
  logic     [3:0] arb_inp_ready;

  typedef enum logic [1:0] {
    RSP_MAC,
    RSP_VT,
    RSP_TV,
    RSP_SIMPLE
  } ope_rsp_sel_e;

  assign arb_inp_data[RSP_MAC]    = mac_done_rsp;
  assign arb_inp_data[RSP_VT]     = vt_done_rsp;
  assign arb_inp_data[RSP_TV]     = tv_done_rsp;
  assign arb_inp_data[RSP_SIMPLE] = clean_done_rsp;

  assign arb_inp_valid[RSP_MAC]    = mac_done_valid;
  assign arb_inp_valid[RSP_VT]     = vt_done_valid;
  assign arb_inp_valid[RSP_TV]     = tv_done_valid;
  assign arb_inp_valid[RSP_SIMPLE] = clean_done_valid;

  assign mac_done_ready    = arb_inp_ready[RSP_MAC];
  assign vt_done_ready     = arb_inp_ready[RSP_VT];
  assign tv_done_ready     = arb_inp_ready[RSP_TV];
  assign clean_done_ready = arb_inp_ready[RSP_SIMPLE];

  stream_arbiter #( 
    .DATA_T  (ope_rsp_t),
    .N_INP   (4        ),
    .ARBITER ("rr"     )
  ) i_rsp_arb ( 
    .clk_i       (clk_i           ),
    .rst_ni      (rst_ni          ),
    .inp_data_i  (arb_inp_data    ),
    .inp_valid_i (arb_inp_valid   ),
    .inp_ready_o (arb_inp_ready   ),
    .oup_data_o  (ope_rsp_o       ),
    .oup_valid_o (ope_rsp_valid_o ),
    .oup_ready_i (ope_rsp_ready_i )
  );

  ////////////////////////////////////////////////////////////////////////
  ////              Move and Accumulator State Registers              ////
  ////////////////////////////////////////////////////////////////////////

  always_ff @(posedge clk_i or negedge rst_ni) begin : seq_block
    if (!rst_ni) begin
      vt_busy_q         <= 1'b0;
      vt_req_q <= spatz_req_t'(0);
      vt_word_idx_q <= $bits(vt_word_idx_q)'(0);
      tv_busy_q         <= 1'b0;
      tv_req_q <= spatz_req_t'(0);
      tv_word_idx_q <= $bits(tv_word_idx_q)'(0);
      tv_data_q <= vrf_data_t'(0);
      tv_data_latched_q <= 1'b0;
      acc_zero_q <= $bits(acc_zero_q)'(0);
    end else begin
      acc_zero_q          <= acc_zero_d;

      if (vt_req_valid && vt_req_ready) begin
        vt_req_q        <= spatz_req_vt;
      end
      vt_busy_q <= vt_busy_d;
      vt_word_idx_q <= vt_word_idx_d;

      if (tv_req_valid && tv_req_ready) begin
        tv_req_q        <= spatz_req_tv;
      end
      tv_busy_q <= tv_busy_d;
      tv_word_idx_q <= tv_word_idx_d;

      // Latch TV VRF data on first rvalid; hold until acc write succeeds.
      // Needed because fma_result_valid (higher priority) may block TV for
      // one cycle -- data must be retained for the retry.
      if (!tv_busy_q || tv_acc_wen) begin
        tv_data_latched_q <= 1'b0;
      end else if ((tv_busy_q && vrf_rvalid_i[2]) && !tv_data_latched_q) begin
        tv_data_q         <= vrf_rdata_i[2];
        tv_data_latched_q <= 1'b1;
      end
    end
  end : seq_block

  ////////////////////////////////////////////////////////////////////////
  ////                        Parameter Checks                        ////
  ////////////////////////////////////////////////////////////////////////

  if ((TE == 0) || ((TE & (TE-1)) != 0)) $error("[OPE] TE must be power of 2.");
  if ((CE == 0) || ((CE & (CE-1)) != 0)) $error("[OPE] CE must be power of 2.");

endmodule : spatz_ope
