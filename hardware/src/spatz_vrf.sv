// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// The register file stores all vectors.

module spatz_vrf
  import spatz_pkg::*;
  #(
    parameter int unsigned NrReadPorts  = 5,
    parameter int unsigned NrWritePorts = 3
  ) (
    input  logic                          clk_i,
    input  logic                          rst_ni,
    // Write ports
    input  vreg_addr_t [NrWritePorts-1:0] waddr_i,
    input  vreg_data_t [NrWritePorts-1:0] wdata_i,
    input  logic       [NrWritePorts-1:0] we_i,
    input  vreg_be_t   [NrWritePorts-1:0] wbe_i,
    output logic       [NrWritePorts-1:0] wvalid_o,
    // Read ports
    input  vreg_addr_t [NrReadPorts-1:0]  raddr_i,
    input  logic       [NrReadPorts-1:0]  re_i,
    output vreg_data_t [NrReadPorts-1:0]  rdata_o,
    output logic       [NrReadPorts-1:0]  rvalid_o
  );

  ////////////////
  // Parameters //
  ////////////////

  localparam int unsigned NrReadPortsPerBank = 3;

  localparam int unsigned RegWidth      = VLEN/NrVRFBanks;
  localparam int unsigned ElemWidth     = N_IPU*ELEN;
  localparam int unsigned NrElemPerBank = RegWidth/ElemWidth;

  //////////////
  // Typedefs //
  //////////////

  typedef logic [$bits(vreg_addr_t)-$clog2(NrVRFBanks)-1:0] vregfile_addr_t;

  /////////////
  // Signals //
  /////////////

  // Write signals
  vregfile_addr_t [NrVRFBanks-1:0] waddr;
  vreg_data_t     [NrVRFBanks-1:0] wdata;
  logic           [NrVRFBanks-1:0] we;
  vreg_be_t       [NrVRFBanks-1:0] wbe;

  // Read signals
  vregfile_addr_t [NrVRFBanks-1:0][NrReadPortsPerBank-1:0] raddr;
  logic           [NrVRFBanks-1:0][NrReadPortsPerBank-1:0] re;
  vreg_data_t     [NrVRFBanks-1:0][NrReadPortsPerBank-1:0] rdata;

  ///////////////////
  // Write Mapping //
  ///////////////////

  logic [NrVRFBanks-1:0][NrWritePorts-1:0] write_request;
  for (genvar bank = 0; bank < NrVRFBanks; bank++) begin: gen_write_request
    for (genvar port = 0; port < NrWritePorts; port++) begin
      assign write_request[bank][port] = we_i[port] && waddr_i[port].bank == bank;
    end
  end: gen_write_request

  always_comb begin : proc_write
    waddr    = '0;
    wdata    = '0;
    we       = '0;
    wbe      = '0;
    wvalid_o = '0;

    // For each bank, we have a priority based access scheme. First priority always has the VFU,
    // second priority has the LSU, and third priority has the slide unit.
    for (int unsigned bank = 0; bank < NrVRFBanks; bank++) begin
      // Bank write port 0 - Priority: vd (0) -> lsu (1) -> sld (2)
      if (write_request[bank][VFU_VD_WD]) begin
        waddr[bank]         = waddr_i[VFU_VD_WD].vreg;
        wdata[bank]         = wdata_i[VFU_VD_WD];
        we[bank]            = 1'b1;
        wbe[bank]           = wbe_i[VFU_VD_WD];
        wvalid_o[VFU_VD_WD] = 1'b1;
      end else if (write_request[bank][VLSU_VD_WD]) begin
        waddr[bank]          = waddr_i[VLSU_VD_WD].vreg;
        wdata[bank]          = wdata_i[VLSU_VD_WD];
        we[bank]             = 1'b1;
        wbe[bank]            = wbe_i[VLSU_VD_WD];
        wvalid_o[VLSU_VD_WD] = 1'b1;
      end else if (write_request[bank][VSLDU_VD_WD]) begin
        waddr[bank]           = waddr_i[VSLDU_VD_WD].vreg;
        wdata[bank]           = wdata_i[VSLDU_VD_WD];
        we[bank]              = 1'b1;
        wbe[bank]             = wbe_i[VSLDU_VD_WD];
        wvalid_o[VSLDU_VD_WD] = 1'b1;
      end
    end
  end

  //////////////////
  // Read Mapping //
  //////////////////

  logic [NrVRFBanks-1:0][NrReadPorts-1:0] read_request;
  for (genvar bank = 0; bank < NrVRFBanks; bank++) begin: gen_read_request
    for (genvar port = 0; port < NrReadPorts; port++) begin
      assign read_request[bank][port] = re_i[port] && raddr_i[port].bank == bank;
    end
  end: gen_read_request

  always_comb begin : proc_read
    raddr    = '0;
    re       = '0;
    rvalid_o = '0;
    rdata_o  = 'x;

    // For each port or each bank we have a priority based access scheme.
    // Port zero can only be accessed by the VFU (vs2). Port one can be accessed by
    // the VFU (vs1) and then by the slide unit. Port two can be accessed first by the
    // VFU (vd), then by the LSU.
    for (int unsigned bank = 0; bank < NrVRFBanks; bank++) begin
      // Bank read port 0 - Priority: VFU (2)
      if (read_request[bank][VFU_VS2_RD]) begin
        raddr[bank][0]       = raddr_i[VFU_VS2_RD].vreg;
        re[bank][0]          = 1'b1;
        rdata_o[VFU_VS2_RD]  = rdata[bank][0];
        rvalid_o[VFU_VS2_RD] = 1'b1;
      end

      // Bank read port 1 - Priority: VFU (1) -> VSLDU
      if (read_request[bank][VFU_VS1_RD]) begin
        raddr[bank][1]       = raddr_i[VFU_VS1_RD].vreg;
        re[bank][1]          = 1'b1;
        rdata_o[VFU_VS1_RD]  = rdata[bank][1];
        rvalid_o[VFU_VS1_RD] = 1'b1;
      end else if (read_request[bank][VSLDU_VS2_RD]) begin
        raddr[bank][1]         = raddr_i[VSLDU_VS2_RD].vreg;
        re[bank][1]            = 1'b1;
        rdata_o[VSLDU_VS2_RD]  = rdata[bank][1];
        rvalid_o[VSLDU_VS2_RD] = 1'b1;
      end

      // Bank read port 2 - Priority: VFU (D) -> VLSU
      if (read_request[bank][VFU_VD_RD]) begin
        raddr[bank][2]      = raddr_i[VFU_VD_RD].vreg;
        re[bank][2]         = 1'b1;
        rdata_o[VFU_VD_RD]  = rdata[bank][2];
        rvalid_o[VFU_VD_RD] = 1'b1;
      end else if (read_request[bank][VLSU_VD_RD]) begin
        raddr[bank][2]       = raddr_i[VLSU_VD_RD].vreg;
        re[bank][2]          = 1'b1;
        rdata_o[VLSU_VD_RD]  = rdata[bank][2];
        rvalid_o[VLSU_VD_RD] = 1'b1;
      end
    end
  end

  ////////////////
  // VREG Banks //
  ////////////////

  for (genvar bank = 0; bank < NrVRFBanks; bank++) begin : gen_reg_banks
    vregfile #(
      .NrReadPorts(NrReadPortsPerBank)
    ) i_vregfile (
      .clk_i  (clk_i      ),
      .rst_ni (rst_ni     ),
      .waddr_i(waddr[bank]),
      .wdata_i(wdata[bank]),
      .we_i   (we[bank]   ),
      .wbe_i  (wbe[bank]  ),
      .raddr_i(raddr[bank]),
      .re_i   (re[bank]   ),
      .rdata_o(rdata[bank])
    );
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

  if (NrElemPerBank == 0)
    $error("[spatz_vrf] The number of elements per bank can not be zero.");

  if (RegWidth < ElemWidth)
    $error("[spatz_vrf] The register width has to be bigger than the element width.");

  if (spatz_pkg::N_IPU * spatz_pkg::ELEN * NrVRFBanks > spatz_pkg::VLEN)
    $error("[spatz_vrf] The vector register length has to be equal to or larger than N_IPU*ELEN*NrVRegBanks.");

endmodule : spatz_vrf
