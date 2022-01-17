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
  input  logic clk_i,
  input  logic rst_ni,
  // Write ports
  input  vreg_addr_t [NrWritePorts-1:0] waddr_i,
  input  vreg_data_t [NrWritePorts-1:0] wdata_i,
  input  logic       [NrWritePorts-1:0] we_i,
  input  vreg_be_t   [NrWritePorts-1:0] wbe_i,
  output logic       [NrWritePorts-1:0] wvalid_o,
  // Read ports
  input  vreg_addr_t [NrReadPorts-1:0] raddr_i,
  input  logic       [NrReadPorts-1:0] re_i,
  output vreg_data_t [NrReadPorts-1:0] rdata_o,
  output logic       [NrReadPorts-1:0] rvalid_o
);

  ////////////////
  // Parameters //
  ////////////////

  localparam int unsigned NrBanks             = 4;
  localparam int unsigned NrReadPortsPerBank  = 3;
  localparam int unsigned NrWritePortsPerBank = 1;

  localparam int unsigned RegWidth      = VLEN/NrBanks;
  localparam int unsigned ElemWidth     = N_IPU*ELEN;
  localparam int unsigned NrElemPerBank = RegWidth/ElemWidth;

  //////////////
  // Typedefs //
  //////////////

  typedef logic [$bits(vreg_addr_t)-$clog2(NrBanks)-1:0] vregfile_addr_t;

  ///////////////
  // Functions //
  ///////////////

  // Calculate the register from the vector address
  function automatic vregfile_addr_t gen_vreg_addr (vreg_addr_t vaddr);
    if (NrElemPerBank == 'd1) begin
      gen_vreg_addr = vaddr[$bits(vreg_addr_t)-1:$bits(vreg_addr_t)-$clog2(NRVREG)];
    end else begin
      gen_vreg_addr = {vaddr[$bits(vreg_addr_t)-1:$bits(vreg_addr_t)-$clog2(NRVREG)], vaddr[cf_math_pkg::idx_width(NrElemPerBank)-1:0]};
    end
  endfunction : gen_vreg_addr

  /////////////
  // Signals //
  /////////////

  // Write signals
  vregfile_addr_t [NrBanks-1:0][NrWritePortsPerBank-1:0] waddr;
  vreg_data_t     [NrBanks-1:0][NrWritePortsPerBank-1:0] wdata;
  logic           [NrBanks-1:0][NrWritePortsPerBank-1:0] we;
  vreg_be_t       [NrBanks-1:0][NrWritePortsPerBank-1:0] wbe;

  // Read signals
  vregfile_addr_t [NrBanks-1:0][NrReadPortsPerBank-1:0] raddr;
  vreg_data_t     [NrBanks-1:0][NrReadPortsPerBank-1:0] rdata;

  ///////////////////
  // Write Mapping //
  ///////////////////

  always_comb begin : proc_write
    waddr = '0;
    wdata = '0;
    we = '0;
    wbe = '0;
    wvalid_o = '0;

    // For each bank, we have a priority based access scheme. First priority always has the VFU,
    // second priority has the LSU, and third priority has the slide unit.
    for (int unsigned i = 0; i < NrBanks; i++) begin
      // Bank write port 0 - Priority: vd (0) -> lsu (1) -> sld (2)
      if (we_i[VFU_VD_WD] && waddr_i[VFU_VD_WD][$clog2(NrElemPerBank)+$clog2(NrBanks)-1:$clog2(NrElemPerBank)] == i) begin
        waddr[i] = gen_vreg_addr(waddr_i[VFU_VD_WD]);
        wdata[i] = wdata_i[VFU_VD_WD];
        we[i] = 1'b1;
        wbe[i] = wbe_i[VFU_VD_WD];
        wvalid_o[VFU_VD_WD] = 1'b1;
      end else if (we_i[VLSU_VD_WD] && waddr_i[VLSU_VD_WD][$clog2(NrElemPerBank)+$clog2(NrBanks)-1:$clog2(NrElemPerBank)] == i) begin
        waddr[i] = gen_vreg_addr(waddr_i[VLSU_VD_WD]);
        wdata[i] = wdata_i[VLSU_VD_WD];
        we[i] = 1'b1;
        wbe[i] = wbe_i[VLSU_VD_WD];
        wvalid_o[VLSU_VD_WD] = 1'b1;
      end else if (we_i[VSLD_VD_WD] && waddr_i[VSLD_VD_WD][$clog2(NrElemPerBank)+$clog2(NrBanks)-1:$clog2(NrElemPerBank)] == i) begin
        waddr[i] = gen_vreg_addr(waddr_i[VSLD_VD_WD]);
        wdata[i] = wdata_i[VSLD_VD_WD];
        we[i] = 1'b1;
        wbe[i] = wbe_i[VSLD_VD_WD];
        wvalid_o[VSLD_VD_WD] = 1'b1;
      end
    end
  end

  //////////////////
  // Read Mapping //
  //////////////////

  always_comb begin : proc_read
    raddr = '0;
    rvalid_o = '0;
    rdata_o = '0;

    // For each port or each bank we have a priority based access scheme.
    // Port zero can only be accessed by the VFU (vs2). Port one can be accessed by
    // the VFU (vs1) and then by the slide unit. Port two can be accessed first by the
    // VFU (vd), then by the LSU, and finally by the slide unit.
    for (int unsigned i = 0; i < NrBanks; i++) begin
      // Bank read port 0 - Priority: vs2
      if (re_i[VFU_VS2_RD] && raddr_i[VFU_VS2_RD][$clog2(NrElemPerBank)+$clog2(NrBanks)-1:$clog2(NrElemPerBank)] == i) begin
        raddr[i][0]          = gen_vreg_addr(raddr_i[VFU_VS2_RD]);
        rdata_o[VFU_VS2_RD]  = rdata[i][0];
        rvalid_o[VFU_VS2_RD] = 1'b1;
      end
      // Bank read port 1 - Priority: vs1 -> sld
      if (re_i[VFU_VS1_RD] && raddr_i[VFU_VS1_RD][$clog2(NrElemPerBank)+$clog2(NrBanks)-1:$clog2(NrElemPerBank)] == i) begin
        raddr[i][1]          = gen_vreg_addr(raddr_i[VFU_VS1_RD]);
        rdata_o[VFU_VS1_RD]  = rdata[i][1];
        rvalid_o[VFU_VS1_RD] = 1'b1;
      end else if (re_i[VSLD_VS2_RD] && raddr_i[VSLD_VS2_RD][$clog2(NrElemPerBank)+$clog2(NrBanks)-1:$clog2(NrElemPerBank)] == i) begin
        raddr[i][1]           = gen_vreg_addr(raddr_i[VSLD_VS2_RD]);
        rdata_o[VSLD_VS2_RD]  = rdata[i][1];
        rvalid_o[VSLD_VS2_RD] = 1'b1;
      end
      // Bank read port 2 - Priority: vd -> lsu -> sld
      if (re_i[VFU_VD_RD] && raddr_i[VFU_VD_RD][$clog2(NrElemPerBank)+$clog2(NrBanks)-1:$clog2(NrElemPerBank)] == i) begin
        raddr[i][2]         = gen_vreg_addr(raddr_i[VFU_VD_RD]);
        rdata_o[VFU_VD_RD]  = rdata[i][2];
        rvalid_o[VFU_VD_RD] = 1'b1;
      end else if (re_i[VLSU_VD_RD] && raddr_i[VLSU_VD_RD][$clog2(NrElemPerBank)+$clog2(NrBanks)-1:$clog2(NrElemPerBank)] == i) begin
        raddr[i][2]          = gen_vreg_addr(raddr_i[VLSU_VD_RD]);
        rdata_o[VLSU_VD_RD]  = rdata[i][2];
        rvalid_o[VLSU_VD_RD] = 1'b1;
      end else if (re_i[VSLD_VS2_RD] && raddr_i[VSLD_VS2_RD][$clog2(NrElemPerBank)+$clog2(NrBanks)-1:$clog2(NrElemPerBank)] == i) begin
        raddr[i][2]           = gen_vreg_addr(raddr_i[VSLD_VS2_RD]);
        rdata_o[VSLD_VS2_RD]  = rdata[i][2];
        rvalid_o[VSLD_VS2_RD] = 1'b1;
      end
    end
  end

  ////////////////
  // VREG Banks //
  ////////////////

  for (genvar bank = 0; bank < NrBanks; bank++) begin : gen_reg_banks
    vregfile #(
      .NrReadPorts (NrReadPortsPerBank),
      .NrWritePorts(NrWritePortsPerBank),
      .NrRegs      (NRVREG),
      .EnableWBE   (1'b1),
      .RegWidth    (RegWidth),
      .ElemWidth   (ElemWidth),
      .VAddrWidth  ($bits(vregfile_addr_t))
    ) i_vregfile (
      .clk_i  (clk_i),
      .rst_ni (rst_ni),
      .waddr_i(waddr[bank]),
      .wdata_i(wdata[bank]),
      .we_i   (we[bank]),
      .wbe_i  (wbe[bank]),
      .raddr_i(raddr[bank]),
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

  if (NrReadPorts / NrReadPortsPerBank > NrBanks)
    $error("[spatz_vrf] The number of vregfile banks needs to be increased to handle the number of read ports.");

  if (NrWritePorts / NrWritePortsPerBank > NrBanks)
    $error("[spatz_vrf] The number of vregfile banks needs to be increased to handle the number of write ports.");

  if (NrElemPerBank == 0)
    $error("[spatz_vrf] The number of elements per bank can not be zero.");

  if (RegWidth < ElemWidth)
    $error("[spatz_vrf] The register width has to be bigger than the element width.");

  if (spatz_pkg::N_IPU * spatz_pkg::ELEN * NrBanks > spatz_pkg::VLEN)
    $error("[spatz_vrf] The vector register length has to be equal to or larger than N_IPU*ELEN*NrVRegBanks.");

endmodule : spatz_vrf
