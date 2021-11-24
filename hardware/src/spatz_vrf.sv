// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

module spatz_vrf import spatz_pkg::*; #(
  parameter int unsigned NR_READ_PORTS  = 0,
  parameter int unsigned NR_WRITE_PORTS = 0,
  parameter int unsigned NR_BANKS       = 4
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

  // Write dummy out
  assign wvalid_o = we_i;

  // Read dummy in
  //assign rdata_o  = '0;
  assign rvalid_o = re_i;

  for (genvar bank = 0; bank < NR_BANKS; bank++) begin : gen_reg_banks
    vregfile #(
      .NR_READ_PORTS (NR_READ_PORTS),
      .NR_WRITE_PORTS(NR_WRITE_PORTS),
      .NR_REGS       (NRVREG),
      .ENABLE_WBE    (1),
      .REG_WIDTH     (VLEN/NR_BANKS),
      .ELEM_WIDTH    (N_IPU*ELEN),
      .VADDR_WIDTH   ($bits(vreg_addr_t))
    ) i_vregfile (
      .clk_i  (clk_i),
      .rst_ni (rst_ni),
      .waddr_i(waddr_i),
      .wdata_i(wdata_i),
      .we_i   (we_i),
      .wbe_i  (wbe_i),
      .raddr_i(raddr_i),
      .rdata_o(rdata_o)
    );
  end

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
          always_comb begin
            if (we_i[i] && ((wbe_i[i][l] && ENABLE_WBE) || ~ENABLE_WBE) && waddr_i[i][VADDR_WIDTH-1:$clog2(NUM_ELEM_PER_REG)] == j && waddr_i[i][$clog2(NUM_ELEM_PER_REG)-1:0] == k) waddr_onehot[i][j][k][l] = 1'b1;
            else waddr_onehot[i][j][k][l] = 1'b0;
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

  for (genvar i = 0; i < NR_READ_PORTS; i++) assign rdata_o[i] = mem[raddr_i[i][VADDR_WIDTH-1:$clog2(NUM_ELEM_PER_REG)]][raddr_i[i][$clog2(NUM_ELEM_PER_REG)-1:0]];

endmodule : vregfile
