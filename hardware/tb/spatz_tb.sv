// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

/*import "DPI-C" function void read_elf (input string filename);
import "DPI-C" function byte get_section (output longint address, output longint len);
import "DPI-C" context function byte read_section(input longint address, inout byte buffer[]);*/

module spatz_tb;

	////////////////
	// Parameters //
	////////////////

	timeunit			1ns;
	timeprecision	1ps;

	localparam 		ClockPeriod = 1ns;

	logic clk;
	logic rst_n;

	always #(ClockPeriod/2) clk = !clk;

	initial begin
		clk 	= 1'b0;
		rst_n = 1'b0;

		repeat (5)
			#(ClockPeriod);

		rst_n = 1'b1;

		repeat (10)
			#(ClockPeriod)

		$finish;
	end

	///////////
	// Spatz //
	///////////

	spatz dut (
		.clk_i					(clk),
		.rst_ni					(rst_n),
		.instr_i        (32'h0c257557),
		.instr_illegal_o(),
		.instr_valid_i  (1'b1),
		.rs1_i          (32'd128),
		.rs2_i 					('0),
		.rd_o           ()
	);

endmodule : spatz_tb
