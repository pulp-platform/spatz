// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Diyou Shen <dishen@iis.ee.ethz.ch>

// This module is used to remap the memory transaction from/to SPM/DRAM and cores

`include "common_cells/registers.svh"

module spatz_addr_mapper #(
	// Number of input and one-side output ports
  parameter int unsigned NumIO            		= 0,
  // Full address length
  parameter int unsigned AddrWidth						= 32,
  // SPM address length
  parameter int unsigned SPMAddrWidth         = 16,
  // Data width
  parameter int unsigned DataWidth						= 32,
  // FIFO depth for the response channel
  parameter int unsigned FifoDepth 						= 2,
  // signal types for core and cache
  parameter type         mem_req_t          	= logic,
  parameter type         mem_rsp_t          	= logic,
  // signal types for spm (shorter addr)
  parameter type         spm_req_t          	= logic,
  parameter type         spm_rsp_t          	= logic,

  // Derived parameter *Do not override*
  parameter type         addr_t             	= logic [AddrWidth-1:0],
  parameter type         spm_addr_t           = logic [SPMAddrWidth-1:0],
  parameter type         data_t             	= logic [DataWidth-1:0]
) (
  input  logic            					clk_i,
  input  logic            					rst_ni,

  // CSR Side (Mapping Info)

  // Input Side
  input  mem_req_t	[NumIO-1:0] 		mem_req_i,
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
  output spm_req_t 	[NumIO-1:0]			spm_req_o,
  input  spm_rsp_t  [NumIO-1:0]  		spm_rsp_i,
  /// Cache Side
  output mem_req_t 	[NumIO-1:0]			cache_req_o,
  input  mem_rsp_t  [NumIO-1:0]  		cache_rsp_i
);

	// ---------------------------
	// Signal & Parameter Defines
	// ---------------------------
  addr_t spm_end_addr;
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
			end
		end
	end

  // -------------
  // Response Side
  // -------------
	// Simutanueous responses from both Cache and SPM is possible but not common
	// An 2-1 arbiter/mux is needed to pick from it (round-robin?)
	// FIFO may not be necessary in this case as I don't expect this kind of conflict is common

  // Test Only, directly wire rsp
  assign mem_rsp_o = spm_rsp_i;

	// for (genvar j = 0; unsigned'(j) < NumIO; j++) begin : gen_rsp   
	// 	rr_arb_tree #(
  //     .NumIn     ( 2    		  ),
  //     .DataType  ( mem_rsp_t ),
  //     .ExtPrio   ( 1'b0    		),
  //     .AxiVldRdy ( 1'b1  			),
  //     .LockIn    ( 1'b1     	)
  //   ) i_rsp_arb (
  //     .clk_i,
  //     .rst_ni,
  //     .flush_i,
  //     .rr_i    ( '0     			),
  //     .req_i   ( out_valid[j] ),
  //     .gnt_o   ( out_ready[j] ),
  //     .data_i  ( {spm_rsp_i[j].q, cache_rsp_i[j].q} ),
  //     .req_o   ( arb_valid    ),
  //     .gnt_i   ( arb_ready    ),
  //     .data_o  ( arb.data     ),
  //     .idx_o   ( arb.idx      )
  //   );

  //   // A small FIFO to keep some responses
  //   fifo_v3 # 		(
  //   	.DEPTH			( FifoDepth 				),
  //   	.DATA_WIDTH ( $bits(mem_rsp_t)	)
  //   ) i_rsp_fifo (
  //   	.clk_i,
  //     .rst_ni,
  //     .flush_i,
  //     .full_o  		(),
  //     .empty_o 		(),
  //     .usage_o 		( /*unused*/ ),
  //     .data_i 		(),
  //     .push_i 		(),
  //     .data_o 		(),
  //     .pop_i 			()
  //   );
  // end


endmodule
