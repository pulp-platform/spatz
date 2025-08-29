// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich
//
// The register file stores all vectors, organized into banks.

module spatz_vrf
  import spatz_pkg::*;
  #(
    parameter int unsigned NrReadPorts  = 5,
    parameter int unsigned NrWritePorts = 3,
    parameter int unsigned FpuBufDepth  = 4
  ) (
    input  logic                         clk_i,
    input  logic                         rst_ni,
    input  logic                         testmode_i,
    // Write ports
    input  vrf_addr_t [NrWritePorts-1:0] waddr_i,
    input  vrf_data_t [NrWritePorts-1:0] wdata_i,
    input  logic      [NrWritePorts-1:0] we_i,
    input  vrf_be_t   [NrWritePorts-1:0] wbe_i, // 32 bits* 3 WritePorts
    output logic      [NrWritePorts-1:0] wvalid_o,
`ifdef BUF_FPU
    // Signal to track if  result can be buffered or not
    input  logic      [$clog2(FpuBufDepth)-1:0] fpu_buf_usage_i,
`endif
    // Read ports
    input  vrf_addr_t [NrReadPorts-1:0]  raddr_i,
    input  logic      [NrReadPorts-1:0]  re_i,
    output vrf_data_t [NrReadPorts-1:0]  rdata_o,
    output logic      [NrReadPorts-1:0]  rvalid_o
`ifdef VENTAGLIO
    ,
    // VTL redirect inputs: per-port flags that re-route a VRF access into
    // the Ventaglio bank instead of a regular VRF bank.
    input  logic      [NrWritePorts-1:0] vtl_redirect_write_i,
    input  logic      [NrReadPorts-1:0]  vtl_redirect_read_i,
    // Write master ports to VTL
    output vrf_addr_t                    waddr_o,
    output vrf_data_t                    wdata_o,
    output logic                         we_o,
    output vrf_be_t                      wbe_o,
    input  logic                         wvalid_i,
    output logic                         wscatter_en_o,
    // Read master ports to VTL
    output vrf_addr_t                    raddr_o,
    output logic                         re_o,
    input  vrf_data_t                    rdata_i,
    input  logic                         rvalid_i,
    output logic                         rgather_en_o
`endif
  );

`include "common_cells/registers.svh"

  ////////////////
  // Parameters //
  ////////////////

  localparam int unsigned NrReadPortsPerBank = 3;

  //////////////
  // Typedefs //
  //////////////

  typedef logic [$bits(vrf_addr_t)-$clog2(NrVRFBanks)-1:0] vregfile_addr_t; // divide the addresses into banks.

  // the word index within one bank
  function automatic logic [$clog2(NrWordsPerBank)-1:0] f_vreg(vrf_addr_t addr);
    f_vreg = addr[$clog2(NrVRFWords)-1:$clog2(NrVRFBanks)];
  endfunction: f_vreg

  function automatic logic [$clog2(NrVRFBanks)-1:0] f_bank(vrf_addr_t addr);
`ifdef DOUBLE_BW
    // Use a simple vector register to bank mapping
    // No particular performance benefit from barber pole layout since 3R ports per bank is already available
    f_bank = addr[$clog2(NrVRFBanks)-1:0];
`else
    // Is this vreg divisible by eight?
    automatic logic [1:0] vreg8 = addr[$clog2(8*NrWordsPerVector) +: 2];

    // Barber's pole. Advance the starting bank of each vector by one every eight vector registers.
    f_bank = addr[$clog2(NrVRFBanks)-1:0] + vreg8;
`endif
  endfunction: f_bank

  /////////////
  // Signals //
  /////////////

  // Write signals
  vregfile_addr_t [NrVRFBanks-1:0] waddr;
  vrf_data_t      [NrVRFBanks-1:0] wdata;
  logic           [NrVRFBanks-1:0] we;
  vrf_be_t        [NrVRFBanks-1:0] wbe;

  // Signals to handle conflicts between VFU and VLSU interfaces
  logic           [NrVRFBanks-1:0] w_vlsu_vfu_conflict;
  logic           [NrVRFBanks-1:0] w_vfu;

  // Read signals
  vregfile_addr_t [NrVRFBanks-1:0][NrReadPortsPerBank-1:0] raddr;
  vrf_data_t      [NrVRFBanks-1:0][NrReadPortsPerBank-1:0] rdata;

  ///////////////////
  // Write Mapping //
  ///////////////////

  logic [NrVRFBanks-1:0][NrWritePorts-1:0] write_request;
  always_comb begin: gen_write_request
    for (int bank = 0; bank < NrVRFBanks; bank++) begin
      for (int port = 0; port < NrWritePorts; port++) begin
`ifdef VENTAGLIO
        write_request[bank][port] = we_i[port] && f_bank(waddr_i[port]) == bank && (vtl_redirect_write_i[port] != 1'b1);
`else
        write_request[bank][port] = we_i[port] && f_bank(waddr_i[port]) == bank;
`endif
      end
    end
  end: gen_write_request

  always_comb begin : proc_write
    waddr    = '0;
    wdata    = '0;
    we       = '0;
    wbe      = '0;
    wvalid_o = '0;

`ifdef VENTAGLIO
    // signals for request VTL forwarding
    waddr_o = '0;
    wdata_o = '0;
    we_o    =  0;
    wbe_o   = '0;
    wscatter_en_o = 1'b0;

    // forward the write request to VTL
    // write requests from VFU has the highest priority
    // no write request from VTL will be forwarded
    if (vtl_redirect_write_i[VFU_VD_WD] == 1'b1) begin
      waddr_o        = waddr_i[VFU_VD_WD];
      wdata_o        = wdata_i[VFU_VD_WD];
      we_o           = we_i[VFU_VD_WD];
      wbe_o          = wbe_i[VFU_VD_WD];
      wvalid_o[VFU_VD_WD] = wvalid_i;
      wscatter_en_o  = 1'b1; // TODO (bowwang): we assume only write from VFU need scatter, which may not hold
    end else if (vtl_redirect_write_i[VLSU_VD_WD] == 1'b1) begin
      waddr_o        = waddr_i[VLSU_VD_WD];
      wdata_o        = wdata_i[VLSU_VD_WD];
      we_o           = we_i[VLSU_VD_WD];
      wbe_o          = wbe_i[VLSU_VD_WD];
      wvalid_o[VLSU_VD_WD] = wvalid_i;
      wscatter_en_o  = 1'b0;
    end
`endif


    // For each bank, we have a priority based access scheme. First priority always has the VFU,
    // second priority has the LSU, and third priority has the slide unit.
    for (int unsigned bank = 0; bank < NrVRFBanks; bank++) begin
`ifdef BUF_FPU
`ifdef DOUBLE_BW
      automatic logic write_request_vlsu = write_request[bank][VLSU_VD_WD0];
`else
      automatic logic write_request_vlsu = write_request[bank][VLSU_VD_WD];
`endif
      w_vlsu_vfu_conflict[bank] = write_request_vlsu & write_request[bank][VFU_VD_WD];
      // Prioritize VFU when VFU buffer usage is high
      // Otherwise VLSU gets the priority
      w_vfu[bank] = w_vlsu_vfu_conflict[bank] && (fpu_buf_usage_i >= (FpuBufDepth-2));
`else
      // If no buffering is done, prioritize VFU always
      w_vfu[bank] = 1'b1;
`endif

`ifdef DOUBLE_BW
      if (~w_vfu[bank]) begin
        // Prioritize VLSU interfaces
        if (write_request[bank][VLSU_VD_WD0]) begin
          waddr[bank]          = f_vreg(waddr_i[VLSU_VD_WD0]);
          wdata[bank]          = wdata_i[VLSU_VD_WD0];
          we[bank]             = 1'b1;
          wbe[bank]            = wbe_i[VLSU_VD_WD0];
          wvalid_o[VLSU_VD_WD0] = 1'b1;
        end else if (write_request[bank][VLSU_VD_WD1]) begin
          waddr[bank]          = f_vreg(waddr_i[VLSU_VD_WD1]);
          wdata[bank]          = wdata_i[VLSU_VD_WD1];
          we[bank]             = 1'b1;
          wbe[bank]            = wbe_i[VLSU_VD_WD1];
          wvalid_o[VLSU_VD_WD1] = 1'b1;
        end else if (write_request[bank][VFU_VD_WD]) begin
          waddr[bank]         = f_vreg(waddr_i[VFU_VD_WD]);
          wdata[bank]         = wdata_i[VFU_VD_WD];
          we[bank]            = 1'b1;
          wbe[bank]           = wbe_i[VFU_VD_WD];
          wvalid_o[VFU_VD_WD] = 1'b1;
        end else if (write_request[bank][VSLDU_VD_WD]) begin
          waddr[bank]           = f_vreg(waddr_i[VSLDU_VD_WD]);
          wdata[bank]           = wdata_i[VSLDU_VD_WD];
          we[bank]              = 1'b1;
          wbe[bank]             = wbe_i[VSLDU_VD_WD];
          wvalid_o[VSLDU_VD_WD] = 1'b1;
        end
      end else begin
        // Prioritize VFU
        if (write_request[bank][VFU_VD_WD]) begin
          waddr[bank]         = f_vreg(waddr_i[VFU_VD_WD]);
          wdata[bank]         = wdata_i[VFU_VD_WD];
          we[bank]            = 1'b1;
          wbe[bank]           = wbe_i[VFU_VD_WD];
          wvalid_o[VFU_VD_WD] = 1'b1;
        end else if (write_request[bank][VLSU_VD_WD0]) begin
          waddr[bank]          = f_vreg(waddr_i[VLSU_VD_WD0]);
          wdata[bank]          = wdata_i[VLSU_VD_WD0];
          we[bank]             = 1'b1;
          wbe[bank]            = wbe_i[VLSU_VD_WD0];
          wvalid_o[VLSU_VD_WD0] = 1'b1;
        end else if (write_request[bank][VLSU_VD_WD1]) begin
          waddr[bank]          = f_vreg(waddr_i[VLSU_VD_WD1]);
          wdata[bank]          = wdata_i[VLSU_VD_WD1];
          we[bank]             = 1'b1;
          wbe[bank]            = wbe_i[VLSU_VD_WD1];
          wvalid_o[VLSU_VD_WD1] = 1'b1;
        end else if (write_request[bank][VSLDU_VD_WD]) begin
          waddr[bank]           = f_vreg(waddr_i[VSLDU_VD_WD]);
          wdata[bank]           = wdata_i[VSLDU_VD_WD];
          we[bank]              = 1'b1;
          wbe[bank]             = wbe_i[VSLDU_VD_WD];
          wvalid_o[VSLDU_VD_WD] = 1'b1;
        end
      end
`else
      if (~w_vfu[bank]) begin
        // Prioritize VLSU interfaces
        if (write_request[bank][VLSU_VD_WD]) begin
          waddr[bank]          = f_vreg(waddr_i[VLSU_VD_WD]);
          wdata[bank]          = wdata_i[VLSU_VD_WD];
          we[bank]             = 1'b1;
          wbe[bank]            = wbe_i[VLSU_VD_WD];
          wvalid_o[VLSU_VD_WD] = 1'b1;
        end else if (write_request[bank][VFU_VD_WD]) begin
          waddr[bank]         = f_vreg(waddr_i[VFU_VD_WD]);
          wdata[bank]         = wdata_i[VFU_VD_WD];
          we[bank]            = 1'b1;
          wbe[bank]           = wbe_i[VFU_VD_WD];
          wvalid_o[VFU_VD_WD] = 1'b1;
        end else if (write_request[bank][VSLDU_VD_WD]) begin
          waddr[bank]           = f_vreg(waddr_i[VSLDU_VD_WD]);
          wdata[bank]           = wdata_i[VSLDU_VD_WD];
          we[bank]              = 1'b1;
          wbe[bank]             = wbe_i[VSLDU_VD_WD];
          wvalid_o[VSLDU_VD_WD] = 1'b1;
        end
      end else begin
        // Prioritize VFU
        if (write_request[bank][VFU_VD_WD]) begin
          waddr[bank]         = f_vreg(waddr_i[VFU_VD_WD]);
          wdata[bank]         = wdata_i[VFU_VD_WD];
          we[bank]            = 1'b1;
          wbe[bank]           = wbe_i[VFU_VD_WD];
          wvalid_o[VFU_VD_WD] = 1'b1;
        end else if (write_request[bank][VLSU_VD_WD]) begin
          waddr[bank]          = f_vreg(waddr_i[VLSU_VD_WD]);
          wdata[bank]          = wdata_i[VLSU_VD_WD];
          we[bank]             = 1'b1;
          wbe[bank]            = wbe_i[VLSU_VD_WD];
          wvalid_o[VLSU_VD_WD] = 1'b1;
        end else if (write_request[bank][VSLDU_VD_WD]) begin
          waddr[bank]           = f_vreg(waddr_i[VSLDU_VD_WD]);
          wdata[bank]           = wdata_i[VSLDU_VD_WD];
          we[bank]              = 1'b1;
          wbe[bank]             = wbe_i[VSLDU_VD_WD];
          wvalid_o[VSLDU_VD_WD] = 1'b1;
        end
      end
`endif
    end
  end

  //////////////////
  // Read Mapping //
  //////////////////

  logic [NrVRFBanks-1:0][NrReadPorts-1:0] read_request;
  always_comb begin: gen_read_request
    for (int bank = 0; bank < NrVRFBanks; bank++) begin
      for (int port = 0; port < NrReadPorts; port++) begin
`ifdef VENTAGLIO
        read_request[bank][port] = re_i[port] && f_bank(raddr_i[port]) == bank && (vtl_redirect_read_i[port] != 1'b1);
`else
        read_request[bank][port] = re_i[port] && f_bank(raddr_i[port]) == bank;
`endif
      end
    end
  end: gen_read_request

  always_comb begin : proc_read
    raddr    = '0;
    rvalid_o = '0;
    rdata_o  = 'x;

`ifdef VENTAGLIO
    // signals for request VTL forwarding
    raddr_o  = '0;
    re_o     =  0;
    rgather_en_o = 1'b0;

    // forward the read request to VTL with priority
    // TODO (bowwang): we assume only read from VFU need gather, which may not hold
    if (vtl_redirect_read_i[VFU_VD_RD] == 1'b1) begin
      raddr_o        = raddr_i[VFU_VD_RD];
      re_o           = re_i[VFU_VD_RD];
      rgather_en_o   = 1'b1;
      rdata_o[VFU_VD_RD]  = rdata_i;
      rvalid_o[VFU_VD_RD] = rvalid_i;
    end else if (vtl_redirect_read_i[VFU_VS2_RD] == 1'b1) begin
      raddr_o        = raddr_i[VFU_VS2_RD];
      re_o           = re_i[VFU_VS2_RD];
      rgather_en_o   = 1'b1;
      rdata_o[VFU_VS2_RD]  = rdata_i;
      rvalid_o[VFU_VS2_RD] = rvalid_i;
    end else if (vtl_redirect_read_i[VFU_VS1_RD] == 1'b1) begin
      raddr_o        = raddr_i[VFU_VS1_RD];
      re_o           = re_i[VFU_VS1_RD];
      rgather_en_o   = 1'b1;
      rdata_o[VFU_VS1_RD]  = rdata_i;
      rvalid_o[VFU_VS1_RD] = rvalid_i;
    end else if (vtl_redirect_read_i[VLSU_VD_RD] == 1'b1) begin
      raddr_o        = raddr_i[VLSU_VD_RD];
      re_o           = re_i[VLSU_VD_RD];
      rgather_en_o   = 1'b0;
      rdata_o[VLSU_VD_RD]  = rdata_i;
      rvalid_o[VLSU_VD_RD] = rvalid_i;
    end else if (vtl_redirect_read_i[VLSU_VS2_RD] == 1'b1) begin
      raddr_o        = raddr_i[VLSU_VS2_RD];
      re_o           = re_i[VLSU_VS2_RD];
      rgather_en_o   = 1'b0;
      rdata_o[VLSU_VS2_RD]  = rdata_i;
      rvalid_o[VLSU_VS2_RD] = rvalid_i;
    end
`endif


    // For each port or each bank we have a priority based access scheme.
    // Port zero can only be accessed by the VFU (vs2). Port one can be accessed by
    // the VFU (vs1) and then by the slide unit. Port two can be accessed first by the
    // VFU (vd), then by the LSU.
    for (int unsigned bank = 0; bank < NrVRFBanks; bank++) begin
      // Bank read port 0 - Priority: VFU (2) -> VLSU
      if (read_request[bank][VFU_VS2_RD]) begin
        raddr[bank][0]       = f_vreg(raddr_i[VFU_VS2_RD]);
        rdata_o[VFU_VS2_RD]  = rdata[bank][0];
        rvalid_o[VFU_VS2_RD] = 1'b1;
      end
`ifdef DOUBLE_BW
      else if (read_request[bank][VLSU_VD_RD0]) begin
        raddr[bank][0]        = f_vreg(raddr_i[VLSU_VD_RD0]);
        rdata_o[VLSU_VD_RD0]  = rdata[bank][0];
        rvalid_o[VLSU_VD_RD0] = 1'b1;
      end
`else
      else if (read_request[bank][VLSU_VS2_RD]) begin
        raddr[bank][0]        = f_vreg(raddr_i[VLSU_VS2_RD]);
        rdata_o[VLSU_VS2_RD]  = rdata[bank][0];
        rvalid_o[VLSU_VS2_RD] = 1'b1;
      end
`endif

      // Bank read port 1 - Priority: VFU (1) -> VLSU -> VSLDU
      if (read_request[bank][VFU_VS1_RD]) begin
        raddr[bank][1]       = f_vreg(raddr_i[VFU_VS1_RD]);
        rdata_o[VFU_VS1_RD]  = rdata[bank][1];
        rvalid_o[VFU_VS1_RD] = 1'b1;
      end
`ifdef DOUBLE_BW
      else if (read_request[bank][VLSU_VD_RD1]) begin
        raddr[bank][1]         = f_vreg(raddr_i[VLSU_VD_RD1]);
        rdata_o[VLSU_VD_RD1]  = rdata[bank][1];
        rvalid_o[VLSU_VD_RD1] = 1'b1;
      end
`endif
      else if (read_request[bank][VSLDU_VS2_RD]) begin
        raddr[bank][1]         = f_vreg(raddr_i[VSLDU_VS2_RD]);
        rdata_o[VSLDU_VS2_RD]  = rdata[bank][1];
        rvalid_o[VSLDU_VS2_RD] = 1'b1;
      end

      // Bank read port 2 - Priority: VFU (D) -> VLSU
      if (read_request[bank][VFU_VD_RD]) begin
        raddr[bank][2]      = f_vreg(raddr_i[VFU_VD_RD]);
        rdata_o[VFU_VD_RD]  = rdata[bank][2];
        rvalid_o[VFU_VD_RD] = 1'b1;
      end
`ifdef DOUBLE_BW
      // VLSU indices
      else if (read_request[bank][VLSU_VS2_RD0]) begin
        raddr[bank][2]        = f_vreg(raddr_i[VLSU_VS2_RD0]);
        rdata_o[VLSU_VS2_RD0]  = rdata[bank][2];
        rvalid_o[VLSU_VS2_RD0] = 1'b1;
      end else if (read_request[bank][VLSU_VS2_RD1]) begin
        raddr[bank][2]        = f_vreg(raddr_i[VLSU_VS2_RD1]);
        rdata_o[VLSU_VS2_RD1]  = rdata[bank][2];
        rvalid_o[VLSU_VS2_RD1] = 1'b1;
      end
`else
      else if (read_request[bank][VLSU_VD_RD]) begin
        raddr[bank][2]       = f_vreg(raddr_i[VLSU_VD_RD]);
        rdata_o[VLSU_VD_RD]  = rdata[bank][2];
        rvalid_o[VLSU_VD_RD] = 1'b1;
      end
`endif
    end
  end

  ////////////////
  // VREG Banks //
  ////////////////

  for (genvar bank = 0; bank < NrVRFBanks; bank++) begin : gen_reg_banks
    for (genvar cut = 0; cut < N_FU; cut++) begin: gen_vrf_slice
      elen_t [NrReadPortsPerBank-1:0] rdata_int;

      for (genvar port = 0; port < NrReadPortsPerBank; port++) begin: gen_rdata_assignment
        assign rdata[bank][port][ELEN*cut +: ELEN] = rdata_int[port];
      end

      vregfile #(
        .NrReadPorts(NrReadPortsPerBank),
        .NrWords    (NrWordsPerBank    ),
        .WordWidth  (ELEN              )
      ) i_vregfile (
        .clk_i     (clk_i                        ),
        .rst_ni    (rst_ni                       ),
        .testmode_i(testmode_i                   ),
        .waddr_i   (waddr[bank]                  ),
        .wdata_i   (wdata[bank][ELEN*cut +: ELEN]),
        .we_i      (we[bank]                     ),
        .wbe_i     (wbe[bank][ELENB*cut +: ELENB]),
        .raddr_i   (raddr[bank]                  ),
        .rdata_o   (rdata_int                    )
      );
    end
  end

  ////////////////
  // Assertions //
  ////////////////

  if (NrReadPorts < 1)
    $error("[spatz_vrf] The number of read ports has to be greater than zero.");

  if (NrWritePorts < 1)
    $error("[spatz_vrf] The number of write ports has to be greater than zero.");

  if (NrReadPorts / NrReadPortsPerBank > NrVRFBanks)
    $error("[spatz_vrf] The number of vregfile banks needs to be increased to handle the number of read ports.");

endmodule : spatz_vrf
