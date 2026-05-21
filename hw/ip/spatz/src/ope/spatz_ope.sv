// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Pei-Yu Lin <peilin@ethz.ch>
//
// spatz_ope — Standalone Outer Product Engine wrapper for Spatz.

module spatz_ope
  import spatz_pkg::*;
  import rvv_pkg::*;
  import fpnew_pkg::*;
(
  input  logic             clk_i,
  input  logic             rst_ni,

  input  spatz_req_t       spatz_req_i,
  input  logic             spatz_req_valid_i,
  output logic             spatz_req_ready_o,

  output logic             ope_rsp_valid_o,
  input  logic             ope_rsp_ready_i,
  output vfu_rsp_t         ope_rsp_o,

  output vrf_addr_t        vrf_waddr_o,
  output vrf_data_t        vrf_wdata_o,
  output logic             vrf_we_o,
  output vrf_be_t          vrf_wbe_o,
  input  logic             vrf_wvalid_i,
  // Scoreboard IDs: [2]=write, [1]=vs1-read, [0]=vs2-read
  output spatz_id_t  [2:0] vrf_id_o,

  // VRF read ports
  //   [0] -> vs2 (x_input / y_bias)
  //   [1] -> vs1 (w_input)
  output vrf_addr_t  [1:0] vrf_raddr_o,
  output logic       [1:0] vrf_re_o,
  input  vrf_data_t  [1:0] vrf_rdata_i,
  input  logic       [1:0] vrf_rvalid_i,

  // Tile interface
  input  logic                        tile_wvalid_i,
  input  logic [$clog2(NRTILE)-1:0]   tile_widx_i,
  input  logic [$clog2(TE)-1:0]       tile_wrow_i,
  input  vrf_data_t                   tile_wdata_i,
  output logic                        tile_wready_o,

  input  logic                        tile_rvalid_i,
  input  logic [$clog2(NRTILE)-1:0]   tile_ridx_i,
  input  logic [$clog2(TE)-1:0]       tile_rrow_i,
  output vrf_data_t                   tile_rdata_o,
  output logic                        tile_rready_o
);

`include "common_cells/registers.svh"

  ///////////////////////
  //  Operation queue  //
  ///////////////////////

  spatz_req_t spatz_req;
  logic       spatz_req_valid;
  logic       spatz_req_ready;

  spill_register #(
    .T(spatz_req_t)
  ) i_operation_queue (
    .clk_i  (clk_i                                          ),
    .rst_ni (rst_ni                                         ),
    .data_i (spatz_req_i                                    ),
    .valid_i(spatz_req_valid_i && spatz_req_i.ex_unit == OPE),
    .ready_o(spatz_req_ready_o                              ),
    .data_o (spatz_req                                      ),
    .valid_o(spatz_req_valid                                ),
    .ready_i(spatz_req_ready                                )
  );

  //////////////////////////////////////////
  //  Local OPE parameters                //
  //////////////////////////////////////////

  localparam int unsigned NumPipeRegs = 4;

  ///////////////////////////////
  //  OPE tile-state FSM       //
  ///////////////////////////////

  typedef enum logic [2:0] {
    OPE_Idle,
    OPE_Execute,
    OPE_Drain,
    OPE_ReadTile,
    OPE_LoadTile,
    OPE_Zero,
    OPE_Resp
  } ope_state_t;

  ope_state_t ope_state_d, ope_state_q;
  `FF(ope_state_q, ope_state_d, OPE_Idle)

  logic [$clog2(NumPipeRegs+1)-1:0] ope_drain_cnt_d, ope_drain_cnt_q;
  `FF(ope_drain_cnt_q, ope_drain_cnt_d, '0)

  logic [$clog2(TE)-1:0] ope_zero_row_d, ope_zero_row_q;
  `FF(ope_zero_row_q, ope_zero_row_d, '0)

  logic [TE-1:0][TEW-1:0] x_operand_d, x_operand_q;
  logic [TE-1:0][TEW-1:0] w_operand_d, w_operand_q;
  vrf_data_t y_bias_d, y_bias_q;
  `FF(x_operand_q, x_operand_d, '0)
  `FF(w_operand_q, w_operand_d, '0)
  `FF(y_bias_q, y_bias_d, '0)

  // OPE output bus: 2*TE FP32 accumulator read-out
  logic [2*TE-1:0][TEW-1:0] y_bias_load;
  logic [2*TE-1:0][TEW-1:0] z_output;

  /////////////////////////////
  //  OPE engine control     //
  /////////////////////////////

  typedef struct packed {
    logic [$clog2(NRTILE)-1:0]  y_write_tile_index  ;
    logic [$clog2(TE)-1:0]      y_write_row_index   ;
    logic [$clog2(NRTILE)-1:0]  z_read_tile_index   ;
    logic [$clog2(TE)-1:0]      z_read_row_index    ;
    logic [$clog2(NRTILE)-1:0]  tile_write_to_engine;
    logic y_bias_selector   ;
    logic acc_input_selector;
    logic external_loading  ;
    logic tile_enable       ;
  } cntrl_engine_t;

  cntrl_engine_t ope_ctrl;

  //////////////////////////////////////////
  // Tile Subset Specifier (TSS) decode   //
  //////////////////////////////////////////

  logic [3:0]             tss_tile_id;
  logic [2:0]             tss_pattern;
  logic [23:0]            tss_index;
  logic [$clog2(NRTILE)-1:0] tss_tile_index;
  logic [$clog2(TE)-1:0]  tss_row_index;
  logic                   tss_is_row;
  logic                   tss_is_column;
  logic                   tss_valid;

  always_comb begin
    tss_tile_id    = spatz_req.rs1[30:27];
    tss_pattern    = spatz_req.rs1[26:24];
    tss_index      = spatz_req.rs1[23:0];
    tss_tile_index = tss_tile_id >> 2;
    tss_row_index  = tss_index[$clog2(TE)-1:0];
    tss_is_row     = (tss_pattern == 3'd0);
    tss_is_column  = (tss_pattern == 3'd1);
    tss_valid  = (tss_is_row || tss_is_column) && (tss_index < TE);
  end

  /////////////////////
  //  Control proc   //
  /////////////////////

  always_comb begin: control_proc
    spatz_req_ready = 1'b0;
    ope_rsp_valid_o = (ope_state_q == OPE_Resp);
    ope_rsp_o       = '0;

    ope_rsp_o.id     = spatz_req.id;
    ope_rsp_o.rd     = spatz_req.rd[GPRWidth-1:0];
    ope_rsp_o.wb     = 1'b0;
    ope_rsp_o.result = '0;

    if ((ope_state_q == OPE_Resp) && ope_rsp_ready_i) begin
      spatz_req_ready = 1'b1;
    end
  end

  ////////////////////////
  //  VRF address proc  //
  ////////////////////////

  always_comb begin: vreg_addr_proc
    vrf_raddr_o = '0;
    vrf_waddr_o = '0;

    if (spatz_req_valid) begin
      vrf_raddr_o[0] = vrf_addr_t'(spatz_req.vs2) << $clog2(NrWordsPerVector);
      vrf_raddr_o[1] = vrf_addr_t'(spatz_req.vs1) << $clog2(NrWordsPerVector);

      if (spatz_req.op == VTMV_VT && ope_state_q == OPE_ReadTile)
        vrf_waddr_o = vrf_addr_t'(spatz_req.vd) << $clog2(NrWordsPerVector);
    end
  end: vreg_addr_proc

  ////////////////////////
  //  Operand req proc  //
  ////////////////////////

  always_comb begin: operand_req_proc
    vrf_re_o    = '0;
    vrf_we_o    = 1'b0;
    vrf_wbe_o   = '0;
    vrf_wdata_o = '0;

    if (spatz_req_valid) begin
      unique case (spatz_req.op)

        VTFMM, VTFMM_ALT, VTMMU, VTMMS: begin
          if (ope_state_q == OPE_Idle) begin
            vrf_re_o[0] = spatz_req.use_vs2;
            vrf_re_o[1] = spatz_req.use_vs1;
          end
        end

        VTMV_TV: begin
          if (ope_state_q == OPE_Idle)
            vrf_re_o[0] = spatz_req.use_vs2;
        end

        VTMV_VT: begin
          if (ope_state_q == OPE_ReadTile) begin
            vrf_we_o  = spatz_req.use_vd;
            vrf_wbe_o = '1;
            for (int unsigned col = 0; col < TE; col++)
              vrf_wdata_o[col*TEW +: TEW] = z_output[2*col + tss_tile_index[0]];
          end
        end

        default:;
      endcase
    end
  end: operand_req_proc

  assign vrf_id_o[0] = spatz_req.id; // vs2 read
  assign vrf_id_o[1] = spatz_req.id; // vs1 read
  assign vrf_id_o[2] = spatz_req.id; // vd  write

  ///////////////////////
  //  OPE FSM proc     //
  ///////////////////////

  always_comb begin: ope_fsm_proc
    x_operand_d = x_operand_q;
    w_operand_d = w_operand_q;
    y_bias_d    = y_bias_q;
    y_bias_load = z_output;  // default: feed accumulator output back

    ope_state_d      = ope_state_q;
    ope_drain_cnt_d  = ope_drain_cnt_q;
    ope_zero_row_d   = ope_zero_row_q;

    // Control defaults
    ope_ctrl                 = '0;
    ope_ctrl.y_bias_selector = 1'b1;
    ope_ctrl.y_write_tile_index  = tss_tile_index;
    ope_ctrl.y_write_row_index   = tss_row_index;
    ope_ctrl.z_read_tile_index   = tss_tile_index;
    ope_ctrl.z_read_row_index    = tss_row_index;
    ope_ctrl.tile_write_to_engine = spatz_req.vd[3:2];

    for (int unsigned col = 0; col < TE; col++) begin
      if (tile_wvalid_i) begin
        // VLSU external write: override target sub-slot; other sub-slot preserved via z_output default
        y_bias_load[2*col + tile_widx_i[0]] = tile_wdata_i[col*TEW +: TEW];
      end else if (ope_state_q == OPE_Zero) begin
        y_bias_load[2*col + tss_tile_index[0]] = '0;
      end else begin
        y_bias_load[2*col + tss_tile_index[0]] = y_bias_q[col*TEW +: TEW];
      end
    end

    unique case (ope_state_q)
      OPE_Idle: begin
        if (tile_wvalid_i) begin
          // External tile write from VLSU (VTLE): read-modify-write into accumulator
          ope_ctrl.z_read_tile_index  = tile_widx_i;
          ope_ctrl.z_read_row_index   = ($clog2(TE))'(tile_wrow_i);
          ope_ctrl.y_write_tile_index = tile_widx_i;
          ope_ctrl.y_write_row_index  = ($clog2(TE))'(tile_wrow_i);
          ope_ctrl.external_loading   = 1'b1;
          ope_ctrl.y_bias_selector    = 1'b0;
          ope_ctrl.tile_enable        = 1'b1;
        end else if (tile_rvalid_i) begin
          // External tile read from VLSU (VTSE): combinational readout via z_output
          ope_ctrl.z_read_tile_index  = tile_ridx_i;
          ope_ctrl.z_read_row_index   = ($clog2(TE))'(tile_rrow_i);
        end else if (spatz_req_valid) begin
          unique case (spatz_req.op)

            VTFMM, VTFMM_ALT, VTMMU, VTMMS: begin
              if (vrf_rvalid_i[0] && vrf_rvalid_i[1]) begin
                x_operand_d = vrf_rdata_i[0][TE*TEW-1:0];
                w_operand_d = vrf_rdata_i[1][TE*TEW-1:0];
                ope_ctrl.tile_write_to_engine= spatz_req.vd[3:2];
                ope_state_d = OPE_Execute;
              end
            end

            VTMV_VT: begin
              if (!tss_valid)
                ope_state_d = OPE_Resp;
              else
                ope_state_d = OPE_ReadTile;
            end

            VTMV_TV: begin
              if (!tss_valid) begin
                ope_state_d = OPE_Resp;
              end else if (vrf_rvalid_i[0]) begin
                y_bias_d    = vrf_rdata_i[0];
                ope_state_d = OPE_LoadTile;
              end
            end

            VTZERO: begin
              if (!tss_valid) begin
                ope_state_d = OPE_Resp;
              end else begin
                ope_zero_row_d = '0;
                ope_state_d    = OPE_Zero;
              end
            end

            VTDISCARD: begin
              ope_state_d = OPE_Resp;
            end

            default:;
          endcase
        end
      end

      OPE_Execute: begin
        ope_ctrl.tile_enable         = 1'b1;
        ope_ctrl.acc_input_selector = 1'b1;

        ope_state_d     = OPE_Drain;
        ope_drain_cnt_d = NumPipeRegs[$bits(ope_drain_cnt_d)-1:0];
      end

      OPE_Drain: begin
        ope_ctrl.tile_enable        = 1'b1;
        ope_ctrl.acc_input_selector = 1'b1;
        if (ope_drain_cnt_q == '0) begin
          ope_state_d      = OPE_Resp;
        end else begin
          ope_drain_cnt_d = ope_drain_cnt_q - 1'b1;
        end
      end

      OPE_ReadTile: begin
        ope_ctrl.z_read_tile_index  = tss_tile_index;
        ope_ctrl.z_read_row_index   = tss_row_index;
        if (vrf_wvalid_i) begin
          ope_state_d      = OPE_Resp;
        end
      end

      OPE_LoadTile: begin
        ope_ctrl.tile_enable       = 1'b1;
        ope_ctrl.external_loading  = 1'b1;
        ope_ctrl.y_bias_selector   = 1'b0;
        
        ope_ctrl.y_write_tile_index = tss_tile_index;
        ope_ctrl.y_write_row_index = tss_row_index;

        ope_state_d      = OPE_Resp;
      end

      OPE_Zero: begin
        ope_ctrl.tile_enable         = 1'b1;
        ope_ctrl.external_loading    = 1'b0;
        ope_ctrl.y_bias_selector     = 1'b0;

        ope_ctrl.y_write_tile_index  = spatz_req.vd[3:2];
        ope_ctrl.y_write_row_index   = ope_zero_row_q;

        if (ope_zero_row_q == ($clog2(TE))'(TE - 1)) begin
          ope_zero_row_d = '0;
          ope_state_d    = OPE_Resp;
        end else begin
          ope_zero_row_d = ope_zero_row_q + 1'b1;
        end
      end

      OPE_Resp: begin
        if (ope_rsp_ready_i) begin
          ope_state_d = OPE_Idle;
        end
      end

      default: ope_drain_cnt_d = ope_drain_cnt_q;
    endcase
  end: ope_fsm_proc

  // Tile interface outputs: ready when OPE is idle
  assign tile_wready_o = (ope_state_q == OPE_Idle);
  assign tile_rready_o = (ope_state_q == OPE_Idle);

  always_comb begin : tile_rdata_proc
    tile_rdata_o = '0;
    for (int unsigned col = 0; col < TE; col++)
      tile_rdata_o[col*TEW +: TEW] = z_output[2*col + tile_ridx_i[0]];
  end : tile_rdata_proc

  ////////////////////////////
  //  Engine (inlined)      //
  ////////////////////////////

  logic [TE-1:0][TE-1:0][2*TEW-1:0] acc_output;
  logic [TE-1:0][TE-1:0][TEW-1:0]   fma_to_acc_output;
  logic                             fma_clk;


  tc_clk_gating ce_clock_gating (
    .clk_i     (clk_i               ),
    .en_i      (ope_ctrl.tile_enable),   // same_fmt
    .test_en_i ('0                  ),
    .clk_o     (fma_clk             )
  );

  // z_output: read one row from all accumulators, both sub-slots
  for (genvar col_index = 0; col_index < TE; col_index++) begin: gen_z_out
    assign z_output[2*col_index  ] = acc_output[ope_ctrl.z_read_row_index][col_index][TEW-1:0];
    assign z_output[2*col_index+1] = acc_output[ope_ctrl.z_read_row_index][col_index][2*TEW-1:TEW];
  end

  logic [NumPipeRegs:0] fma_valid_d, fma_valid_q;
  `FF(fma_valid_q, fma_valid_d, '0)
  assign fma_valid_d = {fma_valid_q[NumPipeRegs-1:0], (ope_state_q == OPE_Execute)};

  generate
    for (genvar row_index = 0; row_index < TE; row_index++) begin: array_row
      for (genvar col_index = 0; col_index < TE; col_index++) begin: array_col
        
        logic acc_ext_wen;

        assign acc_ext_wen =
              (row_index == ope_ctrl.y_write_row_index) &&
              ((ope_ctrl.external_loading) || ope_state_q == OPE_Zero);

        opope_accumulator #(
          .DATA_WIDTH (TEW  ),
          .DEPTH      (NRTILE)
        ) i_accumulator (
          .clk_i              (clk_i                                                        ),
          .rst_ni             (rst_ni                                                       ),
          .flush_i            (1'b0                                                         ),
          .iteration_change_i (1'b0                                                         ),
          .wdata_i            (ope_ctrl.acc_input_selector
                               ? {fma_to_acc_output[row_index][col_index],
                                  fma_to_acc_output[row_index][col_index]}
                               : {y_bias_load[2*col_index+1], y_bias_load[2*col_index]}      ),
          .wen_i              (ope_ctrl.acc_input_selector ? fma_valid_q[NumPipeRegs] : acc_ext_wen),
          .waddr_i            (ope_ctrl.acc_input_selector ? ope_ctrl.tile_write_to_engine
                                                           : ope_ctrl.y_write_tile_index     ),
          .ext_ld_i           (ope_ctrl.external_loading                                    ),
          .raddr_i            (ope_ctrl.z_read_tile_index                                    ),
          .rdata_o            (acc_output[row_index][col_index]                           )
        );

        opope_fma   #(
          .FpFormat    (fpnew_pkg::FP32        ),
          .NumPipeRegs (NumPipeRegs         ),
          .PipeConfig  (fpnew_pkg::DISTRIBUTED ),
          .Stallable   (1'b1                   )
        ) i_fma    (
          .clk_i        ( fma_clk      ),
          .rst_ni       ( rst_ni       ),
          .operands_i   ({ope_ctrl.y_bias_selector
                          ? acc_output[row_index][col_index][ope_ctrl.z_read_tile_index[0]*TEW +: TEW]
                          : fma_to_acc_output[row_index][col_index],
                          w_operand_q[col_index],
                          x_operand_q[row_index]}                ),
          .reg_enable_i ( ope_ctrl.tile_enable ),
          .result_o     ( fma_to_acc_output[row_index][col_index]      )
        );
      end
    end
  endgenerate

endmodule : spatz_ope
