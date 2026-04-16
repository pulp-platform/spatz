// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Tim Fischer <fischeti@iis.ee.ethz.ch>
// Edited by: Cyrill Durrer <cdurrer@iis.ee.ethz.ch>

module fixture_picobello_chip;

  `include "cheshire/typedef.svh"

  import cheshire_pkg::*;
  import picobello_pkg::*;

  `CHESHIRE_TYPEDEF_ALL(, CheshireCfg)

  ///////////
  //  DUT  //
  ///////////

  wire pad_ref_clk;
  wire pad_rst_n;
  wire pad_rtc_clk;
  wire pad_div_clk_out;
  wire pad_bypass_clk;
  wire pad_test_mode;
  wire [1:0] pad_boot_mode;
  wire pad_clk_rst_bypass;

  logic soc_clk;

  // JTAG
  wire pad_jtag_tck;
  wire pad_jtag_trst_n;
  wire pad_jtag_tms;
  wire pad_jtag_tdi;
  wire pad_jtag_tdo;

  // UART
  wire pad_uart_tx;
  wire pad_uart_rx;

  // I2C
  wire pad_i2c_scl;
  wire pad_i2c_sda;

  // SPI
  wire pad_spih_sck;
  wire [SpihNumCs-1:0] pad_spih_csb;
  wire [3:0] pad_spih_sd;

  // GPIO
  wire [31:0] pad_gpio;

  // Serial Link
  wire [SlinkNumChan-1:0] pad_slink_rcv_clk_in;
  wire [SlinkNumChan-1:0] pad_slink_rcv_clk_out;
  wire [SlinkNumChan-1:0][SlinkNumLanes-1:0] pad_slink_in;
  wire [SlinkNumChan-1:0][SlinkNumLanes-1:0] pad_slink_out;

  wire [SlinkNumChan-1:0]                    pad_dram_slink_rcv_clk_i;
  wire [SlinkNumChan-1:0]                    pad_dram_slink_rcv_clk_o;
  wire [SlinkNumChan-1:0][SlinkNumLanes-1:0] pad_dram_slink_i;
  wire [SlinkNumChan-1:0][SlinkNumLanes-1:0] pad_dram_slink_o;

  // Bypass mode
  assign pad_clk_rst_bypass = 1'b0; // Set to 1 to bypass tile-specific clock gating and reset (use global signals instead)


  picobello_chip dut (
    .pad_ref_clk_i               (pad_ref_clk),
    .pad_rst_ni                  (pad_rst_n),
    .pad_rtc_clk_i               (pad_rtc_clk),
    .pad_div_clk_o               (pad_div_clk_out),
    .pad_bypass_fll_i            (pad_bypass_clk),
    .pad_test_mode_i             (pad_test_mode),
    .pad_boot_mode_i             (pad_boot_mode),
    .pad_clk_rst_bypass_i        (pad_clk_rst_bypass),      // default: 0 (use tile-specific clock gating and reset)
    .pad_jtag_tck_i              (pad_jtag_tck),
    .pad_jtag_trst_ni            (pad_jtag_trst_n),
    .pad_jtag_tms_i              (pad_jtag_tms),
    .pad_jtag_tdi_i              (pad_jtag_tdi),
    .pad_jtag_tdo_o              (pad_jtag_tdo),
    .pad_uart_tx_o               (pad_uart_tx),
    .pad_uart_rx_i               (pad_uart_rx),
    .pad_i2c_scl_io              (pad_i2c_scl),
    .pad_i2c_sda_io              (pad_i2c_sda),
    .pad_spih_sck_o              (pad_spih_sck),
    .pad_spih_csb_o              (pad_spih_csb),
    .pad_spih_sd_io              (pad_spih_sd),
    .pad_gpio_io                 (pad_gpio),
    .pad_slink_rcv_clk_i         (pad_slink_rcv_clk_in),
    .pad_slink_rcv_clk_o         (pad_slink_rcv_clk_out),
    .pad_slink_i                 (pad_slink_in),
    .pad_slink_o                 (pad_slink_out),
    .pad_dram_slink_rcv_clk_i    (pad_dram_slink_rcv_clk_i),
    .pad_dram_slink_rcv_clk_o    (pad_dram_slink_rcv_clk_o),
    .pad_dram_slink_i            (pad_dram_slink_i),
    .pad_dram_slink_o            (pad_dram_slink_o)
  );

  ///////////
  //  VIP  //
  ///////////

  axi_mst_req_t axi_slink_mst_req;
  axi_mst_rsp_t axi_slink_mst_rsp;

  axi_llc_req_t axi_llc_mst_req;
  axi_llc_rsp_t axi_llc_mst_rsp;

  assign axi_slink_mst_req = '0;

  // Mirror instance of serial link, reflecting DRAM on FPGA
  serial_link #(
    .axi_req_t    ( axi_llc_req_t ),
    .axi_rsp_t    ( axi_llc_rsp_t ),
    .cfg_req_t    ( reg_req_t ),
    .cfg_rsp_t    ( reg_rsp_t ),
    .aw_chan_t    ( axi_llc_aw_chan_t ),
    .ar_chan_t    ( axi_llc_ar_chan_t ),
    .r_chan_t     ( axi_llc_r_chan_t  ),
    .w_chan_t     ( axi_llc_w_chan_t  ),
    .b_chan_t     ( axi_llc_b_chan_t  ),
    .hw2reg_t     ( serial_link_single_channel_reg_pkg::serial_link_single_channel_hw2reg_t ),
    .reg2hw_t     ( serial_link_single_channel_reg_pkg::serial_link_single_channel_reg2hw_t ),
    .NumChannels  ( SlinkNumChan   ),
    .NumLanes     ( SlinkNumLanes  ),
    .MaxClkDiv    ( SlinkMaxClkDiv )
  ) i_dram_serial_link (
    .clk_i          ( soc_clk ),
    .rst_ni         ( pad_rst_n ),
    .clk_sl_i       ( soc_clk ),
    .rst_sl_ni      ( pad_rst_n ),
    .clk_reg_i      ( soc_clk ),
    .rst_reg_ni     ( pad_rst_n ),
    .testmode_i     ( 1'b0 ),
    .axi_in_req_i   ( '0 ),
    .axi_in_rsp_o   ( ),
    .axi_out_req_o  ( axi_llc_mst_req ),
    .axi_out_rsp_i  ( axi_llc_mst_rsp ),
    .cfg_req_i      ( '0 ),
    .cfg_rsp_o      ( ),
    .ddr_rcv_clk_i  ( pad_dram_slink_rcv_clk_o ),
    .ddr_rcv_clk_o  ( pad_dram_slink_rcv_clk_i ),
    .ddr_i          ( pad_dram_slink_o ),
    .ddr_o          ( pad_dram_slink_i ),
    .isolated_i     ( '0 ),
    .isolate_o      ( ),
    .clk_ena_o      ( ),
    .reset_no       ( )
  );

  vip_cheshire_soc #(
    .DutCfg            ( CheshireCfg   ),
    .UseDramSys        ( 1'b0          ),
    .axi_ext_llc_req_t ( axi_llc_req_t ),
    .axi_ext_llc_rsp_t ( axi_llc_rsp_t ),
    .axi_ext_mst_req_t ( axi_mst_req_t ),
    .axi_ext_mst_rsp_t ( axi_mst_rsp_t )
  ) vip (
    .clk               ( soc_clk               ),
    .rst_n             ( pad_rst_n             ),
    .test_mode         ( pad_test_mode         ),
    .boot_mode         ( pad_boot_mode         ),
    .rtc               ( pad_rtc_clk           ),
    .axi_llc_mst_req   ( axi_llc_mst_req       ),
    .axi_llc_mst_rsp   ( axi_llc_mst_rsp       ),
    .axi_slink_mst_req ( axi_slink_mst_req     ),
    .axi_slink_mst_rsp ( axi_slink_mst_rsp     ),
    .jtag_tck          ( pad_jtag_tck          ),
    .jtag_trst_n       ( pad_jtag_trst_n       ),
    .jtag_tms          ( pad_jtag_tms          ),
    .jtag_tdi          ( pad_jtag_tdi          ),
    .jtag_tdo          ( pad_jtag_tdo          ),
    .uart_tx           ( pad_uart_tx           ),
    .uart_rx           ( pad_uart_rx           ),
    .i2c_sda           ( pad_i2c_sda           ),
    .i2c_scl           ( pad_i2c_scl           ),
    .spih_sck          ( pad_spih_sck          ),
    .spih_csb          ( pad_spih_csb          ),
    .spih_sd           ( pad_spih_sd           ),
    .slink_rcv_clk_i   ( pad_slink_rcv_clk_in  ),
    .slink_rcv_clk_o   ( pad_slink_rcv_clk_out ),
    .slink_i           ( pad_slink_in          ),
    .slink_o           ( pad_slink_out         )
  );

  vip_fll vip_fll (
    .bypass_clk ( pad_bypass_clk ),
    .rtc_clk    ( pad_rtc_clk    ),
    .soc_clk    ( soc_clk        ),
    .ref_clk    ( pad_ref_clk    )
  );

endmodule

module vip_fll(
  output logic bypass_clk,
  input  logic rtc_clk,
  input  logic soc_clk,
  output logic ref_clk
);

  logic ref_clk_sel;

  // Initially, we bypass the FLL
  initial begin
    ref_clk_sel = 1'b0;
    bypass_clk = 1'b1;
  end

  always_comb begin
    ref_clk = ref_clk_sel ? rtc_clk : soc_clk;
  end

  // Enable the FLL clock
  task automatic disable_bypass();
    bypass_clk = 1'b0;
  endtask

  // Apply the slow reference clock
  task automatic apply_ref_clk();
    ref_clk_sel = 1'b1; // Select the RTC clock as reference
  endtask

  // Wait for the FLL to lock
  task automatic lock_wait();
    // Wait for the FLL to lock
    wait(dut.i_tsmc7_FLL.LOCK); // Replace with actual condition when FLL is locked
  endtask

endmodule
