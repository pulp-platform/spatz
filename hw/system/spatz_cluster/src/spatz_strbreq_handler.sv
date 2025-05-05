// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Hong Pang <hopang@iis.ee.ethz.ch>

// This module is to extend the partial-write request to a read-before-write request.
// This complements the missing of 'strb' processing mechanism in insitu-cache controller

`include "common_cells/registers.svh"

module spatz_strbreq_handler #(
	// Data width
	parameter int unsigned 	DataWidth			= 32,
	// signal types for core and cache
	parameter type         	mem_req_t			= logic,
	parameter type         	mem_rsp_t			= logic,
	parameter type 					reqrsp_user_t	= logic,
	// Derived parameter *Do not override*
	parameter type         	data_t				= logic [DataWidth-1:0]
) (
	/// System clock
	input logic 			clk_i,
	/// Asynchronous active low reset.
	input logic 			rst_ni,
	/// Spatz_CC side: Input memory request (before strb handling)
	input mem_req_t 	strb_req_i,
	/// Spatz_CC side: Response ready signal from Spatz_CC
	input logic 			strb_rsp_ready_i,
	/// Cache side: Input memory response
	input mem_rsp_t		strb_rsp_i,
	/// Cache side: Output memory request (after strb handling - read before wirte)
	output mem_req_t	strb_req_o,
	/// Cache side: Response ready signal to Cache
	output logic 			strb_rsp_ready_o,
	/// Spatz_CC side: Output memory response
	output mem_rsp_t	strb_rsp_o
);

	/// States for the partial-write request handling
	typedef enum logic [1:0] {
	    FsmInitAndRead,
	    FsmReadWait,
	    FsmWrite,
	    FsmWriteWait
	} strb_req_e;

	data_t 			aft_strb_data;

	// Strb handling Fsm states
	strb_req_e	strbreq_state_d, strbreq_state_q;
	logic				load_strbreq_state;
	// Inserted read data, used to be combined with the disturbed data of the partial write req
	data_t 			rd_strb_data_d, rd_strb_data_q;
	logic				load_rd_strb_data;

	assign load_strbreq_state	= (strbreq_state_d	!= strbreq_state_q);
	assign load_rd_strb_data	= (rd_strb_data_d	!= rd_strb_data_q);

	`FFLARN(strbreq_state_q, strbreq_state_d, load_strbreq_state, FsmInitAndRead, clk_i, rst_ni)
	`FFLARN(rd_strb_data_q, rd_strb_data_d, load_rd_strb_data, '0, clk_i, rst_ni)

	always_comb begin
		strbreq_state_d 	= strbreq_state_q;
		rd_strb_data_d		= rd_strb_data_q;

		/// By default, this module is a pass-through, output req/rsp is wired to input req/rsp respectively
		strb_req_o			= strb_req_i;
		// aft_strb_data is input req data if req is not partial-write, otherwise it is final processed result
		strb_req_o.q.data 	= aft_strb_data;
		strb_rsp_ready_o 	= strb_rsp_ready_i;
		strb_rsp_o			= strb_rsp_i;

		unique case (strbreq_state_q)
			// FsmInit:	This state monitors detect the valid partial-write request and start processing.
			FsmInitAndRead: begin
				if (strb_req_i.q_valid) begin
					// This is a valid partial-write request, start handling mechanism
					if (strb_req_i.q.write && (strb_req_i.q.strb != '1)) begin
						/// Cut the input-output request pass-through path
						// Don't accept the partial write request for now
						strb_rsp_o.q_ready 	= 1'b0;
						// Output a valid read request
						strb_req_o.q_valid	= 1'b1;
						strb_req_o.q.write 	= 1'b0;
						strb_req_o.q.strb 	= '1;

						/// State switching
						// If the downstream module is ready to accept the request, switch to `FsmWrite` state
						// Otherwise, switch to `FsmReadWait` state
						if (strb_rsp_i.q_ready) begin
							strbreq_state_d	= FsmWrite;
						end else begin
							strbreq_state_d	= FsmReadWait;
						end
					end
				end
			end
			// FsmReadWait: Keep valid the inserted read request till accepted by downstream module
			FsmReadWait: begin
				/// Cut the input-output request pass-through path
				// Don't accept the partial write request for now
				strb_rsp_o.q_ready 	= 1'b0;
				// Keep the inserted read request info
				strb_req_o.q_valid	= 1'b1;
				strb_req_o.q.write 	= 1'b0;
				strb_req_o.q.strb 	= '1;

				/// State switching
				// If the downstream module is ready to accept the request, switch to `FsmWrite` state
				if (strb_rsp_i.q_ready) begin
					strbreq_state_d	= FsmWrite;
				end
			end
			FsmWrite: begin
				/// Cut the input-output request pass-through path
				// Don't accept the partial write request for now
				strb_rsp_o.q_ready 	= 1'b0;
				// Don't output any valid request
				strb_req_o.q_valid 	= 1'b0;

				/// Wait for inserted read response
				// If the user info of a valid response matches input request, send after-strb write request
				if (strb_rsp_i.p_valid && (strb_rsp_i.p.user == strb_req_i.q.user)) begin
					/// Accept the response
					strb_rsp_ready_o 	= 1'b1;
					/// Don't propagate this respnose to the output rsp `strb_rsp_o`
					strb_rsp_o.p_valid 	= 1'b0;
					/// Form processed write request
					// Record inserted read data for undisturbed part of the partial write request
					rd_strb_data_d 		= strb_rsp_i.p.data;
					strb_req_o.q_valid 	= 1'b1;
					strb_req_o.q.write 	= 1'b1;
					strb_req_o.q.strb 	= '1;
					// If the downstream module can accept the req, this partial-write req can be served
					strb_rsp_o.q_ready	= strb_rsp_i.q_ready;

					/// State switching
					// If the downstream module is ready to accept the request, handling mechanism complete and 
					// switch to `FsmInitAndRead` state. Otherwise, switch to `FsmWriteWait` state
					if (strb_rsp_i.q_ready) begin
						strbreq_state_d	= FsmInitAndRead;
					end else begin
						strbreq_state_d	= FsmWriteWait;
					end
				end
			end
			FsmWriteWait: begin
				/// Cut the input-output request pass-through path
				// Keep the inserted read request info
				strb_req_o.q_valid 	= 1'b1;
				strb_req_o.q.write 	= 1'b1;
				strb_req_o.q.strb 	= '1;

				/// State switching
				// If the downstream module is ready to accept the request, switch to `FsmWrite` state
				if (strb_rsp_i.q_ready) begin
					strbreq_state_d	= FsmInitAndRead;
				end
			end
			default: /*do nothing*/;
		endcase // strbreq_state_q
	end

	// Instantiate DataWidth/8 MUXs to select whether to take input req data or the inserted read req data
	// Output result: aft_strb_data
	for (genvar i = 0; i < (DataWidth/8); i++) begin
		assign aft_strb_data[((i + 1) * 8 - 1) : (i * 8)]	= strb_req_i.q.strb[i] ? 
																												strb_req_i.q.data[((i + 1) * 8 - 1) : (i * 8)] : 
																												rd_strb_data_d[((i + 1) * 8 - 1) : (i * 8)];
	end
endmodule
