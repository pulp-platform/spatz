// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Diyou Shen <dishen@iis.ee.ethz.ch>

// This module is used to remap the memory transaction from/to SPM/DRAM and cores

`include "common_cells/registers.svh"

module spatz_addr_mapper #(
  // Number of input and one-side output ports
  parameter int unsigned NumIO                = 0,
  // Full address length
  parameter int unsigned AddrWidth            = 32,
  // SPM address length
  parameter int unsigned SPMAddrWidth         = 16,
  // Data width
  parameter int unsigned DataWidth            = 32,
  // FIFO depth for the response channel
  parameter int unsigned FifoDepth             = 2,
  // signal types for core and cache
  parameter type         mem_req_t            = logic,
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
  input  logic                      clk_i,
  input  logic                      rst_ni,

  // CSR Side (Mapping Info)

  // Input Side
  input  mem_req_t  [NumIO-1:0]     mem_req_i,
  output mem_rsp_t  [NumIO-1:0]     mem_rsp_o,
  output logic      [NumIO-1:0]     error_o,

  // Reg and AddrMap
  input  addr_t                     tcdm_start_address_i,
  input  addr_t                     tcdm_end_address_i,
  input  addr_t                     spm_size_i,

  // Two output will need different address size: cahce will need full addr, spm only needs log2(TCDMSize) bits
  // We may need to expand the signals here to correctly wire them
  // Output Side
  /// SPM Side
  output spm_req_t  [NumIO-1:0]      spm_req_o,
  input  spm_rsp_t  [NumIO-1:0]      spm_rsp_i,
  /// Cache Side
  output mem_req_t  [NumIO-1:0]      cache_req_o,
  output logic      [NumIO-1:0]      cache_pready_o,
  input  mem_rsp_t  [NumIO-1:0]      cache_rsp_i
);

  // ---------------------------
  // Signal & Parameter Defines
  // ---------------------------
  typedef logic select_t;

  addr_t    spm_end_addr;
  select_t  [NumIO-1:0] target_select;
  // logic     [NumIO-1:0] mem_q_ready;

  localparam int unsigned SPM   = 1;
  localparam int unsigned CACHE = 0;

  assign spm_end_addr = tcdm_start_address_i + spm_size_i;

  // -------------
  // Request Side
  // -------------
  // Two methods of assigning:
  // 1. forward requests directly, control valid signals
  // 2. check both valid and request
  
  always_comb begin
    for (int j = 0; unsigned'(j) < NumIO; j++) begin : gen_req
      // Initial values
      spm_req_o[j]   = '0;
      cache_req_o[j] = '0;
      error_o[j]     = '0;
      if ((mem_req_i[j].q.addr >= tcdm_start_address_i) & (mem_req_i[j].q.addr < tcdm_end_address_i)) begin
        target_select[j] = SPM;
        if (mem_req_i[j].q.addr > spm_end_addr) begin
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
          spm_req_o[j].q_valid = mem_req_i[j].q_valid;
        end
      end else begin
        // Wire to Cache outputs
        cache_req_o[j] = mem_req_i[j];
        target_select[j] = CACHE;
      end
    end
  end

  // -------------
  // Response Side
  // -------------
  // Simutanueous responses from both Cache and SPM is possible but not common
  // An 2-1 arbiter/mux is needed to pick from it (round-robin?)
  // Note there is no p_ready signal, we may need a FIFO to protect the rsp

  for (genvar j = 0; unsigned'(j) < NumIO; j++) begin : gen_rsp
    logic postarb_valid;
    logic postarb_ready;
    logic arb_select;

    assign mem_rsp_o[j].p_valid = postarb_valid;
    assign postarb_ready = 1'b1;
    always_comb begin : hs_comb
      // The q_ready HS signal should follow the request select, not response
      mem_rsp_o[j].q_ready = cache_rsp_i[j].q_ready;
      // Use spm by default
      arb_select           = 1'b1;
      cache_pready_o[j]    = 1'b0;

      if (target_select[j] == SPM) begin
        mem_rsp_o[j].q_ready = spm_rsp_i[j].q_ready;
      end

      if (spm_rsp_i[j].p_valid) begin
        // SPM always has priority
        // stop cache response if spm is responding
        arb_select        = 1'b1;
        cache_pready_o[j] = 1'b0;
      end else begin
        if (cache_rsp_i[j].p_valid) begin
          arb_select      = 1'b0;
        end
        cache_pready_o[j] = 1'b1;
      end
    end

    rr_arb_tree #(
      .NumIn     ( 2              ),
      .DataType  ( mem_rsp_chan_t ),
      .ExtPrio   ( 1'b1           ),
      .AxiVldRdy ( 1'b1           ),
      .LockIn    ( 1'b0           )
    ) i_rsp_arb (
      .clk_i   ( clk_i                                          ),
      .rst_ni  ( rst_ni                                         ),
      .flush_i ( '0                                             ),
      .rr_i    ( arb_select                                     ),
      .req_i   ( {spm_rsp_i[j].p_valid, cache_rsp_i[j].p_valid} ),
      .gnt_o   (                                                ),
      .data_i  ( {spm_rsp_i[j].p,       cache_rsp_i[j].p}       ),
      .req_o   ( postarb_valid                                  ),
      .gnt_i   ( postarb_ready                                  ),
      .data_o  ( mem_rsp_o[j].p                                 ),
      .idx_o   ( /* not used */                                 )
    );

    // TODO: add a FIFO; should we make it fallthrough?

  end


endmodule
