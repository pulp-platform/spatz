// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Hong Pang <hopang@iis.ee.ethz.ch>

// Description: This is the partial-write merge tree, it detects write 
// requests that points to the sae emory address and merge them together
// The merged request is ouput on the lowest id port that is merged

module spatz_strbreq_merge_tree #(
	/// Number of input and one-side output ports
	parameter int unsigned 	NumIO 						= 10,
	/// Outstanding memory access
	parameter int unsigned	NumOutstandingMem = 16,
	/// The total amount of cores.
	parameter int unsigned 	NumCores 					= 2,
	/// Number of input and one-side output ports
	// **** can only be the power of 2 ****
  parameter int unsigned	MergeNum					= 8,
	/// Data width
	parameter int unsigned 	DataWidth					= 32,
	/// signal types for core and cache
	parameter type         	mem_req_t					= logic,
	parameter type         	mem_rsp_t					= logic,
	/// Request id
	parameter type 					req_id_t					= logic,
	/// Req user type
	parameter type 					tcdm_user_t 			= logic
) (
	/// System clock
	input logic 					clk_i,
	/// Asynchronous active low reset.
	input logic 					rst_ni,
	/// Spatz_CC side: Input memory request (before strb handling)
	input mem_req_t		[NumIO-1:0]	unmerge_req_i,
	/// Spatz_CC side: Response ready signal from Spatz_CC
	input logic				[NumIO-1:0]	unmerge_pready_i,
	/// Cache side: Input memory response
	input mem_rsp_t		[NumIO-1:0]	merge_rsp_i,
	/// Cache side: Output memory request (after strb handling - read before wirte)
	output mem_req_t 	[NumIO-1:0]	merge_req_o,
	/// Cache side: Response ready signal to Cache
	output logic			[NumIO-1:0]	merge_pready_o,
	/// Spatz_CC side: Output memory response
	output mem_rsp_t	[NumIO-1:0]	unmerge_rsp_o
);

	// Pass non-VFU requests from input to output
	for (genvar i = 0; i < NumCores; i++) begin
		assign merge_req_o[(i+1)*(NumIO/NumCores)-1: (MergeNum/NumCores)+i*(NumIO/NumCores)]		= unmerge_req_i[(i+1)*(NumIO/NumCores)-1: (MergeNum/NumCores)+i*(NumIO/NumCores)];
		assign unmerge_rsp_o[(i+1)*(NumIO/NumCores)-1: (MergeNum/NumCores)+i*(NumIO/NumCores)]		= merge_rsp_i[(i+1)*(NumIO/NumCores)-1: (MergeNum/NumCores)+i*(NumIO/NumCores)];
		assign merge_pready_o[(i+1)*(NumIO/NumCores)-1: (MergeNum/NumCores)+i*(NumIO/NumCores)] 	= unmerge_pready_i[(i+1)*(NumIO/NumCores)-1: (MergeNum/NumCores)+i*(NumIO/NumCores)];
	end

	// If no merge need, this module just pass the input request and response to the output
	if (MergeNum == 1) begin
		// Pass non-VFU requests from input to output
		for (genvar i = 0; i < NumCores; i++) begin
			assign merge_req_o[(MergeNum/NumCores)+i*(NumIO/NumCores)-1: i*(NumIO/NumCores)]		= unmerge_req_i[(MergeNum/NumCores)+i*(NumIO/NumCores)-1: i*(NumIO/NumCores)];
			assign unmerge_rsp_o[(MergeNum/NumCores)+i*(NumIO/NumCores)-1: i*(NumIO/NumCores)]		= merge_rsp_i[(MergeNum/NumCores)+i*(NumIO/NumCores)-1: i*(NumIO/NumCores)];
			assign merge_pready_o[(MergeNum/NumCores)+i*(NumIO/NumCores)-1: i*(NumIO/NumCores)] 	= unmerge_pready_i[(MergeNum/NumCores)+i*(NumIO/NumCores)-1: i*(NumIO/NumCores)];
		end
	// 8 Ports needs to be checked
	end if (MergeNum == 8) begin
		mem_req_t	[NumIO-1:0]	inter_unit_req0, inter_unit_req1;
		logic 		[NumIO-1:0]	inter_unit_pready0, inter_unit_pready1;
		mem_rsp_t	[NumIO-1:0]	inter_unit_rsp0, inter_unit_rsp1;

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit10 (
			.clk_i           	(clk_i										),
			.rst_ni          	(rst_ni										),
			.unmerge_req_i   	(unmerge_req_i[1:0]				),
			.unmerge_pready_i	(unmerge_pready_i[1:0]		),
			.merge_rsp_i     	(inter_unit_rsp0[1:0]			),
			.merge_req_o     	(inter_unit_req0[1:0]			),
			.merge_pready_o  	(inter_unit_pready0[1:0]	),
			.unmerge_rsp_o   	(unmerge_rsp_o[1:0]				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit32 (
			.clk_i           	(clk_i										),
			.rst_ni          	(rst_ni										),
			.unmerge_req_i   	(unmerge_req_i[3:2]				),
			.unmerge_pready_i	(unmerge_pready_i[3:2]		),
			.merge_rsp_i     	(inter_unit_rsp0[3:2]			),
			.merge_req_o     	(inter_unit_req0[3:2]			),
			.merge_pready_o  	(inter_unit_pready0[3:2]	),
			.unmerge_rsp_o   	(unmerge_rsp_o[3:2]				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit65 (
			.clk_i           	(clk_i										),
			.rst_ni          	(rst_ni										),
			.unmerge_req_i   	(unmerge_req_i[6:5]				),
			.unmerge_pready_i	(unmerge_pready_i[6:5]		),
			.merge_rsp_i     	(inter_unit_rsp0[6:5]			),
			.merge_req_o     	(inter_unit_req0[6:5]			),
			.merge_pready_o  	(inter_unit_pready0[6:5]	),
			.unmerge_rsp_o   	(unmerge_rsp_o[6:5]				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit87 (
			.clk_i           	(clk_i										),
			.rst_ni          	(rst_ni										),
			.unmerge_req_i   	(unmerge_req_i[8:7]				),
			.unmerge_pready_i	(unmerge_pready_i[8:7]		),
			.merge_rsp_i     	(inter_unit_rsp0[8:7]			),
			.merge_req_o     	(inter_unit_req0[8:7]			),
			.merge_pready_o  	(inter_unit_pready0[8:7]	),
			.unmerge_rsp_o   	(unmerge_rsp_o[8:7]				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit20 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req0[2], inter_unit_req0[0]}				),
			.unmerge_pready_i	({inter_unit_pready0[2], inter_unit_pready0[0]}	),
			.merge_rsp_i     	({inter_unit_rsp1[2], inter_unit_rsp1[0]}				),
			.merge_req_o     	({inter_unit_req1[2], inter_unit_req1[0]}				),
			.merge_pready_o  	({inter_unit_pready1[2], inter_unit_pready1[0]}	),
			.unmerge_rsp_o   	({inter_unit_rsp0[2], inter_unit_rsp0[0]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit75 (
			.clk_i           	(clk_i											),
			.rst_ni          	(rst_ni											),
			.unmerge_req_i   	({inter_unit_req0[7], inter_unit_req0[5]}				),
			.unmerge_pready_i	({inter_unit_pready0[7], inter_unit_pready0[5]}	),
			.merge_rsp_i     	({inter_unit_rsp1[7], inter_unit_rsp1[5]}				),
			.merge_req_o     	({inter_unit_req1[7], inter_unit_req1[5]}				),
			.merge_pready_o  	({inter_unit_pready1[7], inter_unit_pready1[5]}	),
			.unmerge_rsp_o   	({inter_unit_rsp0[7], inter_unit_rsp0[5]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit50 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req1[5], inter_unit_req1[0]}				),
			.unmerge_pready_i	({inter_unit_pready1[5], inter_unit_pready1[0]}	),
			.merge_rsp_i     	({merge_rsp_i[5], merge_rsp_i[0]}								),
			.merge_req_o     	({merge_req_o[5], merge_req_o[0]}								),
			.merge_pready_o  	({merge_pready_o[5], merge_pready_o[0]}					),
			.unmerge_rsp_o   	({inter_unit_rsp1[5], inter_unit_rsp1[0]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit72 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req1[7], inter_unit_req1[2]}				),
			.unmerge_pready_i	({inter_unit_pready1[7], inter_unit_pready1[2]}	),
			.merge_rsp_i     	({merge_rsp_i[7], merge_rsp_i[2]}								),
			.merge_req_o     	({merge_req_o[7], merge_req_o[2]}								),
			.merge_pready_o  	({merge_pready_o[7], merge_pready_o[2]}					),
			.unmerge_rsp_o   	({inter_unit_rsp1[7], inter_unit_rsp1[2]}				)
		);
		
		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit31 (
			.clk_i           	(clk_i																							),
			.rst_ni          	(rst_ni																							),
			.unmerge_req_i   	({inter_unit_req0[3], inter_unit_req0[1]}						),
			.unmerge_pready_i	({inter_unit_pready0[3], inter_unit_pready0[1]}			),
			.merge_rsp_i     	({inter_unit_rsp1[3], inter_unit_rsp1[1]}						),
			.merge_req_o     	({inter_unit_req1[3], inter_unit_req1[1]}						),
			.merge_pready_o  	({inter_unit_pready1[3], inter_unit_pready1[1]}			),
			.unmerge_rsp_o   	({inter_unit_rsp0[3], inter_unit_rsp0[1]}						)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit86 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req0[8], inter_unit_req0[6]}				),
			.unmerge_pready_i	({inter_unit_pready0[8], inter_unit_pready0[6]}	),
			.merge_rsp_i     	({inter_unit_rsp1[8], inter_unit_rsp1[6]}				),
			.merge_req_o     	({inter_unit_req1[8], inter_unit_req1[6]}				),
			.merge_pready_o  	({inter_unit_pready1[8], inter_unit_pready1[6]}	),
			.unmerge_rsp_o   	({inter_unit_rsp0[8], inter_unit_rsp0[6]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit61 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req1[6], inter_unit_req1[1]}				),
			.unmerge_pready_i	({inter_unit_pready1[6], inter_unit_pready1[1]}	),
			.merge_rsp_i     	({merge_rsp_i[6], merge_rsp_i[1]}								),
			.merge_req_o     	({merge_req_o[6], merge_req_o[1]}								),
			.merge_pready_o  	({merge_pready_o[6], merge_pready_o[1]}					),
			.unmerge_rsp_o   	({inter_unit_rsp1[6], inter_unit_rsp1[1]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit83 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req1[8], inter_unit_req1[3]}				),
			.unmerge_pready_i	({inter_unit_pready1[8], inter_unit_pready1[3]}	),
			.merge_rsp_i     	({merge_rsp_i[8], merge_rsp_i[3]}								),
			.merge_req_o     	({merge_req_o[8], merge_req_o[3]}								),
			.merge_pready_o  	({merge_pready_o[8], merge_pready_o[3]}					),
			.unmerge_rsp_o   	({inter_unit_rsp1[8], inter_unit_rsp1[3]}				)
		);
	end
endmodule
