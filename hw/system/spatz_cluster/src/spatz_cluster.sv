// Copyright 2023 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>

`include "axi/assign.svh"
`include "axi/typedef.svh"
`include "common_cells/assertions.svh"
`include "common_cells/registers.svh"
`include "mem_interface/assign.svh"
`include "mem_interface/typedef.svh"
`include "register_interface//assign.svh"
`include "register_interface/typedef.svh"
`include "reqrsp_interface/assign.svh"
`include "reqrsp_interface/typedef.svh"
`include "snitch_vm/typedef.svh"
`include "tcdm_interface/assign.svh"
`include "tcdm_interface/typedef.svh"

/// Spatz many-core cluster with improved TCDM interconnect.
/// Spatz Cluster Top-Level.
module spatz_cluster
  import spatz_pkg::*;
  import fpnew_pkg::fpu_implementation_t;
  import snitch_pma_pkg::snitch_pma_t;
  #(
    /// Width of physical address.
    parameter int                     unsigned               AxiAddrWidth                       = 48,
    /// Width of AXI port.
    parameter int                     unsigned               AxiDataWidth                       = 512,
    /// AXI: id width in.
    parameter int                     unsigned               AxiIdWidthIn                       = 2,
    /// AXI: id width out.
    parameter int                     unsigned               AxiIdWidthOut                      = 2,
    /// AXI: user width.
    parameter int                     unsigned               AxiUserWidth                       = 1,
    /// Address from which to fetch the first instructions.
    parameter logic                            [31:0]        BootAddr                           = 32'h0,
    /// Address to indicate start of L2
    parameter logic                   [AxiAddrWidth-1:0]     L2Addr                             = 48'h0,
    parameter logic                   [AxiAddrWidth-1:0]     L2Size                             = 48'h0,
    /// The total amount of cores.
    parameter int                     unsigned               NrCores                            = 8,
    /// Data/TCDM memory depth per cut (in words).
    parameter int                     unsigned               TCDMDepth                          = 1024,
    /// Cluster peripheral address region size (in kB).
    parameter int                     unsigned               ClusterPeriphSize                  = 64,
    /// Number of TCDM Banks.
    parameter int                     unsigned               NrBanks                            = 2 * NrCores,
    /// Size of DMA AXI buffer.
    parameter int                     unsigned               DMAAxiReqFifoDepth                 = 3,
    /// Size of DMA request fifo.
    parameter int                     unsigned               DMAReqFifoDepth                    = 3,
    /// Width of a single icache line.
    parameter                         unsigned               ICacheLineWidth                    = 0,
    /// Number of icache lines per set.
    parameter int                     unsigned               ICacheLineCount                    = 0,
    /// Number of icache sets.
    parameter int                     unsigned               ICacheSets                         = 0,
    // PMA Configuration
    parameter snitch_pma_t                                   SnitchPMACfg                       = '{default: 0},
    /// # Core-global parameters
    /// FPU configuration.
    parameter fpu_implementation_t                           FPUImplementation        [NrCores] = '{default: fpu_implementation_t'(0)},
    /// Spatz FPU/IPU Configuration
    parameter int                     unsigned               NumSpatzFPUs                       = 4,
    parameter int                     unsigned               NumSpatzIPUs                       = 1,
    /// Per-core enabling of the custom `Xdma` ISA extensions.
    parameter bit                              [NrCores-1:0] Xdma                               = '{default: '0},
    /// # Per-core parameters
    /// Per-core integer outstanding loads
    parameter int                     unsigned               NumIntOutstandingLoads   [NrCores] = '{default: '0},
    /// Per-core integer outstanding memory operations (load and stores)
    parameter int                     unsigned               NumIntOutstandingMem     [NrCores] = '{default: '0},
    /// Per-core Spatz outstanding loads
    parameter int                     unsigned               NumSpatzOutstandingLoads [NrCores] = '{default: '0},
    /// ## Timing Tuning Parameters
    /// Insert Pipeline registers into off-loading path (response)
    parameter bit                                            RegisterOffloadRsp                 = 1'b0,
    /// Insert Pipeline registers into data memory path (request)
    parameter bit                                            RegisterCoreReq                    = 1'b0,
    /// Insert Pipeline registers into data memory path (response)
    parameter bit                                            RegisterCoreRsp                    = 1'b0,
    /// Insert Pipeline registers after each memory cut
    parameter bit                                            RegisterTCDMCuts                   = 1'b0,
    /// Decouple external AXI plug
    parameter bit                                            RegisterExt                        = 1'b0,
    parameter axi_pkg::xbar_latency_e                        XbarLatency                        = axi_pkg::CUT_ALL_PORTS,
    /// Outstanding transactions on the AXI network
    parameter int                     unsigned               MaxMstTrans                        = 4,
    parameter int                     unsigned               MaxSlvTrans                        = 4,
    /// # Interface
    /// AXI Ports
    parameter type                                           axi_in_req_t                       = logic,
    parameter type                                           axi_in_resp_t                      = logic,
    parameter type                                           axi_out_req_t                      = logic,
    parameter type                                           axi_out_resp_t                     = logic,
    /// SRAM configuration
    parameter type                                           impl_in_t                          = logic,
    // Memory latency parameter. Most of the memories have a read latency of 1. In
    // case you have memory macros which are pipelined you want to adjust this
    // value here. This only applies to the TCDM. The instruction cache macros will break!
    // In case you are using the `RegisterTCDMCuts` feature this adds an
    // additional cycle latency, which is taken into account here.
    parameter int                     unsigned               MemoryMacroLatency                 = 1 + RegisterTCDMCuts,
    /// # SRAM Configuration rules needed: L1D Tag + L1D Data + L1D FIFO + L1I Tag + L1I Data
    /*** ATTENTION: `NrSramCfg` should be changed if `L1NumDataBank` and `L1NumTagBank` is changed ***/
    parameter int                     unsigned               NrSramCfg                          = 128 + 8 + 2 + ICacheSets + ICacheSets
  ) (
    /// System clock.
    input  logic                             clk_i,
    /// Asynchronous active high reset. This signal is assumed to be _async_.
    input  logic                             rst_ni,
    /// Per-core debug request signal. Asserting this signals puts the
    /// corresponding core into debug mode. This signal is assumed to be _async_.
    input  logic          [NrCores-1:0]      debug_req_i,
    /// Machine external interrupt pending. Usually those interrupts come from a
    /// platform-level interrupt controller. This signal is assumed to be _async_.
    input  logic          [NrCores-1:0]      meip_i,
    /// Machine timer interrupt pending. Usually those interrupts come from a
    /// core-local interrupt controller such as a timer/RTC. This signal is
    /// assumed to be _async_.
    input  logic          [NrCores-1:0]      mtip_i,
    /// Core software interrupt pending. Usually those interrupts come from
    /// another core to facilitate inter-processor-interrupts. This signal is
    /// assumed to be _async_.
    input  logic          [NrCores-1:0]      msip_i,
    /// First hartid of the cluster. Cores of a cluster are monotonically
    /// increasing without a gap, i.e., a cluster with 8 cores and a
    /// `hart_base_id_i` of 5 get the hartids 5 - 12.
    input  logic          [9:0]              hart_base_id_i,
    /// Base address of cluster. TCDM and cluster peripheral location are derived from
    /// it. This signal is pseudo-static.
    input  logic          [AxiAddrWidth-1:0] cluster_base_addr_i,
    /// Per-cluster probe on the cluster status. Can be written by the cores to indicate
    /// to the overall system that the cluster is executing something.
    output logic                             cluster_probe_o,
    /// AXI Core cluster in-port.
    input  axi_in_req_t                      axi_in_req_i,
    output axi_in_resp_t                     axi_in_resp_o,
    /// AXI Core cluster out-port to core.
    output axi_out_req_t                     axi_out_req_o,
    input  axi_out_resp_t                    axi_out_resp_i,
    /// AXI Core cluster out-port to L2 Mem.
    output axi_out_req_t                     axi_out_l2_req_o,
    input  axi_out_resp_t                    axi_out_l2_resp_i,
    /// SRAM Configuration: L1D Data + L1D Tag + L1D FIFO + L1I Data + L1I Tag
    input  impl_in_t      [NrSramCfg-1:0]    impl_i,
    /// Indicate the program execution is error
    output logic                             error_o
  );
  // ---------
  // Imports
  // ---------
  import snitch_pkg::*;
  import snitch_icache_pkg::icache_events_t;

  // ---------
  // Constants
  // ---------
  /// Minimum width to hold the core number.
  localparam int unsigned CoreIDWidth       = cf_math_pkg::idx_width(NrCores);
  localparam int unsigned TCDMMemAddrWidth  = $clog2(TCDMDepth);
  localparam int unsigned TCDMSize          = NrBanks * TCDMDepth * (DataWidth/8);
  // The short address for SPM
  localparam int unsigned SPMAddrWidth      = $clog2(TCDMSize);
  // Enlarge the address width for Spatz due to cache
  localparam int unsigned TCDMAddrWidth     = 32;
  localparam int unsigned BanksPerSuperBank = AxiDataWidth / DataWidth;
  localparam int unsigned NrSuperBanks      = NrBanks / BanksPerSuperBank;

  function automatic int unsigned get_tcdm_ports(int unsigned core);
    return spatz_pkg::N_FU + 1;
  endfunction

  function automatic int unsigned get_tcdm_port_offs(int unsigned core_idx);
    automatic int n = 0;
    for (int i = 0; i < core_idx; i++) n += get_tcdm_ports(i);
    return n;
  endfunction

  localparam int unsigned NrTCDMPortsPerCore          = get_tcdm_ports(0);
  localparam int unsigned NrTCDMPortsCores            = get_tcdm_port_offs(NrCores);
  localparam int unsigned NumTCDMIn                   = NrTCDMPortsCores + 1;
  localparam logic        [AxiAddrWidth-1:0] TCDMMask = ~(TCDMSize-1);

  // Core Request, SoC Request
  localparam int unsigned NrNarrowMasters = 2;

  // Narrow AXI network parameters
  localparam int unsigned NarrowIdWidthIn  = AxiIdWidthIn;
  localparam int unsigned NarrowIdWidthOut = NarrowIdWidthIn + $clog2(NrNarrowMasters);
  localparam int unsigned NarrowDataWidth  = ELEN;
  localparam int unsigned NarrowUserWidth  = AxiUserWidth;

  // TCDM, Peripherals, SoC Request
  localparam int unsigned NrNarrowSlaves = 3;
  localparam int unsigned NrNarrowRules  = NrNarrowSlaves - 1;

  // Core Request, DMA, Instruction cache
  /// Additional one for L1 DCache
  localparam int unsigned NrWideMasters  = 3 + 1;
  localparam int unsigned WideIdWidthOut = AxiIdWidthOut;
  localparam int unsigned WideIdWidthIn  = WideIdWidthOut - $clog2(NrWideMasters);
  // DMA X-BAR configuration
  localparam int unsigned NrWideSlaves   = 3 + 1; // one prot for L2, one for L3/LLC (virtual)

  // AXI Configuration
  localparam axi_pkg::xbar_cfg_t ClusterXbarCfg = '{
    NoSlvPorts        : NrNarrowMasters,
    NoMstPorts        : NrNarrowSlaves,
    MaxMstTrans       : MaxMstTrans,
    MaxSlvTrans       : MaxSlvTrans,
    FallThrough       : 1'b0,
    LatencyMode       : XbarLatency,
    AxiIdWidthSlvPorts: NarrowIdWidthIn,
    AxiIdUsedSlvPorts : NarrowIdWidthIn,
    UniqueIds         : 1'b0,
    AxiAddrWidth      : AxiAddrWidth,
    AxiDataWidth      : NarrowDataWidth,
    NoAddrRules       : NrNarrowRules,
    default           : '0
  };

  // DMA configuration struct
  localparam axi_pkg::xbar_cfg_t DmaXbarCfg = '{
    NoSlvPorts        : NrWideMasters,
    NoMstPorts        : NrWideSlaves,
    MaxMstTrans       : MaxMstTrans,
    MaxSlvTrans       : MaxSlvTrans,
    FallThrough       : 1'b0,
    LatencyMode       : XbarLatency,
    AxiIdWidthSlvPorts: WideIdWidthIn,
    AxiIdUsedSlvPorts : WideIdWidthIn,
    UniqueIds         : 1'b0,
    AxiAddrWidth      : AxiAddrWidth,
    AxiDataWidth      : AxiDataWidth,
    NoAddrRules       : NrWideSlaves - 1,
    default           : '0
  };

  // // L1 Cache
  // // Address width of cache
  // localparam int unsigned L1AddrWidth     = 32;
  // // Cache lane width
  // localparam int unsigned L1LineWidth     = 128;
  // // Cache ways
  // localparam int unsigned L1Associativity = 4;
  // // Pesudo dual bank
  // localparam int unsigned L1BankFactor    = 2;
  // // Coalecser window
  // localparam int unsigned L1CoalFactor    = 2;
  // // Total number of Data banks
  // localparam int unsigned L1NumDataBank   = 128;
  // // SPM view: Number of banks in each bank wrap (Use to mitigate routing complexity of such many banks)
  // localparam int unsigned L1BankPerWP     = L1NumDataBank / NrBanks;
  // // 8 * 1024 * 64 / 512 = 1024)
  // // Number of entrys of L1 Cache
  // localparam int unsigned L1NumEntry      = NrBanks * TCDMDepth * DataWidth / L1LineWidth;
  // // Number of bank wraps SPM can see
  // localparam int unsigned L1NumWrapper    = L1NumDataBank / L1BankPerWP;
  // // Number of cache entries each cache way has
  // localparam int unsigned L1CacheWayEntry = L1NumEntry / L1Associativity;
  // // Number of cache sets each cache way has
  // localparam int unsigned L1NumSet        = L1CacheWayEntry / L1BankFactor;
  // // Number of Tag banks
  // localparam int unsigned L1NumTagBank    = L1BankFactor * L1Associativity;
  // // Number of lines per bank unit
  // localparam int unsigned DepthPerBank    = TCDMDepth / L1BankPerWP;
  // // Cache total size in KB
  // localparam int unsigned L1Size          = NrBanks * TCDMDepth * DataWidth / 8 / 1024;

  // Address width of cache
  localparam int unsigned L1AddrWidth     = 32;
  // Cache lane width
  localparam int unsigned L1LineWidth     = AxiDataWidth;
  // Coalecser window
  localparam int unsigned L1CoalFactor    = 2;
  // Total number of Data banks
  localparam int unsigned L1NumDataBank   = 128;
  // Number of bank wraps SPM can see
  localparam int unsigned L1NumWrapper    = NrBanks;
  // SPM view: Number of banks in each bank wrap (Use to mitigate routing complexity of such many banks)
  localparam int unsigned L1BankPerWP     = L1NumDataBank / NrBanks;
  // Pesudo dual bank
  localparam int unsigned L1BankFactor    = 2;
  // Cache ways (total way number across multiple cache controllers)
  localparam int unsigned L1Associativity = L1NumDataBank / (L1LineWidth / DataWidth) / L1BankFactor;
  // 8 * 1024 * 64 / 512 = 1024)
  // Number of entrys of L1 Cache (total number across multiple cache controllers)
  localparam int unsigned L1NumEntry      = NrBanks * TCDMDepth * DataWidth / L1LineWidth;
  // Number of cache entries each cache way has
  localparam int unsigned L1CacheWayEntry = L1NumEntry / L1Associativity;
  // Number of cache sets each cache way has
  localparam int unsigned L1NumSet        = L1CacheWayEntry / L1BankFactor;
  // Number of Tag banks
  localparam int unsigned L1NumTagBank    = L1BankFactor * L1Associativity;
  // Number of lines per bank unit
  localparam int unsigned DepthPerBank    = TCDMDepth / L1BankPerWP;
  // Cache total size in KB
  localparam int unsigned L1Size          = NrBanks * TCDMDepth * DataWidth / 8 / 1024;
  // Number of cache controller (now is fixde to NrCores (if we change it, we need to change the controller axi output id width too)
  localparam int unsigned NumL1CacheCtrl      = NrCores;
  // Number of data banks assigned to each cache controller
  localparam int unsigned NumDataBankPerCtrl  = L1NumDataBank / NumL1CacheCtrl;
  // Number of tag banks assigned to each cache controller
  localparam int unsigned NumTagBankPerCtrl   = L1NumTagBank / NumL1CacheCtrl;
  // Number of ways per cache controller
  localparam int unsigned L1AssoPerCtrl       = L1Associativity / NumL1CacheCtrl;
  // Number of entries per cache controller
  localparam int unsigned L1NumEntryPerCtrl   = L1NumEntry / NumL1CacheCtrl;

  // --------
  // Typedefs
  // --------
  typedef logic [AxiAddrWidth-1:0] addr_t;
  typedef logic [NarrowDataWidth-1:0] data_t;
  typedef logic [63:0] tag_data_t;
  typedef logic [NarrowDataWidth/8-1:0] strb_t;
  typedef logic [AxiDataWidth-1:0] data_dma_t;
  typedef logic [AxiDataWidth/8-1:0] strb_dma_t;
  typedef logic [NarrowIdWidthIn-1:0] id_mst_t;
  typedef logic [NarrowIdWidthOut-1:0] id_slv_t;
  typedef logic [WideIdWidthIn-1:0] id_dma_mst_t;
  typedef logic [WideIdWidthOut-1:0] id_dma_slv_t;
  typedef logic [WideIdWidthIn-$clog2(NumL1CacheCtrl)-1:0] id_dcache_mst_t;
  typedef logic [NarrowUserWidth-1:0] user_t;
  typedef logic [AxiUserWidth-1:0] user_dma_t;

  typedef logic [TCDMMemAddrWidth-1:0] tcdm_mem_addr_t;
  typedef logic [TCDMAddrWidth-1:0] tcdm_addr_t;
  typedef logic [SPMAddrWidth-1:0] spm_addr_t;

  typedef logic [$clog2(NumSpatzOutstandingLoads[0])-1:0] reqid_t;

  typedef logic [$clog2(L1NumSet)-1:0] tcdm_bank_addr_t;

  typedef struct packed {
    logic [CoreIDWidth-1:0] core_id;
    logic is_core;
    reqid_t req_id;
  } tcdm_user_t;

  // The metadata type used to restore the information from req to rsp
  typedef struct packed {
    tcdm_user_t user;
    logic       write;
  } tcdm_meta_t;


  // Regbus peripherals.
  `AXI_TYPEDEF_ALL(axi_mst, addr_t, id_mst_t, data_t, strb_t, user_t)
  `AXI_TYPEDEF_ALL(axi_slv, addr_t, id_slv_t, data_t, strb_t, user_t)
  `AXI_TYPEDEF_ALL(axi_mst_dma, addr_t, id_dma_mst_t, data_dma_t, strb_dma_t, user_dma_t)
  `AXI_TYPEDEF_ALL(axi_slv_dma, addr_t, id_dma_slv_t, data_dma_t, strb_dma_t, user_dma_t)
  `AXI_TYPEDEF_ALL(axi_dcache, addr_t, id_dcache_mst_t, data_dma_t, strb_dma_t, user_dma_t)

  `REQRSP_TYPEDEF_ALL(reqrsp, addr_t, data_t, strb_t)

  `MEM_TYPEDEF_ALL(mem, tcdm_mem_addr_t, data_t, strb_t, tcdm_user_t)
  `MEM_TYPEDEF_ALL(mem_dma, tcdm_mem_addr_t, data_dma_t, strb_dma_t, logic)

  `TCDM_TYPEDEF_ALL(tcdm, tcdm_addr_t, data_t, strb_t, tcdm_user_t)
  `TCDM_TYPEDEF_ALL(tcdm_dma, tcdm_addr_t, data_dma_t, strb_dma_t, logic)
  `TCDM_TYPEDEF_ALL(spm, spm_addr_t, data_t, strb_t, tcdm_user_t)

  `REG_BUS_TYPEDEF_ALL(reg, addr_t, data_t, strb_t)
  `REG_BUS_TYPEDEF_ALL(reg_dma, addr_t, data_dma_t, strb_dma_t)

  // Event counter increments for the TCDM.
  typedef struct packed {
    /// Number requests going in
    logic [$clog2(NrTCDMPortsCores):0] inc_accessed;
    /// Number of requests stalled due to congestion
    logic [$clog2(NrTCDMPortsCores):0] inc_congested;
  } tcdm_events_t;

  // Event counter increments for DMA.
  typedef struct packed {
    logic aw_stall, ar_stall, r_stall, w_stall,
    buf_w_stall, buf_r_stall;
    logic aw_valid, aw_ready, aw_done, aw_bw;
    logic ar_valid, ar_ready, ar_done, ar_bw;
    logic r_valid, r_ready, r_done, r_bw;
    logic w_valid, w_ready, w_done, w_bw;
    logic b_valid, b_ready, b_done;
    logic dma_busy;
    axi_pkg::len_t aw_len, ar_len;
    axi_pkg::size_t aw_size, ar_size;
    logic [$clog2(AxiDataWidth/8):0] num_bytes_written;
  } dma_events_t;

  typedef struct packed {
    int unsigned idx;
    addr_t start_addr;
    addr_t end_addr;
  } xbar_rule_t;

  typedef struct packed {
    acc_addr_e addr;
    logic [5:0] id;
    logic [31:0] data_op;
    data_t data_arga;
    data_t data_argb;
    addr_t data_argc;
  } acc_issue_req_t;

  typedef struct packed {
    logic accept;
    logic writeback;
    logic loadstore;
    logic exception;
    logic isfloat;
  } acc_issue_rsp_t;

  typedef struct packed {
    logic [5:0] id;
    logic error;
    data_t data;
  } acc_rsp_t;

  `SNITCH_VM_TYPEDEF(AxiAddrWidth)

  typedef struct packed {
    // Slow domain.
    logic flush_i_valid;
    addr_t inst_addr;
    logic inst_cacheable;
    logic inst_valid;
    // Fast domain.
    acc_issue_req_t acc_req;
    logic acc_qvalid;
    logic acc_pready;
    // Slow domain.
    logic [1:0] ptw_valid;
    va_t [1:0] ptw_va;
    pa_t [1:0] ptw_ppn;
  } hive_req_t;

  typedef struct packed {
    // Slow domain.
    logic flush_i_ready;
    logic [31:0] inst_data;
    logic inst_ready;
    logic inst_error;
    // Fast domain.
    logic acc_qready;
    acc_rsp_t acc_resp;
    logic acc_pvalid;
    // Slow domain.
    logic [1:0] ptw_ready;
    l0_pte_t [1:0] ptw_pte;
    logic [1:0] ptw_is_4mega;
  } hive_rsp_t;

  // -----------
  // Assignments
  // -----------
  // Calculate start and end address of TCDM based on the `cluster_base_addr_i`.
  addr_t tcdm_start_address, tcdm_end_address;
  assign tcdm_start_address = (cluster_base_addr_i & TCDMMask);
  assign tcdm_end_address   = (tcdm_start_address + TCDMSize) & TCDMMask;

  addr_t cluster_periph_start_address, cluster_periph_end_address;
  assign cluster_periph_start_address = tcdm_end_address;
  assign cluster_periph_end_address   = tcdm_end_address + ClusterPeriphSize * 1024;

  localparam int unsigned ClusterReserve = 4096; // 4 MiB
  localparam int unsigned ClusterL2Size  = 8192; // 8 MiB
  addr_t cluster_l2_start_address, cluster_l2_end_address;
  assign cluster_l2_start_address = L2Addr;
  assign cluster_l2_end_address   = L2Addr + L2Size;

  // ----------------
  // Wire Definitions
  // ----------------
  // 1. AXI
  axi_slv_req_t  [NrNarrowSlaves-1:0]  narrow_axi_slv_req;
  axi_slv_resp_t [NrNarrowSlaves-1:0]  narrow_axi_slv_rsp;
  axi_mst_req_t  [NrNarrowMasters-1:0] narrow_axi_mst_req;
  axi_mst_resp_t [NrNarrowMasters-1:0] narrow_axi_mst_rsp;

  // DMA AXI buses
  axi_mst_dma_req_t  [NrWideMasters-1:0] wide_axi_mst_req;
  axi_mst_dma_resp_t [NrWideMasters-1:0] wide_axi_mst_rsp;
  axi_slv_dma_req_t  [NrWideSlaves-1 :0] wide_axi_slv_req;
  axi_slv_dma_resp_t [NrWideSlaves-1 :0] wide_axi_slv_rsp;

  // AXI req/rsp from/to cache controllers
  axi_dcache_req_t   [NumL1CacheCtrl-1:0] dcache_axi_req;
  axi_dcache_resp_t  [NumL1CacheCtrl-1:0] dcache_axi_rsp;


  // 2. Memory Subsystem (Banks)
  mem_req_t [NrSuperBanks-1:0][BanksPerSuperBank-1:0] ic_req;
  mem_rsp_t [NrSuperBanks-1:0][BanksPerSuperBank-1:0] ic_rsp;

  mem_dma_req_t [NrSuperBanks-1:0] sb_dma_req;
  mem_dma_rsp_t [NrSuperBanks-1:0] sb_dma_rsp;

  // 3. Memory Subsystem (Interconnect)
  tcdm_dma_req_t ext_dma_req;
  tcdm_dma_rsp_t ext_dma_rsp;

  // AXI Ports into TCDM (from SoC).
  spm_req_t axi_soc_req;
  spm_rsp_t axi_soc_rsp;

  tcdm_req_t [NrTCDMPortsCores-1:0] tcdm_req;
  tcdm_rsp_t [NrTCDMPortsCores-1:0] tcdm_rsp;

  core_events_t [NrCores-1:0] core_events;
  tcdm_events_t               tcdm_events;
  dma_events_t                dma_events;
  snitch_icache_pkg::icache_events_t [NrCores-1:0] icache_events;

  // 4. Memory Subsystem (Core side).
  reqrsp_req_t [NrCores-1:0] core_req, filtered_core_req;
  reqrsp_rsp_t [NrCores-1:0] core_rsp, filtered_core_rsp;

  // 5. Peripheral Subsystem
  reg_req_t reg_req;
  reg_rsp_t reg_rsp;

  // 6. BootROM
  reg_dma_req_t bootrom_reg_req;
  reg_dma_rsp_t bootrom_reg_rsp;

  // 7. Misc. Wires.
  logic               icache_prefetch_enable;
  logic [NrCores-1:0] cl_interrupt;

  // 8. L1 D$
  spm_req_t   [NrTCDMPortsCores-1:0] spm_req;
  spm_rsp_t   [NrTCDMPortsCores-1:0] spm_rsp;

  tcdm_req_t  [NrTCDMPortsCores-1:0] unmerge_req, strb_hdl_req, cache_req;
  tcdm_rsp_t  [NrTCDMPortsCores-1:0] unmerge_rsp, strb_hdl_rsp, cache_rsp;

  logic       [NumL1CacheCtrl-1:0][NrTCDMPortsPerCore-1:0] cache_req_valid;
  logic       [NumL1CacheCtrl-1:0][NrTCDMPortsPerCore-1:0] cache_req_ready;
  tcdm_addr_t [NumL1CacheCtrl-1:0][NrTCDMPortsPerCore-1:0] cache_req_addr;
  tcdm_user_t [NumL1CacheCtrl-1:0][NrTCDMPortsPerCore-1:0] cache_req_meta;
  logic       [NumL1CacheCtrl-1:0][NrTCDMPortsPerCore-1:0] cache_req_write;
  data_t      [NumL1CacheCtrl-1:0][NrTCDMPortsPerCore-1:0] cache_req_data;

  logic       [NumL1CacheCtrl-1:0][NrTCDMPortsPerCore-1:0] cache_rsp_valid;
  logic       [NumL1CacheCtrl-1:0][NrTCDMPortsPerCore-1:0] cache_rsp_ready;
  logic       [NumL1CacheCtrl-1:0][NrTCDMPortsPerCore-1:0] cache_rsp_write;
  data_t      [NumL1CacheCtrl-1:0][NrTCDMPortsPerCore-1:0] cache_rsp_data;
  tcdm_user_t [NumL1CacheCtrl-1:0][NrTCDMPortsPerCore-1:0] cache_rsp_meta;

  logic            [NumL1CacheCtrl-1:0][NumTagBankPerCtrl-1:0] l1_tag_bank_req;
  logic            [NumL1CacheCtrl-1:0][NumTagBankPerCtrl-1:0] l1_tag_bank_we;
  tcdm_bank_addr_t [NumL1CacheCtrl-1:0][NumTagBankPerCtrl-1:0] l1_tag_bank_addr;
  tag_data_t       [NumL1CacheCtrl-1:0][NumTagBankPerCtrl-1:0] l1_tag_bank_wdata;
  logic            [NumL1CacheCtrl-1:0][NumTagBankPerCtrl-1:0] l1_tag_bank_be;
  tag_data_t       [NumL1CacheCtrl-1:0][NumTagBankPerCtrl-1:0] l1_tag_bank_rdata;

  logic            [NumL1CacheCtrl-1:0][NumDataBankPerCtrl-1:0] l1_data_bank_req;
  logic            [NumL1CacheCtrl-1:0][NumDataBankPerCtrl-1:0] l1_data_bank_we;
  tcdm_bank_addr_t [NumL1CacheCtrl-1:0][NumDataBankPerCtrl-1:0] l1_data_bank_addr;
  data_t           [NumL1CacheCtrl-1:0][NumDataBankPerCtrl-1:0] l1_data_bank_wdata;
  logic            [NumL1CacheCtrl-1:0][NumDataBankPerCtrl-1:0] l1_data_bank_be;
  data_t           [NumL1CacheCtrl-1:0][NumDataBankPerCtrl-1:0] l1_data_bank_rdata;
  logic            [NumL1CacheCtrl-1:0][NumDataBankPerCtrl-1:0] l1_data_bank_gnt;

  logic            [L1NumWrapper-1:0][L1BankPerWP-1:0]      l1_cache_wp_req;
  logic            [L1NumWrapper-1:0][L1BankPerWP-1:0]      l1_cache_wp_we;
  tcdm_bank_addr_t [L1NumWrapper-1:0][L1BankPerWP-1:0]      l1_cache_wp_addr;
  data_t           [L1NumWrapper-1:0][L1BankPerWP-1:0]      l1_cache_wp_wdata;
  strb_t           [L1NumWrapper-1:0][L1BankPerWP-1:0]      l1_cache_wp_be;
  data_t           [L1NumWrapper-1:0][L1BankPerWP-1:0]      l1_cache_wp_rdata;
  logic            [L1NumWrapper-1:0][L1BankPerWP-1:0]      l1_cache_wp_gnt;

  // Used to bridge `l1_data_bank*` and `l1_cache_wp*` signals
  logic            [L1NumDataBank-1:0] l1_data_bank_req_flat;
  logic            [L1NumDataBank-1:0] l1_data_bank_we_flat;
  tcdm_bank_addr_t [L1NumDataBank-1:0] l1_data_bank_addr_flat;
  data_t           [L1NumDataBank-1:0] l1_data_bank_wdata_flat;
  logic            [L1NumDataBank-1:0] l1_data_bank_be_flat;
  data_t           [L1NumDataBank-1:0] l1_data_bank_rdata_flat;
  logic            [L1NumDataBank-1:0] l1_data_bank_gnt_flat;

  // Requests/Response to/from outside DRAM from cache controllers
  axi_mst_dma_req_t  [NumL1CacheCtrl-1:0] wide_dcache_mst_req;
  axi_mst_dma_resp_t [NumL1CacheCtrl-1:0] wide_dcache_mst_rsp;

  logic                   l1d_insn_valid;
  logic [NumL1CacheCtrl-1:0] l1d_insn_ready;
  logic [1:0]             l1d_insn;
  tcdm_bank_addr_t        cfg_spm_size;
  tcdm_addr_t             spm_size;
  logic                   l1d_busy;

  // High if a port access an illegal SPM region (mapped to cache)
  logic [NrTCDMPortsCores-1:0] spm_error;


  // 9. SRAM Configuration
  impl_in_t [L1NumWrapper-1:0][L1BankPerWP-1:0] impl_l1d_data;
  impl_in_t [L1NumTagBank-1:0]                  impl_l1d_tag;
  impl_in_t [1:0]                               impl_l1d_fifo;

  impl_in_t [ICacheSets-1:0] impl_l1i_data;
  impl_in_t [ICacheSets-1:0] impl_l1i_tag;

  assign {impl_l1d_data, impl_l1d_tag, impl_l1d_fifo, impl_l1i_data, impl_l1i_tag} = impl_i;
  assign error_o = |spm_error;


  // -------------
  // DMA Subsystem
  // -------------
  // Optionally decouple the external wide AXI master port.
  axi_cut #(
    .Bypass     (!RegisterExt         ),
    .aw_chan_t  (axi_slv_dma_aw_chan_t),
    .w_chan_t   (axi_slv_dma_w_chan_t ),
    .b_chan_t   (axi_slv_dma_b_chan_t ),
    .ar_chan_t  (axi_slv_dma_ar_chan_t),
    .r_chan_t   (axi_slv_dma_r_chan_t ),
    .axi_req_t  (axi_slv_dma_req_t    ),
    .axi_resp_t (axi_slv_dma_resp_t   )
  ) i_cut_ext_wide_out (
    .clk_i      (clk_i                      ),
    .rst_ni     (rst_ni                     ),
    .slv_req_i  (wide_axi_slv_req[SoCDMAOut]),
    .slv_resp_o (wide_axi_slv_rsp[SoCDMAOut]),
    .mst_req_o  (axi_out_req_o              ),
    .mst_resp_i (axi_out_resp_i             )
  );

  axi_cut #(
    .Bypass     (!RegisterExt         ),
    .aw_chan_t  (axi_slv_dma_aw_chan_t),
    .w_chan_t   (axi_slv_dma_w_chan_t ),
    .b_chan_t   (axi_slv_dma_b_chan_t ),
    .ar_chan_t  (axi_slv_dma_ar_chan_t),
    .r_chan_t   (axi_slv_dma_r_chan_t ),
    .axi_req_t  (axi_slv_dma_req_t    ),
    .axi_resp_t (axi_slv_dma_resp_t   )
  ) i_cut_ext_l2_wide_out (
    .clk_i      (clk_i                  ),
    .rst_ni     (rst_ni                 ),
    .slv_req_i  (wide_axi_slv_req[L2Mem]),
    .slv_resp_o (wide_axi_slv_rsp[L2Mem]),
    .mst_req_o  (axi_out_l2_req_o       ),
    .mst_resp_i (axi_out_l2_resp_i      )
  );

  axi_cut #(
    .Bypass     (!RegisterExt     ),
    .aw_chan_t  (axi_mst_aw_chan_t),
    .w_chan_t   (axi_mst_w_chan_t ),
    .b_chan_t   (axi_mst_b_chan_t ),
    .ar_chan_t  (axi_mst_ar_chan_t),
    .r_chan_t   (axi_mst_r_chan_t ),
    .axi_req_t  (axi_mst_req_t    ),
    .axi_resp_t (axi_mst_resp_t   )
  ) i_cut_ext_narrow_in (
    .clk_i      (clk_i                       ),
    .rst_ni     (rst_ni                      ),
    .slv_req_i  (axi_in_req_i                ),
    .slv_resp_o (axi_in_resp_o               ),
    .mst_req_o  (narrow_axi_mst_req[SoCDMAIn]),
    .mst_resp_i (narrow_axi_mst_rsp[SoCDMAIn])
  );

  logic       [DmaXbarCfg.NoSlvPorts-1:0][$clog2(DmaXbarCfg.NoMstPorts)-1:0] dma_xbar_default_port;
  xbar_rule_t [DmaXbarCfg.NoAddrRules-1:0]                                   dma_xbar_rule;

  assign dma_xbar_default_port = '{default: SoCDMAOut};
  assign dma_xbar_rule         = '{
    '{
      idx       : TCDMDMA,
      start_addr: tcdm_start_address,
      end_addr  : tcdm_end_address
    },
    '{
      idx       : BootROM,
      start_addr: BootAddr,
      end_addr  : BootAddr + 'h1000
    },
    '{
      idx       : L2Mem,
      start_addr: cluster_l2_start_address,
      end_addr  : cluster_l2_end_address
    }
  };

  localparam bit [DmaXbarCfg.NoSlvPorts-1:0] DMAEnableDefaultMstPort = '1;
  axi_xbar #(
    .Cfg           (DmaXbarCfg           ),
    .ATOPs         (0                    ),
    .slv_aw_chan_t (axi_mst_dma_aw_chan_t),
    .mst_aw_chan_t (axi_slv_dma_aw_chan_t),
    .w_chan_t      (axi_mst_dma_w_chan_t ),
    .slv_b_chan_t  (axi_mst_dma_b_chan_t ),
    .mst_b_chan_t  (axi_slv_dma_b_chan_t ),
    .slv_ar_chan_t (axi_mst_dma_ar_chan_t),
    .mst_ar_chan_t (axi_slv_dma_ar_chan_t),
    .slv_r_chan_t  (axi_mst_dma_r_chan_t ),
    .mst_r_chan_t  (axi_slv_dma_r_chan_t ),
    .slv_req_t     (axi_mst_dma_req_t    ),
    .slv_resp_t    (axi_mst_dma_resp_t   ),
    .mst_req_t     (axi_slv_dma_req_t    ),
    .mst_resp_t    (axi_slv_dma_resp_t   ),
    .rule_t        (xbar_rule_t          )
  ) i_axi_dma_xbar (
    .clk_i                 (clk_i                  ),
    .rst_ni                (rst_ni                 ),
    .test_i                (1'b0                   ),
    .slv_ports_req_i       (wide_axi_mst_req       ),
    .slv_ports_resp_o      (wide_axi_mst_rsp       ),
    .mst_ports_req_o       (wide_axi_slv_req       ),
    .mst_ports_resp_i      (wide_axi_slv_rsp       ),
    .addr_map_i            (dma_xbar_rule          ),
    .en_default_mst_port_i (DMAEnableDefaultMstPort),
    .default_mst_port_i    (dma_xbar_default_port  )
  );

  addr_t ext_dma_req_q_addr_nontrunc;

  axi_to_mem_interleaved #(
    .axi_req_t  (axi_slv_dma_req_t     ),
    .axi_resp_t (axi_slv_dma_resp_t    ),
    .AddrWidth  (AxiAddrWidth          ),
    .DataWidth  (AxiDataWidth          ),
    .IdWidth    (WideIdWidthOut        ),
    .NumBanks   (1                     ),
    .BufDepth   (MemoryMacroLatency + 1)
  ) i_axi_to_mem_dma (
    .clk_i        (clk_i                                 ),
    .rst_ni       (rst_ni                                ),
    .busy_o       (/* Unused */                          ),
    .test_i       (1'b0                                  ),
    .axi_req_i    (wide_axi_slv_req[TCDMDMA]             ),
    .axi_resp_o   (wide_axi_slv_rsp[TCDMDMA]             ),
    .mem_req_o    (ext_dma_req.q_valid                   ),
    .mem_gnt_i    (ext_dma_rsp.q_ready                   ),
    .mem_addr_o   (ext_dma_req_q_addr_nontrunc           ),
    .mem_wdata_o  (ext_dma_req.q.data                    ),
    .mem_strb_o   (ext_dma_req.q.strb                    ),
    .mem_atop_o   (/* The DMA does not support atomics */),
    .mem_we_o     (ext_dma_req.q.write                   ),
    .mem_rvalid_i (ext_dma_rsp.p_valid                   ),
    .mem_rdata_i  (ext_dma_rsp.p.data                    )
  );

  assign ext_dma_req.q.addr = tcdm_addr_t'(ext_dma_req_q_addr_nontrunc);
  assign ext_dma_req.q.amo  = reqrsp_pkg::AMONone;
  assign ext_dma_req.q.user = '0;

  spatz_tcdm_interconnect #(
    .NumInp                (1                 ),
    .NumOut                (NrSuperBanks      ),
    .tcdm_req_t            (tcdm_dma_req_t    ),
    .tcdm_rsp_t            (tcdm_dma_rsp_t    ),
    .mem_req_t             (mem_dma_req_t     ),
    .mem_rsp_t             (mem_dma_rsp_t     ),
    .user_t                (logic             ),
    .MemAddrWidth          (TCDMMemAddrWidth  ),
    .DataWidth             (AxiDataWidth      ),
    .MemoryResponseLatency (MemoryMacroLatency)
  ) i_dma_interconnect (
    .clk_i     (clk_i      ),
    .rst_ni    (rst_ni     ),
    .req_i     (ext_dma_req),
    .rsp_o     (ext_dma_rsp),
    .mem_req_o (sb_dma_req ),
    .mem_rsp_i (sb_dma_rsp )
  );

  // ----------------
  // Memory Subsystem
  // ----------------
  for (genvar i = 0; i < NrSuperBanks; i++) begin : gen_tcdm_super_bank

    mem_req_t [BanksPerSuperBank-1:0] amo_req;
    mem_rsp_t [BanksPerSuperBank-1:0] amo_rsp;

    logic           [BanksPerSuperBank-1:0] mem_cs, mem_wen;
    tcdm_mem_addr_t [BanksPerSuperBank-1:0] mem_add;
    tcdm_mem_addr_t [BanksPerSuperBank-1:0] mem_add_max;
    strb_t          [BanksPerSuperBank-1:0] mem_be;
    data_t          [BanksPerSuperBank-1:0] mem_rdata, mem_wdata;
    tcdm_meta_t     [BanksPerSuperBank-1:0] bank_req_meta, mem_req_meta, bank_rsp_meta;

    mem_wide_narrow_mux #(
      .NarrowDataWidth  (NarrowDataWidth),
      .WideDataWidth    (AxiDataWidth   ),
      .mem_narrow_req_t (mem_req_t      ),
      .mem_narrow_rsp_t (mem_rsp_t      ),
      .mem_wide_req_t   (mem_dma_req_t  ),
      .mem_wide_rsp_t   (mem_dma_rsp_t  )
    ) i_tcdm_mux (
      .clk_i           (clk_i                ),
      .rst_ni          (rst_ni               ),
      .in_narrow_req_i (ic_req [i]           ),
      .in_narrow_rsp_o (ic_rsp [i]           ),
      .in_wide_req_i   (sb_dma_req [i]       ),
      .in_wide_rsp_o   (sb_dma_rsp [i]       ),
      .out_req_o       (amo_req              ),
      .out_rsp_i       (amo_rsp              ),
      .sel_wide_i      (sb_dma_req[i].q_valid)
    );

    // generate banks of the superbank
    for (genvar j = 0; j < BanksPerSuperBank; j++) begin : gen_tcdm_bank
       tc_sram_impl #(
        .NumWords  (TCDMDepth),
        .DataWidth (DataWidth),
        .ByteWidth (8        ),
        .NumPorts  (1        ),
        .Latency   (1        )
      ) i_spm_mem (
        .clk_i   (clk_i       ),
        .rst_ni  (rst_ni      ),
        .impl_i  ('0          ),
        .impl_o  (/* Unused */),
        .req_i   (mem_cs[j]      ),
        .we_i    (mem_wen[j]     ),
        .addr_i  (mem_add[j]     ),
        .wdata_i (mem_wdata[j]   ),
        .be_i    (mem_be[j]      ),
        .rdata_o (mem_rdata[j]   )
      );

      data_t amo_rdata_local;

      // TODO(zarubaf): Share atomic units between mutltiple cuts
      snitch_amo_shim #(
        .AddrMemWidth ( TCDMMemAddrWidth ),
        .DataWidth    ( DataWidth        ),
        .CoreIDWidth  ( CoreIDWidth      )
      ) i_amo_shim (
        .clk_i          (clk_i                     ),
        .rst_ni         (rst_ni                    ),
        .valid_i        (amo_req[j].q_valid        ),
        .ready_o        (amo_rsp[j].q_ready        ),
        .addr_i         (amo_req[j].q.addr         ),
        .write_i        (amo_req[j].q.write        ),
        .wdata_i        (amo_req[j].q.data         ),
        .wstrb_i        (amo_req[j].q.strb         ),
        .core_id_i      (amo_req[j].q.user.core_id ),
        .is_core_i      (amo_req[j].q.user.is_core ),
        .rdata_o        (amo_rdata_local           ),
        .amo_i          (amo_req[j].q.amo          ),
        .mem_req_o      (mem_cs[j]                 ),
        .mem_add_o      (mem_add[j]                ),
        .mem_wen_o      (mem_wen[j]                ),
        .mem_wdata_o    (mem_wdata[j]              ),
        .mem_be_o       (mem_be[j]                 ),
        .mem_rdata_i    (mem_rdata[j]              ),
        .dma_access_i   (sb_dma_req[i].q_valid     ),
        // TODO(zarubaf): Signal AMO conflict somewhere. Socregs?
        .amo_conflict_o (/* Unused */              )
      );

      // Insert a pipeline register at the output of each SRAM.
      shift_reg #(
        .dtype(data_t                ),
        .Depth(int'(RegisterTCDMCuts))
      ) i_sram_pipe (
        .clk_i (clk_i            ),
        .rst_ni(rst_ni           ),
        .d_i   (amo_rdata_local  ),
        .d_o   (amo_rsp[j].p.data)
      );

      // the meta data information
      assign bank_req_meta[j] = '{
        user:  amo_req[j].q.user,
        write: amo_req[j].q.write,
        default: '0
      };
      assign amo_rsp[j].p.user  = bank_rsp_meta[j].user;
      assign amo_rsp[j].p.write = bank_rsp_meta[j].write;

      shift_reg #(
        .dtype(tcdm_meta_t           ),
        .Depth(int'(RegisterTCDMCuts))
      ) i_req_meta_pipe (
        .clk_i (clk_i            ),
        .rst_ni(rst_ni           ),
        .d_i   (bank_req_meta[j] ),
        .d_o   (mem_req_meta[j]  )
      );
      shift_reg #(
        .dtype(tcdm_meta_t           ),
        .Depth(int'(RegisterTCDMCuts))
      ) i_rsp_meta_pipe (
        .clk_i (clk_i            ),
        .rst_ni(rst_ni           ),
        .d_i   (mem_req_meta[j]  ),
        .d_o   (bank_rsp_meta[j] )
      );
    end
  end

  logic  [NrTCDMPortsCores-1:0] unmerge_pready, strb_hdl_pready, cache_pready;
  assign spm_size        = cfg_spm_size * 1024;

  // split the requests for spm or cache from core side
  spatz_addr_mapper #(
    .NumIO          (NrTCDMPortsCores ),
    .AddrWidth      (L1AddrWidth      ),
    .SPMAddrWidth   (SPMAddrWidth     ),
    .DataWidth      (DataWidth        ),
    .mem_req_t      (tcdm_req_t       ),
    .mem_rsp_t      (tcdm_rsp_t       ),
    .mem_rsp_chan_t (tcdm_rsp_chan_t  ),
    .spm_req_t      (spm_req_t        ),
    .spm_rsp_t      (spm_rsp_t        )
  ) i_tcdm_mapper (
    .clk_i                (clk_i           ),
    .rst_ni               (rst_ni          ),
    // Input
    .mem_req_i            (tcdm_req        ),
    .mem_rsp_o            (tcdm_rsp        ),
    .error_o              (spm_error       ),
    // Address
    .tcdm_start_address_i (tcdm_start_address[L1AddrWidth-1:0] ),
    .tcdm_end_address_i   (tcdm_end_address[L1AddrWidth-1:0]   ),
    .spm_size_i           (spm_size        ),
    .flush_i              (l1d_busy        ),
    // Output
    .spm_req_o            (spm_req         ),
    .spm_rsp_i            (spm_rsp         ),
    .cache_req_o          (unmerge_req     ),
    .cache_pready_o       (unmerge_pready  ),
    .cache_rsp_i          (unmerge_rsp     )
  );

  spatz_strbreq_merge_tree #(
    .NumIO              (NrTCDMPortsCores             ),
    .NumOutstandingMem  (NumSpatzOutstandingLoads[0]  ),
    .MergeNum           (NrTCDMPortsCores - NrCores   ),
    .DataWidth          (DataWidth                    ),
    .mem_req_t          (tcdm_req_t                   ),
    .mem_rsp_t          (tcdm_rsp_t                   ),
    .req_id_t           (reqid_t                      ),
    .tcdm_user_t        (tcdm_user_t                  )
  ) i_strbreq_merge_tree (
    .clk_i           (clk_i),
    .rst_ni          (rst_ni),
    .unmerge_req_i   (unmerge_req       ),
    .unmerge_pready_i(unmerge_pready    ),
    .merge_rsp_i     (strb_hdl_rsp      ),
    .merge_req_o     (strb_hdl_req      ),
    .merge_pready_o  (strb_hdl_pready   ),
    .unmerge_rsp_o   (unmerge_rsp       )
  );

  for (genvar j = 0; j < NrTCDMPortsCores; j++) begin: gen_strb_hdlr
    spatz_strbreq_handler #(
      .DataWidth      (DataWidth    ),
      .mem_req_t      (tcdm_req_t   ),
      .mem_rsp_t      (tcdm_rsp_t   ),
      .reqrsp_user_t  (tcdm_user_t  )
    ) i_strbreq_handler (
      .clk_i            (clk_i              ),
      .rst_ni           (rst_ni             ),
      .strb_req_i       (strb_hdl_req[j]    ),
      .strb_rsp_ready_i (strb_hdl_pready[j] ),
      .strb_rsp_i       (cache_rsp[j]       ),
      .strb_req_o       (cache_req[j]       ),
      .strb_rsp_ready_o (cache_pready[j]    ),
      .strb_rsp_o       (strb_hdl_rsp[j]    )
    );
  end

  /// Wire requests after strb handling to the cache controller
  // Hong TODO: Now we hardwire ports to controller ports one to one.
  //            But later, we need to distribute these requests to the corresponding controllers based on
  //            partition id (or address range).
  for (genvar i = 0; i < NumL1CacheCtrl; i++) begin
    for (genvar j = 0; j < NrTCDMPortsPerCore; j++) begin
      assign cache_req_valid[i][j] = cache_req[i*NrTCDMPortsPerCore+j].q_valid;
      assign cache_req_addr[i][j]  = cache_req[i*NrTCDMPortsPerCore+j].q.addr;
      assign cache_req_meta[i][j]  = cache_req[i*NrTCDMPortsPerCore+j].q.user;
      assign cache_req_write[i][j] = cache_req[i*NrTCDMPortsPerCore+j].q.write;
      assign cache_req_data[i][j]  = cache_req[i*NrTCDMPortsPerCore+j].q.data;

      assign cache_rsp_ready[i][j]   = cache_pready[i*NrTCDMPortsPerCore+j];
      assign cache_rsp[i*NrTCDMPortsPerCore+j].p_valid = cache_rsp_valid[i][j];
      assign cache_rsp[i*NrTCDMPortsPerCore+j].q_ready = cache_req_ready[i][j];
      assign cache_rsp[i*NrTCDMPortsPerCore+j].p.data  = cache_rsp_data[i][j];
      assign cache_rsp[i*NrTCDMPortsPerCore+j].p.user  = cache_rsp_meta[i][j];

      assign cache_rsp[i*NrTCDMPortsPerCore+j].p.write = cache_rsp_write[i][j];
    end
  end

  tcdm_bank_addr_t num_spm_lines;
  assign num_spm_lines = cfg_spm_size * (DepthPerBank / L1Size);

  for (genvar cb = 0; cb < NumL1CacheCtrl; cb++) begin: gen_l1_cache_ctrl
    flamingo_spatz_cache_ctrl #(
      // Core
      .NumPorts         (NrTCDMPortsPerCore ),
      .CoalExtFactor    (L1CoalFactor      ),
      .AddrWidth        (L1AddrWidth       ),
      .WordWidth        (DataWidth         ),
      // Cache
      .NumCacheEntry    (L1NumEntryPerCtrl ),
      .CacheLineWidth   (L1LineWidth       ),
      .SetAssociativity (L1AssoPerCtrl     ),
      .BankFactor       (L1BankFactor      ),
      // Type
      .core_meta_t      (tcdm_user_t       ),
      .impl_in_t        (impl_in_t         ),
      .axi_req_t        (axi_dcache_req_t  ),
      .axi_resp_t       (axi_dcache_resp_t )
    ) i_l1_controller (
      .clk_i                 (clk_i                    ),
      .rst_ni                (rst_ni                   ),
      .impl_i                (impl_l1d_fifo            ),
      // Sync Control
      .cache_sync_valid_i    (l1d_insn_valid           ),
      .cache_sync_ready_o    (l1d_insn_ready[cb]        ),
      .cache_sync_insn_i     (l1d_insn                 ),
      // SPM Size
      // The calculation of spm region in cache is different
      // than other modules (needs to times 2)
      .bank_depth_for_SPM_i  (num_spm_lines            ),
      // Request
      .core_req_valid_i      (cache_req_valid[cb]          ),
      .core_req_ready_o      (cache_req_ready[cb]          ),
      .core_req_addr_i       (cache_req_addr[cb]           ),
      .core_req_meta_i       (cache_req_meta[cb]           ),
      .core_req_write_i      (cache_req_write[cb]          ),
      .core_req_wdata_i      (cache_req_data[cb]           ),
      // Response
      .core_resp_valid_o     (cache_rsp_valid[cb]          ),
      .core_resp_ready_i     (cache_rsp_ready[cb]          ),
      .core_resp_write_o     (cache_rsp_write[cb]          ),
      .core_resp_data_o      (cache_rsp_data[cb]           ),
      .core_resp_meta_o      (cache_rsp_meta[cb]           ),
      // AXI refill
      .axi_req_o             (dcache_axi_req[cb] ),
      .axi_resp_i            (dcache_axi_rsp[cb] ),
      // .axi_resp_i            (wide_axi_mst_rsp[DCache] ),
      // Tag Banks
      .tcdm_tag_bank_req_o   (l1_tag_bank_req[cb]          ),
      .tcdm_tag_bank_we_o    (l1_tag_bank_we[cb]           ),
      .tcdm_tag_bank_addr_o  (l1_tag_bank_addr[cb]         ),
      .tcdm_tag_bank_wdata_o (l1_tag_bank_wdata[cb]        ),
      .tcdm_tag_bank_be_o    (l1_tag_bank_be[cb]           ),
      .tcdm_tag_bank_rdata_i (l1_tag_bank_rdata[cb]        ),
      // Data Banks
      .tcdm_data_bank_req_o  (l1_data_bank_req[cb]         ),
      .tcdm_data_bank_we_o   (l1_data_bank_we[cb]          ),
      .tcdm_data_bank_addr_o (l1_data_bank_addr[cb]        ),
      .tcdm_data_bank_wdata_o(l1_data_bank_wdata[cb]       ),
      .tcdm_data_bank_be_o   (l1_data_bank_be[cb]          ),
      .tcdm_data_bank_rdata_i(l1_data_bank_rdata[cb]       ),
      .tcdm_data_bank_gnt_i  (l1_data_bank_gnt[cb]         )
    );

    for (genvar j = 0; j < NumTagBankPerCtrl; j++) begin
      tc_sram_impl #(
        .NumWords  (L1CacheWayEntry/L1BankFactor),
        .DataWidth ($bits(tag_data_t)               ),
        .ByteWidth ($bits(tag_data_t)               ),
        .NumPorts  (1                           ),
        .Latency   (1                           ),
        .SimInit   ("zeros"                     ),
        .impl_in_t (impl_in_t                   )
      ) i_meta_bank (
        .clk_i  (clk_i               ),
        .rst_ni (rst_ni              ),
        .impl_i (impl_l1d_tag     [cb*NumL1CacheCtrl+j]),
        .impl_o (/* unsed */         ),
        .req_i  (l1_tag_bank_req  [cb][j]),
        .we_i   (l1_tag_bank_we   [cb][j]),
        .addr_i (l1_tag_bank_addr [cb][j]),
        .wdata_i(l1_tag_bank_wdata[cb][j]),
        .be_i   (l1_tag_bank_be   [cb][j]),
        .rdata_o(l1_tag_bank_rdata[cb][j])
      );
    end

    for (genvar j = 0; j < NumDataBankPerCtrl; j++) begin : gen_l1_data_banks
      tc_sram_impl #(
        .NumWords   (L1CacheWayEntry/L1BankFactor),
        .DataWidth  (DataWidth),
        .ByteWidth  (DataWidth),
        .NumPorts   (1),
        .Latency    (1),
        .SimInit    ("zeros")
      ) i_data_bank (
        .clk_i  (clk_i                   ),
        .rst_ni (rst_ni                  ),
        .impl_i ('0                      ),
        .impl_o (/* unsed */             ),
        .req_i  (l1_data_bank_req  [cb][j]),
        .we_i   (l1_data_bank_we   [cb][j]),
        .addr_i (l1_data_bank_addr [cb][j]),
        .wdata_i(l1_data_bank_wdata[cb][j]),
        .be_i   (l1_data_bank_be   [cb][j]),
        .rdata_o(l1_data_bank_rdata[cb][j])
      );

      assign l1_data_bank_gnt[cb][j] = 1'b1;
    end

    // // Wire cache requests from controller ports to data banks
    // for (genvar j = 0; j < NumDataBankPerCtrl; j++) begin
    //   assign l1_data_bank_req_flat  [i*NumDataBankPerCtrl+j]  = l1_data_bank_req  [i][j];
    //   assign l1_data_bank_we_flat   [i*NumDataBankPerCtrl+j]  = l1_data_bank_we   [i][j];
    //   assign l1_data_bank_addr_flat [i*NumDataBankPerCtrl+j]  = l1_data_bank_addr [i][j];
    //   assign l1_data_bank_wdata_flat[i*NumDataBankPerCtrl+j]  = l1_data_bank_wdata[i][j];
    //   assign l1_data_bank_be_flat   [i*NumDataBankPerCtrl+j]  = l1_data_bank_be   [i][j];

    //   assign l1_data_bank_rdata[i][j] = l1_data_bank_rdata_flat [i*NumDataBankPerCtrl+j];
    //   assign l1_data_bank_gnt[i][j]   = l1_data_bank_gnt_flat[i*NumDataBankPerCtrl+j];
    // end
  end

  // for (genvar i = 0; i < NrSuperBanks; i++) begin
  //   // 8 Bank Wraps per SuperBank
  //   for (genvar j = 0; j < BanksPerSuperBank; j++) begin
  //     // 8 Banks per Bank Wrap
  //     for (genvar k = 0; k < L1BankPerWP; k++) begin
  //       // [i*8+j][k]         => [0][0], [0][1], [0][2]....
  //       // [i*8*4 + j*4 + k]  => [0],    [1]   , [2], ...
  //       assign l1_cache_wp_req   [i*BanksPerSuperBank+j][k] = l1_data_bank_req_flat   [i*BanksPerSuperBank*L1BankPerWP+j*L1BankPerWP+k];
  //       assign l1_cache_wp_we    [i*BanksPerSuperBank+j][k] = l1_data_bank_we_flat    [i*BanksPerSuperBank*L1BankPerWP+j*L1BankPerWP+k];
  //       assign l1_cache_wp_addr  [i*BanksPerSuperBank+j][k] = l1_data_bank_addr_flat  [i*BanksPerSuperBank*L1BankPerWP+j*L1BankPerWP+k];
  //       assign l1_cache_wp_wdata [i*BanksPerSuperBank+j][k] = l1_data_bank_wdata_flat [i*BanksPerSuperBank*L1BankPerWP+j*L1BankPerWP+k];
  //       // L1 Cache has a write granularity of an entire cache line
  //       assign l1_cache_wp_be    [i*BanksPerSuperBank+j][k] =
  //             (l1_data_bank_be_flat[i*BanksPerSuperBank*L1BankPerWP+j*L1BankPerWP+k]) ? {(NarrowDataWidth/8){1'b1}} : '0;

  //       assign l1_data_bank_rdata_flat[i*BanksPerSuperBank*L1BankPerWP+j*L1BankPerWP+k] = l1_cache_wp_rdata[i*BanksPerSuperBank+j][k];
  //       assign l1_data_bank_gnt_flat  [i*BanksPerSuperBank*L1BankPerWP+j*L1BankPerWP+k] = l1_cache_wp_gnt  [i*BanksPerSuperBank+j][k];
  //     end
  //   end
  // end

  // Hong TODO: Now we multiplex axi requests from multiple cache controllers, 
  //            the id width is prepended to the existing id value to tell which 
  //            cache controller request is picked, later we can replace this if 
  //            we have some more appealing mux policy.
  axi_mux #(
    .SlvAxiIDWidth ( WideIdWidthIn-$clog2(NumL1CacheCtrl) ), // ID width of the slave ports
    .slv_aw_chan_t ( axi_dcache_aw_chan_t          ), // AW Channel Type, slave ports
    .mst_aw_chan_t ( axi_mst_dma_aw_chan_t          ), // AW Channel Type, master port
    .w_chan_t      ( axi_mst_dma_w_chan_t           ), //  W Channel Type, all ports
    .slv_b_chan_t  ( axi_dcache_b_chan_t           ), //  B Channel Type, slave ports
    .mst_b_chan_t  ( axi_mst_dma_b_chan_t           ), //  B Channel Type, master port
    .slv_ar_chan_t ( axi_dcache_ar_chan_t          ), // AR Channel Type, slave ports
    .mst_ar_chan_t ( axi_mst_dma_ar_chan_t          ), // AR Channel Type, master port
    .slv_r_chan_t  ( axi_dcache_r_chan_t           ), //  R Channel Type, slave ports
    .mst_r_chan_t  ( axi_mst_dma_r_chan_t           ), //  R Channel Type, master port
    .slv_req_t     ( axi_dcache_req_t              ),
    .slv_resp_t    ( axi_dcache_resp_t             ),
    .mst_req_t     ( axi_mst_dma_req_t              ),
    .mst_resp_t    ( axi_mst_dma_resp_t             ),
    .NoSlvPorts    ( NumL1CacheCtrl                 ) // Number of Masters for the module
  ) i_dcache_axi_mux (
    .clk_i       ( clk_i                    ),
    .rst_ni      ( rst_ni                   ),
    .test_i      ( 1'b0                     ),  // Test Mode enable
    .slv_reqs_i  ( dcache_axi_req           ),
    .slv_resps_o ( dcache_axi_rsp           ),
    .mst_req_o   ( wide_axi_mst_req[DCache] ),
    .mst_resp_i  ( wide_axi_mst_rsp[DCache] )
  );


  // We have multiple banks form a pesudo bank (BankWP)
  spatz_tcdm_interconnect #(
    .NumInp                (NumTCDMIn           ),
    .NumOut                (L1NumWrapper        ),
    .tcdm_req_t            (spm_req_t           ),
    .tcdm_rsp_t            (spm_rsp_t           ),
    .mem_req_t             (mem_req_t           ),
    .mem_rsp_t             (mem_rsp_t           ),
    .MemAddrWidth          (TCDMMemAddrWidth    ),
    .DataWidth             (DataWidth           ),
    .user_t                (tcdm_user_t         ),
    .MemoryResponseLatency (1 + RegisterTCDMCuts)
  ) i_tcdm_interconnect (
    .clk_i     (clk_i                  ),
    .rst_ni    (rst_ni                 ),
    .req_i     ({axi_soc_req, spm_req} ),
    .rsp_o     ({axi_soc_rsp, spm_rsp} ),
    .mem_req_o (ic_req                 ),
    .mem_rsp_i (ic_rsp                 )
  );

  hive_req_t [NrCores-1:0] hive_req;
  hive_rsp_t [NrCores-1:0] hive_rsp;

  for (genvar i = 0; i < NrCores; i++) begin : gen_core
    localparam int unsigned TcdmPorts     = get_tcdm_ports(i);
    localparam int unsigned TcdmPortsOffs = get_tcdm_port_offs(i);

    axi_mst_dma_req_t axi_dma_req;
    axi_mst_dma_resp_t axi_dma_res;
    interrupts_t irq;
    dma_events_t dma_core_events;

    sync #(.STAGES (2))
    i_sync_debug (.clk_i, .rst_ni, .serial_i (debug_req_i[i]), .serial_o (irq.debug));
    sync #(.STAGES (2))
    i_sync_meip (.clk_i, .rst_ni, .serial_i (meip_i[i]), .serial_o (irq.meip));
    sync #(.STAGES (2))
    i_sync_mtip (.clk_i, .rst_ni, .serial_i (mtip_i[i]), .serial_o (irq.mtip));
    sync #(.STAGES (2))
    i_sync_msip (.clk_i, .rst_ni, .serial_i (msip_i[i]), .serial_o (irq.msip));
    assign irq.mcip = cl_interrupt[i];

    tcdm_req_t [TcdmPorts-1:0] tcdm_req_wo_user;

    logic [31:0] hart_id;
    assign hart_id = hart_base_id_i + i;

    spatz_cc #(
      .BootAddr                (BootAddr                   ),
      .L2Addr                  (L2Addr                     ),
      .L2Size                  (L2Size                     ),
      .RVE                     (1'b0                       ),
      .RVF                     (RVF                        ),
      .RVD                     (RVD                        ),
      .RVV                     (RVV                        ),
      .Xdma                    (Xdma[i]                    ),
      .AddrWidth               (AxiAddrWidth               ),
      .DataWidth               (NarrowDataWidth            ),
      .UserWidth               (AxiUserWidth               ),
      .DMADataWidth            (AxiDataWidth               ),
      .DMAIdWidth              (AxiIdWidthIn               ),
      .SnitchPMACfg            (SnitchPMACfg               ),
      .DMAAxiReqFifoDepth      (DMAAxiReqFifoDepth         ),
      .DMAReqFifoDepth         (DMAReqFifoDepth            ),
      .dreq_t                  (reqrsp_req_t               ),
      .drsp_t                  (reqrsp_rsp_t               ),
      .tcdm_req_t              (tcdm_req_t                 ),
      .tcdm_req_chan_t         (tcdm_req_chan_t            ),
      .tcdm_rsp_t              (tcdm_rsp_t                 ),
      .tcdm_rsp_chan_t         (tcdm_rsp_chan_t            ),
      .axi_req_t               (axi_mst_dma_req_t          ),
      .axi_ar_chan_t           (axi_mst_dma_ar_chan_t      ),
      .axi_aw_chan_t           (axi_mst_dma_aw_chan_t      ),
      .axi_rsp_t               (axi_mst_dma_resp_t         ),
      .hive_req_t              (hive_req_t                 ),
      .hive_rsp_t              (hive_rsp_t                 ),
      .acc_issue_req_t         (acc_issue_req_t            ),
      .acc_issue_rsp_t         (acc_issue_rsp_t            ),
      .acc_rsp_t               (acc_rsp_t                  ),
      .dma_events_t            (dma_events_t               ),
      .dma_perf_t              (axi_dma_pkg::dma_perf_t    ),
      .XDivSqrt                (1'b0                       ),
      .XF16                    (1'b1                       ),
      .XF16ALT                 (1'b1                       ),
      .XF8                     (1'b1                       ),
      .XF8ALT                  (1'b1                       ),
      .IsoCrossing             (1'b0                       ),
      .NumIntOutstandingLoads  (NumIntOutstandingLoads[i]  ),
      .NumIntOutstandingMem    (NumIntOutstandingMem[i]    ),
      .NumSpatzOutstandingLoads(NumSpatzOutstandingLoads[i]),
      .FPUImplementation       (FPUImplementation[i]       ),
      .RegisterOffloadRsp      (RegisterOffloadRsp         ),
      .RegisterCoreReq         (RegisterCoreReq            ),
      .RegisterCoreRsp         (RegisterCoreRsp            ),
      .NumSpatzFPUs            (NumSpatzFPUs               ),
      .NumSpatzIPUs            (NumSpatzIPUs               ),
      .TCDMAddrWidth           (SPMAddrWidth               )
    ) i_spatz_cc (
      .clk_i            (clk_i                               ),
      .clk_d2_i         (clk_i                               ),
      .rst_ni           (rst_ni                              ),
      .testmode_i       (1'b0                                ),
      .hart_id_i        (hart_id                             ),
      .hive_req_o       (hive_req[i]                         ),
      .hive_rsp_i       (hive_rsp[i]                         ),
      .irq_i            (irq                                 ),
      .data_req_o       (core_req[i]                         ),
      .data_rsp_i       (core_rsp[i]                         ),
      .tcdm_req_o       (tcdm_req_wo_user                    ),
      .tcdm_rsp_i       (tcdm_rsp[TcdmPortsOffs +: TcdmPorts]),
      .axi_dma_req_o    (axi_dma_req                         ),
      .axi_dma_res_i    (axi_dma_res                         ),
      .axi_dma_busy_o   (/* Unused */                        ),
      .axi_dma_perf_o   (/* Unused */                        ),
      .axi_dma_events_o (dma_core_events                     ),
      .core_events_o    (core_events[i]                      ),
      .tcdm_addr_base_i (tcdm_start_address                  )
    );
    for (genvar j = 0; j < TcdmPorts; j++) begin : gen_tcdm_user
      always_comb begin
        tcdm_req[TcdmPortsOffs+j].q              = tcdm_req_wo_user[j].q;
        tcdm_req[TcdmPortsOffs+j].q.user.core_id = i[CoreIDWidth-1:0];
        tcdm_req[TcdmPortsOffs+j].q.user.is_core = 1;
        tcdm_req[TcdmPortsOffs+j].q_valid        = tcdm_req_wo_user[j].q_valid;
      end
    end
    if (Xdma[i]) begin : gen_dma_connection
      assign wide_axi_mst_req[SDMAMst] = axi_dma_req;
      assign axi_dma_res               = wide_axi_mst_rsp[SDMAMst];
      assign dma_events                = dma_core_events;
    end else begin
      assign axi_dma_res = '0;
    end
  end

  // ----------------
  // Instruction Cache
  // ----------------

  addr_t [NrCores-1:0]       inst_addr;
  logic  [NrCores-1:0]       inst_cacheable;
  logic  [NrCores-1:0][31:0] inst_data;
  logic  [NrCores-1:0]       inst_valid;
  logic  [NrCores-1:0]       inst_ready;
  logic  [NrCores-1:0]       inst_error;
  logic  [NrCores-1:0]       flush_valid;
  logic  [NrCores-1:0]       flush_ready;

  for (genvar i = 0; i < NrCores; i++) begin : gen_unpack_icache
    assign inst_addr[i]      = hive_req[i].inst_addr;
    assign inst_cacheable[i] = hive_req[i].inst_cacheable;
    assign inst_valid[i]     = hive_req[i].inst_valid;
    assign flush_valid[i]    = hive_req[i].flush_i_valid;
    assign hive_rsp[i]       = '{
      inst_data    : inst_data[i],
      inst_ready   : inst_ready[i],
      inst_error   : inst_error[i],
      flush_i_ready: flush_ready[i],
      default      : '0
    };
  end

  snitch_icache #(
    .NR_FETCH_PORTS     ( NrCores                                            ),
    .L0_LINE_COUNT      ( 8                                                  ),
    .LINE_WIDTH         ( ICacheLineWidth                                    ),
    .LINE_COUNT         ( ICacheLineCount                                    ),
    .SET_COUNT          ( ICacheSets                                         ),
    .FETCH_AW           ( AxiAddrWidth                                       ),
    .FETCH_DW           ( 32                                                 ),
    .FILL_AW            ( AxiAddrWidth                                       ),
    .FILL_DW            ( AxiDataWidth                                       ),
    .EARLY_LATCH        ( 0                                                  ),
    .L0_EARLY_TAG_WIDTH ( snitch_pkg::PAGE_SHIFT - $clog2(ICacheLineWidth/8) ),
    .ISO_CROSSING       ( 1'b0                                               ),
    .axi_req_t          ( axi_mst_dma_req_t                                  ),
    .axi_rsp_t          ( axi_mst_dma_resp_t                                 ),
    .sram_cfg_data_t    ( impl_in_t                                          ),
    .sram_cfg_tag_t     ( impl_in_t                                          )
  ) i_snitch_icache (
    .clk_i                ( clk_i                    ),
    .clk_d2_i             ( clk_i                    ),
    .rst_ni               ( rst_ni                   ),
    .enable_prefetching_i ( icache_prefetch_enable   ),
    .icache_events_o      ( icache_events            ),
    .flush_valid_i        ( flush_valid              ),
    .flush_ready_o        ( flush_ready              ),
    .inst_addr_i          ( inst_addr                ),
    .inst_cacheable_i     ( inst_cacheable           ),
    .inst_data_o          ( inst_data                ),
    .inst_valid_i         ( inst_valid               ),
    .inst_ready_o         ( inst_ready               ),
    .inst_error_o         ( inst_error               ),
    .sram_cfg_tag_i       ( impl_l1i_tag             ),
    .sram_cfg_data_i      ( impl_l1i_data            ),
    .axi_req_o            ( wide_axi_mst_req[ICache] ),
    .axi_rsp_i            ( wide_axi_mst_rsp[ICache] )
  );

  // --------
  // Cores SoC
  // --------
  spatz_barrier #(
    .AddrWidth (AxiAddrWidth ),
    .NrPorts   (NrCores      ),
    .dreq_t    (reqrsp_req_t ),
    .drsp_t    (reqrsp_rsp_t )
  ) i_snitch_barrier (
    .clk_i                          (clk_i                       ),
    .rst_ni                         (rst_ni                      ),
    .in_req_i                       (core_req                    ),
    .in_rsp_o                       (core_rsp                    ),
    .out_req_o                      (filtered_core_req           ),
    .out_rsp_i                      (filtered_core_rsp           ),
    .cluster_periph_start_address_i (cluster_periph_start_address)
  );

  reqrsp_req_t core_to_axi_req;
  reqrsp_rsp_t core_to_axi_rsp;
  user_t       cluster_user;
  // Atomic ID, needs to be unique ID of cluster
  // cluster_id + HartIdOffset + 1 (because 0 is for non-atomic masters)
  assign cluster_user = (hart_base_id_i / NrCores) + (hart_base_id_i % NrCores) + 1'b1;

  reqrsp_mux #(
    .NrPorts   (NrCores         ),
    .AddrWidth (AxiAddrWidth    ),
    .DataWidth (NarrowDataWidth ),
    .req_t     (reqrsp_req_t    ),
    .rsp_t     (reqrsp_rsp_t    ),
    .RespDepth (2               )
  ) i_reqrsp_mux_core (
    .clk_i     (clk_i            ),
    .rst_ni    (rst_ni           ),
    .slv_req_i (filtered_core_req),
    .slv_rsp_o (filtered_core_rsp),
    .mst_req_o (core_to_axi_req  ),
    .mst_rsp_i (core_to_axi_rsp  ),
    .idx_o     (/*unused*/       )
  );

  reqrsp_to_axi #(
    .DataWidth    (NarrowDataWidth),
    .UserWidth    (NarrowUserWidth),
    .reqrsp_req_t (reqrsp_req_t   ),
    .reqrsp_rsp_t (reqrsp_rsp_t   ),
    .axi_req_t    (axi_mst_req_t  ),
    .axi_rsp_t    (axi_mst_resp_t )
  ) i_reqrsp_to_axi_core (
    .clk_i        (clk_i                      ),
    .rst_ni       (rst_ni                     ),
    .user_i       (cluster_user               ),
    .reqrsp_req_i (core_to_axi_req            ),
    .reqrsp_rsp_o (core_to_axi_rsp            ),
    .axi_req_o    (narrow_axi_mst_req[CoreReq]),
    .axi_rsp_i    (narrow_axi_mst_rsp[CoreReq])
  );

  xbar_rule_t [NrNarrowRules-1:0] cluster_xbar_rules;

  assign cluster_xbar_rules = '{
    '{
      idx       : TCDM,
      start_addr: tcdm_start_address,
      end_addr  : tcdm_end_address
    },
    '{
      idx       : ClusterPeripherals,
      start_addr: cluster_periph_start_address,
      end_addr  : cluster_periph_end_address
    }
  };

  localparam bit   [ClusterXbarCfg.NoSlvPorts-1:0]                                                        ClusterEnableDefaultMstPort = '1;
  localparam logic [ClusterXbarCfg.NoSlvPorts-1:0][cf_math_pkg::idx_width(ClusterXbarCfg.NoMstPorts)-1:0] ClusterXbarDefaultPort      = '{default: SoC};

  axi_xbar #(
    .Cfg           (ClusterXbarCfg   ),
    .slv_aw_chan_t (axi_mst_aw_chan_t),
    .mst_aw_chan_t (axi_slv_aw_chan_t),
    .w_chan_t      (axi_mst_w_chan_t ),
    .slv_b_chan_t  (axi_mst_b_chan_t ),
    .mst_b_chan_t  (axi_slv_b_chan_t ),
    .slv_ar_chan_t (axi_mst_ar_chan_t),
    .mst_ar_chan_t (axi_slv_ar_chan_t),
    .slv_r_chan_t  (axi_mst_r_chan_t ),
    .mst_r_chan_t  (axi_slv_r_chan_t ),
    .slv_req_t     (axi_mst_req_t    ),
    .slv_resp_t    (axi_mst_resp_t   ),
    .mst_req_t     (axi_slv_req_t    ),
    .mst_resp_t    (axi_slv_resp_t   ),
    .rule_t        (xbar_rule_t      )
  ) i_cluster_xbar (
    .clk_i                 (clk_i                      ),
    .rst_ni                (rst_ni                     ),
    .test_i                (1'b0                       ),
    .slv_ports_req_i       (narrow_axi_mst_req         ),
    .slv_ports_resp_o      (narrow_axi_mst_rsp         ),
    .mst_ports_req_o       (narrow_axi_slv_req         ),
    .mst_ports_resp_i      (narrow_axi_slv_rsp         ),
    .addr_map_i            (cluster_xbar_rules         ),
    .en_default_mst_port_i (ClusterEnableDefaultMstPort),
    .default_mst_port_i    (ClusterXbarDefaultPort     )
  );

  // ---------
  // Slaves
  // ---------
  // 1. TCDM
  // Add an adapter that allows access from AXI to the TCDM.
  axi_to_tcdm #(
    .axi_req_t  (axi_slv_req_t         ),
    .axi_rsp_t  (axi_slv_resp_t        ),
    .tcdm_req_t (spm_req_t             ),
    .tcdm_rsp_t (spm_rsp_t             ),
    .AddrWidth  (AxiAddrWidth          ),
    .DataWidth  (NarrowDataWidth       ),
    .IdWidth    (NarrowIdWidthOut      ),
    .BufDepth   (MemoryMacroLatency + 1)
  ) i_axi_to_tcdm (
    .clk_i      (clk_i                   ),
    .rst_ni     (rst_ni                  ),
    .axi_req_i  (narrow_axi_slv_req[TCDM]),
    .axi_rsp_o  (narrow_axi_slv_rsp[TCDM]),
    .tcdm_req_o (axi_soc_req             ),
    .tcdm_rsp_i (axi_soc_rsp             )
  );

  // 2. Peripherals
  axi_to_reg #(
    .ADDR_WIDTH         (AxiAddrWidth     ),
    .DATA_WIDTH         (NarrowDataWidth  ),
    .AXI_MAX_WRITE_TXNS (1                ),
    .AXI_MAX_READ_TXNS  (1                ),
    .DECOUPLE_W         (0                ),
    .ID_WIDTH           (NarrowIdWidthOut ),
    .USER_WIDTH         (NarrowUserWidth  ),
    .axi_req_t          (axi_slv_req_t    ),
    .axi_rsp_t          (axi_slv_resp_t   ),
    .reg_req_t          (reg_req_t        ),
    .reg_rsp_t          (reg_rsp_t        )
  ) i_axi_to_reg (
    .clk_i      (clk_i                                 ),
    .rst_ni     (rst_ni                                ),
    .testmode_i (1'b0                                  ),
    .axi_req_i  (narrow_axi_slv_req[ClusterPeripherals]),
    .axi_rsp_o  (narrow_axi_slv_rsp[ClusterPeripherals]),
    .reg_req_o  (reg_req                               ),
    .reg_rsp_i  (reg_rsp                               )
  );

  spatz_cluster_peripheral #(
    .AddrWidth     (AxiAddrWidth    ),
    .SPMWidth      ($clog2(L1NumSet)),
    .reg_req_t     (reg_req_t       ),
    .reg_rsp_t     (reg_rsp_t       ),
    .tcdm_events_t (tcdm_events_t   ),
    .dma_events_t  (dma_events_t    ),
    .NrCores       (NrCores         )
  ) i_snitch_cluster_peripheral (
    .clk_i                    (clk_i                 ),
    .rst_ni                   (rst_ni                ),
    .reg_req_i                (reg_req               ),
    .reg_rsp_o                (reg_rsp               ),
    /// The TCDM always starts at the cluster base.
    .tcdm_start_address_i     (tcdm_start_address    ),
    .tcdm_end_address_i       (tcdm_end_address      ),
    .icache_prefetch_enable_o (icache_prefetch_enable),
    .cl_clint_o               (cl_interrupt          ),
    .cluster_hart_base_id_i   (hart_base_id_i        ),
    .core_events_i            (core_events           ),
    .tcdm_events_i            (tcdm_events           ),
    .dma_events_i             (dma_events            ),
    .icache_events_i          (icache_events         ),
    .cluster_probe_o          (cluster_probe_o       ),
    .l1d_spm_size_o           (cfg_spm_size          ),
    .l1d_insn_o               (l1d_insn              ),
    .l1d_insn_valid_o         (l1d_insn_valid        ),
    .l1d_insn_ready_i         (&l1d_insn_ready       ),
    .l1d_busy_o               (l1d_busy              )
  );

  // 3. BootROM
  axi_to_reg #(
    .ADDR_WIDTH         (AxiAddrWidth      ),
    .DATA_WIDTH         (AxiDataWidth      ),
    .AXI_MAX_WRITE_TXNS (1                 ),
    .AXI_MAX_READ_TXNS  (1                 ),
    .DECOUPLE_W         (0                 ),
    .ID_WIDTH           (WideIdWidthOut    ),
    .USER_WIDTH         (AxiUserWidth      ),
    .axi_req_t          (axi_slv_dma_req_t ),
    .axi_rsp_t          (axi_slv_dma_resp_t),
    .reg_req_t          (reg_dma_req_t     ),
    .reg_rsp_t          (reg_dma_rsp_t     )
  ) i_axi_to_reg_bootrom (
    .clk_i      (clk_i                    ),
    .rst_ni     (rst_ni                   ),
    .testmode_i (1'b0                     ),
    .axi_req_i  (wide_axi_slv_req[BootROM]),
    .axi_rsp_o  (wide_axi_slv_rsp[BootROM]),
    .reg_req_o  (bootrom_reg_req          ),
    .reg_rsp_i  (bootrom_reg_rsp          )
  );

  bootrom i_bootrom (
    .clk_i  (clk_i                        ),
    .req_i  (bootrom_reg_req.valid        ),
    .addr_i (addr_t'(bootrom_reg_req.addr)),
    .rdata_o(bootrom_reg_rsp.rdata        )
  );
  `FF(bootrom_reg_rsp.ready, bootrom_reg_req.valid, 1'b0)
  assign bootrom_reg_rsp.error = 1'b0;

  // Upsize the narrow SoC connection
  `AXI_TYPEDEF_ALL(axi_mst_dma_narrow, addr_t, id_dma_mst_t, data_t, strb_t, user_t)
  axi_mst_dma_narrow_req_t  narrow_axi_slv_req_soc;
  axi_mst_dma_narrow_resp_t narrow_axi_slv_resp_soc;

  axi_iw_converter #(
    .AxiAddrWidth          (AxiAddrWidth             ),
    .AxiDataWidth          (NarrowDataWidth          ),
    .AxiUserWidth          (AxiUserWidth             ),
    .AxiSlvPortIdWidth     (NarrowIdWidthOut         ),
    .AxiSlvPortMaxUniqIds  (1                        ),
    .AxiSlvPortMaxTxnsPerId(1                        ),
    .AxiSlvPortMaxTxns     (1                        ),
    .AxiMstPortIdWidth     (WideIdWidthIn            ),
    .AxiMstPortMaxUniqIds  (1                        ),
    .AxiMstPortMaxTxnsPerId(1                        ),
    .slv_req_t             (axi_slv_req_t            ),
    .slv_resp_t            (axi_slv_resp_t           ),
    .mst_req_t             (axi_mst_dma_narrow_req_t ),
    .mst_resp_t            (axi_mst_dma_narrow_resp_t)
  ) i_soc_port_iw_convert (
    .clk_i      (clk_i                   ),
    .rst_ni     (rst_ni                  ),
    .slv_req_i  (narrow_axi_slv_req[SoC] ),
    .slv_resp_o (narrow_axi_slv_rsp[SoC] ),
    .mst_req_o  (narrow_axi_slv_req_soc  ),
    .mst_resp_i (narrow_axi_slv_resp_soc )
  );

  axi_dw_converter #(
    .AxiAddrWidth       (AxiAddrWidth               ),
    .AxiIdWidth         (WideIdWidthIn              ),
    .AxiMaxReads        (2                          ),
    .AxiSlvPortDataWidth(NarrowDataWidth            ),
    .AxiMstPortDataWidth(AxiDataWidth               ),
    .ar_chan_t          (axi_mst_dma_ar_chan_t      ),
    .aw_chan_t          (axi_mst_dma_aw_chan_t      ),
    .b_chan_t           (axi_mst_dma_b_chan_t       ),
    .slv_r_chan_t       (axi_mst_dma_narrow_r_chan_t),
    .slv_w_chan_t       (axi_mst_dma_narrow_b_chan_t),
    .axi_slv_req_t      (axi_mst_dma_narrow_req_t   ),
    .axi_slv_resp_t     (axi_mst_dma_narrow_resp_t  ),
    .mst_r_chan_t       (axi_mst_dma_r_chan_t       ),
    .mst_w_chan_t       (axi_mst_dma_w_chan_t       ),
    .axi_mst_req_t      (axi_mst_dma_req_t          ),
    .axi_mst_resp_t     (axi_mst_dma_resp_t         )
  ) i_soc_port_dw_upsize (
    .clk_i      (clk_i                        ),
    .rst_ni     (rst_ni                       ),
    .slv_req_i  (narrow_axi_slv_req_soc       ),
    .slv_resp_o (narrow_axi_slv_resp_soc      ),
    .mst_req_o  (wide_axi_mst_req[CoreReqWide]),
    .mst_resp_i (wide_axi_mst_rsp[CoreReqWide])
  );

  // --------------------
  // TCDM event counters
  // --------------------
  logic [NrTCDMPortsCores-1:0] flat_acc, flat_con;
  for (genvar i = 0; i < NrTCDMPortsCores; i++) begin : gen_event_counter
    `FFARN(flat_acc[i], tcdm_req[i].q_valid, '0, clk_i, rst_ni)
    `FFARN(flat_con[i], tcdm_req[i].q_valid & ~tcdm_rsp[i].q_ready, '0, clk_i, rst_ni)
  end

  popcount #(
    .INPUT_WIDTH ( NrTCDMPortsCores )
  ) i_popcount_req (
    .data_i     ( flat_acc                 ),
    .popcount_o ( tcdm_events.inc_accessed )
  );

  popcount #(
    .INPUT_WIDTH ( NrTCDMPortsCores )
  ) i_popcount_con (
    .data_i     ( flat_con                  ),
    .popcount_o ( tcdm_events.inc_congested )
  );

  // -------------
  // Sanity Checks
  // -------------
  // Sanity check the parameters. Not every configuration makes sense.
  `ASSERT_INIT(CheckSuperBankSanity, NrBanks >= BanksPerSuperBank);
  `ASSERT_INIT(CheckSuperBankFactor, (NrBanks % BanksPerSuperBank) == 0);
  // Check that the cluster base address aligns to the TCDMSize.
  `ASSERT(ClusterBaseAddrAlign, ((TCDMSize - 1) & cluster_base_addr_i) == 0)
  // Make sure we only have one DMA in the system.
  `ASSERT_INIT(NumberDMA, $onehot0(Xdma))

endmodule
