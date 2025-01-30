// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Hong Pang <hopang@iis.ee.ethz.ch>

// Description: The basic unit composing the partial-write merge tree. This 
// module checks whether the two requesta are writing to the same address, 
// if so, merge two requests into one.

`include "common_cells/registers.svh"

module spatz_strbreq_merge_unit #(
	/// Data width
	parameter int unsigned 	DataWidth						= 32,
	/// Outstanding memory access
	parameter int unsigned	NumOutstandingMem 	= 16,
	/// Signal types for core and cache
	parameter type         	mem_req_t						= logic,
	parameter type         	mem_rsp_t						= logic,
	/// Request id
	parameter type 					req_id_t						= logic,
	/// Req user type
	parameter type 					tcdm_user_t 				= logic
) (
	/// System clock
	input logic 						clk_i,
	/// Asynchronous active low reset.
	input logic 						rst_ni,
	/// Spatz_CC side: Input memory request (before strb handling)
	input mem_req_t		[1:0] unmerge_req_i,
	/// Spatz_CC side: Response ready signal from Spatz_CC
	input logic				[1:0] unmerge_pready_i,
	/// Cache side: Input memory response
	input mem_rsp_t		[1:0]	merge_rsp_i,
	/// Cache side: Output memory request (after strb handling - read before wirte)
	output mem_req_t	[1:0]	merge_req_o,
	/// Cache side: Response ready signal to Cache
	output logic			[1:0] merge_pready_o,
	/// Spatz_CC side: Output memory response
	output mem_rsp_t	[1:0]	unmerge_rsp_o
);

	/// Entry type of the id-user LUT for merged requests
	typedef struct packed {
		logic 			valid;
		tcdm_user_t user_req1;
	} id_user_t;

	/// Request merge signal
	logic to_merge;

	/// Conditions to merge two requestts:
	// 1. Both requests are valid
	// 2. Both requests "write" to the same address
	assign to_merge = unmerge_req_i[0].q_valid && unmerge_req_i[1].q_valid && 
										unmerge_req_i[0].q.write && unmerge_req_i[1].q.write &&
										(unmerge_req_i[0].q.addr == unmerge_req_i[1].q.addr); 

	/// LUT for pending merged req id (for port 0) and user(for port 1)
	id_user_t [NumOutstandingMem - 1 : 0]	id_user_lut_d, id_user_lut_q;

	`FFLARN(id_user_lut_q, id_user_lut_d, '1, '0, clk_i, rst_ni)
	
	always_comb begin
		id_user_lut_d 		= id_user_lut_q;

		// Pass input request and merge port info to the output, by default
		merge_req_o[0].q_valid	= unmerge_req_i[0].q_valid;
		merge_req_o[0].q.addr	= unmerge_req_i[0].q.addr;
		merge_req_o[0].q.write	= unmerge_req_i[0].q.write;
		merge_req_o[0].q.amo	= unmerge_req_i[0].q.amo;
		merge_req_o[0].q.strb	= unmerge_req_i[0].q.strb;
		merge_req_o[0].q.user	= unmerge_req_i[0].q.user;

		merge_req_o[1].q_valid	= unmerge_req_i[1].q_valid;
		merge_req_o[1].q.addr	= unmerge_req_i[1].q.addr;
		merge_req_o[1].q.write	= unmerge_req_i[1].q.write;
		merge_req_o[1].q.amo	= unmerge_req_i[1].q.amo;
		merge_req_o[1].q.strb	= unmerge_req_i[1].q.strb;
		merge_req_o[1].q.user	= unmerge_req_i[1].q.user;
		merge_req_o[1].q.data	= unmerge_req_i[1].q.data;

		merge_pready_o			= unmerge_pready_i;
		unmerge_rsp_o 			= merge_rsp_i;

		/// Merge these two requests
		if (to_merge) begin
			// Invalidate request at output port 1
			merge_req_o[1].q_valid		= 1'b0;
			// Add strb of request 1 to request 0
			merge_req_o[0].q.strb 		= unmerge_req_i[0].q.strb | unmerge_req_i[1].q.strb;
			// Since request 1 is merged into request 0, it is accpeted when port 0
			// is ready to accept req0
			unmerge_rsp_o[1].q_ready 	= merge_rsp_i[0].q_ready;

			/// If port 0 is ready to handle merged req, store this req user/id info into LUT
			if (merge_rsp_i[0].q_ready) begin
				id_user_lut_d[unmerge_req_i[0].q.user.req_id].valid 	= 1'b1;
				id_user_lut_d[unmerge_req_i[0].q.user.req_id].user_req1 = unmerge_req_i[1].q.user;
			end
		end

		/// The merged request receives its response
		if (merge_rsp_i[0].p_valid && merge_rsp_i[0].p.write && id_user_lut_q[merge_rsp_i[0].p.user.req_id].valid) begin
			// Since this is a merged request, we need to ensure both port 0 and 1 are ready to receive it
			if (&unmerge_pready_i) begin
				// Link response port 1 to port 0, but keep its own user
				unmerge_rsp_o[1] 					= merge_rsp_i[0];
				unmerge_rsp_o[1].q_ready 	= merge_rsp_i[1].q_ready;
				unmerge_rsp_o[1].p.user 	= id_user_lut_q[merge_rsp_i[0].p.user.req_id].user_req1;
				// Cut port 2's original response path
				merge_pready_o[1] 				= 1'b0;
				// Invalidate corresponding id-user LUT entry
				id_user_lut_d[merge_rsp_i[0].p.user.req_id].valid = 1'b0;
			end else begin
				// Wait for port 1 to be ready to receive the response
				merge_pready_o[0] 				= 1'b0;
			end
		end
	end

	for (genvar i = 0; i < (DataWidth/8); i++) begin: gen_merge_data_assignment
		assign merge_req_o[0].q.data[((i + 1) * 8 - 1) : (i * 8)]	= (to_merge && unmerge_req_i[1].q.strb[i]) ? 
																																unmerge_req_i[1].q.data[((i + 1) * 8 - 1) : (i * 8)] : 
																																unmerge_req_i[0].q.data[((i + 1) * 8 - 1) : (i * 8)];
	end
endmodule