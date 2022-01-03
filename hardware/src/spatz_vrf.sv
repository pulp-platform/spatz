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

  localparam int unsigned RegWidth  = VLEN/NrBanks;
  localparam int unsigned ElemWidth = N_IPU*ELEN;
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
    // second priority has the LSU, and third priority had the slide unit.
    for (int unsigned i = 0; i < NrBanks; i++) begin
      // Bank write port 0 - Priority: vd (0) -> lsu (1) -> sld (2)
      if (we_i[0] && waddr_i[0][$bits(vreg_addr_t)-$clog2(NRVREG)-1:$clog2(NrElemPerBank)] == i) begin
        waddr[i] = gen_vreg_addr(waddr_i[0]);
        wdata[i] = wdata_i[0];
        we[i] = 1'b1;
        wbe[i] = wbe_i[0];
        wvalid_o[0] = 1'b1;
      end else if (we_i[1] && waddr_i[1][$bits(vreg_addr_t)-$clog2(NRVREG)-1:$clog2(NrElemPerBank)] == i) begin
        waddr[i] = gen_vreg_addr(waddr_i[1]);
        wdata[i] = wdata_i[1];
        we[i] = 1'b1;
        wbe[i] = wbe_i[1];
        wvalid_o[1] = 1'b1;
      end else if (we_i[2] && waddr_i[2][$bits(vreg_addr_t)-$clog2(NRVREG)-1:$clog2(NrElemPerBank)] == i) begin
        waddr[i] = gen_vreg_addr(waddr_i[2]);
        wdata[i] = wdata_i[2];
        we[i] = 1'b1;
        wbe[i] = wbe_i[2];
        wvalid_o[2] = 1'b1;
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
      // Bank read port 0 - Priority: vs2 (0)
      if (re_i[0] && raddr_i[0][$bits(vreg_addr_t)-$clog2(NRVREG)-1:$clog2(NrElemPerBank)] == i) begin
        raddr[i][0] = gen_vreg_addr(raddr_i[0]);
        rdata_o[0] = rdata[i][0];
        rvalid_o[0] = 1'b1;
      end
      // Bank read port 1 - Priority: vs1 (1) -> sld (4)
      if (re_i[1] && raddr_i[1][$bits(vreg_addr_t)-$clog2(NRVREG)-1:$clog2(NrElemPerBank)] == i) begin
        raddr[i][1] = gen_vreg_addr(raddr_i[1]);
        rdata_o[1] = rdata[i][1];
        rvalid_o[1] = 1'b1;
      end else if (re_i[4] && raddr_i[4][$bits(vreg_addr_t)-$clog2(NRVREG)-1:$clog2(NrElemPerBank)] == i) begin
        raddr[i][1] = gen_vreg_addr(raddr_i[4]);
        rdata_o[4] = rdata[i][1];
        rvalid_o[4] = 1'b1;
      end
      // Bank read port 2 - Priority: vd (2) -> lsu (3) -> sld (4)
      if (re_i[2] && raddr_i[2][$bits(vreg_addr_t)-$clog2(NRVREG)-1:$clog2(NrElemPerBank)] == i) begin
        raddr[i][2] = gen_vreg_addr(raddr_i[2]);
        rdata_o[2] = rdata[i][2];
        rvalid_o[2] = 1'b1;
      end else if (re_i[3] && raddr_i[3][$bits(vreg_addr_t)-$clog2(NRVREG)-1:$clog2(NrElemPerBank)] == i) begin
        raddr[i][2] = gen_vreg_addr(raddr_i[3]);
        rdata_o[3] = rdata[i][2];
        rvalid_o[3] = 1'b1;
      end else if (re_i[4] && raddr_i[4][$bits(vreg_addr_t)-$clog2(NRVREG)-1:$clog2(NrElemPerBank)] == i) begin
        raddr[i][2] = gen_vreg_addr(raddr_i[4]);
        rdata_o[4] = rdata[i][2];
        rvalid_o[4] = 1'b1;
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

  if (spatz_pkg::N_IPU*spatz_pkg::ELEN*NrBanks > spatz_pkg::VLEN)
    $error("[spatz_vrf] The vector register length has to be equal to or larger than N_IPU*ELEN*NrVRegBanks.");

endmodule : spatz_vrf

module vregfile #(
  parameter int unsigned NrReadPorts  = 0,
  parameter int unsigned NrWritePorts = 0,
  parameter int unsigned NrRegs       = 32,
  parameter logic        EnableWBE    = 1,
  parameter int unsigned RegWidth     = 32,
  parameter int unsigned ElemWidth    = 32,
  parameter int unsigned VAddrWidth   = 5
) (
  input  logic clk_i,
  input  logic rst_ni,
  // Write ports
  input  logic [NrWritePorts-1:0][VAddrWidth-1:0]  waddr_i,
  input  logic [NrWritePorts-1:0][ElemWidth-1:0]   wdata_i,
  input  logic [NrWritePorts-1:0]                  we_i,
  input  logic [NrWritePorts-1:0][ElemWidth/8-1:0] wbe_i,
  // Read ports
  input  logic [NrReadPorts-1:0][VAddrWidth-1:0]   raddr_i,
  output logic [NrReadPorts-1:0][ElemWidth-1:0]    rdata_o
);

  /////////////////
  // Localparams //
  /////////////////

  localparam int unsigned NumElemPerReg       = RegWidth/ElemWidth;
  localparam int unsigned NumBytesPerElem     = EnableWBE ? ElemWidth/8 : 1;
  localparam int unsigned NumBytesPerElemSize = EnableWBE ? 8 : ElemWidth;

  /////////////
  // Signals //
  /////////////

  // Gated clock
  logic clk;

  // Register file memory
  logic [NrRegs-1:0][NumElemPerReg-1:0][NumBytesPerElem-1:0][NumBytesPerElemSize-1:0] mem;

  // Write data sampling
  logic [NrWritePorts-1:0][NumBytesPerElem-1:0][NumBytesPerElemSize-1:0]       wdata_q;
  logic [NrRegs-1:0][NumElemPerReg-1:0][NumBytesPerElem-1:0][NrWritePorts-1:0] waddr_onehot;

  ///////////////////
  // Register File //
  ///////////////////

  // Main vregfile clock gate
  tc_clk_gating i_regfile_cg (
    .clk_i    (clk_i),
    .en_i     (|we_i),
    .test_en_i(1'b0),
    .clk_o    (clk)
  );

  // Sample Input Data
  for (genvar i = 0; i < NrWritePorts; i++) begin
    always_ff @(posedge clk) begin
      wdata_q[i] <= wdata_i[i];
    end
  end

  // Create latch clock signal
  for (genvar i = 0; i < NrWritePorts; i++) begin
    // Select which destination bytes to write into
    for (genvar j = 0; j < NrRegs; j++) begin
      for (genvar k = 0; k < NumElemPerReg; k++) begin
        for (genvar l = 0; l < NumBytesPerElem; l++) begin
          if (NumElemPerReg == 'd1) begin
            assign waddr_onehot[j][k][l][i] = (we_i[i] & ((wbe_i[i][l] & EnableWBE) | ~EnableWBE)
                                                       & waddr_i[i][VAddrWidth-1:0] == j);
          end else begin
            assign waddr_onehot[j][k][l][i] = (we_i[i] & ((wbe_i[i][l] & EnableWBE) | ~EnableWBE)
                                                       & waddr_i[i][VAddrWidth-1:$clog2(NumElemPerReg)] == j
                                                       & waddr_i[i][$clog2(NumElemPerReg)-1:0] == k);
          end
        end
      end
    end
  end

  // Store new data to memory
  for (genvar i = 0; i < NrRegs; i++) begin
    for (genvar j = 0; j < NumElemPerReg; j++) begin
      for (genvar k = 0; k < NumBytesPerElem; k++) begin
        logic clk_latch;

        tc_clk_gating i_regfile_cg (
          .clk_i    (clk),
          .en_i     (|waddr_onehot[i][j][k]),
          .test_en_i(1'b0),
          .clk_o    (clk_latch)
        );

        /* verilator lint_off NOLATCH */
        always_latch begin
          // TODO: Generalize for more than one write port
          if (clk_latch) begin
            mem[i][j][k] <= wdata_q[0][k];
          end
        end
        /* verilator lint_on NOLATCH */
      end
    end
  end

  // Read data from memory
  for (genvar i = 0; i < NrReadPorts; i++) begin
    if (NumElemPerReg == 'd1) begin
      assign rdata_o[i] = mem[raddr_i[i]];
    end else begin
      assign rdata_o[i] = mem[raddr_i[i][VAddrWidth-1:$clog2(NumElemPerReg)]][raddr_i[i][$clog2(NumElemPerReg)-1:0]];
    end
  end

endmodule : vregfile
