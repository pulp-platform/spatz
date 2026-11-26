// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

module spatz_ras #(
  parameter int unsigned NrCores       = 2,
  parameter int unsigned NumTcdmBanks  = 32,
  parameter int unsigned AxiAddrWidth  = 32,
  parameter int unsigned AxiDataWidth  = 32,
  parameter int unsigned AxiIdWidth    = 1,
  parameter int unsigned AxiUserWidth  = 1,
  parameter type         axi_req_t     = logic,
  parameter type         axi_rsp_t     = logic,
  parameter logic [15:0] ComponentId   = 16'h0010,
  parameter logic [15:0] InstId        = 16'h0000,
  parameter int unsigned BankAddrW     = 12
) (
  input  logic                           clk_i,
  input  logic                           rst_ni,
  input  axi_req_t                       axi_req_i,
  output axi_rsp_t                       axi_rsp_o,
  input  logic [63:0]                    timestamp_i,
  input  logic [NrCores-1:0]             vrf_cor_i,
  input  logic [NrCores-1:0]             vrf_unc_i,
  input  logic [NrCores-1:0]             fpu_sequencer_cor_i,
  input  logic [NrCores-1:0]             fpu_sequencer_unc_i,
  input  logic [NrCores-1:0]             vlsu_cor_i,
  input  logic [NrCores-1:0]             vlsu_unc_i,
  input  logic [NrCores-1:0]             vfu_cor_i,
  input  logic [NrCores-1:0]             vfu_unc_i,
  input  logic [NrCores-1:0]             controller_cor_i,
  input  logic [NrCores-1:0]             controller_unc_i,
  input  logic [NrCores-1:0]             vsldu_cor_i,
  input  logic [NrCores-1:0]             vsldu_unc_i,
  input  logic [NrCores-1:0]             snitch_cor_i,
  input  logic [NrCores-1:0]             snitch_unc_i,
  input  logic [3:0]                     quad_cor_i,
  input  logic [3:0]                     quad_unc_i,
  input  logic [NumTcdmBanks-1:0]        tcdm_rd_cor_i,
  input  logic [NumTcdmBanks-1:0]        tcdm_rd_unc_i,
  input  logic [NumTcdmBanks-1:0]        tcdm_scrub_cor_i,
  input  logic [NumTcdmBanks-1:0]        tcdm_scrub_unc_i,
  input  logic                           dma_cor_i,
  input  logic                           dma_unc_i,
  input  logic                           icache_cor_i,
  input  logic                           icache_unc_i,
  input  logic                           tcdm_interconnect_cor_i,
  input  logic                           tcdm_interconnect_unc_i,
  input  logic                           axi_interconnect_cor_i,
  input  logic                           axi_interconnect_unc_i,
  output logic [2:0]                     ras_lo_o,
  output logic [2:0]                     ras_hi_o,
  output logic [2:0]                     ras_plat_o
);
  import reri_pkg::*;

  // -----------------
  // Spatz cores
  // -----------------
  logic [7*NrCores-1:0] spatz_cor, spatz_unc;

  assign spatz_cor = {snitch_cor_i, vsldu_cor_i, controller_cor_i, vfu_cor_i, vlsu_cor_i,
                      fpu_sequencer_cor_i, vrf_cor_i};
  assign spatz_unc = {snitch_unc_i, vsldu_unc_i, controller_unc_i, vfu_unc_i, vlsu_unc_i,
                      fpu_sequencer_unc_i, vrf_unc_i};

  // -----------------
  // TCDM
  // -----------------
  logic acc_cor, acc_unc, scr_cor, scr_unc;

  assign acc_cor = |tcdm_rd_cor_i;
  assign acc_unc = |tcdm_rd_unc_i;
  assign scr_cor = |tcdm_scrub_cor_i;
  assign scr_unc = |tcdm_scrub_unc_i;

  // -----------------
  // Others
  // -----------------
  logic [3:0] rest_cor, rest_unc;

  assign rest_cor = {axi_interconnect_cor_i, tcdm_interconnect_cor_i, icache_cor_i, dma_cor_i};
  assign rest_unc = {axi_interconnect_unc_i, tcdm_interconnect_unc_i, icache_unc_i, dma_unc_i};

  // -----------------
  // Records
  // -----------------
  err_report_t [3:0] err;

  always_comb begin
    err = '0;
    // Spatz cores
    err[0].valid = |{spatz_cor, spatz_unc};
    err[0].ce    = |spatz_cor;
    err[0].uec   = |spatz_unc;
    err[0].pri   = |spatz_unc ? 2'd2 : 2'd0;
    err[0].c     = ~|spatz_unc;
    err[0].ec    = EcInternalUnit;
    err[0].iv    = 1'b1;
    err[0].info  = 64'({spatz_unc, spatz_cor});
    // Quadrilatero
    err[1].valid = |{quad_cor_i, quad_unc_i};
    err[1].ce    = |quad_cor_i;
    err[1].uec   = |quad_unc_i;
    err[1].pri   = |quad_unc_i ? 2'd2 : 2'd0;
    err[1].c     = ~|quad_unc_i;
    err[1].ec    = EcInternalUnit;
    err[1].iv    = 1'b1;
    err[1].info  = 64'({quad_unc_i, quad_cor_i});
    // TCDM
    err[2].valid = acc_cor | acc_unc | scr_cor | scr_unc;
    err[2].ce    = acc_cor | scr_cor;
    err[2].uec   = acc_unc;
    err[2].ued   = scr_unc;
    err[2].pri   = (acc_unc | scr_unc) ? 2'd2 : 2'd0;
    err[2].c     = ~acc_unc;
    err[2].scrub = scr_cor;
    err[2].tt    = (scr_cor | scr_unc) ? TtImplicitRead : TtNone;
    err[2].ec    = (scr_cor | scr_unc) ? EcSysMemScrub : EcSysMemData;
    err[2].iv    = 1'b1;
    err[2].info  = 64'({tcdm_scrub_cor_i | tcdm_scrub_unc_i, tcdm_rd_cor_i | tcdm_rd_unc_i});
    // DMA, instruction cache, interconnects
    err[3].valid = |{rest_cor, rest_unc};
    err[3].ce    = |rest_cor;
    err[3].uec   = |rest_unc;
    err[3].pri   = |rest_unc ? 2'd2 : 2'd0;
    err[3].c     = ~|rest_unc;
    err[3].ec    = |{rest_cor[3:2], rest_unc[3:2]} ? EcInterconnectData : EcInternalUnit;
    err[3].iv    = 1'b1;
    err[3].info  = 64'({rest_unc, rest_cor});
  end

  // -----------------
  // Bank
  // -----------------
  reri_axi_bank #(
    .AxiAddrWidth  ( AxiAddrWidth ),
    .AxiDataWidth  ( AxiDataWidth ),
    .AxiIdWidth    ( AxiIdWidth   ),
    .AxiUserWidth  ( AxiUserWidth ),
    .axi_req_t     ( axi_req_t    ),
    .axi_rsp_t     ( axi_rsp_t    ),
    .Protected     ( 1'b1         ),
    .EnableSelfRep ( 1'b1         ),
    .EnableUncCnt  ( 1'b1         ),
    .NumSrc        ( 4            ),
    .ComponentId   ( ComponentId  ),
    .InstId        ( InstId       ),
    .BankAddrW     ( BankAddrW    )
  ) i_ras_bank (
    .clk_i,
    .rst_ni,
    .axi_req_i,
    .axi_rsp_o,
    .timestamp_i,
    .err_i          ( err        ),
    .ras_lo_o       (            ),
    .ras_hi_o       (            ),
    .ras_plat_o     (            ),
    .ras_lo_lvl_o   ( ras_lo_o   ),
    .ras_hi_lvl_o   ( ras_hi_o   ),
    .ras_plat_lvl_o ( ras_plat_o ),
    .ecc_cor_o      (            ),
    .ecc_unc_o      (            ),
    .tmr_mismatch_o (            )
  );

endmodule
