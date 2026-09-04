// Copyright 2026
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// HPDcache-based L1 controller for the Spatz cluster ("spatz_cluster_hpd"
// variant). Structural counterpart to flamingo_spatz_cache_ctrl.sv, with
// InSitu-Cache replaced by OpenHW CV-HPDcache.
//
// Datapath:
//   core_req[NumPorts] -> par_coalescer_top -> id_buffer -> field mapper
//     -> hpdcache -> hpdcache_mem_to_axi_{read,write} -> axi_req_o/resp_i
//
// par_coalescer_top and id_buffer are reused unmodified from the InSitu
// path (insitu-cache dependency). HPDcache owns its own internal storage
// (behavioral SRAM macros) and does not share physical TCDM banks with the
// SPM, unlike InSitu-Cache -- so there are no tag/data-bank ports here.
//
// No cache_sync_* side-channel: HPDcache exposes flush/invalidate as
// ordinary op-coded requests (HPDCACHE_REQ_CMO_*) on the same channel as
// loads/stores, so the scalar core issues them directly instead of going
// through a separate side-channel FSM.

`include "hpdcache_typedef.svh"

module hpd_spatz_cache_ctrl
  import hpdcache_pkg::*;
  import reqrsp_pkg::*;
#(
  /*************************
  * Core Access Parameters *
  *************************/
  /// Number of spatz core complex ports
  parameter int unsigned NumPorts        = 10,
  /// Coalescer extend factor
  parameter int unsigned CoalExtFactor   = 1,
  /// Meta information payload from spatz/snitch
  parameter type         core_meta_t     = logic [7:0],
  /// Address width of the narrow request from spatz
  parameter int unsigned AddrWidth       = 32,
  /// Width of a word (coalescer upstream granularity)
  parameter int unsigned WordWidth       = 64,

  /**********************
  * Cache Configuration *
  **********************/
  /// Total number of cache entries (sets * ways) -- sized to match the
  /// InSitu-Cache variant's NumCacheEntry for a fair capacity comparison.
  parameter int unsigned NumCacheEntry   = 512,
  /// Word width of a cache line (bits)
  parameter int unsigned CacheLineWidth  = 512,
  /// Set associativity (HPDcache "ways")
  parameter int unsigned SetAssociativity = 4,

  /*******************************
  * MSHR Configuration (sweep)   *
  *******************************/
  /// Number of MSHR sets. Total MSHR capacity is HpdMshrSets * HpdMshrWays.
  /// Sized independently from the cache's own NumCacheEntry/SetAssociativity
  /// -- swept when comparing against InSitu-Cache's effectively-unbounded
  /// MSHR. mshrUseRegbank below auto-derives from the product, and the
  /// {way,set} pair must fit in the AXI ID field the memory-side master
  /// port actually has (see spatz_cluster_wrapper.sv.tpl's
  /// IwcAxiIdOutWidth comment for the truncation bug this constrains).
  parameter int unsigned HpdMshrSets = 4,
  /// Number of MSHR ways.
  parameter int unsigned HpdMshrWays = 4,

  /************************
  * AXI Bus Configuration *
  ************************/
  parameter type axi_req_t  = logic,
  parameter type axi_resp_t = logic,
  // Individual AXI channel types, needed by hpdcache_mem_to_axi_{read,write}
  // -- passed explicitly rather than derived via type(axi_req_o.ar) etc.,
  // since QuestaSim's vlog rejects type() on hierarchical/member references
  // (vlog-13531).
  parameter type ar_chan_t  = logic,
  parameter type r_chan_t   = logic,
  parameter type aw_chan_t  = logic,
  parameter type w_chan_t   = logic,
  parameter type b_chan_t   = logic
) (
  input  logic clk_i,
  input  logic rst_ni,

  /// spatz requests
  // Packed arrays throughout (not unpacked [NumPorts]-suffixed), matching
  // both the caller's cache_req_* signal types (spatz_cluster_hpd.sv) and
  // par_coalescer_top's own upstream_req_* ports -- QuestaSim rejects
  // implicit packed<->unpacked connections at a port boundary (vsim-3906).
  input  logic       [NumPorts-1:0]               core_req_valid_i,
  output logic       [NumPorts-1:0]               core_req_ready_o,
  input  logic       [NumPorts-1:0][AddrWidth-1:0] core_req_addr_i,
  input  core_meta_t [NumPorts-1:0]               core_req_meta_i,
  input  logic       [NumPorts-1:0]               core_req_write_i,
  input  logic       [NumPorts-1:0][WordWidth-1:0] core_req_wdata_i,
  /// AMO type per port (reqrsp_pkg::AMONone for a plain load/store). Carried
  /// through the coalescer alongside core_meta_t (see coal_info_t below) and
  /// mapped to HPDcache's own AMO op-codes -- HPDcache has built-in AMO
  /// support (hpdcache_amo.sv) that InSitu's cache path never wired in.
  input  amo_op_e    [NumPorts-1:0]               core_req_amo_i,

  /// spatz responses
  output logic       [NumPorts-1:0]               core_resp_valid_o,
  input  logic       [NumPorts-1:0]               core_resp_ready_i,
  output logic       [NumPorts-1:0]               core_resp_write_o,
  output logic       [NumPorts-1:0][WordWidth-1:0] core_resp_data_o,
  output core_meta_t [NumPorts-1:0]               core_resp_meta_o,

  /// AXI port (memory-side refill/writeback)
  output axi_req_t  axi_req_o,
  input  axi_resp_t axi_resp_i,

  /// Magic address for the software CMO trigger (see l1cache.c's
  /// l1d_init/l1d_flush under #ifdef HPDCACHE): a plain store to this
  /// address is intercepted below and turned into a CMO instead of a
  /// normal cache write, since HPDcache has no cache_sync_* side-channel
  /// the way InSitu-Cache does. Runtime value (cluster placement is
  /// runtime-configurable), computed by the caller as tcdm_start_address --
  /// the same address software's cluster_mem.start resolves to, and the
  /// exact chunk snrt_alloc_init() reserves from the L1 allocator under
  /// #ifdef HPDCACHE.
  input  logic [AddrWidth-1:0] cache_init_addr_i
);

  /////////////////////////////////////
  //  Sweep knobs (HPDcache sizing)   //
  /////////////////////////////////////
  // HpdMshrSets/HpdMshrWays are now module parameters (see above), threaded
  // from the hjson cfg's hpd_mshr_sets/hpd_mshr_ways fields via
  // spatz_cluster_wrapper.sv.tpl -> spatz_cluster_hpd.sv, so MSHR capacity
  // can be swept per-cfg without editing this file. The rest are still
  // plain localparams -- edit directly between runs; not (yet) threaded
  // through the hjson config.
  localparam int unsigned HpdWbufDirEntries = 16;
  localparam int unsigned HpdWbufDataEntries = 8;
  localparam int unsigned HpdRtabEntries    = 8;
  /// Depth of the coalescer<->HPDcache bridge's id_buffer. Bounds how many
  /// coalesced transactions can be in flight through the bridge at once;
  /// must be >= the MSHR+RTAB+WBUF capacity actually meant to be exercised,
  /// or the bridge itself becomes the bottleneck instead of HPDcache.
  localparam int unsigned HpdIdBufferEntries = 64;
  /// Width of the memory-interface transaction ID HPDcache hands to AXI.
  localparam int unsigned HpdMemIdWidth     = 8;

  /////////////////////////////////////
  //  HPDcache static configuration   //
  /////////////////////////////////////
  // Single HPDcache requester port: the coalescer already merges same-line
  // accesses across NumPorts spatz ports into one CacheLineWidth-wide
  // transaction per cycle, so HPDcache's own multi-requester arbitration
  // (which does not coalesce, just picks one port/cycle) is not needed.
  localparam int unsigned HpdReqWords = CacheLineWidth / WordWidth;

  localparam hpdcache_user_cfg_t HpdUserCfg = '{
    nRequesters:              1,
    paWidth:                  AddrWidth,
    wordWidth:                WordWidth,
    sets:                     NumCacheEntry / SetAssociativity,
    ways:                     SetAssociativity,
    clWords:                  HpdReqWords,
    reqWords:                 HpdReqWords,
    reqTransIdWidth:          $clog2(HpdIdBufferEntries),
    reqSrcIdWidth:            1,
    victimSel:                HPDCACHE_VICTIM_PLRU,
    dataWaysPerRamWord:       1,
    dataSetsPerRam:           NumCacheEntry / SetAssociativity,
    dataRamByteEnable:        1'b1,
    accessWords:              HpdReqWords,
    mshrSets:                 HpdMshrSets,
    mshrWays:                 HpdMshrWays,
    mshrWaysPerRamWord:       1,
    mshrSetsPerRam:           HpdMshrSets,
    mshrRamByteEnable:        1'b1,
    mshrUseRegbank:           (HpdMshrSets * HpdMshrWays <= 16),
    cbufEntries:              4,
    refillCoreRspFeedthrough: 1'b1,
    refillFifoDepth:          2,
    wbufDirEntries:           HpdWbufDirEntries,
    wbufDataEntries:          HpdWbufDataEntries,
    wbufWords:                HpdReqWords,
    wbufTimecntWidth:         3,
    rtabEntries:              HpdRtabEntries,
    flushEntries:             4,
    flushFifoDepth:           2,
    memAddrWidth:             AddrWidth,
    memIdWidth:               HpdMemIdWidth,
    memDataWidth:             CacheLineWidth,
    wtEn:                     1'b0,
    wbEn:                     1'b1,
    lowLatency:                1'b0,
    eccEn:                    1'b0,
    eccScrubberEn:            1'b0
  };
  localparam hpdcache_cfg_t HpdCfg = hpdcacheBuildConfig(HpdUserCfg);

  localparam type wbuf_timecnt_t        = logic [HpdCfg.u.wbufTimecntWidth-1:0];
  localparam type hpdcache_tag_t        = logic [HpdCfg.tagWidth-1:0];
  localparam type hpdcache_data_word_t  = logic [HpdCfg.u.wordWidth-1:0];
  localparam type hpdcache_data_be_t    = logic [HpdCfg.u.wordWidth/8-1:0];
  localparam type hpdcache_req_offset_t = logic [HpdCfg.reqOffsetWidth-1:0];
  localparam type hpdcache_req_data_t   = logic [HpdCfg.u.reqWords-1:0][HpdCfg.u.wordWidth-1:0];
  localparam type hpdcache_req_be_t     = logic [HpdCfg.u.reqWords-1:0][HpdCfg.u.wordWidth/8-1:0];
  localparam type hpdcache_req_sid_t    = logic [HpdCfg.u.reqSrcIdWidth-1:0];
  localparam type hpdcache_req_tid_t    = logic [HpdCfg.u.reqTransIdWidth-1:0];
  localparam type hpdcache_req_t =
      `HPDCACHE_DECL_REQ_T(hpdcache_req_offset_t, hpdcache_req_data_t, hpdcache_req_be_t,
                            hpdcache_req_sid_t, hpdcache_req_tid_t, hpdcache_tag_t);
  localparam type hpdcache_rsp_t =
      `HPDCACHE_DECL_RSP_T(hpdcache_req_data_t, hpdcache_req_sid_t, hpdcache_req_tid_t);

  localparam type hpdcache_mem_addr_t = logic [HpdCfg.u.memAddrWidth-1:0];
  localparam type hpdcache_mem_id_t   = logic [HpdCfg.u.memIdWidth-1:0];
  localparam type hpdcache_mem_data_t = logic [HpdCfg.u.memDataWidth-1:0];
  localparam type hpdcache_mem_be_t   = logic [HpdCfg.u.memDataWidth/8-1:0];
  localparam type hpdcache_nline_t    = logic [HpdCfg.nlineWidth-1:0];

  `HPDCACHE_TYPEDEF_MEM_REQ_T(hpdcache_mem_req_t, hpdcache_mem_addr_t, hpdcache_mem_id_t);
  `HPDCACHE_TYPEDEF_MEM_RESP_R_T(hpdcache_mem_resp_r_t, hpdcache_mem_id_t, hpdcache_mem_data_t);
  `HPDCACHE_TYPEDEF_MEM_REQ_W_T(hpdcache_mem_req_w_t, hpdcache_mem_data_t, hpdcache_mem_be_t);
  `HPDCACHE_TYPEDEF_MEM_RESP_W_T(hpdcache_mem_resp_w_t, hpdcache_mem_id_t);

  //////////////////////////////////////
  //        Types Definition          //
  //////////////////////////////////////

  typedef logic [AddrWidth-1:0]              addr_t;
  typedef logic [CacheLineWidth-1:0]         coalescing_data_t;
  typedef logic [CacheLineWidth/WordWidth-1:0] coalescing_mask_t;
  typedef logic [$clog2(CacheLineWidth/WordWidth)-1:0] coal_ofst_t;

  // Per-lane payload carried through the coalescer: core_meta_t unchanged,
  // plus the AMO type so it survives coalescing (par_coalescer_top's own
  // port list has no separate "amo" slot -- info_t is the only opaque,
  // per-lane channel it preserves end-to-end).
  typedef struct packed {
    amo_op_e    amo;
    core_meta_t meta;
  } coal_info_t;

  typedef struct packed {
    logic                                    id;
    logic       [NumPorts*CoalExtFactor-1:0] hitmap;
    coal_ofst_t [NumPorts*CoalExtFactor-1:0] ofsts;
    coal_info_t [NumPorts*CoalExtFactor-1:0] infos;
  } coalescing_info_t;

  // Bridge-side info payload: the write bit rides along with the stashed
  // coalescing_info_t because HPDcache's response never echoes the op
  // type, unlike InSitu-Cache's own response channel.
  typedef struct packed {
    logic             write;
    coalescing_info_t coal_info;
  } bridge_info_t;

  typedef struct packed {
    addr_t             addr;
    bridge_info_t       info;
    logic               write;
    hpdcache_req_data_t wdata;
    hpdcache_req_be_t   wstrb;
  } bridge_wide_req_t;

  typedef struct packed {
    bridge_info_t        info;
    logic                write;
    hpdcache_req_data_t  data;
  } bridge_wide_resp_t;

  typedef struct packed {
    addr_t               addr;
    hpdcache_req_tid_t    info;
    logic                 write;
    hpdcache_req_data_t   wdata;
    hpdcache_req_be_t     wstrb;
  } bridge_narrow_req_t;

  typedef struct packed {
    hpdcache_req_tid_t    info;
    logic                 write;
    hpdcache_req_data_t   data;
  } bridge_narrow_resp_t;

  //////////////////////////////////////
  //        Signal Definition         //
  //////////////////////////////////////

  logic              coalescing_req_valid, coalescing_req_ready;
  addr_t             coalescing_req_addr;
  coalescing_info_t  coalescing_req_info;
  logic              coalescing_req_write;
  coalescing_data_t  coalescing_req_wdata;
  coalescing_mask_t  coalescing_req_wmask;

  logic              coalescing_resp_valid, coalescing_resp_ready;
  coalescing_data_t  coalescing_resp_data;
  coalescing_info_t  coalescing_resp_info;
  logic              coalescing_resp_write;

  bridge_wide_req_t    bridge_slv_req;
  bridge_wide_resp_t   bridge_slv_resp;
  bridge_narrow_req_t  bridge_mst_req;
  logic                bridge_mst_req_valid, bridge_mst_req_ready;
  bridge_narrow_resp_t bridge_mst_resp;
  logic                bridge_mst_resp_valid, bridge_mst_resp_ready;

  logic          hpd_core_req_valid, hpd_core_req_ready;
  hpdcache_req_t hpd_core_req;
  logic          hpd_core_rsp_valid;
  hpdcache_rsp_t hpd_core_rsp;

  // hpdcache's core_req_*/core_rsp_* ports are unpacked arrays sized by
  // HPDcacheCfg.u.nRequesters (=1 here, single requester port -- see
  // "HPDcache static configuration" above). Some synthesis front-ends
  // (seen in the physical-design flow) reject a `'{...}` assignment-pattern
  // literal used directly as a port-connection actual (VER-721), even
  // though QuestaSim accepts it -- so these are wrapped in real 1-element
  // array signals instead of connecting `'{...}` literals at the
  // instantiation below.
  logic          hpd_core_req_valid_arr [HpdCfg.u.nRequesters];
  logic          hpd_core_req_ready_arr [HpdCfg.u.nRequesters];
  hpdcache_req_t hpd_core_req_arr       [HpdCfg.u.nRequesters];
  logic          hpd_core_req_abort_arr [HpdCfg.u.nRequesters];
  hpdcache_tag_t hpd_core_req_tag_arr   [HpdCfg.u.nRequesters];
  hpdcache_pma_t hpd_core_req_pma_arr   [HpdCfg.u.nRequesters];
  logic          hpd_core_rsp_valid_arr [HpdCfg.u.nRequesters];
  hpdcache_rsp_t hpd_core_rsp_arr       [HpdCfg.u.nRequesters];

  assign hpd_core_req_valid_arr[0] = hpd_core_req_valid;
  assign hpd_core_req_ready        = hpd_core_req_ready_arr[0];
  assign hpd_core_req_arr[0]       = hpd_core_req;
  assign hpd_core_req_abort_arr[0] = 1'b0;
  assign hpd_core_req_tag_arr[0]   = '0;
  assign hpd_core_req_pma_arr[0]   = '0;
  assign hpd_core_rsp_valid        = hpd_core_rsp_valid_arr[0];
  assign hpd_core_rsp              = hpd_core_rsp_arr[0];

  // Response-channel elastic buffer: HPDcache's core_rsp_valid_o has no
  // ready input (push-only). It cannot overflow this buffer because
  // total outstanding transactions are already capped by id_buffer's
  // HpdIdBufferEntries admission bound, so sizing the buffer the same
  // depth makes overflow structurally impossible.
  logic                hpd_resp_buf_full;
  bridge_narrow_resp_t hpd_resp_buf_data;

  hpdcache_mem_req_t    mem_req_read;
  logic                 mem_req_read_valid, mem_req_read_ready;
  hpdcache_mem_resp_r_t mem_resp_read;
  logic                 mem_resp_read_valid, mem_resp_read_ready;

  hpdcache_mem_req_t    mem_req_write;
  logic                 mem_req_write_valid, mem_req_write_ready;
  hpdcache_mem_req_w_t  mem_req_write_data;
  logic                 mem_req_write_data_valid, mem_req_write_data_ready;
  hpdcache_mem_resp_w_t mem_resp_write;
  logic                 mem_resp_write_valid, mem_resp_write_ready;

  /////////////////////////////////////
  //        Function Utility         //
  /////////////////////////////////////

  // hpdcache_req_be_t is a 2D packed array ([reqWords][wordWidth/8]), one
  // byte-enable sub-vector per word -- unlike InSitu's flat cache_strb_t,
  // so each word's mask bit is replicated across that word's own bytes
  // rather than indexed as a single flat per-byte vector.
  function automatic hpdcache_req_be_t mask_to_be(input coalescing_mask_t mask);
    automatic hpdcache_req_be_t be;
    for (int w = 0; w < CacheLineWidth/WordWidth; w++) begin
      be[w] = {(WordWidth/8){mask[w]}};
    end
    return be;
  endfunction

  // Resolve the transaction op-code from the coalesced info: LOAD/STORE by
  // default, or an AMO op if any active (hitmap) lane in this coalesced
  // group carries one. ASSUMPTION: at most one lane in a coalesced group is
  // ever an AMO -- AMOs are inherently single-address atomic operations, so
  // coalescing one together with unrelated concurrent same-cacheline
  // accesses from other lanes isn't expected in practice; if it happens
  // anyway, whichever AMO lane is found first (hitmap scan order) wins and
  // determines the op for the whole transaction.
  function automatic hpdcache_req_op_t resolve_op(input coalescing_info_t info,
                                                    input logic             is_write);
    automatic hpdcache_req_op_t op;
    op = is_write ? HPDCACHE_REQ_STORE : HPDCACHE_REQ_LOAD;
    for (int i = 0; i < NumPorts*CoalExtFactor; i++) begin
      if (info.hitmap[i] && (info.infos[i].amo != AMONone)) begin
        case (info.infos[i].amo)
          AMOSwap: op = HPDCACHE_REQ_AMO_SWAP;
          AMOAdd:  op = HPDCACHE_REQ_AMO_ADD;
          AMOAnd:  op = HPDCACHE_REQ_AMO_AND;
          AMOOr:   op = HPDCACHE_REQ_AMO_OR;
          AMOXor:  op = HPDCACHE_REQ_AMO_XOR;
          AMOMax:  op = HPDCACHE_REQ_AMO_MAX;
          AMOMaxu: op = HPDCACHE_REQ_AMO_MAXU;
          AMOMin:  op = HPDCACHE_REQ_AMO_MIN;
          AMOMinu: op = HPDCACHE_REQ_AMO_MINU;
          AMOLR:   op = HPDCACHE_REQ_AMO_LR;
          AMOSC:   op = HPDCACHE_REQ_AMO_SC;
          default: ;
        endcase
        break;
      end
    end
    return op;
  endfunction

  /////////////////////////////////////
  //        Instance Modules         //
  /////////////////////////////////////

  // core_meta_t + core_req_amo_i -> coal_info_t, and back, at the module
  // boundary -- the coalescer itself only ever sees the combined type.
  coal_info_t [NumPorts-1:0] core_req_coal_info, core_resp_coal_info;

  always_comb begin
    for (int unsigned i = 0; i < NumPorts; i++) begin
      core_req_coal_info[i].amo  = core_req_amo_i[i];
      core_req_coal_info[i].meta = core_req_meta_i[i];
      core_resp_meta_o[i]        = core_resp_coal_info[i].meta;
    end
  end

  // 1. Coalescer (reused unmodified from the InSitu path)
  par_coalescer_top #(
    .ReqAddrWidth       (AddrWidth),
    .NumPorts           (NumPorts),
    .ExtFactor          (CoalExtFactor),
    .info_t             (coal_info_t),
    .down_id_t          (logic),
    .UpstreamDataWidth  (WordWidth),
    .DownstreamDataWidth(CacheLineWidth)
  ) i_par_coalescer_for_spatz (
    .clk_i,
    .rst_ni,
    .id_i                   ('0),

    .upstream_req_valid_i   (core_req_valid_i),
    .upstream_req_ready_o   (core_req_ready_o),
    .upstream_req_addr_i    (core_req_addr_i),
    .upstream_req_info_i    (core_req_coal_info),
    .upstream_req_write_i   (core_req_write_i),
    .upstream_req_wdata_i   (core_req_wdata_i),

    .upstream_resp_valid_o  (core_resp_valid_o),
    .upstream_resp_ready_i  (core_resp_ready_i),
    .upstream_resp_write_o  (core_resp_write_o),
    .upstream_resp_data_o   (core_resp_data_o),
    .upstream_resp_info_o   (core_resp_coal_info),

    .downstream_req_valid_o (coalescing_req_valid),
    .downstream_req_ready_i (coalescing_req_ready),
    .downstream_req_addr_o  (coalescing_req_addr),
    .downstream_req_info_o  (coalescing_req_info),
    .downstream_req_write_o (coalescing_req_write),
    .downstream_req_wdata_o (coalescing_req_wdata),
    .downstream_req_wmask_o (coalescing_req_wmask),

    .downstream_resp_valid_i(coalescing_resp_valid),
    .downstream_resp_ready_o(coalescing_resp_ready),
    .downstream_resp_data_i (coalescing_resp_data),
    .downstream_resp_info_i (coalescing_resp_info),
    .downstream_resp_write_i(coalescing_resp_write)
  );

  // 2. Coalescer -> HPDcache field mapping (request side). coalescing_req_op
  // is resolved here (combinationally, from coalescing_req_info, which is
  // stable while coalescing_req_valid holds) rather than after id_buffer --
  // id_buffer's own request side is purely combinational (no registered
  // staging, confirmed from its source: mst_req_valid_o is driven straight
  // from slv_req_valid_i the same cycle), and it only explicitly forwards
  // .addr/.info/.write/.wstrb/.wdata -- an extra .amo field on the request
  // struct would silently NOT be forwarded to the mst side. Computing the
  // op here and consuming it directly at hpd_core_req formation (still the
  // same cycle) avoids relying on that unsupported pass-through.
  hpdcache_req_op_t coalescing_req_op;
  assign coalescing_req_op = resolve_op(coalescing_req_info, coalescing_req_write);

  always_comb begin
    bridge_slv_req.addr        = coalescing_req_addr;
    bridge_slv_req.info.write  = coalescing_req_write;
    bridge_slv_req.info.coal_info = coalescing_req_info;
    bridge_slv_req.write       = coalescing_req_write;
    bridge_slv_req.wdata       = hpdcache_req_data_t'(coalescing_req_wdata);
    bridge_slv_req.wstrb       = mask_to_be(coalescing_req_wmask);
  end

  // 3. id_buffer (reused unmodified from the InSitu path): stashes the wide
  // bridge_info_t against HPDcache's narrow tid, restores it on response.
  id_buffer #(
    .NumEntry  (HpdIdBufferEntries),
    .info_t    (bridge_info_t),
    .slv_req_t (bridge_wide_req_t),
    .slv_resp_t(bridge_wide_resp_t),
    .mst_req_t (bridge_narrow_req_t),
    .mst_resp_t(bridge_narrow_resp_t)
  ) i_id_buffer (
    .clk_i,
    .rst_ni,
    .slv_req_valid_i (coalescing_req_valid),
    .slv_req_ready_o (coalescing_req_ready),
    .slv_req_i       (bridge_slv_req),
    .slv_resp_valid_o(coalescing_resp_valid),
    .slv_resp_ready_i(coalescing_resp_ready),
    .slv_resp_o      (bridge_slv_resp),
    .mst_req_valid_o (bridge_mst_req_valid),
    .mst_req_ready_i (bridge_mst_req_ready),
    .mst_req_o       (bridge_mst_req),
    .mst_resp_valid_i(bridge_mst_resp_valid),
    .mst_resp_ready_o(bridge_mst_resp_ready),
    .mst_resp_i      (bridge_mst_resp)
  );

  assign coalescing_resp_data  = coalescing_data_t'(bridge_slv_resp.data);
  assign coalescing_resp_info  = bridge_slv_resp.info.coal_info;
  // Recovered from the stashed info payload, not id_buffer's own .write
  // pass-through field -- HPDcache's response has no op-type echo to
  // supply it from (see bridge_mst_resp.write below).
  assign coalescing_resp_write = bridge_slv_resp.info.write;

  // 4. Bridge (narrow) -> hpdcache_req_t field mapping. phys_indexed=1
  // because the coalescer already emits a fully-resolved physical
  // address, letting us skip HPDcache's 2nd-cycle abort/tag/pma dance.
  localparam hpdcache_req_size_t HpdReqSize = hpdcache_req_size_t'($clog2(CacheLineWidth/8));

  // Software CMO trigger (see l1cache.c under #ifdef HPDCACHE): a store to
  // cache_init_addr_i is intercepted here and turned into a CMO instead of
  // a normal cache write. Compared at the coalescer's cacheline granularity
  // (matching coalescing_req_addr, not the narrow per-word address) since
  // that's the granularity the whole reserved chunk is guaranteed clear of
  // real data at -- see alloc.c's HPD_CACHE_INIT_RESERVED_BYTES. The CMO
  // type is decoded from wdata (mirrors InSitu's old cache_sync_insn_i
  // encoding): 0=flush+inval all, 1=flush all, 2=inval all, 3=inval all
  // (closest available to InSitu's "full tag init").
  logic is_cache_init_req;
  hpdcache_req_op_t cache_init_op;
  assign is_cache_init_req = (coalescing_req_addr[AddrWidth-1:$clog2(CacheLineWidth/8)] ==
                               cache_init_addr_i[AddrWidth-1:$clog2(CacheLineWidth/8)]);
  always_comb begin
    case (coalescing_req_wdata[1:0])
      2'd1:    cache_init_op = HPDCACHE_REQ_CMO_FLUSH_ALL;
      2'd2,
      2'd3:    cache_init_op = HPDCACHE_REQ_CMO_INVAL_ALL;
      default: cache_init_op = HPDCACHE_REQ_CMO_FLUSH_INVAL_ALL;
    endcase
  end

  always_comb begin
    hpd_core_req.addr_offset = hpdcache_req_offset_t'(bridge_mst_req.addr);
    hpd_core_req.addr_tag    = hpdcache_tag_t'(bridge_mst_req.addr >> HpdCfg.reqOffsetWidth);
    hpd_core_req.wdata       = bridge_mst_req.wdata;
    hpd_core_req.op          = is_cache_init_req ? cache_init_op : coalescing_req_op;
    hpd_core_req.be          = bridge_mst_req.wstrb;
    hpd_core_req.size        = HpdReqSize;
    hpd_core_req.sid         = '0;
    hpd_core_req.tid         = bridge_mst_req.info;
    hpd_core_req.need_rsp    = 1'b1;
    hpd_core_req.phys_indexed = 1'b1;
    // Field-by-field (not a named assignment-pattern literal): some
    // synthesis front-ends (seen in the physical-design flow) reject
    // '{field: value, ...} applied to a struct-typed variable (VER-294),
    // even though QuestaSim accepts it -- same portability class as the
    // port-connection assignment-pattern issue above.
    hpd_core_req.pma.dspm           = 1'b0;
    hpd_core_req.pma.ispm           = 1'b0;
    hpd_core_req.pma.uncacheable    = 1'b0;
    hpd_core_req.pma.io             = 1'b0;
    hpd_core_req.pma.wr_policy_hint = HPDCACHE_WR_POLICY_AUTO;
  end

  assign hpd_core_req_valid   = bridge_mst_req_valid;
  assign bridge_mst_req_ready = hpd_core_req_ready;

  // 5. HPDcache core
  hpdcache #(
    .HPDcacheCfg           (HpdCfg),
    .wbuf_timecnt_t        (wbuf_timecnt_t),
    .hpdcache_tag_t        (hpdcache_tag_t),
    .hpdcache_data_word_t  (hpdcache_data_word_t),
    .hpdcache_data_be_t    (hpdcache_data_be_t),
    .hpdcache_req_offset_t (hpdcache_req_offset_t),
    .hpdcache_req_data_t   (hpdcache_req_data_t),
    .hpdcache_req_be_t     (hpdcache_req_be_t),
    .hpdcache_req_sid_t    (hpdcache_req_sid_t),
    .hpdcache_req_tid_t    (hpdcache_req_tid_t),
    .hpdcache_req_t        (hpdcache_req_t),
    .hpdcache_rsp_t        (hpdcache_rsp_t),
    .hpdcache_mem_addr_t   (hpdcache_mem_addr_t),
    .hpdcache_mem_id_t     (hpdcache_mem_id_t),
    .hpdcache_mem_data_t   (hpdcache_mem_data_t),
    .hpdcache_mem_be_t     (hpdcache_mem_be_t),
    .hpdcache_mem_req_t    (hpdcache_mem_req_t),
    .hpdcache_mem_req_w_t  (hpdcache_mem_req_w_t),
    .hpdcache_mem_resp_r_t (hpdcache_mem_resp_r_t),
    .hpdcache_mem_resp_w_t (hpdcache_mem_resp_w_t)
  ) i_hpdcache (
    .clk_i,
    .rst_ni,

    .wbuf_flush_i(1'b0),

    .core_req_valid_i     (hpd_core_req_valid_arr),
    .core_req_ready_o     (hpd_core_req_ready_arr),
    .core_req_i           (hpd_core_req_arr),
    .core_req_abort_i     (hpd_core_req_abort_arr),
    .core_req_tag_i       (hpd_core_req_tag_arr),
    .core_req_pma_i       (hpd_core_req_pma_arr),

    .core_rsp_valid_o     (hpd_core_rsp_valid_arr),
    .core_rsp_o           (hpd_core_rsp_arr),

    .mem_req_read_ready_i (mem_req_read_ready),
    .mem_req_read_valid_o (mem_req_read_valid),
    .mem_req_read_o       (mem_req_read),

    // No ISPM use in this variant -- separate storage, no hyper-SPM
    // equivalent (see plan discussion).
    .ispm_req_valid_o     (),
    .ispm_req_o           (),
    .ispm_req_abort_o     (),
    .ispm_req_tag_o       (),
    .ispm_req_pma_o       (),
    .ispm_rsp_valid_i     (1'b0),
    .ispm_rsp_i           ('0),

    .mem_resp_read_ready_o(mem_resp_read_ready),
    .mem_resp_read_valid_i(mem_resp_read_valid),
    .mem_resp_read_i      (mem_resp_read),

    .mem_resp_read_inval_i      (1'b0),
    .mem_resp_read_inval_nline_i('0),

    .mem_req_write_ready_i(mem_req_write_ready),
    .mem_req_write_valid_o(mem_req_write_valid),
    .mem_req_write_o      (mem_req_write),

    .mem_req_write_data_ready_i(mem_req_write_data_ready),
    .mem_req_write_data_valid_o(mem_req_write_data_valid),
    .mem_req_write_data_o      (mem_req_write_data),

    .mem_resp_write_ready_o(mem_resp_write_ready),
    .mem_resp_write_valid_i(mem_resp_write_valid),
    .mem_resp_write_i      (mem_resp_write),

    .evt_cache_write_miss_o(),
    .evt_cache_read_miss_o(),
    .evt_cache_dir_unc_err_o(),
    .evt_cache_dir_cor_err_o(),
    .evt_cache_dat_unc_err_o(),
    .evt_cache_dat_cor_err_o(),
    .evt_scrub_complete_o(),
    .evt_uncached_req_o(),
    .evt_cmo_req_o(),
    .evt_write_req_o(),
    .evt_read_req_o(),
    .evt_prefetch_req_o(),
    .evt_req_on_hold_o(),
    .evt_rtab_rollback_o(),
    .evt_stall_refill_o(),
    .evt_stall_o(),

    .wbuf_empty_o(),

    .cfg_enable_i                       (1'b1),
    .cfg_wbuf_threshold_i               (wbuf_timecnt_t'(3)),
    .cfg_wbuf_reset_timecnt_on_write_i  (1'b0),
    .cfg_wbuf_sequential_waw_i          (1'b0),
    .cfg_wbuf_inhibit_write_coalescing_i(1'b0),
    .cfg_prefetch_updt_plru_i           (1'b1),
    .cfg_error_on_cacheable_amo_i       (1'b0),
    .cfg_rtab_single_entry_i            (1'b0),
    .cfg_default_wb_i                   (1'b1),
    .cfg_scrub_enable_i                 (1'b0),
    .cfg_scrub_period_i                 ('0),
    .cfg_scrub_restart_i                (1'b0),
    .cfg_enable_dspm_i                  ('0),
    .cfg_enable_ispm_i                  ('0),
    .cfg_dspm_ways_i                    ('0)
  );

  // 6. Response-side elastic buffer (see signal declaration comment above
  // for why it structurally cannot overflow). hpd_resp_buf_full is derived
  // from the fifo's own ready_o purely for the overflow assertion below --
  // it does not gate hpd_core_rsp_valid_i (HPDcache has no way to respect
  // that backpressure), correctness instead relies on the depth proof.
  assign hpd_resp_buf_data.info  = hpd_core_rsp.tid;
  assign hpd_resp_buf_data.write = 1'b0; // unused, see coalescing_resp_write above
  assign hpd_resp_buf_data.data  = hpd_core_rsp.rdata;

  logic hpd_resp_buf_ready;
  assign hpd_resp_buf_full = ~hpd_resp_buf_ready;

  stream_fifo #(
    .DEPTH (HpdIdBufferEntries),
    .T     (bridge_narrow_resp_t)
  ) i_hpd_resp_buf (
    .clk_i,
    .rst_ni,
    .flush_i   (1'b0),
    .testmode_i(1'b0),
    .usage_o   (),
    .data_i    (hpd_resp_buf_data),
    .valid_i   (hpd_core_rsp_valid),
    .ready_o   (hpd_resp_buf_ready),
    .data_o    (bridge_mst_resp),
    .valid_o   (bridge_mst_resp_valid),
    .ready_i   (bridge_mst_resp_ready)
  );

`ifndef TARGET_SYNTHESIS
  hpd_resp_buf_no_overflow: assert property (
    @(posedge clk_i) disable iff (!rst_ni) hpd_core_rsp_valid |-> !hpd_resp_buf_full)
    else $fatal(1, "hpd_spatz_cache_ctrl: response buffer overflowed -- \
HpdIdBufferEntries no longer bounds HPDcache's outstanding transactions");
`endif

  // 7. HPDcache memory interface -> AXI
  hpdcache_mem_to_axi_read #(
    .hpdcache_mem_req_t   (hpdcache_mem_req_t),
    .hpdcache_mem_resp_r_t(hpdcache_mem_resp_r_t),
    .ar_chan_t            (ar_chan_t),
    .r_chan_t             (r_chan_t)
  ) i_hpd_mem_to_axi_read (
    .req_ready_o   (mem_req_read_ready),
    .req_valid_i   (mem_req_read_valid),
    .req_i         (mem_req_read),

    .resp_ready_i  (mem_resp_read_ready),
    .resp_valid_o  (mem_resp_read_valid),
    .resp_o        (mem_resp_read),

    .axi_ar_valid_o(axi_req_o.ar_valid),
    .axi_ar_o      (axi_req_o.ar),
    .axi_ar_ready_i(axi_resp_i.ar_ready),

    .axi_r_valid_i (axi_resp_i.r_valid),
    .axi_r_i       (axi_resp_i.r),
    .axi_r_ready_o (axi_req_o.r_ready)
  );

  hpdcache_mem_to_axi_write #(
    .hpdcache_mem_req_t   (hpdcache_mem_req_t),
    .hpdcache_mem_req_w_t (hpdcache_mem_req_w_t),
    .hpdcache_mem_resp_w_t(hpdcache_mem_resp_w_t),
    .aw_chan_t            (aw_chan_t),
    .w_chan_t             (w_chan_t),
    .b_chan_t             (b_chan_t)
  ) i_hpd_mem_to_axi_write (
    .req_ready_o        (mem_req_write_ready),
    .req_valid_i        (mem_req_write_valid),
    .req_i              (mem_req_write),

    .req_data_ready_o   (mem_req_write_data_ready),
    .req_data_valid_i   (mem_req_write_data_valid),
    .req_data_i         (mem_req_write_data),

    .resp_ready_i       (mem_resp_write_ready),
    .resp_valid_o       (mem_resp_write_valid),
    .resp_o             (mem_resp_write),

    .axi_aw_valid_o(axi_req_o.aw_valid),
    .axi_aw_o      (axi_req_o.aw),
    .axi_aw_ready_i(axi_resp_i.aw_ready),

    .axi_w_valid_o (axi_req_o.w_valid),
    .axi_w_o       (axi_req_o.w),
    .axi_w_ready_i (axi_resp_i.w_ready),

    .axi_b_valid_i (axi_resp_i.b_valid),
    .axi_b_i       (axi_resp_i.b),
    .axi_b_ready_o (axi_req_o.b_ready)
  );

  //////////////////////////////////////
  //        Parameter Assertion       //
  //////////////////////////////////////
`ifndef TARGET_SYNTHESIS
  function automatic bit is_pow2(input int unsigned x);
    return (x > 0) && ((x & (x - 1)) == 0);
  endfunction

  initial begin
    assert (NumPorts > 0)
      else $fatal(1, "NumPorts must be greater than 0. Current value: %0d", NumPorts);
    assert (CoalExtFactor > 0 && is_pow2(CoalExtFactor))
      else $fatal(1, "CoalExtFactor must be greater than 0 and a power of 2. Current value: %0d",
                   CoalExtFactor);
    assert (WordWidth >= 8 && is_pow2(WordWidth) && WordWidth < CacheLineWidth)
      else $fatal(1, "WordWidth must be >= 8, a power of 2, and less than CacheLineWidth. \
Current value: %0d", WordWidth);
    assert (NumCacheEntry >= 2 && is_pow2(NumCacheEntry) && NumCacheEntry > SetAssociativity)
      else $fatal(1, "NumCacheEntry must be >= 2, a power of 2, and greater than \
SetAssociativity. Current value: %0d", NumCacheEntry);
    assert (CacheLineWidth > $bits(core_meta_t) && is_pow2(CacheLineWidth))
      else $fatal(1, "CacheLineWidth must be greater than the width of core_meta_t and a \
power of 2. Current value: %0d", CacheLineWidth);
    assert (SetAssociativity >= 1 && is_pow2(SetAssociativity))
      else $fatal(1, "SetAssociativity must be >= 1 and a power of 2. Current value: %0d",
                   SetAssociativity);
    assert (HpdMshrSets >= 1 && is_pow2(HpdMshrSets))
      else $fatal(1, "HpdMshrSets must be >= 1 and a power of 2. Current value: %0d",
                   HpdMshrSets);
    assert (HpdMshrWays >= 1 && is_pow2(HpdMshrWays))
      else $fatal(1, "HpdMshrWays must be >= 1 and a power of 2. Current value: %0d",
                   HpdMshrWays);
    // {way,set} must fit in the memory-side master port's actual AXI ID
    // field, or hpdcache_mem_to_axi_read.sv's `axi_ar_o.id = req_i.mem_req_id`
    // silently truncates it -- exactly the bug behind the
    // IwcAxiIdOutWidth widening in spatz_cluster_wrapper.sv.tpl. Sweeping
    // HpdMshrSets/HpdMshrWays up without also widening that constant trips
    // this assertion instead of silently corrupting MSHR ack data.
    assert ($bits(axi_req_o.ar.id) >= $clog2(HpdMshrWays) + $clog2(HpdMshrSets))
      else $fatal(1, "axi_req_o.ar.id is too narrow for HpdMshrWays x HpdMshrSets: needs >= %0d \
bits, has %0d. Widen IwcAxiIdOutWidth in spatz_cluster_wrapper.sv.tpl.",
                   $clog2(HpdMshrWays) + $clog2(HpdMshrSets), $bits(axi_req_o.ar.id));
    assert (is_pow2(HpdIdBufferEntries))
      else $fatal(1, "HpdIdBufferEntries must be a power of 2. Current value: %0d",
                   HpdIdBufferEntries);
    assert ($bits(axi_req_o.aw.addr) >= AddrWidth)
      else $fatal(1, "axi_req_o.aw.addr width must be >= AddrWidth. Current width: %0d, \
AddrWidth: %0d", $bits(axi_req_o.aw.addr), AddrWidth);
    assert ($bits(axi_req_o.ar.addr) >= AddrWidth)
      else $fatal(1, "axi_req_o.ar.addr width must be >= AddrWidth. Current width: %0d, \
AddrWidth: %0d", $bits(axi_req_o.ar.addr), AddrWidth);
    assert ($bits(axi_resp_i.r.data) == CacheLineWidth)
      else $fatal(1, "axi_resp_i.r.data width must equal CacheLineWidth. Current width: %0d, \
CacheLineWidth: %0d", $bits(axi_resp_i.r.data), CacheLineWidth);
    assert ($bits(axi_req_o.w.data) == CacheLineWidth)
      else $fatal(1, "axi_req_o.w.data width must equal CacheLineWidth. Current width: %0d, \
CacheLineWidth: %0d", $bits(axi_req_o.w.data), CacheLineWidth);
  end
`endif

endmodule : hpd_spatz_cache_ctrl
