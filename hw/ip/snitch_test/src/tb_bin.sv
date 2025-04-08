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

  // Monitor VFU cycles.
  int cycles_total         = 0;
  int cycles_vfu_reduction = 0;
  `define CORE0_SPATZ_VFU_REDUCTION_STATE \
      i_dut.i_cluster_wrapper.i_cluster.gen_core[0].i_spatz_cc.i_spatz.i_vfu.reduction_state_q

  initial begin
    forever begin
      @(posedge clk_i);
      if (i_dut.cluster_probe) begin
        cycles_total++;
        if (`CORE0_SPATZ_VFU_REDUCTION_STATE != 3'b000) begin
          cycles_vfu_reduction++;
        end
      end
    end
  end

  // Start `fesvr`.
  initial begin
    automatic int exit_code;

    do begin
      exit_code = fesvr_tick();

      if (exit_code == 0)
        #200ns;
    end while (exit_code == 0);
    exit_code >>= 1;
    if (exit_code > 0) begin
      $error("[FAILURE] Finished with exit code %2d", exit_code);
    end else begin
      $info("[SUCCESS] Program finished successfully");
    end

    // statistics
    if (cycles_total) begin
      $display("");
      $display("=== STATISTICS ===");
      $display("benchmark runtime:  %8d cycles (100.0 %%)", cycles_total);
      $display("VFU reduction:      %8d cycles (%5.1f %%)",
        cycles_vfu_reduction, 100.0 * cycles_vfu_reduction / cycles_total);
    end

    $finish;
  end

endmodule
