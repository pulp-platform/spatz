// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

/// RTL Top-level for `fesvr` simulation.
module tb_bin;
  import "DPI-C" function int fesvr_tick();

  // This can't have a unit otherwise the simulation will not advance, for
  // whatever reason.
  // verilog_lint: waive explicit-parameter-storage-type
  localparam TCK = `ifdef CLKPERIOD `CLKPERIOD `else 1ns `endif;

  logic rst_ni, clk_i;

  testharness i_dut (
    .clk_i,
    .rst_ni
  );

  initial begin
    rst_ni = 0;
    #20ns;
    rst_ni = 1;
  end

  // Generate reset and clock.
  initial begin
    clk_i = 0;
    #100ns;
    forever begin
      clk_i = 1;
      #(TCK/2);
      clk_i = 0;
      #(TCK/2);
    end
  end

  `ifdef TARGET_ZOIX
    `ifdef TARGET_POST_SYNTHESIS
      `include "strobe_tb_bin_FS_ps.sv"
      `include "strobe_tb_bin_FD_ps_CW.sv"
      `include "strobe_tb_bin_OC_OU_ps_CW.sv"
    `else
        `include "./strobe/strobe_tb_bin_FS.sv"
        `include "./strobe/strobe_tb_bin_OC_OU.sv"
        `include "./strobe/strobe_tb_bin_FD.sv"
    `endif
  `endif

  // Start `fesvr`.
  initial begin
    automatic int exit_code;

    do begin
      exit_code = fesvr_tick();

      if (exit_code == 0)
        #200ns;
    end while (exit_code == 0);

    exit_code >>= 1;

    if (exit_code == 0) begin
      $info("[SUCCESS] Program finished successfully");
    end else begin
      $error("[FAILURE] Finished with exit code %2d", exit_code);
    end
    $finish;
  end

endmodule
