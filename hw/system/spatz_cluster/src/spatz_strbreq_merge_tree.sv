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
	parameter type 			req_id_t							= logic,
	/// Req user type
	parameter type 			tcdm_user_t 					= logic
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
	end else if (MergeNum == 8) begin
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
	end else if (MergeNum == 16) begin
		mem_req_t	[MergeNum-1:0]	input_req, inter_unit_req0, inter_unit_req1, inter_unit_req2, output_req;
		logic 		[MergeNum-1:0]	input_preaqdy, inter_unit_pready0, inter_unit_pready1, inter_unit_pready2, output_pready;
		mem_rsp_t	[MergeNum-1:0]	output_rsp, inter_unit_rsp0, inter_unit_rsp1, inter_unit_rsp2, input_rsp;

		assign input_req[MergeNum/NumCores-1:0] = unmerge_req_i[MergeNum/NumCores-1:0];
		assign input_req[MergeNum-1:MergeNum/NumCores] = unmerge_req_i[NumIO-NumCores:NumIO/NumCores];
		assign input_preaqdy[MergeNum/NumCores-1:0] = unmerge_pready_i[MergeNum/NumCores-1:0];
		assign input_preaqdy[MergeNum-1:MergeNum/NumCores] = unmerge_pready_i[NumIO-NumCores:NumIO/NumCores];
		assign input_rsp[MergeNum/NumCores-1:0] = merge_rsp_i[MergeNum/NumCores-1:0];
		assign input_rsp[MergeNum-1:MergeNum/NumCores] = merge_rsp_i[NumIO-NumCores:NumIO/NumCores];

		assign merge_req_o[MergeNum/NumCores-1:0] = output_req[MergeNum/NumCores-1:0];
		assign merge_req_o[NumIO-NumCores:NumIO/NumCores] = output_req[MergeNum-1:MergeNum/NumCores];
		assign merge_pready_o[MergeNum/NumCores-1:0] = output_pready[MergeNum/NumCores-1:0];
		assign merge_pready_o[NumIO-NumCores:NumIO/NumCores] = output_pready[MergeNum-1:MergeNum/NumCores];
		assign unmerge_rsp_o[MergeNum/NumCores-1:0] = output_rsp[MergeNum/NumCores-1:0];
		assign unmerge_rsp_o[NumIO-NumCores:NumIO/NumCores] = output_rsp[MergeNum-1:MergeNum/NumCores];

		// Level 0
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
			.unmerge_req_i   	(input_req[1:0]				),
			.unmerge_pready_i	(input_preaqdy[1:0]		),
			.merge_rsp_i     	(inter_unit_rsp0[1:0]			),
			.merge_req_o     	(inter_unit_req0[1:0]			),
			.merge_pready_o  	(inter_unit_pready0[1:0]	),
			.unmerge_rsp_o   	(output_rsp[1:0]				)
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
			.unmerge_req_i   	(input_req[3:2]				),
			.unmerge_pready_i	(input_preaqdy[3:2]		),
			.merge_rsp_i     	(inter_unit_rsp0[3:2]			),
			.merge_req_o     	(inter_unit_req0[3:2]			),
			.merge_pready_o  	(inter_unit_pready0[3:2]	),
			.unmerge_rsp_o   	(output_rsp[3:2]				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit54 (
			.clk_i           	(clk_i										),
			.rst_ni          	(rst_ni										),
			.unmerge_req_i   	(input_req[5:4]				),
			.unmerge_pready_i	(input_preaqdy[5:4]		),
			.merge_rsp_i     	(inter_unit_rsp0[5:4]			),
			.merge_req_o     	(inter_unit_req0[5:4]			),
			.merge_pready_o  	(inter_unit_pready0[5:4]	),
			.unmerge_rsp_o   	(output_rsp[5:4]				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit76 (
			.clk_i           	(clk_i										),
			.rst_ni          	(rst_ni										),
			.unmerge_req_i   	(input_req[7:6]				),
			.unmerge_pready_i	(input_preaqdy[7:6]		),
			.merge_rsp_i     	(inter_unit_rsp0[7:6]			),
			.merge_req_o     	(inter_unit_req0[7:6]			),
			.merge_pready_o  	(inter_unit_pready0[7:6]	),
			.unmerge_rsp_o   	(output_rsp[7:6]				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit98 (
			.clk_i           	(clk_i										),
			.rst_ni          	(rst_ni										),
			.unmerge_req_i   	(input_req[9:8]				),
			.unmerge_pready_i	(input_preaqdy[9:8]		),
			.merge_rsp_i     	(inter_unit_rsp0[9:8]			),
			.merge_req_o     	(inter_unit_req0[9:8]			),
			.merge_pready_o  	(inter_unit_pready0[9:8]	),
			.unmerge_rsp_o   	(output_rsp[9:8]				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unitba (
			.clk_i           	(clk_i										),
			.rst_ni          	(rst_ni										),
			.unmerge_req_i   	(input_req[11:10]				),
			.unmerge_pready_i	(input_preaqdy[11:10]		),
			.merge_rsp_i     	(inter_unit_rsp0[11:10]			),
			.merge_req_o     	(inter_unit_req0[11:10]			),
			.merge_pready_o  	(inter_unit_pready0[11:10]	),
			.unmerge_rsp_o   	(output_rsp[11:10]				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unitdc (
			.clk_i           	(clk_i										),
			.rst_ni          	(rst_ni										),
			.unmerge_req_i   	(input_req[13:12]				),
			.unmerge_pready_i	(input_preaqdy[13:12]		),
			.merge_rsp_i     	(inter_unit_rsp0[13:12]			),
			.merge_req_o     	(inter_unit_req0[13:12]			),
			.merge_pready_o  	(inter_unit_pready0[13:12]	),
			.unmerge_rsp_o   	(output_rsp[13:12]				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unitfe (
			.clk_i           	(clk_i										),
			.rst_ni          	(rst_ni										),
			.unmerge_req_i   	(input_req[15:14]				),
			.unmerge_pready_i	(input_preaqdy[15:14]		),
			.merge_rsp_i     	(inter_unit_rsp0[15:14]			),
			.merge_req_o     	(inter_unit_req0[15:14]			),
			.merge_pready_o  	(inter_unit_pready0[15:14]	),
			.unmerge_rsp_o   	(output_rsp[15:14]				)
		);

		// Level 1
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
		) i_strbreq_merge_unit31 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req0[3], inter_unit_req0[1]}				),
			.unmerge_pready_i	({inter_unit_pready0[3], inter_unit_pready0[1]}	),
			.merge_rsp_i     	({inter_unit_rsp1[3], inter_unit_rsp1[1]}				),
			.merge_req_o     	({inter_unit_req1[3], inter_unit_req1[1]}				),
			.merge_pready_o  	({inter_unit_pready1[3], inter_unit_pready1[1]}	),
			.unmerge_rsp_o   	({inter_unit_rsp0[3], inter_unit_rsp0[1]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit64 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req0[6], inter_unit_req0[4]}				),
			.unmerge_pready_i	({inter_unit_pready0[6], inter_unit_pready0[4]}	),
			.merge_rsp_i     	({inter_unit_rsp1[6], inter_unit_rsp1[4]}				),
			.merge_req_o     	({inter_unit_req1[6], inter_unit_req1[4]}				),
			.merge_pready_o  	({inter_unit_pready1[6], inter_unit_pready1[4]}	),
			.unmerge_rsp_o   	({inter_unit_rsp0[6], inter_unit_rsp0[4]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit75 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
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
		) i_strbreq_merge_unita8 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req0[10], inter_unit_req0[8]}				),
			.unmerge_pready_i	({inter_unit_pready0[10], inter_unit_pready0[8]}	),
			.merge_rsp_i     	({inter_unit_rsp1[10], inter_unit_rsp1[8]}				),
			.merge_req_o     	({inter_unit_req1[10], inter_unit_req1[8]}				),
			.merge_pready_o  	({inter_unit_pready1[10], inter_unit_pready1[8]}	),
			.unmerge_rsp_o   	({inter_unit_rsp0[10], inter_unit_rsp0[8]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unitb9 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req0[11], inter_unit_req0[9]}				),
			.unmerge_pready_i	({inter_unit_pready0[11], inter_unit_pready0[9]}	),
			.merge_rsp_i     	({inter_unit_rsp1[11], inter_unit_rsp1[9]}				),
			.merge_req_o     	({inter_unit_req1[11], inter_unit_req1[9]}				),
			.merge_pready_o  	({inter_unit_pready1[11], inter_unit_pready1[9]}	),
			.unmerge_rsp_o   	({inter_unit_rsp0[11], inter_unit_rsp0[9]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unitec (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req0[14], inter_unit_req0[12]}				),
			.unmerge_pready_i	({inter_unit_pready0[14], inter_unit_pready0[12]}	),
			.merge_rsp_i     	({inter_unit_rsp1[14], inter_unit_rsp1[12]}				),
			.merge_req_o     	({inter_unit_req1[14], inter_unit_req1[12]}				),
			.merge_pready_o  	({inter_unit_pready1[14], inter_unit_pready1[12]}	),
			.unmerge_rsp_o   	({inter_unit_rsp0[14], inter_unit_rsp0[12]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unitfd (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req0[15], inter_unit_req0[13]}				),
			.unmerge_pready_i	({inter_unit_pready0[15], inter_unit_pready0[13]}	),
			.merge_rsp_i     	({inter_unit_rsp1[15], inter_unit_rsp1[13]}				),
			.merge_req_o     	({inter_unit_req1[15], inter_unit_req1[13]}				),
			.merge_pready_o  	({inter_unit_pready1[15], inter_unit_pready1[13]}	),
			.unmerge_rsp_o   	({inter_unit_rsp0[15], inter_unit_rsp0[13]}				)
		);

		// Level 2
		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit40 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req1[4], inter_unit_req1[0]}				),
			.unmerge_pready_i	({inter_unit_pready1[4], inter_unit_pready1[0]}	),
			.merge_rsp_i     	({inter_unit_rsp2[4], inter_unit_rsp2[0]}				),
			.merge_req_o     	({inter_unit_req2[4], inter_unit_req2[0]}				),
			.merge_pready_o  	({inter_unit_pready2[4], inter_unit_pready2[0]}	),
			.unmerge_rsp_o   	({inter_unit_rsp1[4], inter_unit_rsp1[0]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit62 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req1[6], inter_unit_req1[2]}				),
			.unmerge_pready_i	({inter_unit_pready1[6], inter_unit_pready1[2]}	),
			.merge_rsp_i     	({inter_unit_rsp2[6], inter_unit_rsp2[2]}				),
			.merge_req_o     	({inter_unit_req2[6], inter_unit_req2[2]}				),
			.merge_pready_o  	({inter_unit_pready2[6], inter_unit_pready2[2]}	),
			.unmerge_rsp_o   	({inter_unit_rsp1[6], inter_unit_rsp1[2]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit51 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req1[5], inter_unit_req1[1]}				),
			.unmerge_pready_i	({inter_unit_pready1[5], inter_unit_pready1[1]}	),
			.merge_rsp_i     	({inter_unit_rsp2[5], inter_unit_rsp2[1]}				),
			.merge_req_o     	({inter_unit_req2[5], inter_unit_req2[1]}				),
			.merge_pready_o  	({inter_unit_pready2[5], inter_unit_pready2[1]}	),
			.unmerge_rsp_o   	({inter_unit_rsp1[5], inter_unit_rsp1[1]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit73 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req1[7], inter_unit_req1[3]}				),
			.unmerge_pready_i	({inter_unit_pready1[7], inter_unit_pready1[3]}	),
			.merge_rsp_i     	({inter_unit_rsp2[7], inter_unit_rsp2[3]}				),
			.merge_req_o     	({inter_unit_req2[7], inter_unit_req2[3]}				),
			.merge_pready_o  	({inter_unit_pready2[7], inter_unit_pready2[3]}	),
			.unmerge_rsp_o   	({inter_unit_rsp1[7], inter_unit_rsp1[3]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unitc8 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req1[12], inter_unit_req1[8]}				),
			.unmerge_pready_i	({inter_unit_pready1[12], inter_unit_pready1[8]}	),
			.merge_rsp_i     	({inter_unit_rsp2[12], inter_unit_rsp2[8]}				),
			.merge_req_o     	({inter_unit_req2[12], inter_unit_req2[8]}				),
			.merge_pready_o  	({inter_unit_pready2[12], inter_unit_pready2[8]}	),
			.unmerge_rsp_o   	({inter_unit_rsp1[12], inter_unit_rsp1[8]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unitea (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req1[14], inter_unit_req1[10]}				),
			.unmerge_pready_i	({inter_unit_pready1[14], inter_unit_pready1[10]}	),
			.merge_rsp_i     	({inter_unit_rsp2[14], inter_unit_rsp2[10]}				),
			.merge_req_o     	({inter_unit_req2[14], inter_unit_req2[10]}				),
			.merge_pready_o  	({inter_unit_pready2[14], inter_unit_pready2[10]}	),
			.unmerge_rsp_o   	({inter_unit_rsp1[14], inter_unit_rsp1[10]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unitd9 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req1[13], inter_unit_req1[9]}				),
			.unmerge_pready_i	({inter_unit_pready1[13], inter_unit_pready1[9]}	),
			.merge_rsp_i     	({inter_unit_rsp2[13], inter_unit_rsp2[9]}				),
			.merge_req_o     	({inter_unit_req2[13], inter_unit_req2[9]}				),
			.merge_pready_o  	({inter_unit_pready2[13], inter_unit_pready2[9]}	),
			.unmerge_rsp_o   	({inter_unit_rsp1[13], inter_unit_rsp1[9]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unitfb (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req1[15], inter_unit_req1[11]}				),
			.unmerge_pready_i	({inter_unit_pready1[15], inter_unit_pready1[11]}	),
			.merge_rsp_i     	({inter_unit_rsp2[15], inter_unit_rsp2[11]}				),
			.merge_req_o     	({inter_unit_req2[15], inter_unit_req2[11]}				),
			.merge_pready_o  	({inter_unit_pready2[15], inter_unit_pready2[11]}	),
			.unmerge_rsp_o   	({inter_unit_rsp1[15], inter_unit_rsp1[11]}				)
		);

		// Level 3
		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit80 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req2[8], inter_unit_req2[0]}				),
			.unmerge_pready_i	({inter_unit_pready2[8], inter_unit_pready2[0]}	),
			.merge_rsp_i     	({input_rsp[8], input_rsp[0]}								),
			.merge_req_o     	({output_req[8], output_req[0]}								),
			.merge_pready_o  	({output_pready[8], output_pready[0]}					),
			.unmerge_rsp_o   	({inter_unit_rsp2[8], inter_unit_rsp2[0]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unitc4 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req2[12], inter_unit_req2[4]}				),
			.unmerge_pready_i	({inter_unit_pready2[12], inter_unit_pready2[4]}	),
			.merge_rsp_i     	({input_rsp[12], input_rsp[4]}								),
			.merge_req_o     	({output_req[12], output_req[4]}								),
			.merge_pready_o  	({output_pready[12], output_pready[4]}					),
			.unmerge_rsp_o   	({inter_unit_rsp2[12], inter_unit_rsp2[4]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unita2 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req2[10], inter_unit_req2[2]}				),
			.unmerge_pready_i	({inter_unit_pready2[10], inter_unit_pready2[2]}	),
			.merge_rsp_i     	({input_rsp[10], input_rsp[2]}								),
			.merge_req_o     	({output_req[10], output_req[2]}								),
			.merge_pready_o  	({output_pready[10], output_pready[2]}					),
			.unmerge_rsp_o   	({inter_unit_rsp2[10], inter_unit_rsp2[2]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unite6 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req2[14], inter_unit_req2[6]}				),
			.unmerge_pready_i	({inter_unit_pready2[14], inter_unit_pready2[6]}	),
			.merge_rsp_i     	({input_rsp[14], input_rsp[6]}								),
			.merge_req_o     	({output_req[14], output_req[6]}								),
			.merge_pready_o  	({output_pready[14], output_pready[6]}					),
			.unmerge_rsp_o   	({inter_unit_rsp2[14], inter_unit_rsp2[6]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unit91 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req2[9], inter_unit_req2[1]}				),
			.unmerge_pready_i	({inter_unit_pready2[9], inter_unit_pready2[1]}	),
			.merge_rsp_i     	({input_rsp[9], input_rsp[1]}								),
			.merge_req_o     	({output_req[9], output_req[1]}								),
			.merge_pready_o  	({output_pready[9], output_pready[1]}					),
			.unmerge_rsp_o   	({inter_unit_rsp2[9], inter_unit_rsp2[1]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unitd5 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req2[13], inter_unit_req2[5]}				),
			.unmerge_pready_i	({inter_unit_pready2[13], inter_unit_pready2[5]}	),
			.merge_rsp_i     	({input_rsp[13], input_rsp[5]}								),
			.merge_req_o     	({output_req[13], output_req[5]}								),
			.merge_pready_o  	({output_pready[13], output_pready[5]}					),
			.unmerge_rsp_o   	({inter_unit_rsp2[13], inter_unit_rsp2[5]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unitb3 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req2[11], inter_unit_req2[3]}				),
			.unmerge_pready_i	({inter_unit_pready2[11], inter_unit_pready2[3]}	),
			.merge_rsp_i     	({input_rsp[11], input_rsp[3]}								),
			.merge_req_o     	({output_req[11], output_req[3]}								),
			.merge_pready_o  	({output_pready[11], output_pready[3]}					),
			.unmerge_rsp_o   	({inter_unit_rsp2[11], inter_unit_rsp2[3]}				)
		);

		spatz_strbreq_merge_unit #(
			.DataWidth					(DataWidth					),
			.NumOutstandingMem 	(NumOutstandingMem 	),
			.mem_req_t					(mem_req_t					),
			.mem_rsp_t					(mem_rsp_t					),
			.req_id_t						(req_id_t						),
			.tcdm_user_t 				(tcdm_user_t				)
		) i_strbreq_merge_unitf7 (
			.clk_i           	(clk_i																					),
			.rst_ni          	(rst_ni																					),
			.unmerge_req_i   	({inter_unit_req2[15], inter_unit_req2[7]}				),
			.unmerge_pready_i	({inter_unit_pready2[15], inter_unit_pready2[7]}	),
			.merge_rsp_i     	({input_rsp[15], input_rsp[7]}								),
			.merge_req_o     	({output_req[15], output_req[7]}								),
			.merge_pready_o  	({output_pready[15], output_pready[7]}					),
			.unmerge_rsp_o   	({inter_unit_rsp2[15], inter_unit_rsp2[7]}				)
		);
	end
endmodule