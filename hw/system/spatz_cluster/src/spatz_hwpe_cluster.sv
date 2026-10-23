// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Adapted from `konark_cluster` for the Spatz cluster.

`include "axi/assign.svh"
`include "axi/typedef.svh"
`include "tcdm_interface/typedef.svh"

/// Spatz cluster with the OPOPE HWPE attached through the cluster's externally-exposed
/// narrow AXI plug (control) and wide TCDM plugs (data).
module spatz_hwpe_cluster
  import spatz_cluster_pkg::*;
(
  input  logic                clk_i,
  input  logic                rst_ni,
  input  logic [NumCores-1:0] debug_req_i,
  input  logic [NumCores-1:0] meip_i,
  input  logic [NumCores-1:0] mtip_i,
  input  logic [NumCores-1:0] msip_i,
  output logic                cluster_probe_o,
  input  spatz_axi_in_req_t   axi_in_req_i,
  output spatz_axi_in_resp_t  axi_in_resp_o,
  output spatz_axi_out_req_t  axi_out_req_o,
  input  spatz_axi_out_resp_t axi_out_resp_i
);

  ////////////////////
  // Spatz Cluster  //
  ////////////////////

  localparam int unsigned HWPECtrlAddrWidth = 32;
  localparam int unsigned HWPECtrlDataWidth = 32;
  typedef logic [HWPECtrlAddrWidth-1:0] addr_hwpe_ctrl_t;
  typedef logic [HWPECtrlDataWidth-1:0] data_hwpe_ctrl_t;
  typedef logic [3:0]                   strb_hwpe_ctrl_t;

  spatz_axi_narrow_ext_req_t  cluster_narrow_ext_req;
  spatz_axi_narrow_ext_resp_t cluster_narrow_ext_rsp;

  spatz_tcdm_ext_req_t [NumExpWideTcdmPorts-1:0] cluster_tcdm_ext_req;
  spatz_tcdm_ext_rsp_t [NumExpWideTcdmPorts-1:0] cluster_tcdm_ext_rsp;

  `AXI_TYPEDEF_ALL(cluster_narrow_ext_dw_conv, axi_addr_t, narrow_ext_id_t, data_hwpe_ctrl_t,
                   strb_hwpe_ctrl_t, axi_user_t)

  cluster_narrow_ext_dw_conv_req_t  cluster_narrow_ext_dw_conv_req;
  cluster_narrow_ext_dw_conv_resp_t cluster_narrow_ext_dw_conv_rsp;

  `TCDM_TYPEDEF_ALL(hwpectrl, addr_hwpe_ctrl_t, data_hwpe_ctrl_t, strb_hwpe_ctrl_t, logic)

  hwpectrl_req_t hwpectrl_req;
  hwpectrl_rsp_t hwpectrl_rsp;

  logic [NumCores-1:0] hwpe_evt;

  spatz_cluster_wrapper i_cluster (
    .clk_i,
    .rst_ni,
    .debug_req_i,
    // HWPE events are routed to the cores through the external interrupt line
    .meip_i            (meip_i | hwpe_evt),
    .mtip_i,
    .msip_i,
    .cluster_probe_o,
    .narrow_ext_req_o  (cluster_narrow_ext_req),
    .narrow_ext_resp_i (cluster_narrow_ext_rsp),
    .tcdm_ext_req_i    (cluster_tcdm_ext_req),
    .tcdm_ext_resp_o   (cluster_tcdm_ext_rsp),
    .axi_in_req_i,
    .axi_in_resp_o,
    .axi_out_req_o,
    .axi_out_resp_i
  );

  ///////////
  // HWPEs //
  ///////////

  /* Adapters between cluster and HWPE */

  // Convert narrow AXI's DW down to 32
  axi_dw_converter #(
    .AxiMaxReads        (1),
    .AxiSlvPortDataWidth(SpatzNarrowAxiDataWidth),
    .AxiMstPortDataWidth(HWPECtrlDataWidth),
    .AxiAddrWidth       (SpatzAxiAddrWidth),
    .AxiIdWidth         (SpatzNarrowExtIdWidth),
    .aw_chan_t          (spatz_axi_narrow_ext_aw_chan_t),
    .mst_w_chan_t       (cluster_narrow_ext_dw_conv_w_chan_t),
    .slv_w_chan_t       (spatz_axi_narrow_ext_w_chan_t),
    .b_chan_t           (spatz_axi_narrow_ext_b_chan_t),
    .ar_chan_t          (spatz_axi_narrow_ext_ar_chan_t),
    .mst_r_chan_t       (cluster_narrow_ext_dw_conv_r_chan_t),
    .slv_r_chan_t       (spatz_axi_narrow_ext_r_chan_t),
    .axi_mst_req_t      (cluster_narrow_ext_dw_conv_req_t),
    .axi_mst_resp_t     (cluster_narrow_ext_dw_conv_resp_t),
    .axi_slv_req_t      (spatz_axi_narrow_ext_req_t),
    .axi_slv_resp_t     (spatz_axi_narrow_ext_resp_t)
  ) i_axi_dw_hwpe (
    .clk_i     (clk_i),
    .rst_ni    (rst_ni),
    .slv_req_i (cluster_narrow_ext_req),
    .slv_resp_o(cluster_narrow_ext_rsp),
    .mst_req_o (cluster_narrow_ext_dw_conv_req),
    .mst_resp_i(cluster_narrow_ext_dw_conv_rsp)
  );

  axi_to_tcdm #(
    .axi_req_t (cluster_narrow_ext_dw_conv_req_t),
    .axi_rsp_t (cluster_narrow_ext_dw_conv_resp_t),
    .tcdm_req_t(hwpectrl_req_t),
    .tcdm_rsp_t(hwpectrl_rsp_t),
    .IdWidth   (SpatzNarrowExtIdWidth),
    .AddrWidth (HWPECtrlAddrWidth),
    .DataWidth (HWPECtrlDataWidth)
  ) i_axi_to_hwpe_ctrl (
    .clk_i     (clk_i),
    .rst_ni    (rst_ni),
    .axi_req_i (cluster_narrow_ext_dw_conv_req),
    .axi_rsp_o (cluster_narrow_ext_dw_conv_rsp),
    .tcdm_req_o(hwpectrl_req),
    .tcdm_rsp_i(hwpectrl_rsp)
  );

  /* HWPE Subsystem */

  // The HWPE uses the first `HwpeTcdmPlugs` of the externally exposed wide TCDM ports
  localparam int unsigned HwpeTcdmPlugs = NumExpWideTcdmPorts;

  opope_hwpe_subsystem #(
    .tcdm_req_t         (spatz_tcdm_ext_req_t),
    .tcdm_rsp_t         (spatz_tcdm_ext_rsp_t),
    .periph_req_t       (hwpectrl_req_t),
    .periph_rsp_t       (hwpectrl_rsp_t),
    .WideDataWidth      (SpatzAxiDataWidth),
    .PeriphIdWidth      (SpatzNarrowExtIdWidth),
    .NrCores            (NumCores),
    .NumExpWideTcdmPorts(NumExpWideTcdmPorts),
    .MISALIGNED_ACCESSES(1)
  ) i_hwpe_subsystem (
    .clk_i          (clk_i),
    .rst_ni         (rst_ni),
    .test_mode_i    (1'b0),
    .tcdm_req_o     (cluster_tcdm_ext_req[HwpeTcdmPlugs-1:0]),
    .tcdm_rsp_i     (cluster_tcdm_ext_rsp[HwpeTcdmPlugs-1:0]),
    .hwpe_ctrl_req_i(hwpectrl_req),
    .hwpe_ctrl_rsp_o(hwpectrl_rsp),
    .hwpe_evt_o     (hwpe_evt)
  );

endmodule
