// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Michael Rogenmoser <michaero@iis.ee.ethz.ch>

`include "common_cells/registers.svh"

/// Porting from hier-icache:
///   Unsupported: different line width, banks in L1, L0 not fully associative
///   [SH_FETCH_DATA_WIDTH == Cache line width]
///   [SH_NB_BANKS == 1]
///   [PRI_NB_WAYS == L0_LINE_COUNT] -> here fully associative
///   [SH_CACHE_LINE == PRI_CACHE_LINE]
///   NumFetchPorts = NB_CORES
///   L0_LINE_COUNT = PRI_CACHE_SIZE/(bytes per line)
///   LINE_WIDTH = X_CACHE_LINE * DATA_WIDTH -> Use >= 32*NB_CORES for optimal performance
///   LINE_COUNT = SH_CACHE_SIZE/(bytes per line)
///   SET_COUNT = SH_NB_WAYS
///   FetchAddrWidth = FETCH_ADDR_WIDTH
///   FetchDataWidth = PRI_FETCH_DATA_WIDTH
///   AxiAddrWidth = AXI_ADDR
///   AxiDataWidth = AXI_DATA
module pulp_icache_wrap #(
  /// Number of request (fetch) ports
  parameter int NumFetchPorts = -1,
  /// L0 Cache Line Count
  parameter int L0_LINE_COUNT = -1,
  /// Cache Line Width
  /// For optimal performance, use >= 32*NumFetchPorts to allow execution of 32-bit instructions
  /// for each core before requiring another L0-L1 fetch.
  parameter int LINE_WIDTH = -1,
  /// The number of cache lines per set. Power of two; >= 2.
  parameter int LINE_COUNT = -1,
  /// The set associativity of the cache. Power of two; >= 1.
  parameter int SET_COUNT = 1,
  /// Error detection
  parameter int unsigned L1DataParityWidth = 0,
  /// Error detection L0
  parameter int unsigned L0DataParityWidth = L1DataParityWidth,
  /// Fetch interface address width. Same as FILL_AW; >= 1.
  parameter int FetchAddrWidth = -1,
  /// Fetch interface data width. Power of two; >= 8.
  parameter int FetchDataWidth = -1,
  /// Fill interface address width. Same as FETCH_AW; >= 1.
  parameter int AxiAddrWidth = -1,
  /// Fill interface data width. Power of two; >= 8.
  parameter int AxiDataWidth = -1,
  /// Configuration input types for memory cuts used in implementation.
  parameter type sram_cfg_data_t  = logic,
  parameter type sram_cfg_tag_t   = logic,

  parameter type axi_req_t = logic,
  parameter type axi_rsp_t = logic
) (
  input  logic                                                  clk_i,
  input  logic                                                  rst_ni,

  // Processor interface
  input  logic [NumFetchPorts-1:0]                              fetch_req_i,
  input  logic [NumFetchPorts-1:0][FetchAddrWidth-1:0]          fetch_addr_i,
  output logic [NumFetchPorts-1:0]                              fetch_gnt_o,
  output logic [NumFetchPorts-1:0]                              fetch_rvalid_o,
  output logic [NumFetchPorts-1:0][FetchDataWidth-1:0]          fetch_rdata_o,
  output logic [NumFetchPorts-1:0]                              fetch_rerror_o,

  input  logic                                                  enable_prefetching_i,
  output snitch_icache_pkg::icache_l0_events_t [NumFetchPorts-1:0] icache_l0_events_o,
  output snitch_icache_pkg::icache_l1_events_t                  icache_l1_events_o,
  input  logic [NumFetchPorts-1:0]                              flush_valid_i,
  output logic [NumFetchPorts-1:0]                              flush_ready_o,

  // SRAM configs
  input  sram_cfg_data_t                                        sram_cfg_data_i,
  input  sram_cfg_tag_t                                         sram_cfg_tag_i,

  // AXI interface
  output axi_req_t                                              axi_req_o,
  input  axi_rsp_t                                              axi_rsp_i
);
  localparam int unsigned AdapterType = 1;

  logic [NumFetchPorts-1:0] fetch_valid, fetch_ready, fetch_rerror;
  logic [NumFetchPorts-1:0][FetchAddrWidth-1:0] fetch_addr;
  logic [NumFetchPorts-1:0][FetchDataWidth-1:0] fetch_rdata;

  for (genvar i = 0; i < NumFetchPorts; i++) begin : gen_adapter
    if (AdapterType == 0) begin : gen_response_cut

      // Reuquires the core to keep data applied steady while req is high, may not be guaranteed...
      spill_register #(
        .T     (logic [FetchDataWidth-1+1:0]),
        .Bypass(1'b0)
      ) i_spill_reg (
        .clk_i,
        .rst_ni,
        .valid_i ( fetch_ready   [i]                     ),
        .ready_o ( /* Unconnected as always ready */     ),
        .data_i  ( {fetch_rdata  [i], fetch_rerror  [i]} ),
        .valid_o ( fetch_rvalid_o[i]                     ),
        .ready_i ( '1                                    ),
        .data_o  ( {fetch_rdata_o[i], fetch_rerror_o[i]} )
      );

      assign fetch_addr[i] = fetch_addr_i[i];
      assign fetch_valid[i] = fetch_req_i[i];
      assign fetch_gnt_o[i] = fetch_ready[i];

    end else if (AdapterType == 1) begin : gen_request_cut

      logic gnt;

      assign fetch_gnt_o[i] = gnt & fetch_req_i[i];

      spill_register #(
        .T     (logic [FetchAddrWidth-1:0]),
        .Bypass(1'b0)
      ) i_spill_reg (
        .clk_i,
        .rst_ni,
        .valid_i ( fetch_req_i [i] ),
        .ready_o ( gnt             ),
        .data_i  ( fetch_addr_i[i] ),
        .valid_o ( fetch_valid [i] ),
        .ready_i ( fetch_ready [i] ),
        .data_o  ( fetch_addr  [i] )
      );

      assign fetch_rdata_o [i] = fetch_rdata [i];
      assign fetch_rerror_o[i] = fetch_rerror[i];
      assign fetch_rvalid_o[i] = fetch_ready [i] & fetch_valid[i];

    end else begin : gen_flexible_cut
      // This can still be improved, there is still an extra stall cycle sometimes AFAIK...

      logic stalled_d, stalled_q;

      logic spill_valid, spill_ready;
      logic [FetchAddrWidth-1:0] spill_addr;

      spill_register #(
        .T     (logic [FetchAddrWidth-1:0]),
        .Bypass(1'b0)
      ) i_req_spill_reg (
        .clk_i,
        .rst_ni,
        .valid_i ( fetch_req_i [i] ),
        .ready_o ( fetch_gnt_o [i] ),
        .data_i  ( fetch_addr_i[i] ),
        .valid_o ( spill_valid     ),
        .ready_i ( spill_ready     ),
        .data_o  ( spill_addr      )
      );

      always_comb begin
        // Keep steady state
        stalled_d = stalled_q;

        // If already stalled
        if (stalled_q) begin
          // only revert back to unstalled state with sufficient gap
          if (!spill_valid && !fetch_req_i[i])
            stalled_d = 1'b0;
        end else begin
          if (fetch_req_i[i] && !fetch_ready[i])
            stalled_d = 1'b1;
        end
      end
      `FF(stalled_q, stalled_d, '0)

      assign fetch_valid[i] = stalled_q ? spill_valid : fetch_req_i[i];
      assign fetch_addr [i] = stalled_q ? spill_addr : fetch_addr_i[i];

      logic spill_rvalid;
      logic spill_rerror;
      logic [FetchDataWidth-1:0] spill_rdata;

      spill_register #(
        .T     (logic [FetchDataWidth-1+1:0]),
        .Bypass(1'b0)
      ) i_rsp_spill_reg (
        .clk_i,
        .rst_ni,
        .valid_i ( fetch_ready [i]                   ),
        .ready_o ( /* Unconnected as always ready */ ),
        .data_i  ( {fetch_rdata[i], fetch_rerror[i]} ),
        .valid_o ( spill_rvalid                      ),
        .ready_i ( '1                                ),
        .data_o  ( {spill_rdata   , spill_rerror   } )
      );

      assign fetch_rvalid_o[i] = stalled_q ? fetch_ready[i] : spill_rvalid;
      assign fetch_rdata_o [i] = stalled_q ? fetch_rdata [i] : spill_rdata;
      assign fetch_rerror_o[i] = stalled_q ? fetch_rerror[i] : spill_rerror;

    end
  end

  snitch_icache #(
    .NR_FETCH_PORTS     ( NumFetchPorts   ),
    .L0_LINE_COUNT      ( L0_LINE_COUNT   ),
    .LINE_WIDTH         ( LINE_WIDTH      ),
    .LINE_COUNT         ( LINE_COUNT      ),
    .SET_COUNT          ( SET_COUNT       ),
    .FETCH_AW           ( FetchAddrWidth  ),
    .FETCH_DW           ( FetchDataWidth  ),
    .FILL_AW            ( AxiAddrWidth    ),
    .FILL_DW            ( AxiDataWidth    ),
    .FETCH_PRIORITY     ( 1               ),
    .MERGE_FETCHES      ( 1               ),
    .L0_DATA_PARITY_BITS( L0DataParityWidth ),
    .L1_DATA_PARITY_BITS( L1DataParityWidth ),
    .L1_TAG_SCM         ( 1               ),
    .SERIAL_LOOKUP      ( 1               ),
    .NUM_AXI_OUTSTANDING( 4               ),
    .EARLY_LATCH        ( 0               ),
    .ISO_CROSSING       ( 0               ),
    .sram_cfg_data_t    ( sram_cfg_data_t ),
    .sram_cfg_tag_t     ( sram_cfg_tag_t  ),
    .axi_req_t          ( axi_req_t       ),
    .axi_rsp_t          ( axi_rsp_t       )
  ) i_snitch_icache (
    .clk_i,
    .clk_d2_i         ( clk_i                 ),
    .rst_ni,

    .enable_prefetching_i,
    .icache_l0_events_o,
    .icache_l1_events_o,
    .flush_valid_i,
    .flush_ready_o,

    .inst_addr_i      ( fetch_addr            ),
    .inst_data_o      ( fetch_rdata           ),
    .inst_cacheable_i ( {NumFetchPorts{1'b1}} ),
    .inst_valid_i     ( fetch_valid           ),
    .inst_ready_o     ( fetch_ready           ),
    .inst_error_o     ( fetch_rerror          ),

    .sram_cfg_data_i,
    .sram_cfg_tag_i,

    .axi_req_o,
    .axi_rsp_i
  );

endmodule
