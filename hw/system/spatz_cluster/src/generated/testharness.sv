// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

`define wait_for(signal)   do @(negedge clk_i); while (!signal);

`include "axi/assign.svh"
`include "axi/typedef.svh"
`include "reqrsp_interface/typedef.svh"

module testharness (
    input logic clk_i,
    input logic rst_ni
  );

  import spatz_cluster_pkg::*;
  import spatz_cluster_peripheral_reg_pkg::*;
  import axi_pkg::xbar_cfg_t;
  import axi_pkg::xbar_rule_32_t;

  import "DPI-C" function int get_entry_point();

  /*********
   *  AXI  *
   *********/

  localparam NumAXISlaves = 2;
  localparam NumRules     = NumAXISlaves-1;

  spatz_axi_out_req_t  axi_from_cluster_req;
  spatz_axi_out_resp_t axi_from_cluster_resp;
  spatz_axi_in_req_t   axi_to_cluster_req;
  spatz_axi_in_resp_t  axi_to_cluster_resp;


  /*********
   *  DUT  *
   *********/

  logic                cluster_probe;
  logic [NumCores-1:0] debug_req;

  spatz_cluster_wrapper i_cluster_wrapper (
    .clk_i           (clk_i                ),
    .rst_ni          (rst_ni               ),
    .meip_i          ('0                   ),
    .msip_i          ('0                   ),
    .mtip_i          ('0                   ),
    .debug_req_i     ( debug_req           ),
    .axi_out_req_o   (axi_from_cluster_req ),
    .axi_out_resp_i  (axi_from_cluster_resp),
    .axi_in_req_i    (axi_to_cluster_req   ),
    .axi_in_resp_o   (axi_to_cluster_resp  ),
    .cluster_probe_o (cluster_probe        )
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

  /************************
   *  Simulation control  *
   ************************/

  `REQRSP_TYPEDEF_ALL(reqrsp_cluster_in, axi_addr_t, logic [63:0], logic [7:0])
  reqrsp_cluster_in_req_t to_cluster_req;
  reqrsp_cluster_in_rsp_t to_cluster_rsp;

  reqrsp_to_axi #(
    .DataWidth   (DataWidth              ),
    .UserWidth   (SpatzAxiUserWidth      ),
    .axi_req_t   (spatz_axi_in_req_t     ),
    .axi_rsp_t   (spatz_axi_in_resp_t    ),
    .reqrsp_req_t(reqrsp_cluster_in_req_t),
    .reqrsp_rsp_t(reqrsp_cluster_in_rsp_t)
  ) i_axi_to_reqrsp (
    .clk_i       (clk_i              ),
    .rst_ni      (rst_ni             ),
    .user_i      ('0                 ),
    .axi_req_o   (axi_to_cluster_req ),
    .axi_rsp_i   (axi_to_cluster_resp),
    .reqrsp_req_i(to_cluster_req     ),
    .reqrsp_rsp_o(to_cluster_rsp     )
  );

  logic [31:0] entry_point;
  initial begin
    // Idle
    to_cluster_req = '0;
    debug_req      = '0;

    // Wait for a while
    repeat (10)
      @(negedge clk_i);

    // Load the entry point
    entry_point = get_entry_point();
    $display("Loading entry point: %0x", entry_point);

    // Wait for a while
    repeat (1000)
      @(negedge clk_i);

    // Store the entry point in the Spatz cluster
    to_cluster_req = '{
      q: '{
        addr   : PeriStartAddr + SPATZ_CLUSTER_PERIPHERAL_CLUSTER_BOOT_CONTROL_OFFSET,
        data   : {32'b0, entry_point},
        write  : 1'b1,
        strb   : '1,
        amo    : reqrsp_pkg::AMONone,
        default: '0
      },
      q_valid: 1'b1,
      p_ready: 1'b0
    };
    `wait_for(to_cluster_rsp.q_ready);
    to_cluster_req = '0;
    `wait_for(to_cluster_rsp.p_valid);
    to_cluster_req = '{
      p_ready: 1'b1,
      q      : '{
        amo    : reqrsp_pkg::AMONone,
        default: '0
      },
      default: '0
    };
    @(negedge clk_i);
    to_cluster_req = '0;


    // Wake up cores
    debug_req = '1;
    @(negedge clk_i);
    debug_req = '0;
  end

  /********
   *  L2  *
   ********/

  // Wide port into simulation memory.
  tb_memory_axi #(
    .AxiAddrWidth ( SpatzAxiAddrWidth    ),
    .AxiDataWidth ( SpatzAxiDataWidth    ),
    .AxiIdWidth   ( SpatzAxiIdOutWidth   ),
    .AxiUserWidth ( SpatzAxiUserWidth    ),
    .req_t        ( spatz_axi_out_req_t  ),
    .rsp_t        ( spatz_axi_out_resp_t )
  ) i_dma (
    .clk_i (clk_i                ),
    .rst_ni(rst_ni               ),
    .req_i (axi_from_cluster_req ),
    .rsp_o (axi_from_cluster_resp)
  );

endmodule : testharness
