// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Danilo Cammarata, <dcammarata@iis.ee.ethz.ch>, ETH Zurich

`include "common_cells/assertions.svh"

/// Subsystem wrapping the OPOPE HWPE: adapts the cluster-side interfaces (TCDM-like control
/// requests and wide TCDM plugs) to the HWPE (`hwpe_ctrl_intf_periph` and HCI).
module opope_hwpe_subsystem
  import hci_package::*;
  import hwpe_ctrl_package::*;
#(
  parameter type         tcdm_req_t    = logic,
  parameter type         tcdm_rsp_t    = logic,
  parameter type         periph_req_t  = logic,
  parameter type         periph_rsp_t  = logic,
  parameter int unsigned WideDataWidth = 512,
  parameter int unsigned PeriphIdWidth = 2,
  parameter int unsigned NrCores       = 2,
  parameter int unsigned NumExpWideTcdmPorts = 2,
  parameter int unsigned MISALIGNED_ACCESSES = 1,
  // Derived: do not override
  localparam int unsigned HciDataWidth = opope_pkg::DATA_W,
  // Word-aligned accesses (see `spatz_tcdm_aligner`)
  localparam int unsigned NumPlugs = (MISALIGNED_ACCESSES == 1)
      ? (((HciDataWidth / 8) + (WideDataWidth / 8) - (opope_pkg::MemDw / 8)) + (WideDataWidth / 8) - 1) / (WideDataWidth / 8)
      : ((HciDataWidth / 8) + (WideDataWidth / 8) - 1) / (WideDataWidth / 8)
) (
  input  logic clk_i,
  input  logic rst_ni,
  input  logic test_mode_i,
  // TCDM interface (master): only the first `NumPlugs` are driven
  output tcdm_req_t [NumPlugs-1:0] tcdm_req_o,
  input  tcdm_rsp_t [NumPlugs-1:0] tcdm_rsp_i,
  // HWPE control interface (slave)
  input  periph_req_t hwpe_ctrl_req_i,
  output periph_rsp_t hwpe_ctrl_rsp_o,
  // Events (one pulse per core)
  output logic [NrCores-1:0] hwpe_evt_o
);

  /////////////////
  // HWPE Control //
  /////////////////

  hwpe_ctrl_intf_periph #(
    .ID_WIDTH(PeriphIdWidth)
  ) periph (
    .clk(clk_i)
  );

  assign periph.req  = hwpe_ctrl_req_i.q_valid;
  assign periph.add  = hwpe_ctrl_req_i.q.addr;
  assign periph.wen  = ~hwpe_ctrl_req_i.q.write;
  assign periph.be   = hwpe_ctrl_req_i.q.strb;
  assign periph.data = hwpe_ctrl_req_i.q.data;
  assign periph.id   = hwpe_ctrl_req_i.q.user;

  assign hwpe_ctrl_rsp_o.q_ready = periph.gnt;
  assign hwpe_ctrl_rsp_o.p.data  = periph.r_data;
  assign hwpe_ctrl_rsp_o.p_valid = periph.r_valid;

  ///////////
  // OPOPE //
  ///////////

  localparam hci_size_parameter_t HwpeSize = '{
    DW:  HciDataWidth,
    AW:  DEFAULT_AW,
    BW:  DEFAULT_BW,
    UW:  DEFAULT_UW,
    IW:  DEFAULT_IW,
    EW:  DEFAULT_EW,
    EHW: DEFAULT_EHW
  };

  hci_core_intf #(
    .DW  (HwpeSize.DW),
    .AW  (HwpeSize.AW),
    .BW  (HwpeSize.BW),
    .UW  (HwpeSize.UW),
    .IW  (HwpeSize.IW),
    .EW  (HwpeSize.EW),
    .EHW (HwpeSize.EHW)
  ) tcdm (
    .clk(clk_i)
  );

  logic [NrCores-1:0][1:0] evt;

  opope_top #(
    .ID_WIDTH              ( PeriphIdWidth        ),
    .N_CORES               ( NrCores              ),
    .DW                    ( HciDataWidth         ),
    .HCI_SIZE_tcdm       ( HwpeSize             )
  ) i_opope_top (
    .clk_i       ( clk_i       ),
    .rst_ni      ( rst_ni      ),
    .test_mode_i ( test_mode_i ),
    .busy_o      (             ),
    .evt_o       ( evt         ),
    .periph      ( periph      ),
    .tcdm        ( tcdm        )
  );

  for (genvar c = 0; c < NrCores; c++) begin : gen_evt
    assign hwpe_evt_o[c] = |evt[c];
  end

  /* Connect OPOPE to the (wide) TCDM plugs */
  spatz_tcdm_aligner #(
    .tcdm_req_t          ( tcdm_req_t          ),
    .tcdm_rsp_t          ( tcdm_rsp_t          ),
    .DATA_WIDTH          ( HciDataWidth        ),
    .SB_WIDTH            ( WideDataWidth       ),
    .WORD_WIDTH          ( opope_pkg::MemDw    ),
    .ADDR_WIDTH          ( DEFAULT_AW          ),
    .MISALIGNED_ACCESSES ( MISALIGNED_ACCESSES )
  ) i_tcdm_aligner (
    .clk_i               ( clk_i      ),
    .rst_ni              ( rst_ni     ),
    .tcdm_misaligned     ( tcdm       ),
    .tcdm_req_aligned_o  ( tcdm_req_o ),
    .tcdm_rsp_aligned_i  ( tcdm_rsp_i )
  );

  ////////////////
  // Assertions //
  ////////////////

  `ASSERT_INIT(CheckHwpeTcdmPlugsNum, NumPlugs == NumExpWideTcdmPorts)

endmodule
