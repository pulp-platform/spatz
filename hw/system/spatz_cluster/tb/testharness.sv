// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

import "DPI-C" function void read_elf (input string filename);
import "DPI-C" function byte get_section (output longint address, output longint len);
import "DPI-C" context function byte read_section(input longint address, inout byte buffer[]);

`define wait_for(signal) \
  do @(posedge clk); while (!signal);

`include "axi/assign.svh"

module testharness (
    input logic clk_i,
    input logic rst_ni
  );

  import spatz_cluster_pkg::*;
  import axi_pkg::xbar_cfg_t;
  import axi_pkg::xbar_rule_32_t;

  localparam BootAddr    = `ifdef BOOT_ADDR `BOOT_ADDR `else 0 `endif;

  /*********
   *  AXI  *
   *********/

  localparam NumAXIMasters = 1;
  localparam NumAXISlaves  = 2;
  localparam NumRules      = NumAXISlaves-1;

  localparam AxiTBIdWidth = $clog2(NumAXIMasters) + AxiIdWidth;
  typedef logic [AxiTBIdWidth-1:0] axi_tb_id_t;
  `AXI_TYPEDEF_ALL(spatz_tb, axi_addr_t, axi_id_t, axi_data_t, axi_strb_t, axi_user_t)

  typedef enum logic [$clog2(NumAXISlaves)-1:0] {
    UART,
    L2
  } axi_slave_target;

  spatz_req_t  [NumAXIMasters-1:0] axi_mst_req;
  spatz_resp_t [NumAXIMasters-1:0] axi_mst_resp;
  spatz_req_t                      to_cluster_req;
  spatz_resp_t                     to_cluster_resp;

  spatz_tb_req_t  [NumAXISlaves-1:0] axi_mem_req;
  spatz_tb_resp_t [NumAXISlaves-1:0] axi_mem_resp;

  localparam xbar_cfg_t XBarCfg = '{
    NoSlvPorts        : NumAXIMasters,
    NoMstPorts        : NumAXISlaves,
    MaxMstTrans       : 4,
    MaxSlvTrans       : 4,
    FallThrough       : 1'b0,
    LatencyMode       : axi_pkg::CUT_MST_PORTS,
    AxiIdWidthSlvPorts: AxiIdWidth,
    AxiIdUsedSlvPorts : AxiIdWidth,
    UniqueIds         : 0,
    AxiAddrWidth      : AxiAddrWidth,
    AxiDataWidth      : AxiDataWidth,
    NoAddrRules       : NumRules
  };

  /*********
   *  DUT  *
   *********/

  logic fetch_en;
  logic eoc_valid;
  logic kernel_probe;

  spatz_cluster #(
    .TCDMBaseAddr(32'h0   ),
    .BootAddr    (BootAddr)
  ) dut (
    .clk_i          (clk            ),
    .rst_ni         (rst_n          ),
    .fetch_en_i     (fetch_en       ),
    .eoc_valid_o    (eoc_valid      ),
    .busy_o         (/*Unused*/     ),
    .mst_req_o      (axi_mst_req    ),
    .mst_resp_i     (axi_mst_resp   ),
    .slv_req_i      (to_cluster_req ),
    .slv_resp_o     (to_cluster_resp),
    .kernel_probe_o (kernel_probe   )
  );

/**************
 *  VCD Dump  *
 **************/

`ifdef VCD_DUMP
  initial begin: vcd_dump
    // Wait for the reset
    wait (rst_n);

    // Wait until the probe is high
    while (!kernel_probe)
      @(posedge clk);

    // Dump signals of group 0
    $dumpfile(`VCD_DUMP_FILE);
    $dumpvars(0, dut);
    $dumpon;

    // Wait until the probe is low
    while (kernel_probe)
      @(posedge clk);

    $dumpoff;

    // Stop the execution
    $finish(0);
  end: vcd_dump
`endif

  /**********************
   *  AXI Interconnect  *
   **********************/

  localparam axi_addr_t UARTBaseAddr = 64'hC000_0000;
  localparam axi_addr_t UARTEndAddr  = 64'hC000_FFFF;

  xbar_rule_32_t [NumRules-1:0] xbar_routing_rules = '{
    '{idx: UART, start_addr: UARTBaseAddr, end_addr: UARTEndAddr}
  };

  axi_xbar #(
    .Cfg          (XBarCfg            ),
    .slv_aw_chan_t(spatz_aw_chan_t    ),
    .mst_aw_chan_t(spatz_tb_aw_chan_t ),
    .w_chan_t     (spatz_w_chan_t     ),
    .slv_b_chan_t (spatz_b_chan_t     ),
    .mst_b_chan_t (spatz_tb_b_chan_t  ),
    .slv_ar_chan_t(spatz_ar_chan_t    ),
    .mst_ar_chan_t(spatz_tb_ar_chan_t ),
    .slv_r_chan_t (spatz_r_chan_t     ),
    .mst_r_chan_t (spatz_tb_r_chan_t  ),
    .slv_req_t    (spatz_req_t        ),
    .slv_resp_t   (spatz_resp_t       ),
    .mst_req_t    (spatz_tb_req_t     ),
    .mst_resp_t   (spatz_tb_resp_t    ),
    .rule_t       (xbar_rule_32_t     )
  ) i_testbench_xbar (
    .clk_i                (clk                ),
    .rst_ni               (rst_n              ),
    .test_i               (1'b0               ),
    .slv_ports_req_i      (axi_mst_req        ),
    .slv_ports_resp_o     (axi_mst_resp       ),
    .mst_ports_req_o      (axi_mem_req        ),
    .mst_ports_resp_i     (axi_mem_resp       ),
    .addr_map_i           (xbar_routing_rules ),
    .en_default_mst_port_i('1                 ), // default all slave ports to master port Host
    .default_mst_port_i   ({NumAXIMasters{L2}})
  );

  /**********
   *  UART  *
   **********/

  axi_uart #(
    .axi_req_t (spatz_tb_req_t ),
    .axi_resp_t(spatz_tb_resp_t)
  ) i_axi_uart (
    .clk_i     (clk               ),
    .rst_ni    (rst_n             ),
    .testmode_i(1'b0              ),
    .axi_req_i (axi_mem_req[UART] ),
    .axi_resp_o(axi_mem_resp[UART])
  );

  /************************
   *  Write/Read via AXI  *
   ************************/

  task write_to_cluster(input axi_addr_t addr, input axi_data_t data, output axi_pkg::resp_t resp);
    to_cluster_req.aw.id    = 'h18d;
    to_cluster_req.aw.addr  = addr;
    to_cluster_req.aw.size  = 'h2;
    to_cluster_req.aw.burst = axi_pkg::BURST_INCR;
    to_cluster_req.aw_valid = 1'b1;
    `wait_for(to_cluster_resp.aw_ready)

    to_cluster_req.aw_valid = 1'b0;
    to_cluster_req.w.data   = data << addr[ByteOffset +: $clog2(AxiDataWidth/DataWidth)] * DataWidth;
    to_cluster_req.w.strb   = {AxiStrbWidth{1'b1}} << addr[ByteOffset +: $clog2(AxiDataWidth/DataWidth)] * BeWidth;
    to_cluster_req.w.last   = 1'b1;
    to_cluster_req.w.user   = '0;
    to_cluster_req.w_valid  = 1'b1;
    `wait_for(to_cluster_resp.w_ready)

    to_cluster_req.w_valid = 1'b0;
    to_cluster_req.b_ready = 1'b1;
    `wait_for(to_cluster_resp.b_valid)

    resp                   = to_cluster_resp.b.resp;
    to_cluster_req.b_ready = 1'b0;
  endtask

  task read_from_cluster(input axi_addr_t addr, output axi_data_t data, output axi_pkg::resp_t resp);
    to_cluster_req.ar.id    = 'h18d;
    to_cluster_req.ar.addr  = addr;
    to_cluster_req.ar.size  = 'h2;
    to_cluster_req.ar.burst = axi_pkg::BURST_INCR;
    to_cluster_req.ar_valid = 1'b1;
    `wait_for(to_cluster_resp.ar_ready)

    to_cluster_req.ar_valid = 1'b0;
    to_cluster_req.r_ready  = 1'b1;
    `wait_for(to_cluster_resp.r_valid)

    data                   = to_cluster_resp.r.data >> addr[ByteOffset +: $clog2(AxiDataWidth/DataWidth)] * DataWidth;
    resp                   = to_cluster_resp.r.resp;
    to_cluster_req.r_ready = 1'b0;
    $display("[TB] Read %08x from %08x at %t (resp=%d).", data, addr, $time, resp);
  endtask

  // Simulation control
  initial begin
    localparam ctrl_phys_addr = 32'h4000_0000;
    localparam ctrl_size      = 32'h0100_0000;
    localparam ctrl_virt_addr = ctrl_phys_addr;
    axi_addr_t first, last, phys_addr;
    axi_data_t rdata;
    axi_pkg::resp_t resp;
    fetch_en                = 1'b0;
    to_cluster_req          = '{default: '0};
    to_cluster_req.aw.burst = axi_pkg::BURST_INCR;
    to_cluster_req.ar.burst = axi_pkg::BURST_INCR;
    to_cluster_req.aw.cache = axi_pkg::CACHE_MODIFIABLE;
    to_cluster_req.ar.cache = axi_pkg::CACHE_MODIFIABLE;
    // Wait for reset.
    wait (rst_n);
    @(posedge clk);

    // Give the cores time to execute the bootrom's program
    #10ns;

    // Wake up all cores
    write_to_cluster(ctrl_virt_addr + 32'h4, {DataWidth{1'b1}}, resp);
    assert(resp == axi_pkg::RESP_OKAY);

    // Wait for the interrupt
    wait (eoc_valid);
    read_from_cluster(ctrl_virt_addr, rdata, resp);
    assert(resp == axi_pkg::RESP_OKAY);

    $timeformat(-9, 2, " ns", 0);
    $display("[EOC] Simulation ended at %t (retval = %0d).", $time, rdata >> 1);
    $finish(0);
    // Start MemPool
    fetch_en = 1'b1;
  end

  /********
   *  L2  *
   ********/

  // Wide port into simulation memory.
  tb_memory_axi #(
    .AxiAddrWidth (AxiAddrWidth   ),
    .AxiDataWidth (AxiDataWidth   ),
    .AxiIdWidth   (AxiIdWidth     ),
    .AxiUserWidth (AxiUserWidth   ),
    .req_t        (spatz_tb_req_t ),
    .rsp_t        (spatz_tb_resp_t)
  ) i_dma (
    .clk_i (clk_i           ),
    .rst_ni(rst_ni          ),
    .req_i (axi_mem_req[L2] ),
    .rsp_o (axi_mem_resp[L2])
  );

endmodule : testharness
