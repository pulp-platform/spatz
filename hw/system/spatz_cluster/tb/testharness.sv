// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

`define wait_for(signal) \
  do @(posedge clk_i); while (!signal);

`include "axi/assign.svh"
`include "axi/typedef.svh"

module testharness (
    input logic clk_i,
    input logic rst_ni
  );

  import spatz_cluster_pkg::*;
  import axi_pkg::xbar_cfg_t;
  import axi_pkg::xbar_rule_32_t;

  import "DPI-C" function void clint_tick(
    output byte msip[]
  );

  /*********
   *  AXI  *
   *********/

  localparam NumAXISlaves = 2;
  localparam NumRules     = NumAXISlaves-1;

  localparam AxiTBIdWidth = AxiIdOutWidth;
  typedef logic [AxiTBIdWidth-1:0] axi_tb_id_t;
  `AXI_TYPEDEF_ALL(spatz_axi_tb, axi_addr_t, axi_id_out_t, axi_data_t, axi_strb_t, axi_user_t)

  typedef enum logic [$clog2(NumAXISlaves)-1:0] {
    UART,
    L2
  } axi_slave_target;

  spatz_axi_out_req_t  axi_mst_req;
  spatz_axi_out_resp_t axi_mst_resp;
  spatz_axi_in_req_t   to_cluster_req;
  spatz_axi_in_resp_t  to_cluster_resp;

  spatz_axi_tb_req_t  [NumAXISlaves-1:0] axi_mem_req;
  spatz_axi_tb_resp_t [NumAXISlaves-1:0] axi_mem_resp;

  localparam xbar_cfg_t XBarCfg = '{
    NoSlvPorts        : 1,
    NoMstPorts        : NumAXISlaves,
    MaxMstTrans       : 4,
    MaxSlvTrans       : 4,
    FallThrough       : 1'b0,
    LatencyMode       : axi_pkg::CUT_MST_PORTS,
    AxiIdWidthSlvPorts: AxiIdOutWidth,
    AxiIdUsedSlvPorts : AxiIdOutWidth,
    UniqueIds         : 0,
    AxiAddrWidth      : AxiAddrWidth,
    AxiDataWidth      : AxiDataWidth,
    NoAddrRules       : NumRules
  };

  /*********
   *  DUT  *
   *********/

  logic                                   cluster_probe;
  logic                                   eoc;
  logic [spatz_cluster_pkg::NumCores-1:0] msip;

  spatz_cluster_wrapper i_cluster_wrapper (
    .clk_i           (clk_i           ),
    .rst_ni          (rst_ni          ),
    .meip_i          ('0              ),
    .msip_i          (msip            ),
    .mtip_i          ('0              ),
    .debug_req_i     ('0              ),
    .axi_out_req_o   (axi_mst_req     ),
    .axi_out_resp_i  (axi_mst_resp    ),
    .axi_in_req_i    (to_cluster_req  ),
    .axi_in_resp_o   (to_cluster_resp ),
    .cluster_probe_o (cluster_probe   ),
    .eoc_o           (eoc             )
  );

/**************
 *  VCD Dump  *
 **************/

`ifdef VCD_DUMP
  initial begin: vcd_dump
    // Wait for the reset
    wait (rst_ni);

    // Wait until the probe is high
    while (!cluster_probe)
      @(posedge clk_i);

    // Dump signals of group 0
    $dumpfile(`VCD_DUMP_FILE);
    $dumpvars(0, i_cluster_wrapper);
    $dumpon;

    // Wait until the probe is low
    while (cluster_probe)
      @(posedge clk_i);

    $dumpoff;

    // Stop the execution
    $finish(0);
  end: vcd_dump
`endif

  /**********************
   *  AXI Interconnect  *
   **********************/

  localparam axi_addr_t UARTBaseAddr = 48'h0000_C000_0000;
  localparam axi_addr_t UARTEndAddr  = 48'h0000_C000_FFFF;

  xbar_rule_32_t [NumRules-1:0] xbar_routing_rules = '{
    '{idx: UART, start_addr: UARTBaseAddr[31:0], end_addr: UARTEndAddr[31:0]}
  };

  axi_xbar #(
    .Cfg          (XBarCfg                 ),
    .slv_aw_chan_t(spatz_axi_out_aw_chan_t ),
    .mst_aw_chan_t(spatz_axi_tb_aw_chan_t  ),
    .w_chan_t     (spatz_axi_out_w_chan_t  ),
    .slv_b_chan_t (spatz_axi_out_b_chan_t  ),
    .mst_b_chan_t (spatz_axi_tb_b_chan_t   ),
    .slv_ar_chan_t(spatz_axi_out_ar_chan_t ),
    .mst_ar_chan_t(spatz_axi_tb_ar_chan_t  ),
    .slv_r_chan_t (spatz_axi_out_r_chan_t  ),
    .mst_r_chan_t (spatz_axi_tb_r_chan_t   ),
    .slv_req_t    (spatz_axi_out_req_t     ),
    .slv_resp_t   (spatz_axi_out_resp_t    ),
    .mst_req_t    (spatz_axi_tb_req_t      ),
    .mst_resp_t   (spatz_axi_tb_resp_t     ),
    .rule_t       (xbar_rule_32_t          )
  ) i_testbench_xbar (
    .clk_i                (clk_i              ),
    .rst_ni               (rst_ni             ),
    .test_i               (1'b0               ),
    .slv_ports_req_i      (axi_mst_req        ),
    .slv_ports_resp_o     (axi_mst_resp       ),
    .mst_ports_req_o      (axi_mem_req        ),
    .mst_ports_resp_i     (axi_mem_resp       ),
    .addr_map_i           (xbar_routing_rules ),
    .en_default_mst_port_i('1                 ), // default all slave ports to master port Host
    .default_mst_port_i   (L2                 )
  );

  /**********
   *  UART  *
   **********/

  axi_uart #(
    .axi_req_t (spatz_axi_tb_req_t ),
    .axi_resp_t(spatz_axi_tb_resp_t)
  ) i_axi_uart (
    .clk_i     (clk_i             ),
    .rst_ni    (rst_ni            ),
    .testmode_i(1'b0              ),
    .axi_req_i (axi_mem_req[UART] ),
    .axi_resp_o(axi_mem_resp[UART])
  );

  /************************
   *  Simulation control  *
   ************************/

  assign to_cluster_req  = '0;

  // CLINT
  // verilog_lint: waive-start always-ff-non-blocking
  localparam int NumCores = spatz_cluster_pkg::NumCores;
  always_ff @(posedge clk_i) begin
    automatic byte msip_ret[NumCores];
    if (rst_ni) begin
      clint_tick(msip_ret);
      for (int i = 0; i < NumCores; i++) begin
        msip[i] = msip_ret[i];
      end
    end
  end
  // verilog_lint: waive-stop always-ff-non-blocking

  /********
   *  L2  *
   ********/

  // Wide port into simulation memory.
  tb_memory_axi #(
    .AxiAddrWidth (AxiAddrWidth       ),
    .AxiDataWidth (AxiDataWidth       ),
    .AxiIdWidth   (AxiIdOutWidth      ),
    .AxiUserWidth (AxiUserWidth       ),
    .req_t        (spatz_axi_tb_req_t ),
    .rsp_t        (spatz_axi_tb_resp_t)
  ) i_dma (
    .clk_i (clk_i           ),
    .rst_ni(rst_ni          ),
    .req_i (axi_mem_req[L2] ),
    .rsp_o (axi_mem_resp[L2])
  );

endmodule : testharness
