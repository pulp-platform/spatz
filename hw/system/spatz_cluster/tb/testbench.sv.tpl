// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

`define wait_for(signal) \
  do @(negedge clk_i); while (!signal);

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

  // Spatz wide port to SoC (currently dram)
  spatz_axi_out_req_t  axi_from_cluster_req;
  spatz_axi_out_resp_t axi_from_cluster_resp;
  // Spatz wide port to L2
  spatz_axi_out_req_t  axi_l2_req;
  spatz_axi_out_resp_t axi_l2_resp;
  // From SoC to Spatz
  spatz_axi_in_req_t   axi_to_cluster_req;
  spatz_axi_in_resp_t  axi_to_cluster_resp;

% if cfg['axi_cdc_enable']:
  // AXI CDC parameters
  localparam int unsigned AsyncAxiOutAwWidth = (2**SpatzLogDepth) * axi_pkg::aw_width ( SpatzAxiAddrWidth,  SpatzAxiIdOutWidth, SpatzAxiUserWidth );
  localparam int unsigned AsyncAxiOutWWidth  = (2**SpatzLogDepth) * axi_pkg::w_width  ( SpatzAxiDataWidth,  SpatzAxiUserWidth );
  localparam int unsigned AsyncAxiOutBWidth  = (2**SpatzLogDepth) * axi_pkg::b_width  ( SpatzAxiIdOutWidth, SpatzAxiUserWidth );
  localparam int unsigned AsyncAxiOutArWidth = (2**SpatzLogDepth) * axi_pkg::ar_width ( SpatzAxiAddrWidth,  SpatzAxiIdOutWidth, SpatzAxiUserWidth );
  localparam int unsigned AsyncAxiOutRWidth  = (2**SpatzLogDepth) * axi_pkg::r_width  ( SpatzAxiDataWidth,  SpatzAxiIdOutWidth, SpatzAxiUserWidth );

  localparam int unsigned AsyncAxiInAwWidth = (2**SpatzLogDepth) * axi_pkg::aw_width ( SpatzAxiAddrWidth, SpatzAxiIdInWidth, SpatzAxiUserWidth );
  localparam int unsigned AsyncAxiInWWidth  = (2**SpatzLogDepth) * axi_pkg::w_width  ( DataWidth,         SpatzAxiUserWidth );
  localparam int unsigned AsyncAxiInBWidth  = (2**SpatzLogDepth) * axi_pkg::b_width  ( SpatzAxiIdInWidth, SpatzAxiUserWidth );
  localparam int unsigned AsyncAxiInArWidth = (2**SpatzLogDepth) * axi_pkg::ar_width ( SpatzAxiAddrWidth, SpatzAxiIdInWidth, SpatzAxiUserWidth );
  localparam int unsigned AsyncAxiInRWidth  = (2**SpatzLogDepth) * axi_pkg::r_width  ( DataWidth,         SpatzAxiIdInWidth, SpatzAxiUserWidth );

  // AXI Master
  logic [AsyncAxiOutAwWidth-1:0]   async_axi_out_aw_data_o;
  logic [SpatzLogDepth:0]          async_axi_out_aw_wptr_o;
  logic [SpatzLogDepth:0]          async_axi_out_aw_rptr_i;
  logic [AsyncAxiOutWWidth-1:0]    async_axi_out_w_data_o;
  logic [SpatzLogDepth:0]          async_axi_out_w_wptr_o;
  logic [SpatzLogDepth:0]          async_axi_out_w_rptr_i;
  logic [AsyncAxiOutBWidth-1:0]    async_axi_out_b_data_i;
  logic [SpatzLogDepth:0]          async_axi_out_b_wptr_i;
  logic [SpatzLogDepth:0]          async_axi_out_b_rptr_o;
  logic [AsyncAxiOutArWidth-1:0]   async_axi_out_ar_data_o;
  logic [SpatzLogDepth:0]          async_axi_out_ar_wptr_o;
  logic [SpatzLogDepth:0]          async_axi_out_ar_rptr_i;
  logic [AsyncAxiOutRWidth-1:0]    async_axi_out_r_data_i;
  logic [SpatzLogDepth:0]          async_axi_out_r_wptr_i;
  logic [SpatzLogDepth:0]          async_axi_out_r_rptr_o;

  // AXI Slave
  logic [AsyncAxiInAwWidth-1:0]     async_axi_in_aw_data_i;
  logic [SpatzLogDepth:0]           async_axi_in_aw_wptr_i;
  logic [SpatzLogDepth:0]           async_axi_in_aw_rptr_o;
  logic [AsyncAxiInWWidth-1:0]      async_axi_in_w_data_i;
  logic [SpatzLogDepth:0]           async_axi_in_w_wptr_i;
  logic [SpatzLogDepth:0]           async_axi_in_w_rptr_o;
  logic [AsyncAxiInBWidth-1:0]      async_axi_in_b_data_o;
  logic [SpatzLogDepth:0]           async_axi_in_b_wptr_o;
  logic [SpatzLogDepth:0]           async_axi_in_b_rptr_i;
  logic [AsyncAxiInArWidth-1:0]     async_axi_in_ar_data_i;
  logic [SpatzLogDepth:0]           async_axi_in_ar_wptr_i;
  logic [SpatzLogDepth:0]           async_axi_in_ar_rptr_o;
  logic [AsyncAxiInRWidth-1:0]      async_axi_in_r_data_o;
  logic [SpatzLogDepth:0]           async_axi_in_r_wptr_o;
  logic [SpatzLogDepth:0]           async_axi_in_r_rptr_i;

  /*********
   *  CDC  *
   *********/

   axi_cdc_dst #(
     .LogDepth   ( SpatzLogDepth            ),
     .aw_chan_t  ( spatz_axi_out_aw_chan_t  ),
     .w_chan_t   ( spatz_axi_out_w_chan_t   ),
     .b_chan_t   ( spatz_axi_out_b_chan_t   ),
     .ar_chan_t  ( spatz_axi_out_ar_chan_t  ),
     .r_chan_t   ( spatz_axi_out_r_chan_t   ),
     .axi_req_t  ( spatz_axi_out_req_t      ),
     .axi_resp_t ( spatz_axi_out_resp_t     )
   ) i_spatz_cluster_cdc_dst (
     // Asynchronous slave port
     .async_data_slave_aw_data_i ( async_axi_out_aw_data_o ),
     .async_data_slave_aw_wptr_i ( async_axi_out_aw_wptr_o ),
     .async_data_slave_aw_rptr_o ( async_axi_out_aw_rptr_i ),
     .async_data_slave_w_data_i  ( async_axi_out_w_data_o  ),
     .async_data_slave_w_wptr_i  ( async_axi_out_w_wptr_o  ),
     .async_data_slave_w_rptr_o  ( async_axi_out_w_rptr_i  ),
     .async_data_slave_b_data_o  ( async_axi_out_b_data_i  ),
     .async_data_slave_b_wptr_o  ( async_axi_out_b_wptr_i  ),
     .async_data_slave_b_rptr_i  ( async_axi_out_b_rptr_o  ),
     .async_data_slave_ar_data_i ( async_axi_out_ar_data_o ),
     .async_data_slave_ar_wptr_i ( async_axi_out_ar_wptr_o ),
     .async_data_slave_ar_rptr_o ( async_axi_out_ar_rptr_i ),
     .async_data_slave_r_data_o  ( async_axi_out_r_data_i  ),
     .async_data_slave_r_wptr_o  ( async_axi_out_r_wptr_i  ),
     .async_data_slave_r_rptr_i  ( async_axi_out_r_rptr_o  ),
     // Synchronous master port
     .dst_clk_i                  ( clk_i                 ),
     .dst_rst_ni                 ( rst_ni                ),
     .dst_req_o                  ( axi_from_cluster_req  ),
     .dst_resp_i                 ( axi_from_cluster_resp )
   );

   axi_cdc_src #(
     .LogDepth   ( SpatzLogDepth          ),
     .aw_chan_t  ( spatz_axi_in_aw_chan_t ),
     .w_chan_t   ( spatz_axi_in_w_chan_t  ),
     .b_chan_t   ( spatz_axi_in_b_chan_t  ),
     .ar_chan_t  ( spatz_axi_in_ar_chan_t ),
     .r_chan_t   ( spatz_axi_in_r_chan_t  ),
     .axi_req_t  ( spatz_axi_in_req_t     ),
     .axi_resp_t ( spatz_axi_in_resp_t    )
   ) i_spatz_cluster_cdc_src (
     .async_data_master_aw_data_o( async_axi_in_aw_data_i ),
     .async_data_master_aw_wptr_o( async_axi_in_aw_wptr_i ),
     .async_data_master_aw_rptr_i( async_axi_in_aw_rptr_o ),
     .async_data_master_w_data_o ( async_axi_in_w_data_i  ),
     .async_data_master_w_wptr_o ( async_axi_in_w_wptr_i  ),
     .async_data_master_w_rptr_i ( async_axi_in_w_rptr_o  ),
     .async_data_master_b_data_i ( async_axi_in_b_data_o  ),
     .async_data_master_b_wptr_i ( async_axi_in_b_wptr_o  ),
     .async_data_master_b_rptr_o ( async_axi_in_b_rptr_i  ),
     .async_data_master_ar_data_o( async_axi_in_ar_data_i ),
     .async_data_master_ar_wptr_o( async_axi_in_ar_wptr_i ),
     .async_data_master_ar_rptr_i( async_axi_in_ar_rptr_o ),
     .async_data_master_r_data_i ( async_axi_in_r_data_o  ),
     .async_data_master_r_wptr_i ( async_axi_in_r_wptr_o  ),
     .async_data_master_r_rptr_o ( async_axi_in_r_rptr_i  ),
     // synchronous slave port
     .src_clk_i                  ( clk_i                  ),
     .src_rst_ni                 ( rst_ni                 ),
     .src_req_i                  ( axi_to_cluster_req     ),
     .src_resp_o                 ( axi_to_cluster_resp    )
   );
% endif

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
  % if cfg['enable_debug']:
    .debug_req_i     ( debug_req           ),
  % endif
  % if cfg['axi_cdc_enable']:
    % if cfg['sw_rst_enable']:
    .pwr_on_rst_ni   ( rst_ni              ),
    % endif
    .async_axi_in_aw_data_i (  async_axi_in_aw_data_i ),
    .async_axi_in_aw_wptr_i (  async_axi_in_aw_wptr_i ),
    .async_axi_in_aw_rptr_o (  async_axi_in_aw_rptr_o ),
    .async_axi_in_w_data_i  (  async_axi_in_w_data_i  ),
    .async_axi_in_w_wptr_i  (  async_axi_in_w_wptr_i  ),
    .async_axi_in_w_rptr_o  (  async_axi_in_w_rptr_o  ),
    .async_axi_in_b_data_o  (  async_axi_in_b_data_o  ),
    .async_axi_in_b_wptr_o  (  async_axi_in_b_wptr_o  ),
    .async_axi_in_b_rptr_i  (  async_axi_in_b_rptr_i  ),
    .async_axi_in_ar_data_i (  async_axi_in_ar_data_i ),
    .async_axi_in_ar_wptr_i (  async_axi_in_ar_wptr_i ),
    .async_axi_in_ar_rptr_o (  async_axi_in_ar_rptr_o ),
    .async_axi_in_r_data_o  (  async_axi_in_r_data_o  ),
    .async_axi_in_r_wptr_o  (  async_axi_in_r_wptr_o  ),
    .async_axi_in_r_rptr_i  (  async_axi_in_r_rptr_i  ),

    .async_axi_out_aw_data_o ( async_axi_out_aw_data_o ),
    .async_axi_out_aw_wptr_o ( async_axi_out_aw_wptr_o ),
    .async_axi_out_aw_rptr_i ( async_axi_out_aw_rptr_i ),
    .async_axi_out_w_data_o  ( async_axi_out_w_data_o  ),
    .async_axi_out_w_wptr_o  ( async_axi_out_w_wptr_o  ),
    .async_axi_out_w_rptr_i  ( async_axi_out_w_rptr_i  ),
    .async_axi_out_b_data_i  ( async_axi_out_b_data_i  ),
    .async_axi_out_b_wptr_i  ( async_axi_out_b_wptr_i  ),
    .async_axi_out_b_rptr_o  ( async_axi_out_b_rptr_o  ),
    .async_axi_out_ar_data_o ( async_axi_out_ar_data_o ),
    .async_axi_out_ar_wptr_o ( async_axi_out_ar_wptr_o ),
    .async_axi_out_ar_rptr_i ( async_axi_out_ar_rptr_i ),
    .async_axi_out_r_data_i  ( async_axi_out_r_data_i  ),
    .async_axi_out_r_wptr_i  ( async_axi_out_r_wptr_i  ),
    .async_axi_out_r_rptr_o  ( async_axi_out_r_rptr_o  ),
% else:
    .axi_out_req_o   (axi_from_cluster_req ),
    .axi_out_resp_i  (axi_from_cluster_resp),
    .axi_out_l2_req_o   ( axi_l2_req ),
    .axi_out_l2_resp_i  ( axi_l2_resp ),
    .axi_in_req_i    (axi_to_cluster_req   ),
    .axi_in_resp_o   (axi_to_cluster_resp  ),
% endif
% if cfg['axi_isolate_enable']:
    //AXI Isolate
    .axi_isolate_i   ( 1'b0  ),
    .axi_isolated_o  (       ),
% endif
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
  localparam int unsigned DramBase = 32'h8000_0000;
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
    .axi_req_i    ( axi_from_cluster_req      ),
    .axi_resp_o   ( axi_from_cluster_resp     )
  );
  `else
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
  `endif

  // Wide port into simulation memory.
  tb_memory_axi #(
    .AxiAddrWidth ( SpatzAxiAddrWidth    ),
    .AxiDataWidth ( SpatzAxiDataWidth    ),
    .AxiIdWidth   ( SpatzAxiIdOutWidth   ),
    .AxiUserWidth ( SpatzAxiUserWidth    ),
    .req_t        ( spatz_axi_out_req_t  ),
    .rsp_t        ( spatz_axi_out_resp_t )
  ) i_l2mem (
    .clk_i (clk_i                ),
    .rst_ni(rst_ni               ),
    .req_i (axi_l2_req           ),
    .rsp_o (axi_l2_resp          )
  );

endmodule : testharness
