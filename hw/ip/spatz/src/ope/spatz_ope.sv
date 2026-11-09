// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Pei-Yu Lin, ETH Zurich
//
// Outer Product Engine wrapper

// The accumulator is banked by CE x CE compute lanes.  A spatial beat selects
// one CE x CE region of a logical tile; a resident result holds the newest
// pipeline output until it can be forwarded to the next MAC or written back.

module spatz_ope
  import spatz_pkg::*;
  import rvv_pkg::*;
  import fpnew_pkg::*;
#(
  parameter int unsigned CE = OPEComputeEdge,
  parameter int unsigned TE = TileEdge,
  parameter int unsigned TEWB = AccElemBytes,
  parameter bit EnableFMA16 = 1'b0,
  parameter bit EnableFMA32 = 1'b0
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

  // interface with VRF
  output vrf_addr_t        vrf_waddr_o        ,
  output vrf_data_t        vrf_wdata_o        ,
  output logic             vrf_we_o           ,
  output vrf_be_t          vrf_wbe_o          ,
  input  logic             vrf_wvalid_i       ,

  output vrf_addr_t  [2:0] vrf_raddr_o        ,
  output logic       [2:0] vrf_re_o           ,
  input  logic       [2:0] vrf_rgnt_i         ,
  input  vrf_data_t  [2:0] vrf_rdata_i        ,
  input  logic       [2:0] vrf_rvalid_i       ,
  output spatz_id_t  [3:0] vrf_id_o           ,

  // interface with VLSU
  input  logic             tile_wvalid_i      ,
  input  tile_w_req_t      tile_w_req_i       ,
  output logic             tile_wready_o      ,
  input  logic             tile_rvalid_i      ,
  input  tile_r_req_t      tile_r_req_i       ,
  output tile_word_t       tile_rdata_o       ,
  output logic             tile_rready_o
);

`include "common_cells/registers.svh"

  localparam int unsigned NumPipeRegs = 4;
  localparam int unsigned NumFpuPipeRegs = NumPipeRegs - 1;
  localparam int unsigned AccDepthPerCE = (CE > 0) ? (TE / CE) : 0;
  localparam int unsigned SpatialBeats = AccDepthPerCE * AccDepthPerCE;
  localparam int unsigned GroupIdxW = (AccDepthPerCE > 1) ? $clog2(AccDepthPerCE) : 1;
  localparam int unsigned SpatialBeatW = (SpatialBeats > 1) ? $clog2(SpatialBeats) : 1;
  localparam int unsigned ReductionIdxW = (KMAX > 1) ? $clog2(KMAX) : 1;
  localparam int unsigned NrAccumulatorTiles = NrPhysicalTile / AccElemBytes;
  localparam int unsigned AccDepth = NrAccumulatorTiles * SpatialBeats;
  localparam int unsigned AccAddrW = (AccDepth > 1) ? $clog2(AccDepth) : 1;
  localparam int unsigned AccTileIdxW = (NrAccumulatorTiles > 1) ? $clog2(NrAccumulatorTiles) : 1;
  localparam int unsigned AccByteIdxW = (AccElemBytes > 1) ? $clog2(AccElemBytes) : 1;
  localparam int unsigned AccMaskWidth = AccElemBytes / TEWB;
  // A Zvt vector-register group contains at most eight architectural vregs.
  localparam int unsigned MaxVtGroupWords = 8 * NrWordsPerVector;
  /*------------------------------------------------------------*/
  /*                  Shared Types and Signals                  */
  /*------------------------------------------------------------*/

  typedef logic [$clog2(AccDepthPerCE+1)-1:0] group_count_t;
  typedef logic [$clog2(MaxVtGroupWords+1)-1:0] vt_word_count_t;
  typedef logic [$clog2(VRFWordBWidth+1)-1:0] vrf_elem_count_t;
  localparam vrf_elem_count_t MoveElemsPerWord = vrf_elem_count_t'(VRFWordBWidth / TEWB);
  typedef logic [$clog2((TE<<1)+1)-1:0] tile_sum_t;
  typedef logic [5:0] operand_width_t;
  typedef logic [$clog2(VRFWordWidth)-1:0] vrf_bit_offset_t;
  typedef struct packed {
    vt_word_count_t words;
    vlen_t          elems;
    tile_count_t    line_elems;
  } move_shape_t;
  typedef struct packed {
    logic [AccTileIdxW-1:0]  tile;
    logic [AccByteIdxW-1:0]  byte_offset;
    logic [AccMaskWidth-1:0] mask_enable;
  } acc_sel_t;

  typedef struct packed {
    spatz_id_t     id;
    vreg_t         vs1;
    vreg_t         vs2;
    vew_e          sew;
    mt_t           tile;
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
    logic [SpatialBeatW-1:0] beat;
    logic [CE-1:0][CE-1:0]   lane_active;
    logic                    write_acc;
  } fpu_pipe_tag_t;

  // Request queues and arbitration.
  spatz_req_t spatz_req_mac ;
  logic       mac_req_valid ;
  logic       mac_req_ready ;
  logic       mac_req_eligible;
  spatz_req_t spatz_req_tv_queue, tv_pending_req_q;
  logic       tv_req_valid ;
  logic       tv_req_ready ;
  logic       tv_pending_valid_d, tv_pending_valid_q;
  spatz_req_t spatz_req_vt ;
  logic       vt_req_valid ;
  logic       vt_req_ready ;
  spatz_req_t spatz_req_clean ;
  logic       clean_req_valid ;
  logic       clean_req_ready ;
  logic mac_in_ready, tv_in_ready, vt_in_ready, clean_in_ready;
  logic mac_queue_valid, tv_queue_valid, vt_queue_valid, clean_queue_valid;
  logic input_is_tile_to_vreg, input_is_vreg_to_tile;

  // MAC scheduler and active operation.
  mac_op_slot_t mac_op_current_d, mac_op_current_q;
  mac_op_slot_t mac_op_next_d, mac_op_next_q;
  logic [SpatialBeatW-1:0] mac_beat_d, mac_beat_q;
  logic [ReductionIdxW-1:0] mac_reduction_d, mac_reduction_q;
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
  logic mac_nonfpu_ready;
  logic mac_commit_valid, mac_commit_ready;
  logic mac_idle;

  // FPU pipeline and MAC completion.
  logic [CE-1:0][CE-1:0] fma16_ready;
  logic [CE-1:0][CE-1:0] fma32_ready;
  logic [CE-1:0][CE-1:0] sdotp16_ready;
  logic                  fpu_pipe_ready;
  logic fpu_pipe_result_valid;
  logic fpu_pipe_result_fire;
  logic fpu_pipe_result_forward;
  logic fpu_pipe_result_ready;
  logic fpu_pipe_continue_ready;
  logic [NrPhysicalTile-1:0][SpatialBeats-1:0] fpu_pipe_tile_busy, fpu_pipe_tile_busy_next;
  logic [NumPipeRegs-1:0] fpu_pipe_valid, fpu_pipe_valid_next;
  fpu_pipe_tag_t [NumPipeRegs-1:0] fpu_pipe_tag_d, fpu_pipe_tag_q;
  fpu_pipe_tag_t                   fpu_pipe_tag_in;
  logic                            fpu_pipe_advance;
  logic                            fpu_pipe_drain;
  logic                            fpu_pipe_write_acc;
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
  logic        vt_finishing;
  move_shape_t vt_shape_d, vt_shape_q;

  // VRF -> tile move.
  spatz_req_t  tv_req_q                       ;
  logic        tv_busy_d    , tv_busy_q       ;
  logic        tv_commit_valid, tv_commit_ready;
  ope_rsp_t    tv_commit_rsp, tv_done_rsp      ;
  logic        tv_done_valid , tv_done_ready  ;
  logic [$clog2(MaxVtGroupWords)-1:0] tv_word_idx_d, tv_word_idx_q;
  logic        tv_finishing;
  move_shape_t tv_shape_d, tv_shape_q;
  vrf_data_t tv_data_d, tv_data_q;
  logic      tv_data_latched_d, tv_data_latched_q;
  logic      tv_vrf_outstanding_d, tv_vrf_outstanding_q;
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
  logic [CE-1:0][CE-1:0][AccElemWidth-1:0] fpu_addend;
  acc_sel_t mac_acc_sel;
  acc_sel_t fpu_acc_sel;
  acc_sel_t tv_acc_sel;
  acc_sel_t vt_acc_sel;
  acc_sel_t tile_r_acc_sel;
  acc_sel_t tile_w_acc_sel;
  acc_sel_t clean_acc_sel;
  logic [CE-1:0][CE-1:0][AccElemWidth-1:0] fpu_result;
  logic [CE-1:0][CE-1:0] fpu_result_valid;
  logic mac_use_sdotp;
  logic [CE-1:0][CE-1:0][AccElemWidth-1:0] acc_fpu_rdata, acc_fpu_wdata;
  logic [CE-1:0][CE-1:0][AccMaskWidth-1:0] acc_fpu_wen;
  logic [CE-1:0][CE-1:0][AccDepthPerCE-1:0][AccElemWidth-1:0] acc_vrf_rdata, acc_vrf_wdata;
  logic [CE-1:0][CE-1:0][AccMaskWidth-1:0] acc_vrf_wen;
  logic [CE-1:0][CE-1:0][AccDepthPerCE-1:0][AccElemWidth-1:0] acc_vlsu_rdata, acc_vlsu_wdata;
  logic [CE-1:0][CE-1:0][AccMaskWidth-1:0] acc_vlsu_wen;
  logic [AccAddrW-1:0] acc_fpu_raddr, acc_fpu_waddr;
  logic [AccAddrW-1:0] acc_vrf_raddr, acc_vrf_waddr;
  logic [AccAddrW-1:0] acc_vlsu_raddr, acc_vlsu_waddr;
  logic [NrAccumulatorTiles-1:0][SpatialBeats-1:0][AccMaskWidth-1:0] acc_zero_d, acc_zero_q;
  logic tile_read_drain_blocked, tile_write_drain_blocked;
  logic [NrPhysicalTile-1:0] tile_read_pipe_conflict, tile_write_pipe_conflict;
  logic tile_write_mac_conflict, tile_write_zero_conflict;

  spatz_req_t spatz_req_ingress;
  logic spatz_req_ingress_valid, spatz_req_ingress_ready;

  /*------------------------------------------------------------*/
  /*               Request Queues and Arbitration               */
  /*------------------------------------------------------------*/

  // A blocked queue head must neither execute nor win arbitration over an
  // older operation in another queue. Matrix grants describe older IDs only.
  assign mac_req_valid = mac_queue_valid && matrix_enable_i[spatz_req_mac.id];
  assign tv_req_valid = tv_pending_valid_q && matrix_enable_i[tv_pending_req_q.id];
  assign vt_req_valid = vt_queue_valid && matrix_enable_i[spatz_req_vt.id];
  assign clean_req_valid = clean_queue_valid && matrix_enable_i[spatz_req_clean.id];
  assign input_is_tile_to_vreg = spatz_req_ingress.op_ope.tss.tile_valid &&
                                 spatz_req_ingress.use_vd;
  assign input_is_vreg_to_tile = spatz_req_ingress.op_ope.tss.tile_valid &&
                                 spatz_req_ingress.use_vs2;

  spill_register #(.T(spatz_req_t)) i_op_ingress_queue (
    .clk_i  (clk_i                   ),
    .rst_ni (rst_ni                  ),
    .data_i (spatz_req_i             ),
    .valid_i(spatz_req_valid_i       ),
    .ready_o(spatz_req_ready_o       ),
    .data_o (spatz_req_ingress       ),
    .valid_o(spatz_req_ingress_valid ),
    .ready_i(spatz_req_ingress_ready )
  );

  stream_fifo #(
    .FALL_THROUGH(1'b0       ),
    .DEPTH       (4          ),
    .T           (spatz_req_t)
  ) i_op_mac_queue ( 
    .clk_i     (clk_i                                                   ),
    .rst_ni    (rst_ni                                                  ),
    .flush_i   (1'b0                                                    ),
    .testmode_i(1'b0                                                    ),
    .usage_o   (/* Unused */                                            ),
    .data_i    (spatz_req_ingress                                       ),
    .valid_i   (spatz_req_ingress_valid && spatz_req_ingress.op_ope.is_mac),
    .ready_o   (mac_in_ready                                             ),
    .data_o    (spatz_req_mac                                            ),
    .valid_o   (mac_queue_valid                                          ),
    .ready_i   (mac_req_ready                                            )
  );

  spill_register #(.T(spatz_req_t)) i_op_tv_queue ( 
    .clk_i  (clk_i                                                   ),
    .rst_ni (rst_ni                                                  ),
    .data_i (spatz_req_ingress                                       ),
    .valid_i(spatz_req_ingress_valid && input_is_vreg_to_tile),
    .ready_o(tv_in_ready                                                ),
    .data_o (spatz_req_tv_queue                                         ),
    .valid_o(tv_queue_valid                                             ),
    .ready_i(!tv_pending_valid_q                                        )
  );

  always_comb begin : tv_pending_state
    tv_pending_valid_d = tv_pending_valid_q;
    if (tv_pending_valid_q && tv_req_valid && tv_req_ready)
      tv_pending_valid_d = 1'b0;
    else if (!tv_pending_valid_q && tv_queue_valid) begin
      tv_pending_valid_d = 1'b1;
    end
  end

  `FF(tv_pending_valid_q, tv_pending_valid_d, 1'b0)
  `FFL(tv_pending_req_q, spatz_req_tv_queue,
       !tv_pending_valid_q && tv_queue_valid, spatz_req_t'(0))

  spill_register #(.T(spatz_req_t)) i_op_vt_queue ( 
    .clk_i  (clk_i                                                   ),
    .rst_ni (rst_ni                                                  ),
    .data_i (spatz_req_ingress                                       ),
    .valid_i(spatz_req_ingress_valid && input_is_tile_to_vreg),
    .ready_o(vt_in_ready                                                ),
    .data_o (spatz_req_vt                                               ),
    .valid_o(vt_queue_valid                                             ),
    .ready_i(vt_req_ready                                               )
  );

  spill_register #(.T(spatz_req_t)) i_op_clean_queue ( 
    .clk_i  (clk_i                                                   ),
    .rst_ni (rst_ni                                                  ),
    .data_i (spatz_req_ingress                                       ),
    .valid_i(spatz_req_ingress_valid &&
             (spatz_req_ingress.op_ope.is_zero_tile || spatz_req_ingress.op_ope.is_discard)),
    .ready_o(clean_in_ready                                                ),
    .data_o (spatz_req_clean                                               ),
    .valid_o(clean_queue_valid                                             ),
    .ready_i(clean_req_ready                                               )
  );

  // Shared Request Arbitration
  always_comb begin : req_ready_proc
    mac_req_eligible = 1'b0;
    vt_req_ready     = 1'b0;
    tv_req_ready     = 1'b0;
    clean_req_ready = 1'b0;
    // Simple commands such as VTZERO/VTDISCARD must complete before a younger MAC may start.
    if (clean_req_valid)
      clean_req_ready = clean_commit_ready && clean_acc_ready;
    else if (vt_req_valid && (!vt_busy_q || (vt_finishing && vt_commit_ready)) &&
             !resident_drain_valid_q && !mac_tile_busy[spatz_req_vt.op_ope.tss.tile_id] &&
             (!resident_valid_q || (spatz_req_vt.op_ope.tss.tile_id != resident_tile_q)))
      vt_req_ready = 1'b1;
    else if (tv_req_valid && (!tv_busy_q || (tv_finishing && tv_commit_ready)) &&
             !resident_drain_valid_q && !mac_tile_busy[tv_pending_req_q.op_ope.tss.tile_id] &&
             (!resident_valid_q || (tv_pending_req_q.op_ope.tss.tile_id != resident_tile_q)))
      tv_req_ready = 1'b1;
    else if (mac_req_valid)
      mac_req_eligible = !resident_drain_valid_q && !(tile_rvalid_i && (tile_r_req_i.idx == spatz_req_mac.mtd));
  end : req_ready_proc

  assign mac_req_ready = mac_req_eligible && (!mac_op_next_q.valid || (mac_commit_valid && mac_commit_ready));

  // Backpressure only the queue selected by the current request.  Requiring
  // all four queues to be ready would let an unrelated full queue block an
  // otherwise independent OPE operation.
  always_comb begin : selected_request_ready
    spatz_req_ingress_ready = 1'b0;
    if (spatz_req_ingress.op_ope.is_mac)
      spatz_req_ingress_ready = mac_in_ready;
    else if (input_is_vreg_to_tile)
      spatz_req_ingress_ready = tv_in_ready;
    else if (input_is_tile_to_vreg)
      spatz_req_ingress_ready = vt_in_ready;
    else if (spatz_req_ingress.op_ope.is_zero_tile || spatz_req_ingress.op_ope.is_discard)
      spatz_req_ingress_ready = clean_in_ready;
  end : selected_request_ready

  ////////////////////////////////////////////////////////////////////////
  ////                         MAC Operation                          ////
  ////////////////////////////////////////////////////////////////////////

  // The MAC scheduler only executes instructions in issue order.

  assign mac_group_row = GroupIdxW'(mac_op_beat / AccDepthPerCE);
  assign mac_group_col = GroupIdxW'(mac_op_beat % AccDepthPerCE);

  always_comb begin : mac_active_shape
    mac_active_tm = (mac_op_exec.tm < TE) ? mac_op_exec.tm : tile_count_t'(TE);
    mac_active_tn = (mac_op_exec.tn < TE) ? mac_op_exec.tn : tile_count_t'(TE);
    mac_m_groups = (mac_active_tm + CE - 1) / CE;
    mac_n_groups = (mac_active_tn + CE - 1) / CE;
    mac_next_spatial_beat = mac_op_beat;

    if (({1'b0, mac_group_col} + 1'b1) < mac_n_groups)
      mac_next_spatial_beat = mac_op_beat + 1'b1;
    else
      mac_next_spatial_beat = SpatialBeatW'(({1'b0, mac_group_row} + 1'b1) * AccDepthPerCE);

    mac_lane_active = $bits(mac_lane_active)'(0);
    for (int unsigned row = 0; row < CE; row++) begin
      for (int unsigned col = 0; col < CE; col++) begin
        mac_lane_active[row][col] = ((mac_group_row * CE + row) < mac_active_tm) 
                                  && ((mac_group_col * CE + col) < mac_active_tn);
      end
    end
  end : mac_active_shape

  always_comb begin : mac_op_scheduler
    logic result_match, result_switch, result_can_drain;
    logic tile_hazard;
    logic current_ready;

    result_match = 1'b0;
    result_switch = 1'b0;

    if (fpu_pipe_result_valid && !fpu_pipe_tag_q[NumPipeRegs-1].write_acc) begin
      // Same tile/beat: forward the pipeline result into the dependent MAC.
      result_match  = (fpu_pipe_tag_q[NumPipeRegs-1].tile == mac_op_current_q.op.tile) &&
                      (fpu_pipe_tag_q[NumPipeRegs-1].beat == mac_beat_q);
      // A result for the resident tile may switch ownership without first
      // writing the accumulator, provided no drain is already pending.
      result_switch = resident_valid_q && !resident_drain_valid_q &&
                      (fpu_pipe_tag_q[NumPipeRegs-1].tile == resident_tile_q) &&
                      (mac_op_current_q.op.tile != resident_tile_q);
    end

    // A busy tile/beat is unavailable until its exact result reaches the
    // forwarding stage.  Results for another beat cannot satisfy the hazard.
    tile_hazard = fpu_pipe_tile_busy[mac_op_current_q.op.tile][mac_beat_q] && !result_match;
    result_can_drain = fpu_pipe_result_valid && fpu_pipe_drain;

    current_ready = mac_op_current_q.valid && (mac_current_empty ||
                      (!resident_drain_valid_q && !tile_hazard &&
                        !(tile_rvalid_i && (tile_r_req_i.idx == mac_op_current_q.op.tile)) &&
                          ((!(&fpu_pipe_valid)) || result_match || result_switch || result_can_drain)));

    // Bypass is legal only with no registered MAC.  A dependent request stays
    // registered until the prior result reaches the forwarding stage.
    mac_op_bypass = !mac_op_current_q.valid && !mac_op_next_q.valid &&
                    mac_req_valid && mac_req_eligible && !mac_req_empty &&
                    !fpu_pipe_tile_busy[spatz_req_mac.mtd][SpatialBeatW'(0)];

    // Execute either the ready registered head or an independent empty-queue
    // request through the combinational bypass.
    mac_op_exec_valid = current_ready || mac_op_bypass;

  end : mac_op_scheduler

  assign mac_op_beat = mac_op_bypass ? SpatialBeatW'(0) : mac_beat_q;
  assign mac_reduction_idx = mac_op_bypass ? ReductionIdxW'(0) : mac_reduction_q;

  always_comb begin : mac_op_select
    // Keep selection independent of valid to avoid adding a gate to the MAC
    // critical path; downstream enables are qualified by mac_op_exec_valid.
    mac_op_exec = mac_op_current_q.op;
    if (mac_op_bypass) begin
      mac_op_exec = '{
        tile     : spatz_req_mac.mtd,
        id       : spatz_req_mac.id,
        vs1      : spatz_req_mac.vs1,
        vs2      : spatz_req_mac.vs2,
        sew      : spatz_req_mac.vtype.vsew,
        // Dimensions are snapshotted with the request. tm/tn are clamped to
        // the physical tile edge; tk is already bounded by msettk/KMAX.
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
    mac_req_empty = (spatz_req_mac.op_ope.tm == elen_t'(0)) ||
                    (spatz_req_mac.op_ope.tn == elen_t'(0)) ||
                    (spatz_req_mac.op_ope.tk == elen_t'(0));
    mac_current_empty = (mac_op_current_q.op.tm == tile_count_t'(0)) ||
                        (mac_op_current_q.op.tn == tile_count_t'(0)) ||
                        (mac_op_current_q.op.tk == 3'd0);
    mac_op_empty = mac_op_exec_valid && !mac_op_bypass && mac_current_empty;
  end : mac_op_empty_dimension

  always_comb begin : mac_op_has_more
    mac_op_has_more_spatial = (({1'b0, mac_group_col} + 1'b1) < mac_n_groups) ||
                              (({1'b0, mac_group_row} + 1'b1) < mac_m_groups);
    mac_op_has_more_reductions = !mac_use_sdotp &&
        (({1'b0, mac_reduction_idx} + 1'b1) < mac_op_exec.tk);
    mac_op_has_more_beats = mac_op_has_more_spatial || mac_op_has_more_reductions;
  end : mac_op_has_more

  always_comb begin : mac_op_fire
    mac_nonfpu_ready  = (&vrf_rvalid_i[1:0]) && (mac_op_has_more_beats || mac_commit_ready);
    mac_step_fire = (mac_op_empty && mac_commit_ready) ||
                    (mac_op_exec_valid && !mac_op_empty && fpu_pipe_ready && mac_nonfpu_ready);
    mac_fire = mac_op_exec_valid && !mac_op_empty && fpu_pipe_ready && mac_nonfpu_ready;
  end : mac_op_fire

  assign mac_commit_valid = mac_op_empty ||
                            (mac_op_exec_valid && !mac_op_empty && !mac_op_has_more_beats
                            && fpu_pipe_ready && (&vrf_rvalid_i[1:0]));
  assign mac_idle = !mac_op_current_q.valid && !mac_op_next_q.valid && !(|fpu_pipe_valid);

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
        sew    : spatz_req_mac.vtype.vsew,
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

  /*------------------------------------------------------------*/
  /*                    Resident Tile State                     */
  /*------------------------------------------------------------*/

  assign resident_start_fire = mac_fire && !resident_valid_q && (mac_op_beat == SpatialBeatW'(0));
  // A queued MAC must be able to drain a mismatching resident result before
  // it becomes executable.  Qualifying these conditions with exec_valid made
  // tile_hazard and result_ready wait on each other when the pipeline was full.
  assign resident_switch_req = (mac_op_current_q.valid || mac_op_bypass) && resident_valid_q &&
      !resident_drain_valid_q && (mac_op_exec.tile != resident_tile_q);
  assign resident_switch_fire = resident_switch_req && mac_fire;
  assign resident_beat_mismatch = (mac_op_current_q.valid || mac_op_bypass) && resident_valid_q &&
      !resident_drain_valid_q && fpu_pipe_result_valid && (mac_op_exec.tile == resident_tile_q) &&
      (fpu_pipe_tag_q[NumPipeRegs-1].tile == resident_tile_q) && (fpu_pipe_tag_q[NumPipeRegs-1].beat != mac_op_beat);

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

    if (resident_valid_q && !resident_drain_valid_q && (|fpu_pipe_valid_next)) begin
      // VTSE is already older than any MAC sitting in the OPE queues.  Drain
      // the resident FPU state even when a younger same-tile MAC is queued;
      // that MAC remains blocked by tile_rvalid_i until the read completes.
      if (tile_rvalid_i && (tile_r_req_i.idx == resident_tile_q)) begin
        start_drain = 1'b1;
        start_writeback = 1'b1;
      end else if (!mac_op_current_d.valid && !mac_op_next_d.valid &&
                   clean_req_valid && (spatz_req_clean.op_ope.is_discard)) begin
        start_drain = 1'b1;
      end else if (!mac_op_current_d.valid && !mac_op_next_d.valid && clean_req_valid &&
                   spatz_req_clean.op_ope.is_zero_tile && (spatz_req_clean.mtd == resident_tile_q)) begin
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
                   (tv_pending_req_q.op_ope.tss.tile_id == resident_tile_q)) begin
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

    if (resident_drain_valid_q && !(|fpu_pipe_tile_busy_next[resident_drain_tile_q])) begin
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

  /*------------------------------------------------------------*/
  /*                        FPU Pipeline                        */
  /*------------------------------------------------------------*/

  assign fpu_pipe_ready   = (&fma16_ready) && (&fma32_ready) && (&sdotp16_ready);
  assign fpu_pipe_advance = (mac_fire || (|fpu_pipe_valid)) && fpu_pipe_ready;
  assign fpu_pipe_tag_in = mac_fire ?
      '{valid: 1'b1, tile: mac_op_exec.tile, beat: mac_op_beat,
        lane_active: mac_lane_active, write_acc: 1'b0} : fpu_pipe_tag_t'(0);

  always_comb begin : fpu_pipe_tag_update
    fpu_pipe_tag_d = fpu_pipe_tag_q;
    if (resident_switch_fire) begin
      for (int unsigned stage = 0; stage < NumPipeRegs; stage++)
        fpu_pipe_tag_d[stage].write_acc = 1'b1;
    end
    if (fpu_pipe_advance) begin
      fpu_pipe_tag_d = {fpu_pipe_tag_d[NumPipeRegs-2:0], fpu_pipe_tag_in};
    end
  end : fpu_pipe_tag_update

  // Occupancy is already encoded by the tag pipeline. Derive empty/full and
  // the per-tile beat map instead of storing a second copy in flip-flops.
  always_comb begin : fpu_pipe_occupancy
    fpu_pipe_tile_busy      = '0;
    fpu_pipe_tile_busy_next = '0;
    fpu_pipe_valid          = '0;
    fpu_pipe_valid_next     = '0;
    for (int unsigned stage = 0; stage < NumPipeRegs; stage++) begin
      fpu_pipe_valid[stage] = fpu_pipe_tag_q[stage].valid;
      fpu_pipe_valid_next[stage] = fpu_pipe_tag_d[stage].valid;
      if (fpu_pipe_tag_q[stage].valid) begin
        fpu_pipe_tile_busy[fpu_pipe_tag_q[stage].tile][fpu_pipe_tag_q[stage].beat] = 1'b1;
      end
      if (fpu_pipe_tag_d[stage].valid) begin
        fpu_pipe_tile_busy_next[fpu_pipe_tag_d[stage].tile][fpu_pipe_tag_d[stage].beat] = 1'b1;
      end
    end
  end : fpu_pipe_occupancy

  // Result handshake, drain and forwarding.
  assign fpu_pipe_result_valid = fpu_result_valid[0][0];

  assign fpu_pipe_drain = fpu_pipe_tag_q[NumPipeRegs-1].write_acc || (fpu_pipe_result_valid && ((resident_switch_req &&
         (fpu_pipe_tag_q[NumPipeRegs-1].tile != mac_op_exec.tile)) || resident_beat_mismatch));
  assign fpu_pipe_write_acc = fpu_pipe_drain ||
      (resident_drain_valid_q && resident_drain_writeback_q &&
       (fpu_pipe_tag_q[NumPipeRegs-1].tile == resident_drain_tile_q));

  assign fpu_pipe_continue_ready = mac_op_exec_valid && (&vrf_rvalid_i[1:0]) &&
      (fpu_pipe_tag_q[NumPipeRegs-1].tile == mac_op_exec.tile) && (fpu_pipe_tag_q[NumPipeRegs-1].beat == mac_op_beat);

  assign fpu_pipe_result_ready =
      ((resident_drain_valid_q && (fpu_pipe_tag_q[NumPipeRegs-1].tile == resident_drain_tile_q)) || fpu_pipe_drain) ?
      1'b1 : (fpu_pipe_tag_q[NumPipeRegs-1].write_acc ? mac_commit_ready : fpu_pipe_continue_ready);
  assign fpu_pipe_result_fire = fpu_pipe_result_valid && fpu_pipe_result_ready;
  assign fpu_pipe_result_forward = mac_fire && fpu_pipe_result_fire && !resident_drain_valid_q && !fpu_pipe_drain &&
      (fpu_pipe_tag_q[NumPipeRegs-1].tile == mac_op_exec.tile) && (fpu_pipe_tag_q[NumPipeRegs-1].beat == mac_op_beat) &&
      !fpu_pipe_tag_q[NumPipeRegs-1].write_acc;

  `FF(fpu_pipe_tag_q, fpu_pipe_tag_d, '0)

  ////////////////////////////////////////////////////////////////////////
  ////                  Tile -> VRF Move Operation                    ////
  ////////////////////////////////////////////////////////////////////////

  always_comb begin : vt_move_shape
    logic line_valid;
    tile_count_t line_elems;

    vt_shape_d.words = vt_word_count_t'(0);
    vt_shape_d.elems = vlen_t'(0);
    line_elems = (spatz_req_vt.op_ope.tss.is_row ? spatz_req_vt.op_ope.tn : spatz_req_vt.op_ope.tm) > TE ?
                    tile_count_t'(TE) :
                    tile_count_t'(spatz_req_vt.op_ope.tss.is_row ? spatz_req_vt.op_ope.tn : spatz_req_vt.op_ope.tm);
    vt_shape_d.line_elems = line_elems;

    // One VTMV instruction selects exactly one tile row or column.  VL and
    // vstart mask that line.  LMUL may let the line span consecutive physical
    // vregs, but must never extend the operation into another tile line.
    line_valid = spatz_req_vt.op_ope.tss.is_row ?
                 (spatz_req_vt.op_ope.tm > spatz_req_vt.op_ope.tss.index) :
                 (spatz_req_vt.op_ope.tn > spatz_req_vt.op_ope.tss.index);
    if (line_valid && (line_elems != 0) && (spatz_req_vt.vl != 0)) begin
      vt_shape_d.elems = (spatz_req_vt.vl < line_elems) ? spatz_req_vt.vl : vlen_t'(line_elems);
      vt_shape_d.words = (vt_shape_d.elems + MoveElemsPerWord - 1) / MoveElemsPerWord;
    end
  end


  assign vt_word_fire = vt_busy_q && vt_req_q.use_vd && vrf_wvalid_i;
  assign vt_finishing = vt_busy_q && ((vt_shape_q.words == 0) ||
      (vt_word_fire && (({1'b0, vt_word_idx_q} + 1'b1) >= vt_shape_q.words)));

  always_comb begin : vt_handler
    vt_busy_d        = vt_busy_q;
    vt_word_idx_d    = vt_word_idx_q;
    vt_commit_valid  = vt_finishing;

    if (vt_word_fire && !vt_finishing)
      vt_word_idx_d = vt_word_idx_q + 1'b1;

    if (vt_finishing && vt_commit_ready) begin
      // Retire the current move and accept the next one in the same cycle.
      // This keeps vrf_we_o asserted across back-to-back VTMV_V_T moves.
      vt_busy_d = vt_req_valid && vt_req_ready;
      vt_word_idx_d = $bits(vt_word_idx_d)'(0);
    end else if (!vt_busy_q && vt_req_valid && vt_req_ready) begin
      vt_busy_d = 1'b1;
      vt_word_idx_d = $bits(vt_word_idx_d)'(0);
    end
  end : vt_handler

  ////////////////////////////////////////////////////////////////////////
  ////                  VRF -> Tile Move Operation                    ////
  ////////////////////////////////////////////////////////////////////////

  always_comb begin : tv_move_shape
    logic line_valid;
    tile_count_t line_elems;

    tv_shape_d.words = vt_word_count_t'(0);
    tv_shape_d.elems = vlen_t'(0);
    line_elems = (tv_pending_req_q.op_ope.tss.is_row ? tv_pending_req_q.op_ope.tn : tv_pending_req_q.op_ope.tm) > TE ?
                    tile_count_t'(TE) :
                    tile_count_t'(tv_pending_req_q.op_ope.tss.is_row ? tv_pending_req_q.op_ope.tn : tv_pending_req_q.op_ope.tm);
    tv_shape_d.line_elems = line_elems;

    line_valid = tv_pending_req_q.op_ope.tss.is_row ?
                 (tv_pending_req_q.op_ope.tm > tv_pending_req_q.op_ope.tss.index) :
                 (tv_pending_req_q.op_ope.tn > tv_pending_req_q.op_ope.tss.index);
    if (line_valid && (line_elems != 0) && (tv_pending_req_q.vl != 0)) begin
      tv_shape_d.elems = (tv_pending_req_q.vl < line_elems) ? tv_pending_req_q.vl : vlen_t'(line_elems);
      tv_shape_d.words = (tv_shape_d.elems + MoveElemsPerWord - 1) / MoveElemsPerWord;
    end
  end


  // TV owns the third OPE read port, independently of MAC vs2/vs1.
  assign tv_vrf_avail = tv_data_latched_q || (tv_busy_q && vrf_rvalid_i[2]);
  assign tv_vrf_data  = tv_data_latched_q ? tv_data_q : vrf_rdata_i[2];

  assign tv_acc_wen = tv_busy_q && tv_vrf_avail && !vt_busy_q;
  assign tv_finishing = tv_busy_q && ((tv_shape_q.words == 0) ||
      (tv_acc_wen && (({1'b0, tv_word_idx_q} + 1'b1) >= tv_shape_q.words)));

  always_comb begin : tv_handler
    tv_busy_d        = tv_busy_q;
    tv_word_idx_d    = tv_word_idx_q;
    tv_commit_valid  = tv_finishing;

    if (tv_acc_wen && !tv_finishing)
      tv_word_idx_d = tv_word_idx_q + 1'b1;

    if (tv_finishing && tv_commit_ready) begin
      // Retire the current move and accept the next one in the same cycle.
      // This keeps the VRF read path active across back-to-back VTMV_T_V moves.
      tv_busy_d = tv_req_valid && tv_req_ready;
      tv_word_idx_d = $bits(tv_word_idx_d)'(0);
    end else if (!tv_busy_q && tv_req_valid && tv_req_ready) begin
      tv_busy_d = 1'b1;
      tv_word_idx_d = $bits(tv_word_idx_d)'(0);
    end
  end : tv_handler

  ////////////////////////////////////////////////////////////////////////
  ////                    Tile Clean Operation                        ////
  ////////////////////////////////////////////////////////////////////////
  
  always_comb begin : mac_tile_busy_proc
  // Isn't there any similar registers also like a mask? I want to reduce redundant registers.
    mac_tile_busy = $bits(mac_tile_busy)'(0);

    if (mac_op_current_q.valid)
      mac_tile_busy[mac_op_current_q.op.tile] = 1'b1;
    if (mac_op_next_q.valid)
      mac_tile_busy[mac_op_next_q.op.tile] = 1'b1;
    for (int unsigned tile = 0; tile < NrPhysicalTile; tile++) begin
      if (|fpu_pipe_tile_busy[tile])
        mac_tile_busy[tile] = 1'b1;
    end
  end : mac_tile_busy_proc

  always_comb begin : clean_acc_ready_proc
    clean_acc_ready = 1'b0;
    // Discard is global state invalidation and therefore takes priority if a
    // malformed request ever asserts both command bits.  It waits for every
    // producer/consumer of accumulator state to become idle.
    if (spatz_req_clean.op_ope.is_discard)
      clean_acc_ready = !resident_valid_q && !resident_drain_valid_q &&
                        !mac_op_current_q.valid && !mac_op_next_q.valid &&
                        !(|fpu_pipe_valid) && !vt_busy_q && !tv_busy_q &&
                        !tile_rvalid_i && !tile_wvalid_i;
    // Zero only touches its selected tile and may proceed while unrelated
    // tiles are active, provided no reader/writer owns that tile.
    else if (spatz_req_clean.op_ope.is_zero_tile)
      clean_acc_ready = !resident_drain_valid_q
                        && !mac_tile_busy[spatz_req_clean.mtd]
                        && !(tile_rvalid_i && (tile_r_req_i.idx == spatz_req_clean.mtd))
                        && (!resident_valid_q || (spatz_req_clean.mtd != resident_tile_q));
  end : clean_acc_ready_proc

  /*------------------------------------------------------------*/
  /*                      VRF Port Access                       */
  /*------------------------------------------------------------*/

  // Read ports [0:1] feed MAC vs2/vs1; read port [2] feeds TV.
  // ID [3] accompanies the independent VT write port.
  always_comb begin : sb_ids
    // The bypass can only execute when there is no registered MAC. Select
    // the scoreboard ID from that registered occupancy bit instead of the
    // combinational bypass grant, which also depends on tile drain state.
    vrf_id_o[0] = mac_op_current_q.valid ? mac_op_current_q.op.id : spatz_req_mac.id;
    vrf_id_o[1] = mac_op_current_q.valid ? mac_op_current_q.op.id : spatz_req_mac.id;
    vrf_id_o[2] = tv_req_q.id;
    vrf_id_o[3] = vt_req_q.id;
  end

  /*------------------------------------------------------------*/
  /*                   VRF -> Tile Read Ports                   */
  /*------------------------------------------------------------*/

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
      unique case (mac_op_exec.sew)
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
      if (mac_use_sdotp) operand_bits = 6'(TEWB << 3);
      reduction_word_offset = mac_use_sdotp ? '0 :
                        vrf_addr_t'(mac_reduction_idx) * row_stride_regs * NrWordsPerVector;
      row_word_offset = vrf_addr_t'((mac_group_row * CE * operand_bits) / VRFWordWidth);
      col_word_offset = vrf_addr_t'((mac_group_col * CE * operand_bits) / VRFWordWidth);
      vrf_re_o[1:0] = 2'b11;
      vrf_raddr_o[0] = vrf_addr_t'(mac_op_exec.vs2) * NrWordsPerVector + reduction_word_offset + row_word_offset;
      vrf_raddr_o[1] = vrf_addr_t'(mac_op_exec.vs1) * NrWordsPerVector + reduction_word_offset + col_word_offset;
    end

    if (tv_busy_q && !tv_data_latched_q && !tv_vrf_outstanding_q) begin
      vrf_re_o[2]    = 1'b1;
      vrf_raddr_o[2] = (vrf_addr_t'(tv_req_q.vs2) << $clog2(NrWordsPerVector)) + vrf_addr_t'(tv_word_idx_q);
    end
  end

  /*------------------------------------------------------------*/
  /*                   Tile -> VRF Write Port                   */
  /*------------------------------------------------------------*/

  always_comb begin : vrf_wr_proc
    logic [VRFWordBWidth-1:0][AccElemWidth-1:0] lane_data;
    logic [VRFWordBWidth-1:0] lane_valid;
    tile_dim_t idx;
    tile_dim_t row_i;
    tile_dim_t col_i;
    vlen_t active_len;
    tile_sum_t elem_idx;
    vlen_t flat_elem_idx;
    tile_dim_t line_word_idx;
    vlen_t vstart_i;

    vrf_waddr_o = vrf_addr_t'(0);
    vrf_we_o    = 1'b0;
    vrf_wbe_o = vrf_be_t'(0);
    vrf_wdata_o = vrf_data_t'(0);
    lane_data = '0;
    lane_valid = '0;

    row_i = tile_dim_t'(0);
    col_i = tile_dim_t'(0);
    elem_idx = tile_sum_t'(0);
    flat_elem_idx = vlen_t'(0);
    line_word_idx = tile_dim_t'(vt_word_idx_q);

    active_len = vt_shape_q.elems;
    vstart_i   = vt_req_q.vstart;

    if (vt_busy_q) begin
      // A flat word offset crosses physical-register boundaries when LMUL>1;
      // the controller has already checked that vd is an aligned group base.
      vrf_waddr_o =(vrf_addr_t'(vt_req_q.vd) << $clog2(NrWordsPerVector)) + vrf_addr_t'(vt_word_idx_q);
      vrf_we_o = vt_req_q.use_vd;

      for (int lane = 0; lane < VRFWordBWidth; lane++) begin
        elem_idx = line_word_idx * MoveElemsPerWord + lane;
        flat_elem_idx = elem_idx;
        if ((flat_elem_idx >= vstart_i) && (flat_elem_idx < active_len) && (elem_idx < vt_shape_q.line_elems) &&
            (lane < MoveElemsPerWord)) begin
          logic [AccElemWidth-1:0] acc_word;

          if (vt_req_q.op_ope.tss.is_row) begin
            row_i = vt_req_q.op_ope.tss.index;
            col_i = elem_idx[$clog2(TE)-1:0];
          end else begin
            row_i = elem_idx[$clog2(TE)-1:0];
            col_i = vt_req_q.op_ope.tss.index;
          end

          acc_word = acc_vrf_rdata[row_i % CE][col_i % CE][col_i / CE];
          for (int unsigned mask_idx = 0; mask_idx < AccMaskWidth; mask_idx++) begin
            if (acc_zero_q[vt_acc_sel.tile][(row_i / CE) * AccDepthPerCE + (col_i / CE)][mask_idx])
              acc_word[mask_idx*TEWB*8 +: TEWB*8] = '0;
          end
          lane_data[lane] = acc_word;
          lane_valid[lane] = 1'b1;
        end
      end
    end
    if (TEWB == 1) begin
        for (int lane = 0; lane < VRFWordWidth / 8; lane++) begin
          if (lane_valid[lane]) begin
            vrf_wdata_o[lane*8 +: 8] = lane_data[lane][vt_acc_sel.byte_offset*8 +: 8];
            vrf_wbe_o[lane] = 1'b1;
          end
        end
    end else if (TEWB == 2) begin
        for (int lane = 0; lane < VRFWordWidth / 16; lane++) begin
          if (lane_valid[lane]) begin
            vrf_wdata_o[lane*16 +: 16] = lane_data[lane][vt_acc_sel.byte_offset*8 +: 16];
            vrf_wbe_o[lane*2 +: 2] = 2'b11;
          end
        end
    end else begin
        for (int lane = 0; lane < VRFWordWidth / AccElemWidth; lane++) begin
          if (lane_valid[lane]) begin
            vrf_wdata_o[lane*AccElemWidth +: AccElemWidth] = lane_data[lane];
            vrf_wbe_o[lane*AccElemBytes +: AccElemBytes] = '1;
          end
        end
    end
  end

  /*------------------------------------------------------------*/
  /*     Shared Accumulator Access and Tile Slice Selection     */
  /*------------------------------------------------------------*/

  // Accumulator slice selection. TEWB statically removes unsupported narrow
  // masks from the accumulator write ports and lazy-zero metadata.
  always_comb begin : acc_select_proc
    mac_acc_sel.tile = AccTileIdxW'(mac_op_exec.tile / AccElemBytes);
    mac_acc_sel.byte_offset = AccByteIdxW'(mac_op_exec.tile % AccElemBytes);
    mac_acc_sel.mask_enable = AccMaskWidth'(1) <<
                              ((mac_op_exec.tile % AccElemBytes) / TEWB);

    fpu_acc_sel.tile = AccTileIdxW'(fpu_pipe_tag_q[NumPipeRegs-1].tile / AccElemBytes);
    fpu_acc_sel.byte_offset = AccByteIdxW'(fpu_pipe_tag_q[NumPipeRegs-1].tile % AccElemBytes);
    fpu_acc_sel.mask_enable = AccMaskWidth'(1) <<
                              ((fpu_pipe_tag_q[NumPipeRegs-1].tile % AccElemBytes) / TEWB);

    tv_acc_sel.tile = AccTileIdxW'(tv_req_q.op_ope.tss.tile_id / AccElemBytes);
    tv_acc_sel.byte_offset = AccByteIdxW'(tv_req_q.op_ope.tss.tile_id % AccElemBytes);
    tv_acc_sel.mask_enable = AccMaskWidth'(1) <<
                             ((tv_req_q.op_ope.tss.tile_id % AccElemBytes) / TEWB);

    vt_acc_sel.tile = AccTileIdxW'(vt_req_q.op_ope.tss.tile_id / AccElemBytes);
    vt_acc_sel.byte_offset = AccByteIdxW'(vt_req_q.op_ope.tss.tile_id % AccElemBytes);
    vt_acc_sel.mask_enable = AccMaskWidth'(1) <<
                             ((vt_req_q.op_ope.tss.tile_id % AccElemBytes) / TEWB);

    tile_r_acc_sel.tile = AccTileIdxW'(tile_r_req_i.idx / AccElemBytes);
    tile_r_acc_sel.byte_offset = AccByteIdxW'(tile_r_req_i.idx % AccElemBytes);
    tile_r_acc_sel.mask_enable = AccMaskWidth'(1) <<
                                 ((tile_r_req_i.idx % AccElemBytes) / TEWB);

    tile_w_acc_sel.tile = AccTileIdxW'(tile_w_req_i.idx / AccElemBytes);
    tile_w_acc_sel.byte_offset = AccByteIdxW'(tile_w_req_i.idx % AccElemBytes);
    tile_w_acc_sel.mask_enable = AccMaskWidth'(1) <<
                                 ((tile_w_req_i.idx % AccElemBytes) / TEWB);

    clean_acc_sel.tile = AccTileIdxW'(spatz_req_clean.mtd / AccElemBytes);
    clean_acc_sel.byte_offset = AccByteIdxW'(spatz_req_clean.mtd % AccElemBytes);
    clean_acc_sel.mask_enable = AccMaskWidth'(1) <<
                                ((spatz_req_clean.mtd % AccElemBytes) / TEWB);
  end : acc_select_proc

  // Compare IDs in parallel with per-tile beat reduction, avoiding a wide beat-vector mux before reduction.
  for (genvar tile = 0; tile < NrPhysicalTile; tile++) begin : gen_tile_access_conflict
    assign tile_read_pipe_conflict[tile] = (tile_r_req_i.idx == mt_t'(tile)) && (|fpu_pipe_tile_busy[tile]);
    assign tile_write_pipe_conflict[tile] = (tile_w_req_i.idx == mt_t'(tile)) && (|fpu_pipe_tile_busy[tile]);
  end

  // The resident FPU result uses its own accumulator write port.  A VLSU
  // transfer of another tile can therefore proceed while that result drains.
  // Transfers of the draining tile must wait until its result is committed.
  assign tile_read_drain_blocked = resident_drain_valid_q &&
      (tile_r_req_i.idx == resident_drain_tile_q);
  assign tile_write_drain_blocked = resident_drain_valid_q &&
      (tile_w_req_i.idx == resident_drain_tile_q);
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
  assign tile_rready_o = !tile_read_drain_blocked && !(|tile_read_pipe_conflict) &&
      !(resident_valid_q && (resident_tile_q == tile_r_req_i.idx)) &&
      !(tv_busy_q && tv_vrf_avail && (tv_req_q.op_ope.tss.tile_id == tile_r_req_i.idx));

  assign tile_wready_o = !tile_write_drain_blocked && !tile_rvalid_i && !(|tile_write_pipe_conflict) &&
      !tile_write_mac_conflict && !tile_write_zero_conflict &&
      !(resident_valid_q && (resident_tile_q == tile_w_req_i.idx)) &&
      !(vt_busy_q && (vt_req_q.op_ope.tss.tile_id == tile_w_req_i.idx)) &&
      !(tv_busy_q && tv_vrf_avail && (tv_req_q.op_ope.tss.tile_id == tile_w_req_i.idx));

  // FMA, VRF moves and VLSU tile transfers have independent accumulator addresses.
  always_comb begin : acc_read_addr_proc
    logic [GroupIdxW-1:0] bank_row;
    tile_dim_t line_word_idx;

    acc_fpu_raddr = AccAddrW'(mac_acc_sel.tile * SpatialBeats + mac_op_beat);
    acc_vrf_raddr = AccAddrW'(0);
    acc_vlsu_raddr = AccAddrW'(0);
    bank_row = GroupIdxW'(0);
    line_word_idx = tile_dim_t'(0);

    if (vt_busy_q) begin
      line_word_idx = tile_dim_t'(vt_word_idx_q);
      bank_row = vt_req_q.op_ope.tss.is_row ? GroupIdxW'(vt_req_q.op_ope.tss.index / CE) :
          GroupIdxW'((line_word_idx * MoveElemsPerWord) / CE);
      acc_vrf_raddr = AccAddrW'(vt_acc_sel.tile * SpatialBeats + bank_row * AccDepthPerCE);
    end else if (tv_busy_q) begin
      line_word_idx = tile_dim_t'(tv_word_idx_q);
      bank_row = tv_req_q.op_ope.tss.is_row ? GroupIdxW'(tv_req_q.op_ope.tss.index / CE) :
          GroupIdxW'((line_word_idx * MoveElemsPerWord) / CE);
      acc_vrf_raddr = AccAddrW'(tv_acc_sel.tile * SpatialBeats + bank_row * AccDepthPerCE);
    end

    if (tile_rvalid_i && tile_rready_o) begin
      bank_row = GroupIdxW'(tile_r_req_i.row / CE);
      acc_vlsu_raddr = AccAddrW'(tile_r_acc_sel.tile * SpatialBeats + bank_row * AccDepthPerCE);
    end else if (tile_wvalid_i && tile_wready_o) begin
      bank_row = GroupIdxW'(tile_w_req_i.row / CE);
      acc_vlsu_raddr = AccAddrW'(tile_w_acc_sel.tile * SpatialBeats + bank_row * AccDepthPerCE);
    end
  end : acc_read_addr_proc

  always_comb begin : acc_access_proc
    tile_dim_t idx;
    tile_dim_t row_i;
    tile_dim_t col_i;

    logic [GroupIdxW-1:0] bank_row;
    tile_sum_t elem_idx;
    vlen_t flat_elem_idx;
    tile_dim_t line_word_idx;

    acc_fpu_wdata = $bits(acc_fpu_wdata)'(0);
    acc_fpu_wen = $bits(acc_fpu_wen)'(0);
    acc_fpu_waddr = AccAddrW'(0);
    acc_vrf_wdata = $bits(acc_vrf_wdata)'(0);
    acc_vrf_wen = $bits(acc_vrf_wen)'(0);
    acc_vrf_waddr = AccAddrW'(0);
    acc_vlsu_wdata = $bits(acc_vlsu_wdata)'(0);
    acc_vlsu_wen = $bits(acc_vlsu_wen)'(0);
    acc_vlsu_waddr = AccAddrW'(0);
    tile_rdata_o = tile_word_t'(0);

    idx = tile_dim_t'(0);
    row_i = tile_dim_t'(0);
    col_i = tile_dim_t'(0);
    bank_row = GroupIdxW'(0);
    elem_idx = tile_sum_t'(0);
    flat_elem_idx = vlen_t'(0);
    line_word_idx = tile_dim_t'(tv_word_idx_q);

    if (tv_acc_wen) begin
      idx        = tv_req_q.op_ope.tss.index;
      bank_row = tv_req_q.op_ope.tss.is_row ?
                 GroupIdxW'(idx / CE) : GroupIdxW'((line_word_idx * MoveElemsPerWord) / CE);

      acc_vrf_waddr = acc_vrf_raddr;
      for (int row = 0; row < CE; row++) begin
        for (int col = 0; col < CE; col++) begin
          for (int beat = 0; beat < AccDepthPerCE; beat++) begin
            acc_vrf_wdata[row][col][beat] = acc_vrf_rdata[row][col][beat];
            for (int mask_idx = 0; mask_idx < AccMaskWidth; mask_idx++) begin
              if (acc_zero_q[tv_acc_sel.tile][bank_row * AccDepthPerCE + beat][mask_idx])
                acc_vrf_wdata[row][col][beat][mask_idx*TEWB*8 +: TEWB*8] = '0;
            end
          end
          acc_vrf_wen[row][col] = tv_acc_sel.mask_enable;
        end
      end

      for (int lane = 0; lane < VRFWordBWidth; lane++) begin
        elem_idx = line_word_idx * MoveElemsPerWord + lane;
        flat_elem_idx = elem_idx;
        if ((flat_elem_idx >= tv_req_q.vstart) && (flat_elem_idx < tv_shape_q.elems) &&
            (elem_idx < tv_shape_q.line_elems) && (lane < MoveElemsPerWord)) begin
          if (tv_req_q.op_ope.tss.is_row) begin
            row_i = idx;
            col_i = elem_idx[$clog2(TE)-1:0];
          end else begin
            row_i = elem_idx[$clog2(TE)-1:0];
            col_i = idx;
          end
          acc_vrf_wen[row_i % CE][col_i % CE] = tv_acc_sel.mask_enable;
          if (TEWB == 1)
            acc_vrf_wdata[row_i % CE][col_i % CE][col_i / CE][tv_acc_sel.byte_offset*8 +: 8] = tv_vrf_data[lane*8 +: 8];
          else if (TEWB == 2)
            acc_vrf_wdata[row_i % CE][col_i % CE][col_i / CE][tv_acc_sel.byte_offset*8 +: 16] =
                16'(tv_vrf_data >> (lane * 16));
          else
            acc_vrf_wdata[row_i % CE][col_i % CE][col_i / CE] = AccElemWidth'(tv_vrf_data >> (lane * AccElemWidth));
        end
      end
    end

    if (tile_wvalid_i && tile_wready_o) begin
      bank_row = GroupIdxW'(tile_w_req_i.row / CE);
      acc_vlsu_waddr = acc_vlsu_raddr;
      for (int row = 0; row < CE; row++) begin
        for (int col = 0; col < CE; col++) begin
          for (int beat = 0; beat < AccDepthPerCE; beat++) begin
            acc_vlsu_wdata[row][col][beat] = acc_vlsu_rdata[row][col][beat];
            for (int mask_idx = 0; mask_idx < AccMaskWidth; mask_idx++) begin
              if (acc_zero_q[tile_w_acc_sel.tile][bank_row * AccDepthPerCE + beat][mask_idx])
                acc_vlsu_wdata[row][col][beat][mask_idx*TEWB*8 +: TEWB*8] = '0;
            end
          end
          acc_vlsu_wen[row][col] = tile_w_acc_sel.mask_enable;
        end
      end
      for (int unsigned byte_idx = 0; byte_idx < TileWordBWidth; byte_idx++) begin
        tile_word_elem_idx_t elem_idx_in_word;
        logic [2:0] elem_byte_idx;

        elem_idx_in_word = tile_word_elem_idx_t'(byte_idx / TEWB);
        elem_byte_idx = 3'(byte_idx % TEWB);
        if (elem_idx_in_word < tile_w_req_i.elems) begin
          col_i = tile_dim_t'(tile_w_req_i.col + elem_idx_in_word);
          acc_vlsu_wen[tile_w_req_i.row % CE][col_i % CE] = tile_w_acc_sel.mask_enable;
          acc_vlsu_wdata[tile_w_req_i.row % CE][col_i % CE][col_i / CE]
              [(tile_w_acc_sel.byte_offset+elem_byte_idx)*8 +: 8] = tile_w_req_i.data[byte_idx*8 +: 8];
        end
      end
    end

    if (fpu_pipe_result_fire && fpu_pipe_write_acc) begin
      acc_fpu_waddr = AccAddrW'(fpu_acc_sel.tile * SpatialBeats + fpu_pipe_tag_q[NumPipeRegs-1].beat);
      for (int row = 0; row < CE; row++) begin
        for (int col = 0; col < CE; col++) begin
          acc_fpu_wen[row][col] = fpu_pipe_tag_q[NumPipeRegs-1].lane_active[row][col] ?
              fpu_acc_sel.mask_enable : AccMaskWidth'(0);
          if (TEWB == 1)
            acc_fpu_wdata[row][col][fpu_acc_sel.byte_offset*8 +: 8] = fpu_result[row][col][7:0];
          else if (TEWB == 2)
            acc_fpu_wdata[row][col][fpu_acc_sel.byte_offset*8 +: 16] = fpu_result[row][col][15:0];
          else
            acc_fpu_wdata[row][col] = fpu_result[row][col];
        end
      end
    end

    if (tile_rvalid_i && tile_rready_o) begin
      bank_row = GroupIdxW'(tile_r_req_i.row / CE);
      for (int unsigned byte_idx = 0; byte_idx < TileWordBWidth; byte_idx++) begin
        tile_word_elem_idx_t elem_idx_in_word;
        logic [2:0] elem_byte_idx;

        elem_idx_in_word = tile_word_elem_idx_t'(byte_idx / TEWB);
        elem_byte_idx = 3'(byte_idx % TEWB);
        if (elem_idx_in_word < tile_r_req_i.elems) begin
          logic [$clog2(CE)-1:0] bank_col;
          logic [GroupIdxW-1:0] group_col;
          logic [AccElemWidth-1:0] acc_word;

          col_i = tile_dim_t'(tile_r_req_i.col + elem_idx_in_word);
          bank_col = $clog2(CE)'(col_i % CE);
          group_col = GroupIdxW'(col_i / CE);
          acc_word = acc_vlsu_rdata[tile_r_req_i.row % CE][bank_col][group_col];
          for (int unsigned mask_idx = 0; mask_idx < AccMaskWidth; mask_idx++) begin
            if (acc_zero_q[tile_r_acc_sel.tile][bank_row * AccDepthPerCE + group_col][mask_idx])
              acc_word[mask_idx*TEWB*8 +: TEWB*8] = '0;
          end
          tile_rdata_o[byte_idx*8 +: 8] =
              acc_word[(tile_r_acc_sel.byte_offset+elem_byte_idx)*8 +: 8];
        end
      end
    end
  end

  always_comb begin : acc_zero_update
    logic [GroupIdxW-1:0] bank_row;
    tile_dim_t line_word_idx;

    acc_zero_d = acc_zero_q;
    bank_row = GroupIdxW'(0);
    line_word_idx = tile_dim_t'(0);

    // Discard invalidates the complete physical accumulator without clearing its
    // data flops.  A set metadata bit makes every corresponding byte read as zero.
    if (clean_req_valid && clean_req_ready && (spatz_req_clean.op_ope.is_discard))
      acc_zero_d = '1;
    else begin
      if (clean_req_valid && clean_req_ready && (spatz_req_clean.op_ope.is_zero_tile)) begin
        for (int beat = 0; beat < SpatialBeats; beat++) begin
          acc_zero_d[clean_acc_sel.tile][beat] |= clean_acc_sel.mask_enable;
        end
      end
      if (fpu_pipe_result_fire && fpu_pipe_write_acc)
        acc_zero_d[fpu_acc_sel.tile][fpu_pipe_tag_q[NumPipeRegs-1].beat] &= ~fpu_acc_sel.mask_enable;
      if (tv_acc_wen) begin
        line_word_idx = tile_dim_t'(tv_word_idx_q);
        bank_row = tv_req_q.op_ope.tss.is_row ? GroupIdxW'(tv_req_q.op_ope.tss.index / CE) :
            GroupIdxW'((line_word_idx * MoveElemsPerWord) / CE);
        for (int beat = 0; beat < AccDepthPerCE; beat++)
          acc_zero_d[tv_acc_sel.tile][bank_row * AccDepthPerCE + beat] &= ~tv_acc_sel.mask_enable;
      end
      if (tile_wvalid_i && tile_wready_o) begin
        bank_row = GroupIdxW'(tile_w_req_i.row / CE);
        for (int beat = 0; beat < AccDepthPerCE; beat++)
          acc_zero_d[tile_w_acc_sel.tile][bank_row * AccDepthPerCE + beat] &= ~tile_w_acc_sel.mask_enable;
      end
    end
  end : acc_zero_update

  for (genvar row = 0; row < CE; row++) begin : gen_acc_row
    for (genvar col = 0; col < CE; col++) begin : gen_acc_col
      opope_accumulator #(
        .DATA_WIDTH        (AccElemWidth ),
        .MASK_GRANULE_BYTES(TEWB         ),
        .DEPTH             (AccDepth     ),
        .RD_PORTS          (AccDepthPerCE),
        .WR_PORTS          (AccDepthPerCE)
      ) i_accumulator ( 
        .clk_i             (clk_i              ),
        .fpu_wdata_i       (acc_fpu_wdata[row][col] ),
        .fpu_wen_i         (acc_fpu_wen[row][col]   ),
        .fpu_waddr_i       (acc_fpu_waddr            ),
        .fpu_raddr_i       (acc_fpu_raddr            ),
        .fpu_rdata_o       (acc_fpu_rdata[row][col]  ),
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

  /*------------------------------------------------------------*/
  /*      FMA Datapath: Operands, Addend and Compute Array      */
  /*------------------------------------------------------------*/
  
  // FMA operand datapath and clock.
  logic fpu_clk;

  logic [CE-1:0][AccElemWidth-1:0] fpu_x_operand, fpu_w_operand;
  logic [CE-1:0][AccElemWidth-1:0] fpu_x_operand_q, fpu_w_operand_q;
  logic [CE-1:0][CE-1:0][AccElemWidth-1:0] fpu_addend_q;
  logic [CE-1:0][CE-1:0] fpu_lane_active_q;
  logic fpu_input_valid_q, fpu_input_sdotp_q;
  vew_e fpu_input_sew_q;
  // Widening FP16 operations consume a packed K pair from each e32 word.
  assign mac_use_sdotp = ((8 << int'(mac_op_exec.sew)) != (TEWB << 3));

  tc_clk_gating i_fpu_clk_gate (
    .clk_i     (clk_i       ),
    .en_i      (mac_fire || (|fpu_pipe_valid)),
    .test_en_i (1'b0          ),
    .clk_o     (fpu_clk     )
  );

  always_comb begin : fpu_operand_select
    operand_width_t operand_bits;
    vrf_bit_offset_t x_bit_offset;
    vrf_bit_offset_t w_bit_offset;

    fpu_x_operand = $bits(fpu_x_operand)'(0);
    fpu_w_operand = $bits(fpu_w_operand)'(0);

    unique case (mac_op_exec.sew)
      EW_8:    operand_bits = 6'd8;
      EW_16:   operand_bits = 6'd16;
      default: operand_bits = 6'd32;
    endcase
    if (mac_use_sdotp) operand_bits = 6'(TEWB << 3);
    x_bit_offset = vrf_bit_offset_t'((mac_group_row * CE * operand_bits) % VRFWordWidth);
    w_bit_offset = vrf_bit_offset_t'((mac_group_col * CE * operand_bits) % VRFWordWidth);
    if (mac_use_sdotp) begin
      for (int unsigned el = 0; el < CE; el++) begin
        fpu_x_operand[el] = vrf_rdata_i[0][x_bit_offset + el*32 +: 32];
        fpu_w_operand[el] = vrf_rdata_i[1][w_bit_offset + el*32 +: 32];
        if (mac_op_exec.tk < 2) begin
          fpu_x_operand[el][31:16] = '0;
          fpu_w_operand[el][31:16] = '0;
        end
      end
    end else unique case (mac_op_exec.sew)
      EW_16: begin
        for (int unsigned el = 0; el < CE; el++) begin
          fpu_x_operand[el][15:0] = vrf_rdata_i[0][x_bit_offset + el*16 +: 16];
          fpu_w_operand[el][15:0] = vrf_rdata_i[1][w_bit_offset + el*16 +: 16];
        end
      end
      EW_8: begin
        for (int unsigned el = 0; el < CE; el++) begin
          fpu_x_operand[el][7:0] = vrf_rdata_i[0][x_bit_offset + el*8 +: 8];
          fpu_w_operand[el][7:0] = vrf_rdata_i[1][w_bit_offset + el*8 +: 8];
        end
      end
      default: begin
        for (int unsigned el = 0; el < CE; el++) begin
          fpu_x_operand[el] = vrf_rdata_i[0][x_bit_offset + el*AccElemWidth +: AccElemWidth];
          fpu_w_operand[el] = vrf_rdata_i[1][w_bit_offset + el*AccElemWidth +: AccElemWidth];
        end
      end
    endcase
  end : fpu_operand_select

  always_comb begin : fpu_addend_proc
    logic [AccMaskWidth-1:0] zero_mask;

    fpu_addend = $bits(fpu_addend)'(0);
    zero_mask = acc_zero_q[mac_acc_sel.tile][mac_op_beat];

    for (int unsigned row = 0; row < CE; row++) begin
      for (int unsigned col = 0; col < CE; col++) begin
        if (fpu_pipe_result_forward) begin
          fpu_addend[row][col] = fpu_result[row][col];
        end else if ((zero_mask & mac_acc_sel.mask_enable) == mac_acc_sel.mask_enable) begin
          fpu_addend[row][col] = $bits(fpu_addend[row][col])'(0);
        end else if (TEWB == 1) begin
          fpu_addend[row][col][7:0] = acc_fpu_rdata[row][col][mac_acc_sel.byte_offset*8 +: 8];
        end else if (TEWB == 2) begin
          fpu_addend[row][col][15:0] = acc_fpu_rdata[row][col][mac_acc_sel.byte_offset*8 +: 16];
        end else begin
          fpu_addend[row][col] = acc_fpu_rdata[row][col];
        end
      end
    end
  end

  // One stage at the A/B/C interface; three stages remain inside FPnew.
  always_ff @(posedge fpu_clk or negedge rst_ni) begin : fpu_input_register
    if (!rst_ni) begin
      fpu_x_operand_q   <= '0;
      fpu_w_operand_q   <= '0;
      fpu_addend_q      <= '0;
      fpu_lane_active_q <= '0;
      fpu_input_valid_q <= 1'b0;
      fpu_input_sdotp_q <= 1'b0;
      fpu_input_sew_q   <= EW_16;
    end else if (fpu_pipe_advance) begin
      fpu_x_operand_q   <= fpu_x_operand;
      fpu_w_operand_q   <= fpu_w_operand;
      fpu_addend_q      <= fpu_addend;
      fpu_lane_active_q <= mac_lane_active;
      fpu_input_valid_q <= mac_fire;
      fpu_input_sdotp_q <= mac_use_sdotp;
      fpu_input_sew_q   <= mac_op_exec.sew;
    end
  end

  for (genvar row = 0; row < CE; row++) begin : gen_fpu_row
    for (genvar col = 0; col < CE; col++) begin : gen_fpu_col

      logic        fma16_valid, fma32_valid;
      logic        sdotp16_valid;
      logic [15:0] fma16_result;
      logic [31:0] fma32_result;
      logic [31:0] sdotp16_result;
      logic [31:0] sdotp16_x_operand, sdotp16_w_operand;

      assign sdotp16_x_operand = fpu_x_operand_q[row];
      assign sdotp16_w_operand = fpu_w_operand_q[col];

      assign fpu_result_valid[row][col] =
          fma16_valid || fma32_valid || sdotp16_valid;

      always_comb begin : result_select
        fpu_result[row][col] = fma32_result;
        if (fma16_valid)
          fpu_result[row][col] = {{(AccElemWidth-16){1'b0}}, fma16_result};
        else if (sdotp16_valid)
          fpu_result[row][col] = sdotp16_result;
      end

      if (EnableFMA16) begin : gen_fma16
        opope_fma #(
          .FpFormat    (fpnew_pkg::FP16       ),
          .NumPipeRegs (NumFpuPipeRegs        ),
          .PipeConfig  (fpnew_pkg::DISTRIBUTED),
          .Stallable   (1'b1                  )
        ) i_fma16 (
          .clk_i          (fpu_clk                                              ),
          .rst_ni         (rst_ni                                               ),
          .operands_i     ({fpu_w_operand_q[col][15:0], fpu_x_operand_q[row][15:0]} ),
          .addend_i       (fpu_addend_q[row][col][15:0]                         ),
          .valid_i        (fpu_input_valid_q && fpu_lane_active_q[row][col] &&
                           !fpu_input_sdotp_q && (TEWB == 2) &&
                           (fpu_input_sew_q == EW_16)),
          .ready_o        (fma16_ready[row][col]                                ),
          .reg_enable_i   (fpu_pipe_advance                                     ),
          .result_valid_o (fma16_valid                                          ),
          .result_ready_i (fpu_pipe_result_ready                                ),
          .result_o       (fma16_result                                         )
        );
      end else begin : gen_no_fma16
        assign fma16_ready[row][col] = 1'b1;
        assign fma16_valid           = 1'b0;
        assign fma16_result          = '0;
      end

      if (EnableFMA32) begin : gen_fma32
      opope_fma #(
        .FpFormat    (fpnew_pkg::FP32       ),
        .NumPipeRegs (NumFpuPipeRegs        ),
        .PipeConfig  (fpnew_pkg::DISTRIBUTED),
        .Stallable   (1'b1                  )
      ) i_fma32 ( 
        .clk_i          (fpu_clk                                              ),
        .rst_ni         (rst_ni                                               ),
        .operands_i     ({fpu_w_operand_q[col], fpu_x_operand_q[row]}         ),
        .addend_i       (fpu_addend_q[row][col]                               ),
        .valid_i        (fpu_input_valid_q && fpu_lane_active_q[row][col] &&
                         !fpu_input_sdotp_q && (TEWB == 4) &&
                         (fpu_input_sew_q == EW_32)),
        .ready_o        (fma32_ready[row][col]                                ),
        .reg_enable_i   (fpu_pipe_advance                                     ),
        .result_valid_o (fma32_valid                                          ),
        .result_ready_i (fpu_pipe_result_ready                                ),
        .result_o       (fma32_result                                         )
      );
      end else begin : gen_no_fma32
        assign fma32_ready[row][col] = 1'b1;
        assign fma32_valid = 1'b0;
        assign fma32_result = '0;
      end

      opope_sdotp #(
        .SrcDotpFpFmtConfig (fpnew_pkg::fmt_logic_t'(6'b001000)),
        .DstDotpFpFmtConfig (fpnew_pkg::fmt_logic_t'(6'b100000)),
        .NumPipeRegs        (NumFpuPipeRegs                     ),
        .PipeConfig         (fpnew_pkg::DISTRIBUTED             ),
        .Stallable          (1'b1                               )
      ) i_sdotp16 (
        .clk_i          (fpu_clk                                                ),
        .rst_ni         (rst_ni                                                 ),
        .operand_a_i    ({{16{1'b0}}, sdotp16_x_operand[15:0]}                  ),
        .operand_b_i    (sdotp16_w_operand[15:0]                               ),
        .operand_c_i    ({{16{1'b0}}, sdotp16_x_operand[31:16]}                 ),
        .operand_d_i    (sdotp16_w_operand[31:16]                              ),
        .dst_operands_i (fpu_addend_q[row][col]                                ),
        .valid_i        (fpu_input_valid_q && fpu_lane_active_q[row][col] &&
                         fpu_input_sdotp_q && (fpu_input_sew_q == EW_16) &&
                         (TEWB == 4)),
        .ready_o        (sdotp16_ready[row][col]                                ),
        .reg_enable_i   (fpu_pipe_advance                                       ),
        .result_valid_o (sdotp16_valid                                          ),
        .result_ready_i (fpu_pipe_result_ready                                  ),
        .result_o       (sdotp16_result                                         )
      );

    end : gen_fpu_col
  end : gen_fpu_row

  /*------------------------------------------------------------*/
  /*                           Commit                           */
  /*------------------------------------------------------------*/

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

  /*------------------------------------------------------------*/
  /*            Move and Accumulator State Registers            */
  /*------------------------------------------------------------*/

  // A move's request and shape advance together only when arbitration accepts it.
  `FFL(vt_req_q, spatz_req_vt, vt_req_valid && vt_req_ready, spatz_req_t'(0))
  `FFL(vt_shape_q, vt_shape_d, vt_req_valid && vt_req_ready, move_shape_t'(0))
  `FF(vt_busy_q, vt_busy_d, 1'b0)
  `FF(vt_word_idx_q, vt_word_idx_d, '0)

  `FFL(tv_req_q, tv_pending_req_q, tv_req_valid && tv_req_ready, spatz_req_t'(0))
  `FFL(tv_shape_q, tv_shape_d, tv_req_valid && tv_req_ready, move_shape_t'(0))
  `FF(tv_busy_q, tv_busy_d, 1'b0)
  `FF(tv_word_idx_q, tv_word_idx_d, '0)

  // Hold a VRF response until the accumulator can accept its word.
  always_comb begin : tv_data_state
    tv_data_d = tv_data_q;
    tv_data_latched_d = tv_data_latched_q;
    if (!tv_busy_q || tv_acc_wen)
      tv_data_latched_d = 1'b0;
    else if (vrf_rvalid_i[2] && !tv_data_latched_q) begin
      tv_data_d = vrf_rdata_i[2];
      tv_data_latched_d = 1'b1;
    end
  end
  `FF(tv_data_q, tv_data_d, vrf_data_t'(0))
  `FF(tv_data_latched_q, tv_data_latched_d, 1'b0)

  always_comb begin : tv_vrf_outstanding_state
    tv_vrf_outstanding_d = tv_vrf_outstanding_q;
    if (!tv_busy_q || vrf_rvalid_i[2])
      tv_vrf_outstanding_d = 1'b0;
    else if (vrf_rgnt_i[2])
      tv_vrf_outstanding_d = 1'b1;
  end
  `FF(tv_vrf_outstanding_q, tv_vrf_outstanding_d, 1'b0)

  // The accumulator data array is unreset; its validity mask starts at zero.
  `FF(acc_zero_q, acc_zero_d, '1)

  /*------------------------------------------------------------*/
  /*                      Parameter Checks                      */
  /*------------------------------------------------------------*/

  if ((TE == 0) || ((TE & (TE-1)) != 0)) $error("[OPE] TE must be power of 2.");
  if ((CE == 0) || ((CE & (CE-1)) != 0)) $error("[OPE] CE must be power of 2.");
  if (TE < CE) $error("[OPE] TE must be greater than or equal to CE.");
  if ((CE != 0) && ((TE % CE) != 0)) $error("[OPE] TE must be an integer multiple of CE.");
  if ((TEWB == 0) || ((TEWB & (TEWB-1)) != 0) || (TEWB > AccElemBytes) ||
      ((AccElemBytes % TEWB) != 0))
    $error("[OPE] TEWB must be a nonzero power-of-two divisor of AccElemBytes.");
endmodule : spatz_ope
