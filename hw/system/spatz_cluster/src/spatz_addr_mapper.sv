// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Diyou Shen <dishen@iis.ee.ethz.ch>

// This module is used to remap the memory transaction from/to SPM/DRAM and cores

`include "common_cells/registers.svh"

module spatz_addr_mapper #(
  // Number of input and SPM-side output ports
  parameter int unsigned NumSpmIO             = 0,
  // Number of Cache-side output ports
  parameter int unsigned NumCacheIO           = 0,
  // Full address length
  parameter int unsigned AddrWidth            = 32,
  // SPM address length
  parameter int unsigned SPMAddrWidth         = 16,
  // Data width
  parameter int unsigned DataWidth            = 32,
  // FIFO depth for the response channel
  parameter int unsigned FifoDepth            = 2,
  // signal types for core and cache
  parameter type         mem_req_t            = logic,
  parameter type         mem_req_chan_t       = logic,
  parameter type         mem_rsp_t            = logic,
  parameter type         mem_rsp_chan_t       = logic,
  // signal types for spm (shorter addr)
  parameter type         spm_req_t            = logic,
  parameter type         spm_rsp_t            = logic,

  // Derived parameter *Do not override*
  parameter type         addr_t               = logic [AddrWidth-1:0],
  parameter type         spm_addr_t           = logic [SPMAddrWidth-1:0],
  parameter type         data_t               = logic [DataWidth-1:0]
) (
  input  logic                        clk_i,
  input  logic                        rst_ni,

  // Input Side
  input  mem_req_t  [NumSpmIO-1:0]    mem_req_i,
  output mem_rsp_t  [NumSpmIO-1:0]    mem_rsp_o,
  output logic      [NumSpmIO-1:0]    error_o,

  // Reg and AddrMap
  input  addr_t                       tcdm_start_address_i,
  input  addr_t                       tcdm_end_address_i,
  input  addr_t                       spm_size_i,
  input  logic                        flush_i,

  // Two output will need different address size: cahce will need full addr, spm only needs log2(TCDMSize) bits
  // We may need to expand the signals here to correctly wire them
  // Output Side
  /// SPM Side
  output spm_req_t  [NumSpmIO-1:0]    spm_req_o,
  input  spm_rsp_t  [NumSpmIO-1:0]    spm_rsp_i,
  /// Cache Side
  output mem_req_t  [NumCacheIO-1:0]  cache_req_o,
  output logic      [NumCacheIO-1:0]  cache_pready_o,
  input  mem_rsp_t  [NumCacheIO-1:0]  cache_rsp_i
);

  // ---------------------------
  // Signal & Parameter Defines
  // ---------------------------
  typedef logic select_t;

  addr_t    spm_start_addr;
  select_t  [NumSpmIO-1:0] target_select;
  // logic     [NumSpmIO-1:0] mem_q_ready;

  mem_req_t [NumSpmIO-1:0] cache_req_arb;
  mem_rsp_t [NumSpmIO-1:0] cache_rsp_arb;
  logic     [NumSpmIO-1:0] cache_pready_arb;

  // Ports define
  localparam int unsigned NUM_PORTS_PER_SPATZ = NumSpmIO/2-1;
  localparam int unsigned SPM_PORT_OFFSET     = NumSpmIO/2;
  localparam int unsigned CACHE_PORT_OFFSET   = NumCacheIO/2-1;

  // First snitch port at cache side
  localparam int unsigned CACHE_SNITCH0 = NumCacheIO-2;
  // Second snitch port at cache side
  localparam int unsigned CACHE_SNITCH1 = NumCacheIO-1;

  localparam int unsigned SPM_SNITCH0 = NumSpmIO/2-1;
  localparam int unsigned SPM_SNITCH1 = NumSpmIO  -1;

  localparam int unsigned SPM   = 1;
  localparam int unsigned CACHE = 0;

  localparam int unsigned ByteOffset = 6;

  // The end address substract the SPM region size would be the actual starting address
  assign spm_start_addr = tcdm_end_address_i - spm_size_i;

  // -------------
  // Request Side
  // -------------
  // Two methods of assigning:
  // 1. forward requests directly, control valid signals
  // 2. check both valid and request

  // To make sure stack is always visitable, we use back-side allocation
  // The available SPM region will ends at the `tcdm_end_address_i` and
  // start from `spm_start_addr`.

  logic [NumCacheIO-1:0]          cache_req_valid;

  always_comb begin
    for (int j = 0; unsigned'(j) < NumSpmIO; j++) begin : gen_req
      // Initial values
      spm_req_o[j]     = '0;
      cache_req_arb[j] = '0;
      error_o[j]       = '0;
      if ((mem_req_i[j].q.addr >= tcdm_start_address_i) & (mem_req_i[j].q.addr < tcdm_end_address_i)) begin
        target_select[j] = SPM;
        if (mem_req_i[j].q.addr < spm_start_addr) begin
          // TODO: set CSR for error handling
          error_o[j] = 1'b1;
        end else begin
          // Wire to SPM outputs
          // Cut unused addr for spm_req_t
          spm_req_o[j].q = '{
            addr:    mem_req_i[j].q.addr[SPMAddrWidth-1:0],
            write:   mem_req_i[j].q.write,
            amo:     mem_req_i[j].q.amo,
            data:    mem_req_i[j].q.data,
            strb:    mem_req_i[j].q.strb,
            user:    mem_req_i[j].q.user,
            default: '0
          };
          // During flushing, stop sending out requests
          spm_req_o[j].q_valid = mem_req_i[j].q_valid && (!flush_i);
        end
      end else begin
        // Wire to Cache outputs
        cache_req_arb[j].q       = mem_req_i[j].q;
        cache_req_arb[j].q_valid = mem_req_i[j].q_valid && (!flush_i);
        target_select[j]         = CACHE;
      end
      cache_req_valid[j] = cache_req_arb[j].q_valid;
    end
  end

  if (NumSpmIO > NumCacheIO) begin : gen_req_mux
    // Only generate MUX for Spatz, Snitch will directly passed through
    for (genvar j = 0; unsigned'(j) < NUM_PORTS_PER_SPATZ; j++) begin : gen_mux_loop
      spatz_cache_mux #(
        .Offset      ( 6              ),
        .DATA_T      ( mem_req_chan_t )
      ) i_req_mux (
        .clk_i,
        .rst_ni,
        .inp_data_i  ( {cache_req_arb[j].q,       cache_req_arb[j+SPM_PORT_OFFSET].q}      ),
        .inp_valid_i ( {cache_req_arb[j].q_valid, cache_req_arb[j+SPM_PORT_OFFSET].q_valid}),
        .inp_ready_o ( {cache_rsp_arb[j].q_ready, cache_rsp_arb[j+SPM_PORT_OFFSET].q_ready}),
        .oup_data_o  ( cache_req_o[j].q ),
        .oup_valid_o ( cache_req_o[j].q_valid ),
        .oup_ready_i ( cache_rsp_i[j].q_ready )
      );

      assign cache_rsp_arb[j].p                       = cache_rsp_i[j].p;
      assign cache_rsp_arb[j+SPM_PORT_OFFSET].p       = cache_rsp_i[j].p;
      assign cache_rsp_arb[j].p_valid                 = cache_rsp_i[j].p.user.core_id == 0 ? cache_rsp_i[j].p_valid : 1'b0;
      assign cache_rsp_arb[j+SPM_PORT_OFFSET].p_valid = cache_rsp_i[j].p.user.core_id == 1 ? cache_rsp_i[j].p_valid : 1'b0;

      assign cache_pready_o[j] = cache_rsp_i[j].p.user.core_id == 0 ? cache_pready_arb[j] : cache_pready_arb[j+SPM_PORT_OFFSET];
    end

    // Connect the Snitch side signals
    assign cache_req_o[CACHE_SNITCH0] = cache_req_arb[SPM_SNITCH0];
    assign cache_req_o[CACHE_SNITCH1] = cache_req_arb[SPM_SNITCH1];

    assign cache_rsp_arb[SPM_SNITCH0] = cache_rsp_i[CACHE_SNITCH0];
    assign cache_rsp_arb[SPM_SNITCH1] = cache_rsp_i[CACHE_SNITCH1];

    assign cache_pready_o[CACHE_SNITCH0] = cache_pready_arb[SPM_SNITCH0];
    assign cache_pready_o[CACHE_SNITCH1] = cache_pready_arb[SPM_SNITCH1];

  end else if (NumSpmIO == NumCacheIO) begin : gen_req_no_mux
    assign cache_req_o   = cache_req_arb;
    assign cache_rsp_arb = cache_rsp_i;
  end else begin
    $error("[ADDR_MAPPER], $Port > SPM, configuration not support");
  end

  // -------------
  // Response Side
  // -------------
  // Simutanueous responses from both Cache and SPM is possible but not common
  // An 2-1 arbiter/mux is needed to pick from it (round-robin?)
  // Note there is no p_ready signal, we may need a FIFO to protect the rsp
  logic [NumSpmIO-1:0] postarb_valid, postarb_ready, arb_select;

  for (genvar j = 0; unsigned'(j) < NumSpmIO; j++) begin : gen_rsp
    assign mem_rsp_o[j].p_valid = postarb_valid[j];
    assign postarb_ready[j] = 1'b1;
    always_comb begin : hs_comb
      // Do not accept any req when cache is flushing
      mem_rsp_o[j].q_ready   = cache_rsp_arb[j].q_ready && !(flush_i);
      // Use spm by default
      arb_select[j]          = SPM;
      cache_pready_arb[j]    = 1'b0;

      if (target_select[j] == SPM) begin
        mem_rsp_o[j].q_ready = spm_rsp_i[j].q_ready && !(flush_i);
      end

      if (spm_rsp_i[j].p_valid) begin
        // SPM always has priority
        // stop cache response if spm is responding
        arb_select[j]       = SPM;
        cache_pready_arb[j] = 1'b0;
      end else begin
        if (cache_rsp_arb[j].p_valid) begin
          arb_select[j]     = CACHE;
        end
        cache_pready_arb[j] = 1'b1;
      end
    end

    rr_arb_tree #(
      .NumIn     ( 2              ),
      .DataType  ( mem_rsp_chan_t ),
      .ExtPrio   ( 1'b1           ),
      .AxiVldRdy ( 1'b1           ),
      .LockIn    ( 1'b0           )
    ) i_rsp_arb (
      .clk_i   ( clk_i                                            ),
      .rst_ni  ( rst_ni                                           ),
      .flush_i ( '0                                               ),
      .rr_i    ( arb_select[j]                                    ),
      .req_i   ( {spm_rsp_i[j].p_valid, cache_rsp_arb[j].p_valid} ),
      .gnt_o   (                                                  ),
      .data_i  ( {spm_rsp_i[j].p,       cache_rsp_arb[j].p}       ),
      .req_o   ( postarb_valid[j]                                 ),
      .gnt_i   ( postarb_ready[j]                                 ),
      .data_o  ( mem_rsp_o[j].p                                   ),
      .idx_o   ( /* not used */                                   )
    );
  end

endmodule
