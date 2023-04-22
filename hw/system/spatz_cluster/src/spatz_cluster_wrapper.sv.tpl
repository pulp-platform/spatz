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
  localparam int unsigned AxiDataWidth = ${cfg['dma_data_width']};
  localparam int unsigned AxiStrbWidth = AxiDataWidth / 8;
  // AXI Address Width
  localparam int unsigned AxiAddrWidth = ${cfg['addr_width']};
  // AXI ID Width
  localparam int unsigned AxiIdInWidth = ${cfg['id_width_in']};
  // AXI User Width
  localparam int unsigned AxiUserWidth = ${cfg['user_width']};

  typedef logic [AxiDataWidth-1:0] axi_data_t;
  typedef logic [AxiStrbWidth-1:0] axi_strb_t;
  typedef logic [AxiAddrWidth-1:0] axi_addr_t;
  typedef logic [AxiIdInWidth-1:0] axi_id_in_t;
  typedef logic [AxiUserWidth-1:0] axi_user_t;

  localparam int unsigned AxiClusterNumMasters = 3;
  localparam int unsigned AxiIdOutWidth = AxiIdInWidth + $clog2(AxiClusterNumMasters);
  typedef logic [AxiIdOutWidth-1:0] axi_id_out_t;

  `AXI_TYPEDEF_ALL(spatz_axi_in, axi_addr_t, axi_id_in_t, logic [63:0], logic [7:0], axi_user_t)
  `AXI_TYPEDEF_ALL(spatz_axi_out, axi_addr_t, axi_id_out_t, axi_data_t, axi_strb_t, axi_user_t)

  ////////////////////
  //  Spatz Cluster //
  ////////////////////

  localparam int unsigned NumCores = ${cfg['nr_cores']};

  localparam int unsigned DataWidth  = 64;
  localparam int unsigned BeWidth    = DataWidth / 8;
  localparam int unsigned ByteOffset = $clog2(BeWidth);

  localparam int unsigned ICacheLineWidth = ${cfg['icache']['cacheline']};
  localparam int unsigned ICacheLineCount = ${cfg['icache']['depth']};
  localparam int unsigned ICacheSets = ${cfg['icache']['sets']};

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

  localparam fpnew_pkg::fpu_implementation_t FPUImplementation [${cfg['nr_cores']}] = '{
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

module ${cfg['name']}_wrapper (
  input  logic                                    clk_i,
  input  logic                                    rst_ni,
% if cfg['enable_debug']:
  input  logic [${cfg['pkg_name']}::NumCores-1:0] debug_req_i,
% endif
  input  logic [${cfg['pkg_name']}::NumCores-1:0] meip_i,
  input  logic [${cfg['pkg_name']}::NumCores-1:0] mtip_i,
  input  logic [${cfg['pkg_name']}::NumCores-1:0] msip_i,
% if not cfg['tie_ports']:
  input  logic [9:0]                             hart_base_id_i,
  input  logic [${cfg['addr_width']-1}:0]                            cluster_base_addr_i,
% endif
  output logic                                    cluster_probe_o,
  input  ${cfg['pkg_name']}::spatz_axi_in_req_t   axi_in_req_i,
  output ${cfg['pkg_name']}::spatz_axi_in_resp_t  axi_in_resp_o,
  output ${cfg['pkg_name']}::spatz_axi_out_req_t  axi_out_req_o,
  input  ${cfg['pkg_name']}::spatz_axi_out_resp_t axi_out_resp_i
);

  localparam int unsigned NumIntOutstandingLoads [${cfg['nr_cores']}] = '{${core_cfg('num_int_outstanding_loads')}};
  localparam int unsigned NumIntOutstandingMem [${cfg['nr_cores']}] = '{${core_cfg('num_int_outstanding_mem')}};
  localparam int unsigned NumSpatzOutstandingLoads [${cfg['nr_cores']}] = '{${core_cfg('num_spatz_outstanding_loads')}};

  // Spatz cluster under test.
  spatz_cluster #(
    .AxiAddrWidth (${cfg['addr_width']}),
    .AxiDataWidth (${cfg['dma_data_width']}),
    .AxiIdWidthIn (${cfg['pkg_name']}::AxiIdInWidth),
    .AxiUserWidth (${cfg['pkg_name']}::AxiUserWidth),
    .BootAddr (${to_sv_hex(cfg['boot_addr'], 32)}),
    .ClusterPeriphSize (${cfg['cluster_periph_size']}),
    .NrCores (${cfg['nr_cores']}),
    .TCDMDepth (${cfg['tcdm']['depth']}),
    .NrBanks (${cfg['tcdm']['banks']}),
    .ICacheLineWidth (${cfg['pkg_name']}::ICacheLineWidth),
    .ICacheLineCount (${cfg['pkg_name']}::ICacheLineCount),
    .ICacheSets (${cfg['pkg_name']}::ICacheSets),
    .FPUImplementation (${cfg['pkg_name']}::FPUImplementation),
    .SnitchPMACfg (${cfg['pkg_name']}::SnitchPMACfg),
    .NumIntOutstandingLoads (NumIntOutstandingLoads),
    .NumIntOutstandingMem (NumIntOutstandingMem),
    .NumSpatzOutstandingLoads (NumSpatzOutstandingLoads),
    .axi_in_req_t (${cfg['pkg_name']}::spatz_axi_in_req_t),
    .axi_in_resp_t (${cfg['pkg_name']}::spatz_axi_in_resp_t),
    .axi_out_req_t (${cfg['pkg_name']}::spatz_axi_out_req_t),
    .axi_out_resp_t (${cfg['pkg_name']}::spatz_axi_out_resp_t),
    .Xdma (${core_cfg_flat('xdma')}),
    .DMAAxiReqFifoDepth (${cfg['dma_axi_req_fifo_depth']}),
    .DMAReqFifoDepth (${cfg['dma_req_fifo_depth']}),
    .RegisterOffloadReq (${int(cfg['timing']['register_offload_req'])}),
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
    .hart_base_id_i (${to_sv_hex(cfg['hart_base_id'], 10)}),
    .cluster_base_addr_i (${to_sv_hex(cfg['cluster_base_addr'], cfg['addr_width'])}),
% else:
    .hart_base_id_i,
    .cluster_base_addr_i,
% endif
    .cluster_probe_o,
    .axi_in_req_i,
    .axi_in_resp_o,
    .axi_out_req_o,
    .axi_out_resp_i
  );
endmodule
