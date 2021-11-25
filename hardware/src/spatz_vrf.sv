// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

module spatz_vrf import spatz_pkg::*; #(
  parameter int unsigned NR_READ_PORTS  = 5,
  parameter int unsigned NR_WRITE_PORTS = 3
) (
  input  logic clk_i,
  input  logic rst_ni,
  // Write ports
  input  vreg_addr_t [NR_WRITE_PORTS-1:0] waddr_i,
  input  vreg_data_t [NR_WRITE_PORTS-1:0] wdata_i,
  input  logic       [NR_WRITE_PORTS-1:0] we_i,
  input  vreg_be_t   [NR_WRITE_PORTS-1:0] wbe_i,
  output logic       [NR_WRITE_PORTS-1:0] wvalid_o,
  // Read ports
  input  vreg_addr_t [NR_READ_PORTS-1:0] raddr_i,
  input  logic       [NR_READ_PORTS-1:0] re_i,
  output vreg_data_t [NR_READ_PORTS-1:0] rdata_o,
  output logic       [NR_READ_PORTS-1:0] rvalid_o
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

  function vregfile_addr_t gen_vreg_addr (vreg_addr_t vaddr);
    if (NrElemPerBank == 'd1) begin
      gen_vreg_addr = vaddr[$bits(vreg_addr_t)-1:$bits(vreg_addr_t)-$clog2(NRVREG)];
    end else begin
      gen_vreg_addr = {vaddr[$bits(vreg_addr_t)-1:$bits(vreg_addr_t)-$clog2(NRVREG)], vaddr[$clog2(NrElemPerBank)-1:0]};
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
      .NR_READ_PORTS ( NrReadPortsPerBank     ),
      .NR_WRITE_PORTS( NrWritePortsPerBank    ),
      .NR_REGS       ( NRVREG                 ),
      .ENABLE_WBE    ( 1                      ),
      .REG_WIDTH     ( RegWidth               ),
      .ELEM_WIDTH    ( ElemWidth              ),
      .VADDR_WIDTH   ( $bits(vregfile_addr_t) )
    ) i_vregfile (
      .clk_i  ( clk_i       ),
      .rst_ni ( rst_ni      ),
      .waddr_i( waddr[bank] ),
      .wdata_i( wdata[bank] ),
      .we_i   ( we[bank]    ),
      .wbe_i  ( wbe[bank]   ),
      .raddr_i( raddr[bank] ),
      .rdata_o( rdata[bank] )
    );
  end

  ////////////////
  // Assertions //
  ////////////////

  if (NR_READ_PORTS < 1)
    $error("[spatz] The number of read ports has to be greater than zero.");

  if (NR_WRITE_PORTS < 1)
    $error("[spatz] The number of write ports has to be greater than zero.");

  if (NR_READ_PORTS / NrReadPortsPerBank > NrBanks)
    $error("[spatz] The number of vregfile banks needs to be increased to handle the number of read ports.");

  if (NR_WRITE_PORTS / NrWritePortsPerBank > NrBanks)
    $error("[spatz] The number of vregfile banks needs to be increased to handle the number of write ports.");

  if (NrElemPerBank == 0)
    $error("[spatz] The number of elements per bank can not be zero.");

  if (RegWidth < ElemWidth)
    $error("[spatz] The register width has to be bigger than the element width.");

  if (spatz_pkg::N_IPU*spatz_pkg::ELEN*NrBanks > spatz_pkg::VLEN)
    $error("[spatz] The vector register length has to be equal to or larger than N_IPU*ELEN*NrVRegBanks.");

endmodule : spatz_vrf

module vregfile #(
  parameter int unsigned NR_READ_PORTS  = 0,
  parameter int unsigned NR_WRITE_PORTS = 0,
  parameter int unsigned NR_REGS        = 32,
  parameter logic        ENABLE_WBE     = 1,
  parameter int unsigned REG_WIDTH      = 32,
  parameter int unsigned ELEM_WIDTH     = 32,
  parameter int unsigned VADDR_WIDTH    = 5
) (
  input  logic clk_i,
  input  logic rst_ni,
  // Write ports
  input  logic [NR_WRITE_PORTS-1:0][VADDR_WIDTH-1:0]  waddr_i,
  input  logic [NR_WRITE_PORTS-1:0][ELEM_WIDTH-1:0]   wdata_i,
  input  logic [NR_WRITE_PORTS-1:0]                   we_i,
  input  logic [NR_WRITE_PORTS-1:0][ELEM_WIDTH/8-1:0] wbe_i,
  // Read ports
  input  logic [NR_READ_PORTS-1:0][VADDR_WIDTH-1:0]   raddr_i,
  output logic [NR_READ_PORTS-1:0][ELEM_WIDTH-1:0]    rdata_o
);

  localparam int unsigned NUM_ELEM_PER_REG = REG_WIDTH/ELEM_WIDTH;
  localparam int unsigned ELEM_WBE = ENABLE_WBE ? ELEM_WIDTH/8 : 1;
  localparam int unsigned ELEM_WBE_SIZE = ENABLE_WBE ? 8 : ELEM_WIDTH;

  logic clk;
  logic [NR_REGS-1:0][NUM_ELEM_PER_REG-1:0][ELEM_WBE-1:0] mem_clocks;

  logic [NR_REGS-1:0][NUM_ELEM_PER_REG-1:0][ELEM_WIDTH-1:0] mem;

  logic [NR_WRITE_PORTS-1:0][ELEM_WIDTH-1:0]                                  wdata_q;
  logic [NR_WRITE_PORTS-1:0][NR_REGS-1:0][NUM_ELEM_PER_REG-1:0][ELEM_WBE-1:0] waddr_onehot;
  logic [NR_REGS-1:0][NUM_ELEM_PER_REG-1:0][ELEM_WBE-1:0][NR_WRITE_PORTS-1:0] waddr_onehot_trans;

  for (genvar i = 0; i < NR_WRITE_PORTS; i++) begin
    for (genvar j = 0; j < NR_REGS; j++) begin
      for (genvar k = 0; k < NUM_ELEM_PER_REG; k++) begin
        for (genvar l = 0; l < ELEM_WBE; l++) begin
          assign waddr_onehot_trans[j][k][l][i] = waddr_onehot[i][j][k][l];
        end
      end
    end
  end

  tc_clk_gating i_regfile_cg (
    .clk_i,
    .en_i      ( |we_i  ),
    .test_en_i ( 1'b0   ),
    .clk_o     ( clk    )
  );

  // Sample Input Data
  for (genvar i = 0; i < NR_WRITE_PORTS; i++) begin
    always_ff @(posedge clk) wdata_q[i] <= wdata_i[i];

    for (genvar j = 0; j < NR_REGS; j++) begin
      for (genvar k = 0; k < NUM_ELEM_PER_REG; k++) begin
        for (genvar l = 0; l < ELEM_WBE; l++) begin
          if (NUM_ELEM_PER_REG > 1) begin
            always_comb begin
              if (we_i[i] && ((wbe_i[i][l] && ENABLE_WBE) || ~ENABLE_WBE) && waddr_i[i][VADDR_WIDTH-1:$clog2(NUM_ELEM_PER_REG)] == j && waddr_i[i][$clog2(NUM_ELEM_PER_REG)-1:0] == k) waddr_onehot[i][j][k][l] = 1'b1;
              else waddr_onehot[i][j][k][l] = 1'b0;
            end
          end else begin
              always_comb begin
              if (we_i[i] && ((wbe_i[i][l] && ENABLE_WBE) || ~ENABLE_WBE) && waddr_i[i][VADDR_WIDTH-1:0] == j) waddr_onehot[i][j][k][l] = 1'b1;
              else waddr_onehot[i][j][k][l] = 1'b0;
            end
          end
        end
      end
    end
  end

  for (genvar i = 0; i < NR_REGS; i++) begin
    for (genvar j = 0; j < NUM_ELEM_PER_REG; j++) begin
      for (genvar k = 0; k < ELEM_WBE; k++) begin
        tc_clk_gating i_regfile_cg (
          .clk_i     ( clk                          ),
          .en_i      ( |waddr_onehot_trans[i][j][k] ),
          .test_en_i ( 1'b0                         ),
          .clk_o     ( mem_clocks[i][j][k]          )
        );
      end
    end
  end

  always_latch begin
    for (int unsigned i = 0; i < NR_REGS; i++) begin
      for (int unsigned j = 0; j < NUM_ELEM_PER_REG; j++) begin
        for (int unsigned k = 0; k < ELEM_WBE; k++) begin
          for (int unsigned l = 0; l < NR_WRITE_PORTS; l++) begin
            if (!rst_ni) begin
              mem[i][j] = '0;
            end if (mem_clocks[i][j][k]) begin
              mem[i][j][ELEM_WBE_SIZE*k +: ELEM_WBE_SIZE] = wdata_q[l][ELEM_WBE_SIZE*k +: ELEM_WBE_SIZE];
            end
          end
        end
      end
    end
  end

  if (NUM_ELEM_PER_REG > 1) begin
    for (genvar i = 0; i < NR_READ_PORTS; i++) assign rdata_o[i] = mem[raddr_i[i][VADDR_WIDTH-1:$clog2(NUM_ELEM_PER_REG)]][raddr_i[i][$clog2(NUM_ELEM_PER_REG)-1:0]];
  end else begin
    for (genvar i = 0; i < NR_READ_PORTS; i++) assign rdata_o[i] = mem[raddr_i[i][VADDR_WIDTH-1:0]][0];
  end

endmodule : vregfile
