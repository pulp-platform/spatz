// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

module spatz import spatz_pkg::*; (
	input 	logic	clk_i,
	input 	logic rst_ni,
	output	logic	inv_o
);

	always_comb begin : proc_neg
		if (!rst_ni) begin
			inv_o = 1'b0;
		end else begin
			inv_o = !clk_i;
		end
	end

endmodule : spatz
