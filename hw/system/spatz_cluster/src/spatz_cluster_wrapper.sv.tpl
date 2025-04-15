// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

${disclaimer}

<%def name="core_cfg(prop)">\
  % for c in cfg['cores']:
${c[prop]}${', ' if not loop.last else ''}\
  % endfor
</%def>\

<%def name="core_cfg_flat(prop)">\
${cfg['nr_cores']}'b\
  % for c in cfg['cores'][::-1]:
${int(c[prop])}\
  % endfor
</%def>\

<%def name="core_isa(isa)">\
${cfg['nr_cores']}'b\
  % for c in cfg['cores'][::-1]:
${int(getattr(c['isa_parsed'], isa))}\
  % endfor
</%def>\

`include "axi/typedef.svh"

// verilog_lint: waive-start package-filename
package ${cfg['pkg_name']};

  ///////////
  //  AXI  //
  ///////////

  // AXI Data Width
  localparam int unsigned SpatzAxiDataWidth = ${cfg['dma_data_width']};
  localparam int unsigned SpatzAxiStrbWidth = SpatzAxiDataWidth / 8;
  // AXI Address Width
  localparam int unsigned SpatzAxiAddrWidth = ${cfg['addr_width']};
  // AXI ID Width
  localparam int unsigned SpatzAxiIdInWidth = ${cfg['id_width_in']};
  localparam int unsigned SpatzAxiIdOutWidth = ${cfg['id_width_out']};

  // FIXED AxiIdOutWidth
  localparam int unsigned IwcAxiIdOutWidth = 3;

  // AXI User Width
  localparam int unsigned SpatzAxiUserWidth = ${cfg['user_width']};

% if cfg['axi_isolate_enable']:
  parameter int unsigned SpatzAxiMaxOutTrans   = ${cfg['trans']};
% endif

  typedef logic [SpatzAxiDataWidth-1:0] axi_data_t;
  typedef logic [SpatzAxiStrbWidth-1:0] axi_strb_t;
  typedef logic [SpatzAxiAddrWidth-1:0] axi_addr_t;
  typedef logic [SpatzAxiIdInWidth-1:0] axi_id_in_t;
  typedef logic [SpatzAxiIdOutWidth-1:0] axi_id_out_t;
  typedef logic [SpatzAxiUserWidth-1:0] axi_user_t;

% if cfg['axi_cdc_enable']:
  localparam int unsigned SpatzLogDepth = 3;
  localparam int unsigned SpatzCdcSyncStages = 2;
  % if cfg['sw_rst_enable']:
  localparam int unsigned SpatzSyncStages = 3;
  % endif
% endif

  `AXI_TYPEDEF_ALL(spatz_axi_in, axi_addr_t, axi_id_in_t, logic [63:0], logic [7:0], axi_user_t)
  `AXI_TYPEDEF_ALL(spatz_axi_out, axi_addr_t, axi_id_out_t, axi_data_t, axi_strb_t, axi_user_t)

  typedef logic [IwcAxiIdOutWidth-1:0] axi_id_out_iwc_t;

  `AXI_TYPEDEF_ALL(spatz_axi_iwc_out, axi_addr_t, axi_id_out_iwc_t, axi_data_t, axi_strb_t, axi_user_t)

  ////////////////////
  //  Spatz Cluster //
  ////////////////////

  localparam int unsigned NumCores = ${cfg['nr_cores']};

  localparam int unsigned DataWidth  = 64;
  localparam int unsigned BeWidth    = DataWidth / 8;
  localparam int unsigned ByteOffset = $clog2(BeWidth);

  localparam int unsigned ICacheLineWidth = ${cfg['icache']['cacheline']};
  localparam int unsigned ICacheLineCount = ${cfg['icache']['depth']};
  localparam int unsigned ICacheWays = ${cfg['icache']['ways']};

  localparam int unsigned TCDMStartAddr = ${to_sv_hex(cfg['cluster_base_addr'], cfg['addr_width'])};
  localparam int unsigned TCDMSize      = ${to_sv_hex(cfg['tcdm']['size'] * 1024, cfg['addr_width'])};

  localparam int unsigned PeriStartAddr = TCDMStartAddr + TCDMSize;

  localparam int unsigned BootAddr      = ${to_sv_hex(cfg['boot_addr'], cfg['addr_width'])};

  function automatic snitch_pma_pkg::rule_t [snitch_pma_pkg::NrMaxRules-1:0] get_cached_regions();
    automatic snitch_pma_pkg::rule_t [snitch_pma_pkg::NrMaxRules-1:0] cached_regions;
    cached_regions = '{default: '0};
% for i, cp in enumerate(cfg['pmas']['cached']):
    cached_regions[${i}] = '{base: ${to_sv_hex(cp[0], cfg['addr_width'])}, mask: ${to_sv_hex(cp[1], cfg['addr_width'])}};
% endfor
    return cached_regions;
  endfunction

  localparam snitch_pma_pkg::snitch_pma_t SnitchPMACfg = '{
      NrCachedRegionRules: ${len(cfg['pmas']['cached'])},
      CachedRegion: get_cached_regions(),
      default: 0
  };

  localparam fpnew_pkg::fpu_implementation_t FPUImplementation [NumCores] = '{
  % for c in cfg['cores']:
    '{
        PipeRegs: // FMA Block
                  '{
                    '{  ${cfg['timing']['lat_comp_fp32']}, // FP32
                        ${cfg['timing']['lat_comp_fp64']}, // FP64
                        ${cfg['timing']['lat_comp_fp16']}, // FP16
                        ${cfg['timing']['lat_comp_fp8']}, // FP8
                        ${cfg['timing']['lat_comp_fp16_alt']}, // FP16alt
                        ${cfg['timing']['lat_comp_fp8_alt']}  // FP8alt
                      },
                    '{1, 1, 1, 1, 1, 1},   // DIVSQRT
                    '{${cfg['timing']['lat_noncomp']},
                      ${cfg['timing']['lat_noncomp']},
                      ${cfg['timing']['lat_noncomp']},
                      ${cfg['timing']['lat_noncomp']},
                      ${cfg['timing']['lat_noncomp']},
                      ${cfg['timing']['lat_noncomp']}},   // NONCOMP
                    '{${cfg['timing']['lat_conv']},
                      ${cfg['timing']['lat_conv']},
                      ${cfg['timing']['lat_conv']},
                      ${cfg['timing']['lat_conv']},
                      ${cfg['timing']['lat_conv']},
                      ${cfg['timing']['lat_conv']}},   // CONV
                    '{${cfg['timing']['lat_sdotp']},
                      ${cfg['timing']['lat_sdotp']},
                      ${cfg['timing']['lat_sdotp']},
                      ${cfg['timing']['lat_sdotp']},
                      ${cfg['timing']['lat_sdotp']},
                      ${cfg['timing']['lat_sdotp']}}    // DOTP
                    },
        UnitTypes: '{'{fpnew_pkg::MERGED,
                       fpnew_pkg::MERGED,
                       fpnew_pkg::MERGED,
                       fpnew_pkg::MERGED,
                       fpnew_pkg::MERGED,
                       fpnew_pkg::MERGED},  // FMA
                    '{fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED}, // DIVSQRT
                    '{fpnew_pkg::PARALLEL,
                        fpnew_pkg::PARALLEL,
                        fpnew_pkg::PARALLEL,
                        fpnew_pkg::PARALLEL,
                        fpnew_pkg::PARALLEL,
                        fpnew_pkg::PARALLEL}, // NONCOMP
                    '{fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED},   // CONV
% if c["xfdotp"]:
                    '{fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED}},  // DOTP
% else:
                    '{fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED}}, // DOTP
% endif
        PipeConfig: fpnew_pkg::${cfg['timing']['fpu_pipe_config']}
    }${',\n' if not loop.last else '\n'}\
  % endfor
  };

endpackage
// verilog_lint: waive-stop package-filename

module ${cfg['name']}_wrapper
 import ${cfg['pkg_name']}::*;
 import fpnew_pkg::fpu_implementation_t;
 import snitch_pma_pkg::snitch_pma_t;
 #(
  parameter int unsigned AxiAddrWidth  = ${cfg['pkg_name']}::SpatzAxiAddrWidth,
  parameter int unsigned AxiDataWidth  = ${cfg['pkg_name']}::SpatzAxiDataWidth,
  parameter int unsigned AxiUserWidth  = ${cfg['pkg_name']}::SpatzAxiUserWidth,
  parameter int unsigned AxiInIdWidth  = ${cfg['pkg_name']}::SpatzAxiIdInWidth,
  parameter int unsigned AxiOutIdWidth = ${cfg['pkg_name']}::SpatzAxiIdOutWidth,
% if cfg['axi_cdc_enable']:
  parameter int unsigned LogDepth      = ${cfg['pkg_name']}::SpatzLogDepth,
  parameter int unsigned CdcSyncStages = ${cfg['pkg_name']}::SpatzCdcSyncStages,
  % if cfg['sw_rst_enable']:
  parameter int unsigned SyncStages    = ${cfg['pkg_name']}::SpatzSyncStages,
  % endif
% endif

  parameter type axi_in_resp_t = spatz_axi_in_resp_t,
  parameter type axi_in_req_t  = spatz_axi_in_req_t,
% if cfg['axi_cdc_enable']:
  parameter type axi_in_aw_chan_t = spatz_axi_in_aw_chan_t,
  parameter type axi_in_w_chan_t  = spatz_axi_in_w_chan_t,
  parameter type axi_in_b_chan_t  = spatz_axi_in_b_chan_t,
  parameter type axi_in_ar_chan_t = spatz_axi_in_ar_chan_t,
  parameter type axi_in_r_chan_t  = spatz_axi_in_r_chan_t,
% endif

  parameter type axi_out_resp_t = spatz_axi_out_resp_t,
% if cfg['axi_cdc_enable']:
  parameter type axi_out_req_t  = spatz_axi_out_req_t,

  parameter type axi_out_aw_chan_t = spatz_axi_out_aw_chan_t,
  parameter type axi_out_w_chan_t  = spatz_axi_out_w_chan_t,
  parameter type axi_out_b_chan_t  = spatz_axi_out_b_chan_t,
  parameter type axi_out_ar_chan_t = spatz_axi_out_ar_chan_t,
  parameter type axi_out_r_chan_t  = spatz_axi_out_r_chan_t,

  // AXI Master
  parameter int unsigned AsyncAxiOutAwWidth = (2**LogDepth) * axi_pkg::aw_width(SpatzAxiAddrWidth,  SpatzAxiIdOutWidth, SpatzAxiUserWidth),
  parameter int unsigned AsyncAxiOutWWidth  = (2**LogDepth) * axi_pkg::w_width (SpatzAxiDataWidth,  SpatzAxiUserWidth),
  parameter int unsigned AsyncAxiOutBWidth  = (2**LogDepth) * axi_pkg::b_width (SpatzAxiIdOutWidth, SpatzAxiUserWidth),
  parameter int unsigned AsyncAxiOutArWidth = (2**LogDepth) * axi_pkg::ar_width(SpatzAxiAddrWidth,  SpatzAxiIdOutWidth, SpatzAxiUserWidth),
  parameter int unsigned AsyncAxiOutRWidth  = (2**LogDepth) * axi_pkg::r_width (SpatzAxiDataWidth,  SpatzAxiIdOutWidth, SpatzAxiUserWidth),

  // AXI Slave
  parameter int unsigned AsyncAxiInAwWidth = (2**LogDepth) * axi_pkg::aw_width(SpatzAxiAddrWidth, SpatzAxiIdInWidth, SpatzAxiUserWidth),
  parameter int unsigned AsyncAxiInWWidth  = (2**LogDepth) * axi_pkg::w_width (SpatzAxiDataWidth, SpatzAxiUserWidth),
  parameter int unsigned AsyncAxiInBWidth  = (2**LogDepth) * axi_pkg::b_width (SpatzAxiIdInWidth, SpatzAxiUserWidth),
  parameter int unsigned AsyncAxiInArWidth = (2**LogDepth) * axi_pkg::ar_width(SpatzAxiAddrWidth, SpatzAxiIdInWidth, SpatzAxiUserWidth),
  parameter int unsigned AsyncAxiInRWidth  = (2**LogDepth) * axi_pkg::r_width (SpatzAxiDataWidth, SpatzAxiIdInWidth, SpatzAxiUserWidth)
% else:
  parameter type axi_out_req_t  = spatz_axi_out_req_t
% endif
)(
  input  logic                clk_i,
  input  logic                rst_ni,
% if cfg['enable_debug']:
  input  logic [NumCores-1:0] debug_req_i,
% endif

  input  logic [NumCores-1:0] meip_i,
  input  logic [NumCores-1:0] mtip_i,
  input  logic [NumCores-1:0] msip_i,
% if not cfg['tie_ports']:
  input  logic [9:0]                    hart_base_id_i,
  input  logic [AxiAddrWidth-1:0]       cluster_base_addr_i,
% endif
  output logic                          cluster_probe_o,
% if cfg['axi_isolate_enable']:
  input  logic axi_isolate_i,
  output logic axi_isolated_o,
% endif
% if cfg['axi_cdc_enable']:
  % if cfg['sw_rst_enable']:
  input  logic                          pwr_on_rst_ni,
  % endif

  // AXI Master port
  output logic [AsyncAxiOutAwWidth-1:0] async_axi_out_aw_data_o,
  output logic [LogDepth:0]             async_axi_out_aw_wptr_o,
  input  logic [LogDepth:0]             async_axi_out_aw_rptr_i,
  output logic [AsyncAxiOutWWidth-1:0]  async_axi_out_w_data_o,
  output logic [LogDepth:0]             async_axi_out_w_wptr_o,
  input  logic [LogDepth:0]             async_axi_out_w_rptr_i,
  input  logic [AsyncAxiOutBWidth-1:0]  async_axi_out_b_data_i,
  input  logic [LogDepth:0]             async_axi_out_b_wptr_i,
  output logic [LogDepth:0]             async_axi_out_b_rptr_o,
  output logic [AsyncAxiOutArWidth-1:0] async_axi_out_ar_data_o,
  output logic [LogDepth:0]             async_axi_out_ar_wptr_o,
  input  logic [LogDepth:0]             async_axi_out_ar_rptr_i,
  input  logic [AsyncAxiOutRWidth-1:0]  async_axi_out_r_data_i,
  input  logic [LogDepth:0]             async_axi_out_r_wptr_i,
  output logic [LogDepth:0]             async_axi_out_r_rptr_o,

  // AXI Slave port
  input  logic [AsyncAxiInArWidth-1:0] async_axi_in_ar_data_i,
  input  logic [LogDepth:0]            async_axi_in_ar_wptr_i,
  output logic [LogDepth:0]            async_axi_in_ar_rptr_o,
  input  logic [AsyncAxiInAwWidth-1:0] async_axi_in_aw_data_i,
  input  logic [LogDepth:0]            async_axi_in_aw_wptr_i,
  output logic [LogDepth:0]            async_axi_in_aw_rptr_o,
  output logic [AsyncAxiInBWidth-1:0]  async_axi_in_b_data_o,
  output logic [LogDepth:0]            async_axi_in_b_wptr_o,
  input  logic [LogDepth:0]            async_axi_in_b_rptr_i,
  output logic [AsyncAxiInRWidth-1:0]  async_axi_in_r_data_o,
  output logic [LogDepth:0]            async_axi_in_r_wptr_o,
  input  logic [LogDepth:0]            async_axi_in_r_rptr_i,
  input  logic [AsyncAxiInWWidth-1:0]  async_axi_in_w_data_i,
  input  logic [LogDepth:0]            async_axi_in_w_wptr_i,
  output logic [LogDepth:0]            async_axi_in_w_rptr_o
%else:
  input  axi_in_req_t   axi_in_req_i,
  output axi_in_resp_t  axi_in_resp_o,
  output axi_out_req_t  axi_out_req_o,
  input  axi_out_resp_t axi_out_resp_i
% endif
);

  localparam int unsigned NumIntOutstandingLoads   [NumCores] = '{${core_cfg('num_int_outstanding_loads')}};
  localparam int unsigned NumIntOutstandingMem     [NumCores] = '{${core_cfg('num_int_outstanding_mem')}};
  localparam int unsigned NumSpatzOutstandingLoads [NumCores] = '{${core_cfg('num_spatz_outstanding_loads')}};
  localparam int unsigned NumSpatzFPUs             [NumCores] = '{default: ${cfg['n_fpu']}};
  localparam int unsigned NumSpatzIPUs             [NumCores] = '{default: ${cfg['n_ipu']}};

  spatz_cluster_pkg::spatz_axi_iwc_out_req_t axi_from_cluster_iwc_req;
  spatz_cluster_pkg::spatz_axi_iwc_out_resp_t axi_from_cluster_iwc_resp;

% if cfg['axi_isolate_enable'] or cfg['axi_cdc_enable']:
  axi_out_req_t  axi_from_cluster_req;
  axi_out_resp_t axi_from_cluster_resp;
% endif

% if cfg['axi_isolate_enable']:
  % if cfg['axi_cdc_enable']:
  // From AXI Isolate to CDC
  axi_out_req_t  axi_from_cluster_iso_req;
  axi_out_resp_t axi_from_cluster_iso_resp;
  % if cfg['sw_rst_enable']:
  logic axi_isolate_sync;

  sync #(
    .STAGES     ( SyncStages ),
    .ResetValue ( 1'b1       )
  ) i_isolate_sync (
    .clk_i,
    .rst_ni   ( pwr_on_rst_ni    ),
    .serial_i ( axi_isolate_i    ),
    .serial_o ( axi_isolate_sync )
  );
  % endif
  % endif

  axi_isolate #(
    .NumPending           ( ${cfg['pkg_name']}::SpatzAxiMaxOutTrans ),
    .TerminateTransaction ( 1              ),
    .AtopSupport          ( 1              ),
    .AxiAddrWidth         ( AxiAddrWidth   ),
    .AxiDataWidth         ( AxiDataWidth   ),
    .AxiIdWidth           ( AxiOutIdWidth  ),
    .AxiUserWidth         ( AxiUserWidth   ),
    .axi_req_t            ( axi_out_req_t  ),
    .axi_resp_t           ( axi_out_resp_t )
  ) i_axi_out_isolate     (
    .clk_i                ( clk_i                 ),
    .rst_ni               ( rst_ni                ),
    .slv_req_i            ( axi_from_cluster_req  ),
    .slv_resp_o           ( axi_from_cluster_resp ),
  % if cfg['axi_cdc_enable']:
    .mst_req_o            ( axi_from_cluster_iso_req   ),
    .mst_resp_i           ( axi_from_cluster_iso_resp  ),
  % else:
    .mst_req_o            ( axi_out_req_o              ),
    .mst_resp_i           ( axi_out_resp_i             ),
  % endif
  % if cfg['axi_cdc_enable'] and cfg['sw_rst_enable']:
    .isolate_i            ( axi_isolate_sync           ),
  % else:
    .isolate_i            ( axi_isolate_i              ),
  % endif
    .isolated_o           ( axi_isolated_o             )
  ) ;
% endif

% if cfg['axi_cdc_enable']:
  // From CDC to Cluster
  axi_in_req_t   axi_to_cluster_req;
  axi_in_resp_t  axi_to_cluster_resp;

  axi_cdc_dst #(
    .LogDepth   ( LogDepth          ),
    .SyncStages ( CdcSyncStages     ),
    .aw_chan_t  ( axi_in_aw_chan_t  ),
    .w_chan_t   ( axi_in_w_chan_t   ),
    .b_chan_t   ( axi_in_b_chan_t   ),
    .ar_chan_t  ( axi_in_ar_chan_t  ),
    .r_chan_t   ( axi_in_r_chan_t   ),
    .axi_req_t  ( axi_in_req_t      ),
    .axi_resp_t ( axi_in_resp_t     )
  ) i_spatz_cluster_cdc_dst (
    // Asynchronous slave port
    .async_data_slave_aw_data_i ( async_axi_in_aw_data_i ),
    .async_data_slave_aw_wptr_i ( async_axi_in_aw_wptr_i ),
    .async_data_slave_aw_rptr_o ( async_axi_in_aw_rptr_o ),
    .async_data_slave_w_data_i  ( async_axi_in_w_data_i  ),
    .async_data_slave_w_wptr_i  ( async_axi_in_w_wptr_i  ),
    .async_data_slave_w_rptr_o  ( async_axi_in_w_rptr_o  ),
    .async_data_slave_b_data_o  ( async_axi_in_b_data_o  ),
    .async_data_slave_b_wptr_o  ( async_axi_in_b_wptr_o  ),
    .async_data_slave_b_rptr_i  ( async_axi_in_b_rptr_i  ),
    .async_data_slave_ar_data_i ( async_axi_in_ar_data_i ),
    .async_data_slave_ar_wptr_i ( async_axi_in_ar_wptr_i ),
    .async_data_slave_ar_rptr_o ( async_axi_in_ar_rptr_o ),
    .async_data_slave_r_data_o  ( async_axi_in_r_data_o  ),
    .async_data_slave_r_wptr_o  ( async_axi_in_r_wptr_o  ),
    .async_data_slave_r_rptr_i  ( async_axi_in_r_rptr_i  ),
    // Synchronous master port
    .dst_clk_i                  ( clk_i                  ),
  % if cfg['sw_rst_enable']:
    .dst_rst_ni                 ( pwr_on_rst_ni          ),
  % else:
    .dst_rst_ni                 ( rst_ni                 ),
  % endif
    .dst_req_o                  ( axi_to_cluster_req     ),
    .dst_resp_i                 ( axi_to_cluster_resp    )
  );

  axi_cdc_src #(
   .LogDepth   ( LogDepth          ),
   .SyncStages ( CdcSyncStages     ),
   .aw_chan_t  ( axi_out_aw_chan_t ),
   .w_chan_t   ( axi_out_w_chan_t  ),
   .b_chan_t   ( axi_out_b_chan_t  ),
   .ar_chan_t  ( axi_out_ar_chan_t ),
   .r_chan_t   ( axi_out_r_chan_t  ),
   .axi_req_t  ( axi_out_req_t     ),
   .axi_resp_t ( axi_out_resp_t    )
  ) i_spatz_cluster_cdc_src (
    // Asynchronous Master port
    .async_data_master_aw_data_o( async_axi_out_aw_data_o ),
    .async_data_master_aw_wptr_o( async_axi_out_aw_wptr_o ),
    .async_data_master_aw_rptr_i( async_axi_out_aw_rptr_i ),
    .async_data_master_w_data_o ( async_axi_out_w_data_o  ),
    .async_data_master_w_wptr_o ( async_axi_out_w_wptr_o  ),
    .async_data_master_w_rptr_i ( async_axi_out_w_rptr_i  ),
    .async_data_master_b_data_i ( async_axi_out_b_data_i  ),
    .async_data_master_b_wptr_i ( async_axi_out_b_wptr_i  ),
    .async_data_master_b_rptr_o ( async_axi_out_b_rptr_o  ),
    .async_data_master_ar_data_o( async_axi_out_ar_data_o ),
    .async_data_master_ar_wptr_o( async_axi_out_ar_wptr_o ),
    .async_data_master_ar_rptr_i( async_axi_out_ar_rptr_i ),
    .async_data_master_r_data_i ( async_axi_out_r_data_i  ),
    .async_data_master_r_wptr_i ( async_axi_out_r_wptr_i  ),
    .async_data_master_r_rptr_o ( async_axi_out_r_rptr_o  ),
    // Synchronous slave port
    .src_clk_i                  ( clk_i                    ),
  % if cfg['sw_rst_enable']:
    .src_rst_ni                 ( pwr_on_rst_ni          ),
  % else:
    .src_rst_ni                 ( rst_ni                 ),
  % endif
  % if cfg['axi_isolate_enable']:
    .src_req_i                  ( axi_from_cluster_iso_req ),
    .src_resp_o                 ( axi_from_cluster_iso_resp)
  % else:
    .src_req_i                  ( axi_from_cluster_req     ),
    .src_resp_o                 ( axi_from_cluster_resp    )
  % endif
  );
% endif

  axi_iw_converter #(
    .AxiSlvPortIdWidth ( spatz_cluster_pkg::IwcAxiIdOutWidth ),
    .AxiMstPortIdWidth ( AxiOutIdWidth ),
    .AxiSlvPortMaxUniqIds ( 2 ),
    .AxiSlvPortMaxTxnsPerId (2),
    .AxiSlvPortMaxTxns (${cfg['trans']}),
    .AxiMstPortMaxUniqIds (2),
    .AxiMstPortMaxTxnsPerId (${cfg['trans']}),
    .AxiAddrWidth ( AxiAddrWidth ),
    .AxiDataWidth ( AxiDataWidth ),
    .AxiUserWidth ( AxiUserWidth ),
    .slv_req_t  (spatz_cluster_pkg::spatz_axi_iwc_out_req_t),
    .slv_resp_t (spatz_cluster_pkg::spatz_axi_iwc_out_resp_t),
    .mst_req_t  ( axi_out_req_t),
    .mst_resp_t ( axi_out_resp_t)
  ) iw_converter(
    .clk_i      ( clk_i  ),
    .rst_ni     ( rst_ni ),
    .slv_req_i  ( axi_from_cluster_iwc_req  ),
    .slv_resp_o ( axi_from_cluster_iwc_resp ),
% if cfg['axi_cdc_enable'] or cfg['axi_isolate_enable']:
    .mst_req_o  ( axi_from_cluster_req  ),
    .mst_resp_i ( axi_from_cluster_resp )
% else:
    .mst_req_o  ( axi_out_req_o  ),
    .mst_resp_i ( axi_out_resp_i )
% endif
   );

  // Spatz cluster under test.
  spatz_cluster #(
    .AxiAddrWidth (AxiAddrWidth),
    .AxiDataWidth (AxiDataWidth),
    .AxiIdWidthIn (AxiInIdWidth),
    .AxiIdWidthOut (spatz_cluster_pkg::IwcAxiIdOutWidth),
    .AxiUserWidth (AxiUserWidth),
    .BootAddr (${to_sv_hex(cfg['boot_addr'], 32)}),
    .ClusterPeriphSize (${cfg['cluster_periph_size']}),
    .NrCores (${cfg['nr_cores']}),
    .TCDMDepth (${cfg['tcdm']['depth']}),
    .NrBanks (${cfg['tcdm']['banks']}),
    .ICacheLineWidth (${cfg['pkg_name']}::ICacheLineWidth),
    .ICacheLineCount (${cfg['pkg_name']}::ICacheLineCount),
    .ICacheWays (${cfg['pkg_name']}::ICacheWays),
    .FPUImplementation (${cfg['pkg_name']}::FPUImplementation),
    .SnitchPMACfg (${cfg['pkg_name']}::SnitchPMACfg),
    .NumIntOutstandingLoads (NumIntOutstandingLoads),
    .NumIntOutstandingMem (NumIntOutstandingMem),
    .NumSpatzOutstandingLoads (NumSpatzOutstandingLoads),
    .NumSpatzFPUs (NumSpatzFPUs),
    .NumSpatzIPUs (NumSpatzIPUs),
    .axi_in_req_t (axi_in_req_t),
    .axi_in_resp_t (axi_in_resp_t),
    .axi_out_req_t (spatz_cluster_pkg::spatz_axi_iwc_out_req_t),
    .axi_out_resp_t (spatz_cluster_pkg::spatz_axi_iwc_out_resp_t),
    .Xdma (${core_cfg_flat('xdma')}),
    .DMAAxiReqFifoDepth (${cfg['dma_axi_req_fifo_depth']}),
    .DMAReqFifoDepth (${cfg['dma_req_fifo_depth']}),
    .RegisterOffloadRsp (${int(cfg['timing']['register_offload_rsp'])}),
    .RegisterCoreReq (${int(cfg['timing']['register_core_req'])}),
    .RegisterCoreRsp (${int(cfg['timing']['register_core_rsp'])}),
    .RegisterTCDMCuts (${int(cfg['timing']['register_tcdm_cuts'])}),
    .RegisterExt (${int(cfg['timing']['register_ext'])}),
    .XbarLatency (axi_pkg::${cfg['timing']['xbar_latency']}),
    .MaxMstTrans (${cfg['trans']}),
    .MaxSlvTrans (${cfg['trans']})
  ) i_cluster (
    .clk_i,
    .rst_ni,
% if cfg['enable_debug']:
    .debug_req_i,
% else:
    .debug_req_i ('0),
% endif
    .meip_i,
    .mtip_i,
    .msip_i,
% if cfg['tie_ports']:
    .hart_base_id_i (${to_sv_hex(cfg['cluster_base_hartid'], 10)}),
    .cluster_base_addr_i (${to_sv_hex(cfg['cluster_base_addr'], cfg['addr_width'])}),
    .axi_core_default_user_i (${to_sv_hex(cfg['cluster_default_axi_user'], cfg['user_width'])}),
% else:
    .hart_base_id_i,
    .cluster_base_addr_i,
    .axi_core_default_user_i,
% endif
    .cluster_probe_o,
% if cfg['axi_cdc_enable']:
    // AXI Slave Port
    .axi_in_req_i   ( axi_to_cluster_req  ),
    .axi_in_resp_o  ( axi_to_cluster_resp ),
% else:
    .axi_in_req_i,
    .axi_in_resp_o,
% endif
    // AXI Master Port
    .axi_out_req_o  ( axi_from_cluster_iwc_req ),
    .axi_out_resp_i ( axi_from_cluster_iwc_resp )
  );

  // Assertions

  if (AxiAddrWidth != ${cfg['pkg_name']}::SpatzAxiAddrWidth)
    $error("[spatz_cluster_wrapper] AXI Address Width does not match the configuration.");

  if (AxiDataWidth != ${cfg['pkg_name']}::SpatzAxiDataWidth)
    $error("[spatz_cluster_wrapper] AXI Data Width does not match the configuration.");

  if (AxiUserWidth != ${cfg['pkg_name']}::SpatzAxiUserWidth)
    $error("[spatz_cluster_wrapper] AXI User Width does not match the configuration.");

  if (AxiInIdWidth != ${cfg['pkg_name']}::SpatzAxiIdInWidth)
    $error("[spatz_cluster_wrapper] AXI Id Width (In) does not match the configuration.");

  if (AxiOutIdWidth != ${cfg['pkg_name']}::SpatzAxiIdOutWidth)
    $error("[spatz_cluster_wrapper] AXI Id Width (Out) does not match the configuration.");

% if cfg['axi_cdc_enable']:
  if (LogDepth != ${cfg['pkg_name']}::SpatzLogDepth)
    $error("[spatz_cluster_wrapper] AXI Log Depth does not match the configuration.");
% endif
endmodule
