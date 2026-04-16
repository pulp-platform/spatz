// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Tim Fischer <fischeti@iis.ee.ethz.ch>

module picobello_chip
  import tsmc7_pkg::*;
  import cheshire_pkg::*;
  import picobello_pkg::*;
(
  inout wire pad_ref_clk_i,
  inout wire pad_rst_ni,
  inout wire pad_rtc_clk_i,
  inout wire pad_div_clk_o,
  inout wire pad_bypass_fll_i,
  inout wire pad_test_mode_i,
  inout wire [1:0] pad_boot_mode_i,
  inout wire pad_clk_rst_bypass_i,
  // JTAG
  inout wire pad_jtag_tck_i,
  inout wire pad_jtag_trst_ni,
  inout wire pad_jtag_tms_i,
  inout wire pad_jtag_tdi_i,
  inout wire pad_jtag_tdo_o,
  // UART interface
  inout wire pad_uart_tx_o,
  inout wire pad_uart_rx_i,
  // I2C interface
  inout wire pad_i2c_scl_io,
  inout wire pad_i2c_sda_io,
  // SPI host interface
  inout wire pad_spih_sck_o,
  inout wire [SpihNumCs-1:0] pad_spih_csb_o,
  inout wire [3:0] pad_spih_sd_io,
  // GPIO interface
  inout wire [31:0] pad_gpio_io,
  // Serial link interface
  inout wire [SlinkNumChan-1:0] pad_slink_rcv_clk_i,
  inout wire [SlinkNumChan-1:0] pad_slink_rcv_clk_o,
  inout wire [SlinkNumChan-1:0][SlinkNumLanes-1:0] pad_slink_i,
  inout wire [SlinkNumChan-1:0][SlinkNumLanes-1:0] pad_slink_o,
  inout wire [SlinkNumChan-1:0] pad_dram_slink_rcv_clk_i,
  inout wire [SlinkNumChan-1:0] pad_dram_slink_rcv_clk_o,
  inout wire [SlinkNumChan-1:0][SlinkNumLanes-1:0] pad_dram_slink_i,
  inout wire [SlinkNumChan-1:0][SlinkNumLanes-1:0] pad_dram_slink_o
);

  `include "common_cells/registers.svh"
  `include "pad_sv_macros.svh"

  csh_reg_req_t [CshRegExtChipCtrl:CshRegExtFLL] reg_req;
  csh_reg_rsp_t [CshRegExtChipCtrl:CshRegExtFLL] reg_rsp;

  `include "apb/typedef.svh"

  `APB_TYPEDEF_ALL(apb, logic [CheshireCfg.AddrWidth-1:0], logic [31:0], logic [3:0])

  io_pad_ctrl_t io_pad_ctrl_in, io_pad_ctrl_in_pd, io_pad_ctrl_in_pu;
  io_pad_ctrl_t io_pad_ctrl_out, io_pad_ctrl_out_pd, io_pad_ctrl_out_pu;

  logic rte;

  PRTEDISABLE i_PRTEDISABLE (
    .RTE(rte)
  );

  ////////////////////////////
  //  Clock/Reset I/O Pads  //
  ////////////////////////////

  logic fll_clk;
  logic ref_clk_i;
  logic soc_clk;
  logic rtc_clk_i;
  logic div_clk_o;
  logic bypass_fll_i;
  logic rst_ni;
  logic test_mode_i;
  logic [1:0] boot_mode_i;
  logic clk_rst_bypass_i;

  `TSMC7_INPUT_IO_PAD_H(pad_ref_clk_i, ref_clk_i, io_pad_ctrl_in)
  `TSMC7_INPUT_IO_PAD_H(pad_rst_ni, rst_ni, io_pad_ctrl_in_pu)
  `TSMC7_INPUT_IO_PAD_H(pad_bypass_fll_i, bypass_fll_i, io_pad_ctrl_in_pd)
  `TSMC7_INPUT_IO_PAD_H(pad_rtc_clk_i, rtc_clk_i, io_pad_ctrl_in)
  `TSMC7_OUTPUT_IO_PAD_H(pad_div_clk_o, div_clk_o, io_pad_ctrl_out)
  `TSMC7_INPUT_IO_PAD_H(pad_test_mode_i, test_mode_i, io_pad_ctrl_in_pd)
  `TSMC7_INPUT_IO_PAD_ARRAY_H(pad_boot_mode_i, boot_mode_i, io_pad_ctrl_in_pd, 2)
  `TSMC7_INPUT_IO_PAD_H(pad_clk_rst_bypass_i, clk_rst_bypass_i, io_pad_ctrl_in_pd)

  //////////////////////
  //  Ctrl Registers  //
  //////////////////////

  apb_req_t ctrl_apb_req;
  apb_resp_t ctrl_apb_rsp;

  import pb_chip_regs_pkg::*;
  typedef logic [PB_CHIP_REGS_MIN_ADDR_WIDTH-1:0] ctrl_addr_t;

  pb_chip_regs__in_t hwif_in;
  pb_chip_regs__out_t hwif_out;

  reg_to_apb #(
    .reg_req_t(csh_reg_req_t),
    .reg_rsp_t(csh_reg_rsp_t),
    .apb_req_t(apb_req_t),
    .apb_rsp_t(apb_resp_t)
  ) i_chip_ctrl_reg_to_apb (
    .clk_i    (soc_clk),
    .rst_ni   (rst_ni),
    .reg_req_i(reg_req[CshRegExtChipCtrl]),
    .reg_rsp_o(reg_rsp[CshRegExtChipCtrl]),
    .apb_req_o(ctrl_apb_req),
    .apb_rsp_i(ctrl_apb_rsp)
  );

  pb_chip_regs i_pb_chip_regs (
    .clk          (soc_clk),
    .arst_n       (rst_ni),
    .s_apb_psel   (ctrl_apb_req.psel),
    .s_apb_penable(ctrl_apb_req.penable),
    .s_apb_pwrite (ctrl_apb_req.pwrite),
    .s_apb_pprot  (ctrl_apb_req.pprot),
    .s_apb_paddr  (ctrl_addr_t'(ctrl_apb_req.paddr)),
    .s_apb_pwdata (ctrl_apb_req.pwdata),
    .s_apb_pstrb  (ctrl_apb_req.pstrb),
    .s_apb_pready (ctrl_apb_rsp.pready),
    .s_apb_prdata (ctrl_apb_rsp.prdata),
    .s_apb_pslverr(ctrl_apb_rsp.pslverr),
    .hwif_in      (hwif_in),
    .hwif_out     (hwif_out)
  );

  always_comb begin
    io_pad_ctrl_in = DefaultInputIoPadCtrl;
    io_pad_ctrl_out = DefaultOutputIoPadCtrl;
    io_pad_ctrl_out.ds = hwif_out.io_ctrl.drv_strength.value;
    io_pad_ctrl_in_pd = io_pad_ctrl_in;
    io_pad_ctrl_in_pd.pd = 1'b1; // Pull down
    io_pad_ctrl_in_pu = io_pad_ctrl_in;
    io_pad_ctrl_in_pu.pu = 1'b1; // Pull up
    io_pad_ctrl_out_pd = io_pad_ctrl_out;
    io_pad_ctrl_out_pd.pd = 1'b1; // Pull down
    io_pad_ctrl_out_pu = io_pad_ctrl_out;
    io_pad_ctrl_out_pu.pu = 1'b1; // Pull up
  end

  /////////////////////
  //  JTAG I/O Pads  //
  /////////////////////

  logic jtag_tck_i;
  logic jtag_trst_ni;
  logic jtag_tms_i;
  logic jtag_tdi_i;
  logic jtag_tdo_o;
  logic jtag_tdo_oe_o;
  io_pad_ctrl_t jtag_tdo_ctrl;

  `TSMC7_INPUT_IO_PAD_H(pad_jtag_tck_i, jtag_tck_i, io_pad_ctrl_in_pd)
  `TSMC7_INPUT_IO_PAD_H(pad_jtag_trst_ni, jtag_trst_ni, io_pad_ctrl_in_pu)
  `TSMC7_INPUT_IO_PAD_H(pad_jtag_tms_i, jtag_tms_i, io_pad_ctrl_in_pd)
  `TSMC7_INPUT_IO_PAD_H(pad_jtag_tdi_i, jtag_tdi_i, io_pad_ctrl_in_pd)
  `TSMC7_OUTPUT_IO_PAD_H(pad_jtag_tdo_o, jtag_tdo_o, jtag_tdo_ctrl)

  always_comb begin
    jtag_tdo_ctrl = io_pad_ctrl_out;
    jtag_tdo_ctrl.oe = jtag_tdo_oe_o;
    jtag_tdo_ctrl.pd = 1'b1;
  end

  /////////////////////
  //  UART I/O Pads  //
  /////////////////////

  logic uart_tx_o;
  logic uart_rx_i;

  `TSMC7_OUTPUT_IO_PAD_H(pad_uart_tx_o, uart_tx_o, io_pad_ctrl_out_pu)
  `TSMC7_INPUT_IO_PAD_H(pad_uart_rx_i, uart_rx_i, io_pad_ctrl_in_pu)

  ////////////////////
  //  I2C I/O Pads  //
  ////////////////////

  logic i2c_sda_o, i2c_sda_i;
  logic i2c_scl_o, i2c_scl_i;
  logic i2c_sda_en_o, i2c_scl_en_o;
  io_pad_ctrl_t i2c_sda_ctrl;
  io_pad_ctrl_t i2c_scl_ctrl;

  `TSMC7_INOUT_IO_PAD_V(pad_i2c_sda_io, i2c_sda_i, i2c_sda_o, i2c_sda_ctrl)
  `TSMC7_INOUT_IO_PAD_V(pad_i2c_scl_io, i2c_scl_i, i2c_scl_o, i2c_scl_ctrl)

  always_comb begin
    // I2C signals should be pull ups
    i2c_sda_ctrl = io_pad_ctrl_out_pu;
    i2c_scl_ctrl = io_pad_ctrl_out_pu;
    i2c_sda_ctrl.oe = i2c_sda_en_o;
    i2c_scl_ctrl.oe = i2c_scl_en_o;
    i2c_sda_ctrl.ie = ~i2c_sda_en_o;
    i2c_scl_ctrl.ie = ~i2c_scl_en_o;
  end

  ////////////////////
  //  SPI I/O Pads  //
  ////////////////////

  logic spih_sck_o, spih_sck_en_o;
  logic [SpihNumCs-1:0] spih_csb_o, spih_csb_en_o;
  logic [3:0] spih_sd_i, spih_sd_o, spih_sd_en_o;
  io_pad_ctrl_t spih_sck_ctrl;
  io_pad_ctrl_t spih_csb_ctrl;
  io_pad_ctrl_t spih_sd_ctrl;

  `TSMC7_OUTPUT_IO_PAD_V(pad_spih_sck_o, spih_sck_o, spih_sck_ctrl)
  `TSMC7_OUTPUT_IO_PAD_ARRAY_V(pad_spih_csb_o, spih_csb_o, spih_csb_ctrl, SpihNumCs)
  `TSMC7_INOUT_IO_PAD_ARRAY_V(pad_spih_sd_io, spih_sd_i, spih_sd_o, spih_sd_ctrl, 4)

  always_comb begin
    spih_sck_ctrl = io_pad_ctrl_out;
    spih_sck_ctrl.oe = spih_sck_en_o;
    spih_csb_ctrl = io_pad_ctrl_out_pu;
    spih_csb_ctrl.oe = &spih_csb_en_o;
    spih_sd_ctrl = io_pad_ctrl_out;
    spih_sd_ctrl.oe = &spih_sd_en_o;
    spih_sd_ctrl.ie = ~(&spih_sd_en_o);
  end

  /////////////////////
  //  GPIO I/O Pads  //
  /////////////////////

  localparam int unsigned NumGpio = 7;

  logic [31:0] gpio_i, gpio_o, gpio_en_o;
  io_pad_ctrl_t [NumGpio-1:0] gpio_ctrl;

  for (genvar i = 0; i < NumGpio; i++) begin : gen_gpio
    `TSMC7_IO_PAD_TEMPLATE(V, gpio_io, pad_gpio_io[i], gpio_i[i], gpio_o[i], gpio_ctrl[i])

    always_comb begin
      gpio_ctrl[i] = io_pad_ctrl_out;
      gpio_ctrl[i].oe = gpio_en_o[i];
      gpio_ctrl[i].ie = ~gpio_en_o[i];
    end
  end

  assign gpio_i[31:NumGpio] = '0; // Tie-off unused GPIOs

  ////////////////////////////
  //  Serial Link I/O Pads  //
  ////////////////////////////

  logic [SlinkNumChan-1:0]                     slink_rcv_clk_i;
  logic [SlinkNumChan-1:0]                     slink_rcv_clk_o;
  logic [SlinkNumChan-1:0][SlinkNumLanes-1:0]  slink_i;
  logic [SlinkNumChan-1:0][SlinkNumLanes-1:0]  slink_o;

  logic [SlinkNumChan-1:0]                     dram_slink_rcv_clk_i;
  logic [SlinkNumChan-1:0]                     dram_slink_rcv_clk_o;
  logic [SlinkNumChan-1:0][SlinkNumLanes-1:0]  dram_slink_i;
  logic [SlinkNumChan-1:0][SlinkNumLanes-1:0]  dram_slink_o;

  `TSMC7_INPUT_IO_PAD_ARRAY_V(pad_slink_rcv_clk_i, slink_rcv_clk_i, io_pad_ctrl_in, SlinkNumChan)
  `TSMC7_OUTPUT_IO_PAD_ARRAY_V(pad_slink_rcv_clk_o, slink_rcv_clk_o, io_pad_ctrl_out, SlinkNumChan)
  `TSMC7_INPUT_IO_PAD_2DARRAY_V(pad_slink_i, slink_i, io_pad_ctrl_in, SlinkNumChan, SlinkNumLanes)
  `TSMC7_OUTPUT_IO_PAD_2DARRAY_V(pad_slink_o, slink_o, io_pad_ctrl_out, SlinkNumChan, SlinkNumLanes)

  `TSMC7_INPUT_IO_PAD_ARRAY_H(pad_dram_slink_rcv_clk_i, dram_slink_rcv_clk_i, io_pad_ctrl_in, SlinkNumChan)
  `TSMC7_OUTPUT_IO_PAD_ARRAY_H(pad_dram_slink_rcv_clk_o, dram_slink_rcv_clk_o, io_pad_ctrl_out, SlinkNumChan)
  `TSMC7_INPUT_IO_PAD_2DARRAY_H(pad_dram_slink_i, dram_slink_i, io_pad_ctrl_in, SlinkNumChan, SlinkNumLanes)
  `TSMC7_OUTPUT_IO_PAD_2DARRAY_H(pad_dram_slink_o, dram_slink_o, io_pad_ctrl_out, SlinkNumChan, SlinkNumLanes)

  ///////////
  //  FLL  //
  ///////////


  apb_req_t fll_apb_req;
  apb_resp_t fll_apb_rsp;

  import apb_fll_pkg::*;

  fll_req_t fll_req;
  fll_rsp_t fll_rsp;

  reg_to_apb #(
    .reg_req_t(csh_reg_req_t),
    .reg_rsp_t(csh_reg_rsp_t),
    .apb_req_t(apb_req_t),
    .apb_rsp_t(apb_resp_t)
  ) i_fll_reg_to_apb (
    .clk_i    (soc_clk),
    .rst_ni   (rst_ni),
    .reg_req_i(reg_req[CshRegExtFLL]),
    .reg_rsp_o(reg_rsp[CshRegExtFLL]),
    .apb_req_o(fll_apb_req),
    .apb_rsp_i(fll_apb_rsp)
  );

  apb_to_fll #(
    .APBAddrWidth(CheshireCfg.AddrWidth),
    .NumFLLs     (1),
    .apb_req_t   (apb_req_t),
    .apb_resp_t  (apb_resp_t)
  ) i_apb_to_fll (
    .clk_i    (soc_clk),
    .rst_ni   (rst_ni),
    .apb_req_i(fll_apb_req),
    .apb_rsp_o(fll_apb_rsp),
    .fll_req_o(fll_req),
    .fll_rsp_i(fll_rsp)
  );

  tsmc7_FLL i_tsmc7_FLL (
    .FLLCLK(fll_clk),
    .FLLOE (hwif_out.fll_ctrl[0].fll_out_en.value),
    .REFCLK(ref_clk_i),
    .LOCK  (fll_rsp.lock),
    .CFGREQ(fll_req.req),
    .CFGACK(fll_rsp.ack),
    .CFGAD (fll_req.addr),
    .CFGD  (fll_req.wdata),
    .CFGQ  (fll_rsp.rdata),
    .CFGWEB(fll_req.wrn),
    .RSTB  (rst_ni),
    .PWD   (1'b0),
    .TM    (test_mode_i),
    .TE    (1'b0),
    .TD    (1'b0),
    .TQ    (/*unused*/),
    .JTD   (1'b0),
    .JTQ   (/*unused*/)
  );

  // Assign the lock status also to the register
  assign hwif_in.fll_status[0].rd_ack = hwif_out.fll_status[0].req;
  assign hwif_in.fll_status[0].rd_data.fll_lock = fll_rsp.lock;

  clk_mux_glitch_free #(
    .NUM_INPUTS        (2)
  ) i_clk_mux_glitch_free (
    .clks_i      ({ref_clk_i, fll_clk}),
    .test_clk_i  (1'b0),
    .test_en_i   (1'b0),
    .async_rstn_i(rst_ni),
    .async_sel_i (bypass_fll_i),
    .clk_o       (soc_clk)
  );

  localparam int unsigned DefaultClkDivValue = 1024;
  logic div_valid_q;

  clk_int_div #(
    .DIV_VALUE_WIDTH      ($clog2(DefaultClkDivValue)+1),
    .DEFAULT_DIV_VALUE    (DefaultClkDivValue)
  ) i_clk_int_div (
    .clk_i         (soc_clk),
    .rst_ni        (rst_ni),
    .en_i          (hwif_out.clk_div_ctrl[0].en.value),
    .test_mode_en_i(1'b0),
    .div_i         (hwif_out.clk_div_ctrl[0].div.value),
    .div_valid_i   (div_valid_q),
    .div_ready_o   ( ),
    .clk_o         (div_clk_o),
    .cycl_count_o  ( )
  );

  `FF(div_valid_q, hwif_out.clk_div_ctrl[0].div.swmod, '0, soc_clk, rst_ni);

  /////////////////////
  //  Picobello Top  //
  /////////////////////

  picobello_top i_picobello_top (
    .clk_i         ( soc_clk ),
    .rst_ni        ( rst_ni   ),
    .rtc_i         ( rtc_clk_i ),
    .clk_rst_bypass_i,
    .test_mode_i,
    .boot_mode_i,
    .jtag_tck_i,
    .jtag_trst_ni,
    .jtag_tms_i,
    .jtag_tdi_i,
    .jtag_tdo_o,
    .jtag_tdo_oe_o,
    .uart_tx_o,
    .uart_rx_i,
    .uart_rts_no     (    ),
    .uart_dtr_no     (    ),
    .uart_cts_ni     ( '0 ),
    .uart_dsr_ni     ( '0 ),
    .uart_dcd_ni     ( '0 ),
    .uart_rin_ni     ( '0 ),
    .i2c_sda_o,
    .i2c_sda_i,
    .i2c_sda_en_o,
    .i2c_scl_o,
    .i2c_scl_i,
    .i2c_scl_en_o,
    .spih_sck_o,
    .spih_sck_en_o,
    .spih_csb_o,
    .spih_csb_en_o,
    .spih_sd_o,
    .spih_sd_en_o,
    .spih_sd_i,
    .gpio_i,
    .gpio_o,
    .gpio_en_o,
    .reg_req_o (reg_req),
    .reg_rsp_i (reg_rsp),
    .slink_rcv_clk_i,
    .slink_rcv_clk_o,
    .slink_i,
    .slink_o,
    .dram_slink_rcv_clk_i,
    .dram_slink_rcv_clk_o,
    .dram_slink_i,
    .dram_slink_o
  );

endmodule
