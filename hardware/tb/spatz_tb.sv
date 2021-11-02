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
	end

	///////////
	// Spatz //
	///////////

	logic inv;

	spatz dut (
		.clk_i	(clk),
		.rst_ni	(rst_n),
		.inv_o	(inv)
	);

endmodule : spatz_tb
