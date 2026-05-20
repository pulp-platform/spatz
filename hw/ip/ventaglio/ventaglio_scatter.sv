module ventaglio_scatter
  import spatz_pkg::*;
  import vtl_pkg::*;
#(
  parameter int unsigned NarrowDataWidth = VTGChannelWidth,
  parameter int unsigned WideDataWidth   = VTGChannelWidth * VENTAGLIO_WFACTOR
) (
  input  logic                               clk_i,
  input  logic                               rst_ni,
  input  logic                               testmode_i,

  // write ports -- from VFU (narrow)
  input  vrf_addr_t                          waddr_i,
  input  vrf_data_t                          wdata_i,
  input  logic                               we_i,
  input  vrf_be_t                            wbe_i,
  output logic                               wvalid_o,

  // write ports -- to Buffer (wide)
  output vrf_addr_t [VENTAGLIO_WFACTOR-1:0]  waddr_o,
  output vrf_data_t [VENTAGLIO_WFACTOR-1:0]  wdata_o,
  output logic      [VENTAGLIO_WFACTOR-1:0]  we_o,
  output vrf_be_t   [VENTAGLIO_WFACTOR-1:0]  wbe_o,
  input  logic      [VENTAGLIO_WFACTOR-1:0]  wvalid_i,

  // control
  input  logic                               scatter_done_i,
  input  logic                        [4:0]  num_beats_per_op_i,

  // index
  input  vrf_data_t                          index_i,
  input  sp_cfg_t                            vtl_cfg_i,
  input  logic                               index_valid_i
);

  `include "common_cells/registers.svh"

  ////////////////////////////
  //     request logics     //
  ////////////////////////////
  vrf_addr_t waddr;

  always_comb begin : proc_write_addr
    waddr   = '0;
    waddr_o = '0;
    we_o    = '0;

    unique case (vtl_cfg_i.sp_cfg_ratio)
      SP_RATIO_050: begin
        // one write maps to 2 channels
        waddr      = waddr_i << 1;
        waddr_o[0] = {waddr[$clog2(NrVRFWords)-1:1], 1'b0};
        waddr_o[1] = {waddr[$clog2(NrVRFWords)-1:1], 1'b1};
        if (we_i) we_o[1:0] = 2'b11;
      end

      SP_RATIO_025: begin
        // one write maps to 4 channels
        waddr      = waddr_i << 2;
        waddr_o[0] = {waddr[$clog2(NrVRFWords)-1:2], 2'b00};
        waddr_o[1] = {waddr[$clog2(NrVRFWords)-1:2], 2'b01};
        waddr_o[2] = {waddr[$clog2(NrVRFWords)-1:2], 2'b10};
        waddr_o[3] = {waddr[$clog2(NrVRFWords)-1:2], 2'b11};
        if (we_i) we_o = '1;
      end

      default: begin
        waddr = waddr_i;
      end
    endcase
  end

  ////////////////////////////
  //  parameterized cores   //
  ////////////////////////////

  vrf_data_t [VENTAGLIO_WFACTOR-1:0] wdata_050, wdata_025;
  vrf_be_t   [VENTAGLIO_WFACTOR-1:0] wbe_050,   wbe_025;
  logic                               wvalid_050, wvalid_025;

  // ------------------------------------------------------------
  // Shared index register (ONE 256-bit register for all datapaths)
  // ------------------------------------------------------------
  vrf_data_t index_q, index_d;
  logic      index_load_req_050, index_load_req_025;
  logic      index_load_req;

  logic active_050, active_025;
  assign active_050 = (vtl_cfg_i.sp_cfg_ratio == SP_RATIO_050);
  assign active_025 = (vtl_cfg_i.sp_cfg_ratio == SP_RATIO_025);

  // Select which datapath may load the shared index register
  always_comb begin
    unique case (vtl_cfg_i.sp_cfg_ratio)
      SP_RATIO_050: index_load_req = index_load_req_050;
      SP_RATIO_025: index_load_req = index_load_req_025;
      default:      index_load_req = 1'b0;
    endcase
  end

  // Shared index FF (uses FF macro only; no dependency on FFL)
  always_comb begin
    index_d = index_q;
    if (index_load_req) index_d = index_i;
  end
  `FF(index_q, index_d, '0)

  // ------------------------------------------------------------
  // 2:4 scatter (effective 2 elems per block, 2 channels used)
  // ------------------------------------------------------------
  ventaglio_scatter_datapath #(
    .NrEffElePerBlk(2),
    .NrElePerBlk   (4),
    .NrCh          (2),
    .IdxWidth      (2),
    .EleWidth      (32)
  ) i_scatter_2of4 (
    .clk_i,
    .rst_ni,
    .active_i         (active_050),

    .scatter_done_i,
    .we_i,
    .waddr_i,
    .wdata_i,
    .wbe_i,

    .index_i,
    .index_valid_i,
    .index_q_i        (index_q),
    .index_load_req_o (index_load_req_050),

    .wvalid_i,
    .num_beats_per_op_i,

    .wdata_o  (wdata_050),
    .wbe_o    (wbe_050),
    .wvalid_o (wvalid_050)
  );

  // ------------------------------------------------------------
  // 1:4 scatter (effective 1 elem per block, 4 channels used)
  // ------------------------------------------------------------
  ventaglio_scatter_datapath #(
    .NrEffElePerBlk(1),
    .NrElePerBlk   (4),
    .NrCh          (4),
    .IdxWidth      (2),
    .EleWidth      (32)
  ) i_scatter_1of4 (
    .clk_i,
    .rst_ni,
    .active_i         (active_025),

    .scatter_done_i,
    .we_i,
    .waddr_i,
    .wdata_i,
    .wbe_i,

    .index_i,
    .index_valid_i,
    .index_q_i        (index_q),
    .index_load_req_o (index_load_req_025),

    .wvalid_i,
    .num_beats_per_op_i,

    .wdata_o  (wdata_025),
    .wbe_o    (wbe_025),
    .wvalid_o (wvalid_025)
  );

  ////////////////////////////
  // Select active format   //
  ////////////////////////////
  always_comb begin
    unique case (vtl_cfg_i.sp_cfg_ratio)
      SP_RATIO_050: begin
        wdata_o  = wdata_050;
        wbe_o    = wbe_050;
        wvalid_o = wvalid_050;
      end

      SP_RATIO_025: begin
        wdata_o  = wdata_025;
        wbe_o    = wbe_025;
        wvalid_o = wvalid_025;
      end

      default: begin
        wdata_o  = '0;
        wbe_o    = '0;
        wvalid_o = 1'b0;
      end
    endcase
  end

  // Optional safety: only one datapath active at a time
  // synopsys translate_off
  always_ff @(posedge clk_i) begin
    if (rst_ni) begin
      assert (!(active_050 && active_025))
        else $error("ventaglio_scatter: both scatter datapaths active simultaneously!");
    end
  end
  // synopsys translate_on

endmodule : ventaglio_scatter
