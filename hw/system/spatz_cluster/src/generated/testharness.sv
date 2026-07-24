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
  import "DPI-C" function void read_elf (input string filename);
  import "DPI-C" function byte get_section (output longint address, output longint len);
  import "DPI-C" context function byte read_section(input longint address, inout byte buffer[]);

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
  logic [3:0]          eoc;

  spatz_cluster_wrapper i_cluster_wrapper (
    .clk_i           (clk_i                ),
    .rst_ni          (rst_ni               ),
    .meip_i          ('0                   ),
    .msip_i          ('0                   ),
    .mtip_i          ('0                   ),
    .eoc_o           (eoc                  ),
    .debug_req_i     ( debug_req           ),
    .axi_out_req_o   (axi_from_cluster_req ),
    .axi_out_resp_i  (axi_from_cluster_resp),
    .axi_in_req_i    (axi_to_cluster_req   ),
    .axi_in_resp_o   (axi_to_cluster_resp  ),
    .cluster_probe_o (cluster_probe        )
  );

  /*****************
   *  TB Crossbar  *
   *****************/

  // Route cluster traffic to the fake UART, the main memory (DRAM region),
  // or a fallback simulation memory for any other address.
  localparam logic [31:0] TbUartAddr = 32'hA000_0000;
  localparam logic [31:0] DramBase   = 32'h8000_0000;
  localparam logic [31:0] DramEnd    = 32'hA000_0000;

  typedef enum int unsigned {
    TbUart  = 0,
    TbDram  = 1,
    TbL2spm = 2
  } tb_xbar_e;

  localparam axi_pkg::xbar_cfg_t TbXbarCfg = '{
    NoSlvPorts        : 1,
    NoMstPorts        : 3,
    MaxMstTrans       : 64,
    MaxSlvTrans       : 64,
    FallThrough       : 1'b0,
    LatencyMode       : axi_pkg::CUT_ALL_PORTS,
    AxiIdWidthSlvPorts: SpatzAxiIdOutWidth,
    AxiIdUsedSlvPorts : SpatzAxiIdOutWidth,
    UniqueIds         : 1'b0,
    AxiAddrWidth      : SpatzAxiAddrWidth,
    AxiDataWidth      : SpatzAxiDataWidth,
    NoAddrRules       : 2,
    default           : '0
  };

  typedef struct packed {
    int unsigned idx;
    axi_addr_t start_addr;
    axi_addr_t end_addr;
  } tb_xbar_rule_t;

  // With a single slave port, the master-side ID width matches the slave side,
  // so the cluster output AXI types are reused on all crossbar ports.
  spatz_axi_out_req_t  [2:0] axi_tb_req;
  spatz_axi_out_resp_t [2:0] axi_tb_resp;

  tb_xbar_rule_t [TbXbarCfg.NoAddrRules-1:0] tb_xbar_rule;
  assign tb_xbar_rule = '{
    '{idx: TbUart, start_addr: TbUartAddr, end_addr: TbUartAddr + 32'h1000},
    '{idx: TbDram, start_addr: DramBase,   end_addr: DramEnd              }
  };

  logic [$clog2(TbXbarCfg.NoMstPorts)-1:0] tb_xbar_default_port;
  assign tb_xbar_default_port = TbL2spm;

  axi_xbar #(
    .Cfg           (TbXbarCfg               ),
    .ATOPs         (0                       ),
    .slv_aw_chan_t (spatz_axi_out_aw_chan_t ),
    .mst_aw_chan_t (spatz_axi_out_aw_chan_t ),
    .w_chan_t      (spatz_axi_out_w_chan_t  ),
    .slv_b_chan_t  (spatz_axi_out_b_chan_t  ),
    .mst_b_chan_t  (spatz_axi_out_b_chan_t  ),
    .slv_ar_chan_t (spatz_axi_out_ar_chan_t ),
    .mst_ar_chan_t (spatz_axi_out_ar_chan_t ),
    .slv_r_chan_t  (spatz_axi_out_r_chan_t  ),
    .mst_r_chan_t  (spatz_axi_out_r_chan_t  ),
    .slv_req_t     (spatz_axi_out_req_t     ),
    .slv_resp_t    (spatz_axi_out_resp_t    ),
    .mst_req_t     (spatz_axi_out_req_t     ),
    .mst_resp_t    (spatz_axi_out_resp_t    ),
    .rule_t        (tb_xbar_rule_t          )
  ) i_axi_tb_xbar (
    .clk_i                 (clk_i                  ),
    .rst_ni                (rst_ni                 ),
    .test_i                (1'b0                   ),
    .slv_ports_req_i       ({axi_from_cluster_req} ),
    .slv_ports_resp_o      ({axi_from_cluster_resp}),
    .mst_ports_req_o       (axi_tb_req             ),
    .mst_ports_resp_i      (axi_tb_resp            ),
    .addr_map_i            (tb_xbar_rule           ),
    .en_default_mst_port_i (1'b1                   ),
    .default_mst_port_i    (tb_xbar_default_port   )
  );

  /**********
   *  UART  *
   **********/

  axi_uart #(
    .axi_req_t (spatz_axi_out_req_t ),
    .axi_resp_t(spatz_axi_out_resp_t)
  ) i_axi_uart (
    .clk_i     (clk_i               ),
    .rst_ni    (rst_ni              ),
    .testmode_i(1'b0                ),
    .axi_req_i (axi_tb_req[TbUart]  ),
    .axi_resp_o(axi_tb_resp[TbUart] )
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

  `REQRSP_TYPEDEF_ALL(reqrsp_cluster_in, axi_addr_t, narrow_axi_data_t, narrow_axi_strb_t)
  reqrsp_cluster_in_req_t to_cluster_req;
  reqrsp_cluster_in_rsp_t to_cluster_rsp;

  reqrsp_to_axi #(
    .DataWidth   (SpatzNarrowAxiDataWidth),
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
    #10;
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

  `ifdef TARGET_DRAMSYS

  localparam int unsigned L2BankWidth    = 512;
  localparam int unsigned L2BankBeWidth  = L2BankWidth / 8;
  localparam              DramType = "DDR4";

  typedef struct packed {
    int                           dram_ctrl_id;
    logic [SpatzAxiAddrWidth-1:0] dram_ctrl_addr;
  } dram_ctrl_interleave_t;

  dram_sim_engine #(
    .ClkPeriod  (1.0ns )
  ) i_dram_engine (
    .clk_i      (clk_i  ),
    .rst_ni     (rst_ni )
  );

  function automatic dram_ctrl_interleave_t getDramCTRLInfo(axi_addr_t addr);
    automatic dram_ctrl_interleave_t res;

    res.dram_ctrl_id    = 0;
    res.dram_ctrl_addr  = addr;
    return res;
  endfunction

  // DRAMSys Initialization
  initial begin : l2_init
    byte                              buffer [];
    axi_addr_t                        address;
    axi_addr_t                        length;
    string                            binary;
    // Initialize memories
    void'($value$plusargs("PRELOAD=%s", binary));
      
    $display("binary %s",binary);

    #1;
    if (binary != "") begin
      // Read ELF
      read_elf(binary);
      $display("Loading %s", binary);
      while (get_section(address, length)) begin
        // Read sections
        // Align data to BankBeWidth
        automatic int nwords = (length + L2BankBeWidth - 1)/L2BankBeWidth;
        $display("Loading section %x of length %x", address, length);
        buffer = new[nwords * L2BankBeWidth];
        void'(read_section(address, buffer));
        if (address >= DramBase) begin
          for (int i = 0; i < nwords * L2BankBeWidth; i++) begin //per byte
            automatic dram_ctrl_interleave_t dram_ctrl_info;
            dram_ctrl_info = getDramCTRLInfo(address + i - DramBase);
            i_axi_dram_sim.i_sim_dram.load_a_byte_to_dram(dram_ctrl_info.dram_ctrl_addr, buffer[i]);
          end
        end else begin
          $display("Cannot initialize address %x, which doesn't fall into the L2 DRAM region.", address);
        end
      end
    end
  end : l2_init

  axi_dram_sim #(
    .BASE         ( DramBase                  ),
    .DRAMType     ( DramType                  ),
    .AxiAddrWidth ( SpatzAxiAddrWidth         ),
    .AxiDataWidth ( SpatzAxiDataWidth         ),
    .AxiIdWidth   ( SpatzAxiIdOutWidth        ),
    .AxiUserWidth ( SpatzAxiUserWidth         ),
    .axi_req_t    ( spatz_axi_out_req_t       ),
    .axi_resp_t   ( spatz_axi_out_resp_t      ),
    .axi_ar_t     ( spatz_axi_out_ar_chan_t   ),
    .axi_r_t      ( spatz_axi_out_r_chan_t    ),
    .axi_aw_t     ( spatz_axi_out_aw_chan_t   ),
    .axi_w_t      ( spatz_axi_out_w_chan_t    ),
    .axi_b_t      ( spatz_axi_out_b_chan_t    )
  ) i_axi_dram_sim (
    .clk_i        ( clk_i                     ),
    .rst_ni       ( rst_ni                    ),
    .axi_req_i    ( axi_tb_req[TbDram]        ),
    .axi_resp_o   ( axi_tb_resp[TbDram]       )
  );

  // End of computation: with DRAMSys, the host cannot poll `tohost` in the
  // simulation memory, so wait for the cluster EOC register instead.
  initial begin
    repeat (1000)
      @(posedge clk_i);
    wait (eoc != '0);
    // The runtime writes (retval << 1) | 1, HTIF-style.
    $display("[EOC] Simulation ended at %t (retval = %0d).", $time, eoc >> 1);
    $finish(0);
  end
  `else
  tb_memory_axi #(
    .AxiAddrWidth ( SpatzAxiAddrWidth    ),
    .AxiDataWidth ( SpatzAxiDataWidth    ),
    .AxiIdWidth   ( SpatzAxiIdOutWidth   ),
    .AxiUserWidth ( SpatzAxiUserWidth    ),
    .req_t        ( spatz_axi_out_req_t  ),
    .rsp_t        ( spatz_axi_out_resp_t )
  ) i_dma (
    .clk_i (clk_i              ),
    .rst_ni(rst_ni             ),
    .req_i (axi_tb_req[TbDram] ),
    .rsp_o (axi_tb_resp[TbDram])
  );
  `endif

  // Fallback simulation memory for accesses outside the DRAM and UART regions.
  tb_memory_axi #(
    .AxiAddrWidth ( SpatzAxiAddrWidth    ),
    .AxiDataWidth ( SpatzAxiDataWidth    ),
    .AxiIdWidth   ( SpatzAxiIdOutWidth   ),
    .AxiUserWidth ( SpatzAxiUserWidth    ),
    .req_t        ( spatz_axi_out_req_t  ),
    .rsp_t        ( spatz_axi_out_resp_t )
  ) i_l2mem (
    .clk_i (clk_i               ),
    .rst_ni(rst_ni              ),
    .req_i (axi_tb_req[TbL2spm] ),
    .rsp_o (axi_tb_resp[TbL2spm])
  );

endmodule : testharness
