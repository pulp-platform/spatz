// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Bowen Wang, ETH Zurich
//
// Standalone Gather/Scatter datapath
//

module ventaglio
  import spatz_pkg::*;
  import rvv_pkg::*;
  import vtl_pkg::*;
  #(
    parameter int unsigned NarrowDataWidth = VTGChannelWidth,
    parameter int unsigned WideDataWidth   = VTGChannelWidth * VENTAGLIO_WFACTOR
  ) (
    input  logic            clk_i,
    input  logic            rst_ni,
    input  logic            testmode_i,

    // Spatz request
    input  spatz_req_t       spatz_req_i,
    input  logic             spatz_req_valid_i,
    // VTL admit handshake to the controller. High when the spill
    // register can accept the next use_vtl op (vventclr or vfx).
    output logic             vtl_req_ready_o,
    input  logic             spatz_vfu_req_ready_i,
    // VTL response (retirement path for vventclr; vfx retires via vfu_rsp)
    output logic             vtl_rsp_valid_o,
    output vsldu_rsp_t       vtl_rsp_o,
    // VFU response
    input  logic             vfu_rsp_valid_i,
    input  vfu_rsp_t         vfu_rsp_i,
    // Slave Write port (single)
    input  vrf_addr_t        waddr_i,
    input  vrf_data_t        wdata_i,
    input  logic             we_i,
    input  vrf_be_t          wbe_i,
    output logic             wvalid_o,
    input  logic             wscatter_en_i,
    // Slave Read port (single)
    input  vrf_addr_t        raddr_i,
    input  logic             re_i,
    output vrf_data_t        rdata_o,
    output logic             rvalid_o,
    input  logic             rgather_en_i,

    // Master VRF interface
    output vrf_addr_t                       vrf_waddr_o,
    output vrf_data_t                       vrf_wdata_o,
    output logic                            vrf_we_o,
    output vrf_be_t                         vrf_wbe_o,
    input  logic                            vrf_wvalid_i,
    output spatz_id_t  [1:0]                vrf_id_o,
    output vrf_addr_t                       vrf_raddr_o,
    output logic                            vrf_re_o,
    input  vrf_data_t                       vrf_rdata_i,
    input  logic                            vrf_rvalid_i
  );

// Include FF
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
    .clk_i  (clk_i                                              ),
    .rst_ni (rst_ni                                             ),
    .data_i (spatz_req_i                                        ),
    .valid_i(spatz_req_valid_i && spatz_req_i.op_vtl.use_vtl    ),
    .ready_o(vtl_req_ready_o                                     ),
    .data_o (spatz_req                                          ),
    .valid_o(spatz_req_valid                                    ),
    .ready_i(spatz_req_ready                                    )
  );


  // State Handling signals
  logic [NrParallelInstructions-1:0] running_d, running_q;
  `FF(running_q, running_d, '0)
  logic new_vtl_request;
  assign new_vtl_request = spatz_req_valid && !running_q[spatz_req.id];

  /******************************/
  /*       Clear Sequencer      */
  /******************************/
  typedef logic [$clog2(VTGNrWordsPerChannel)-1:0] vtg_row_addr_t;

  logic           clear_active_q,  clear_active_d;
  vtg_row_addr_t  clear_row_q,     clear_row_d;
  spatz_id_t      clear_id_q,      clear_id_d;
  `FF(clear_active_q, clear_active_d, 1'b0)
  `FF(clear_row_q,    clear_row_d,    '0)
  `FF(clear_id_q,     clear_id_d,     '0)

  logic clear_last_row;
  assign clear_last_row = (clear_row_q == vtg_row_addr_t'(VTGNrWordsPerChannel - 1));

  // True for the cycle that retires the clear op.
  logic clear_done;
  assign clear_done = clear_active_q && clear_last_row;

  always_comb begin : proc_clear
    clear_active_d = clear_active_q;
    clear_row_d    = clear_row_q;
    clear_id_d     = clear_id_q;

    // Start clearing when a clear_buffer op is freshly latched in the spill.
    if (!clear_active_q && new_vtl_request && spatz_req.op_vtl.clear_buffer) begin
      clear_active_d = 1'b1;
      clear_row_d    = '0;
      clear_id_d     = spatz_req.id;
    end else if (clear_active_q) begin
      if (clear_last_row) begin
        clear_active_d = 1'b0;
      end else begin
        clear_row_d = clear_row_q + 1'b1;
      end
    end
  end

  // Retire the clear op via the SLD response port.
  assign vtl_rsp_valid_o = clear_done;
  assign vtl_rsp_o.id    = clear_id_q;


  /******************************/
  /*       State Handler        */
  /******************************/
  always_comb begin : proc_vtl_state
    running_d = running_q;
    spatz_req_ready = !spatz_req_valid;

    // A new spatz_req is received
    if (new_vtl_request) begin
      running_d[spatz_req.id] = 1'b1;
    end

    // VFU-routed ops retire via vfu_rsp.
    if (running_q[vfu_rsp_i.id] && vfu_rsp_valid_i) begin
      running_d[vfu_rsp_i.id] = 1'b0;
    end
    // vventclr retires via clear_done (vtl_rsp_valid_o).
    if (clear_done && running_q[clear_id_q]) begin
      running_d[clear_id_q] = 1'b0;
    end
    // clean done
    if (spatz_req_valid && spatz_req.op_vtl.clear_buffer) begin
      if (clear_done) spatz_req_ready = 1'b1;
    // macc done
    end else if (spatz_vfu_req_ready_i) begin
      spatz_req_ready = 1'b1;
    end
  end

  //////////////////////////////////
  //   Index Buffer (FIFO)        //
  //////////////////////////////////
  // Fetch signals
  // moniters pre-spill spatz_req channel, triggered by a successful handshake  
  logic       eff_fetch;
  spatz_id_t  eff_id;
  vreg_t      eff_vidx;

  assign eff_fetch = spatz_req_valid_i
                    && spatz_req_i.op_vtl.use_vtl
                    && !spatz_req_i.op_vtl.clear_buffer
                    && vtl_req_ready_o; 
  assign eff_id    = spatz_req_i.id;
  assign eff_vidx  = spatz_req_i.op_vtl.idx_vreg;
  
  // Index used for scatter and gather data path
  // Spill register cut the timing path 
  vrf_data_t  index_q;
  logic       index_valid_q;

  // Pop the current index once the proceeding req has been acknowledged
  logic  idx_spill_pop;
  assign idx_spill_pop = spatz_req_ready && spatz_req_valid
                         && !spatz_req.op_vtl.clear_buffer;

  spill_register #(
    .T(vrf_data_t)
  ) i_idx_spill (
    .clk_i  (clk_i             ),
    .rst_ni (rst_ni            ),
    .data_i (vrf_rdata_i       ),
    .valid_i(vrf_rvalid_i      ),
    .ready_o(/*    UNUSED    */),
    .data_o (index_q           ),
    .valid_o(index_valid_q     ),
    .ready_i(idx_spill_pop     )
  );

  // Index load requests
  typedef struct packed {
    spatz_id_t id;
    vrf_addr_t addr;
  } idx_req_t;

  idx_req_t idx_req_in, idx_req_out;
  assign idx_req_in  = '{ id: eff_id, addr: {eff_vidx, $clog2(NrWordsPerVector)'(1'b0)}};
  assign vrf_id_o[0] = idx_req_out.id;
  assign vrf_raddr_o = idx_req_out.addr;

  spill_register #(
    .T(idx_req_t)
  ) i_idx_req_reg (
    .clk_i  (clk_i             ),
    .rst_ni (rst_ni            ),
    .data_i (idx_req_in        ),
    .valid_i(eff_fetch         ),
    .ready_o(/*    UNUSED    */),
    .data_o (idx_req_out       ),
    .valid_o(vrf_re_o          ),
    .ready_i(vrf_rvalid_i      )
  );

  ////////////////////////////////
  // Counter for each operation //
  ////////////////////////////////

  // signals for gather or scatter
  logic is_gather, is_scatter;

  logic req_proceed;
  logic last_addr_bit_d, last_addr_bit_q;
  `FF(last_addr_bit_q, last_addr_bit_d, '0)

  assign last_addr_bit_d = raddr_i[0];
  assign req_proceed = last_addr_bit_d ^ last_addr_bit_q;

  // Beats per op = vl / elements_per_beat, where elements_per_beat =
  // VRFWordBWidth >> vsew (VRFWordBWidth = N_FU * ELENB). Derivation in
  // README ("Beats per operation"). Truncates for non-aligned vl, which
  // vsetvli prevents in current kernels (VLMAX is a multiple of width).
  logic [4:0] num_beats_per_op;
  assign num_beats_per_op =
      spatz_req.vl >> ($clog2(VRFWordBWidth) - int'(spatz_req.vtype.vsew));

  logic [4:0] op_beat_cnt_d, op_beat_cnt_q;
  `FF(op_beat_cnt_q, op_beat_cnt_d, '0)

  // Per-op beat counter. Kept as an explicit hook for a future long-vector
  // index-refresh: when a single op spans more beats than the index buffer
  // holds, this counter triggers a mid-op refresh. The wrap is gated on
  // `rvalid_o && req_proceed` so the counter only ever wraps the cycle it
  // actually advances — otherwise an idle cycle at `_q == max-1` would
  // silently reset it.
  always_comb begin
    op_beat_cnt_d = op_beat_cnt_q;
    if (is_gather && rvalid_o && req_proceed) begin
      op_beat_cnt_d = (op_beat_cnt_q == num_beats_per_op - 1) ? '0
                                                              : op_beat_cnt_q + 1;
    end
  end

  /******************************/
  /*           Types            */
  /******************************/

  // The input address has type `vrf_addr_t`
  // It is used to address `NRVREG * NrWordsPerVector` words
  // Word is represented as `N_FU * ELEN`, is the bandwidth VPU or VLSU comsume
  // In VTL, each channel provide `N_FU * ELEN` bandwidth as well, so it is channel addressable

  // We currently utilized a interleaved address scheme
  // Word 0             -> Channel 0 Row 0
  // Word 1             -> Channel 1 Row 0
  // Word VTGNrChannels -> Channel 0 Row 1 ...


  // `f_channel` function extract the channel index
  function automatic logic [$clog2(VTGNrChannels)-1:0] f_channel(vrf_addr_t addr);
    f_channel = addr[$clog2(VTGNrChannels)-1:0];
  endfunction: f_channel

  // `f_row` function extract the word (row) index within one channel
  function automatic logic [$clog2(VTGNrWordsPerChannel)-1:0] f_row(vrf_addr_t addr);
    f_row = addr[$clog2(VTGNrWordsPerChannel * VTGNrChannels)-1:$clog2(VTGNrChannels)];
  endfunction: f_row

  // (vtg_row_addr_t typedef defined earlier alongside the clear sequencer)

  /******************************/
  /*          Signals           */
  /******************************/

  // signals for gather or scatter
  logic gather_done;
  `FF(gather_done, spatz_vfu_req_ready_i&&spatz_req_valid, '0)

  // `rgather_en_i` and `wscatter_en_i` signals are attached in VRF bypass logic
  always_comb begin
    is_gather  = spatz_req.op_vtl.gather_vd && rgather_en_i;
    is_scatter = spatz_req.op_vtl.scatter_vd && wscatter_en_i;
  end

  // write signals
  vtg_row_addr_t          [VTGNrChannels-1:0] waddr;
  ventaglio_narrow_data_t [VTGNrChannels-1:0] wdata;
  logic                   [VTGNrChannels-1:0] we;
  ventaglio_narrow_be_t   [VTGNrChannels-1:0] wbe;

  // read signals
  vtg_row_addr_t          [VTGNrChannels-1:0] raddr;
  ventaglio_narrow_data_t [VTGNrChannels-1:0] rdata;

  // write mapping
  logic [VTGNrChannels-1:0]                    write_request;
  logic [VTGNrChannels-1:0][VTGNrChannels-1:0] scatter_write_request;

  // post scatter signals
  logic      [VTGNrChannels-1:0] scatter_we;
  vrf_addr_t [VTGNrChannels-1:0] scatter_waddr;
  vrf_data_t [VTGNrChannels-1:0] scatter_wdata;
  vrf_be_t   [VTGNrChannels-1:0] scatter_wbe;

  logic      [VTGNrChannels-1:0] scatter_wvalid;

  logic                          post_scatter_wvalid;

  always_comb begin: gen_write_request
    for (int channel = 0; channel < VTGNrChannels; channel++) begin
      write_request[channel] = we_i && f_channel(waddr_i) == channel && !is_scatter;
    end
    // scatter requests
    for (int b_channel = 0; b_channel < VTGNrChannels; b_channel++) begin   // channels from the bank side
      for (int s_channel = 0; s_channel < VTGNrChannels; s_channel++) begin // channels from the scatter datapath side
        scatter_write_request[b_channel][s_channel] = scatter_we[s_channel] && f_channel(scatter_waddr[s_channel]) == b_channel && is_scatter;
      end
    end
  end: gen_write_request

  always_comb begin : proc_write
    waddr          = '0;
    wdata          = '0;
    we             = '0;
    wbe            = '0;
    wvalid_o       = 1'b0;
    scatter_wvalid = '0;

    if (clear_active_q) begin // priority 0: vventclr — zero every cell
      for (int unsigned channel = 0; channel < VTGNrChannels; channel++) begin
        waddr[channel] = clear_row_q;
        wdata[channel] = '0;
        we[channel]    = 1'b1;
        wbe[channel]   = '1;
      end
      // wvalid_o stays 0: no slave-port write is being acknowledged.
    end else if (!is_scatter) begin // priority 1: normal requests
      for (int unsigned channel = 0; channel < VTGNrChannels; channel++) begin
        if (write_request[channel]) begin
          waddr[channel] = f_row(waddr_i);
          wdata[channel] = wdata_i;
          we[channel]    = 1'b1;
          wbe[channel]   = wbe_i;
          wvalid_o       = 1'b1;
        end
      end
    end else begin // priority 2: scatter requests
      for (int unsigned b_channel = 0; b_channel < VTGNrChannels; b_channel++) begin
        for (int unsigned s_channel = 0; s_channel < VTGNrChannels; s_channel++) begin
          if (scatter_write_request[b_channel][s_channel]) begin
            waddr[b_channel]           = f_row(scatter_waddr[s_channel]);
            wdata[b_channel]           = scatter_wdata[s_channel];
            we[b_channel]              = 1'b1;
            wbe[b_channel]             = scatter_wbe[s_channel];
            scatter_wvalid[s_channel]  = 1'b1;
          end
        end
      end
      wvalid_o = post_scatter_wvalid;
    end

  end : proc_write

  // read mapping

  // read_request: non-gather request signals
  // gathered_read_request: gather request signals
  logic [VTGNrChannels-1:0]                    read_request;

  logic      [VTGNrChannels-1:0][VTGNrChannels-1:0] gather_read_request;
  logic      [VTGNrChannels-1:0]                    gather_re;
  vrf_addr_t [VTGNrChannels-1:0]                    gather_raddr;

  vrf_data_t [VTGNrChannels-1:0]                    gather_rdata;
  logic      [VTGNrChannels-1:0]                    gather_rvalid;

  // post gather signals
  vrf_data_t post_gather_rdata;
  logic      post_gather_rvalid;

  always_comb begin: gen_read_request
    // normal requests
    for (int channel = 0; channel < VTGNrChannels; channel++) begin
      read_request[channel] = re_i && f_channel(raddr_i) == channel && !(is_gather);
    end
    // gathered requests
    for (int b_channel = 0; b_channel < VTGNrChannels; b_channel++) begin   // channels from the bank side
      for (int g_channel = 0; g_channel < VTGNrChannels; g_channel++) begin // channels from the gather datapath side
        gather_read_request[b_channel][g_channel] = gather_re[g_channel] && f_channel(gather_raddr[g_channel]) == b_channel && is_gather;
      end
    end
  end: gen_read_request

  // this should be extended for gather
  always_comb begin : proc_read
    raddr    = '0;
    rvalid_o = 1'b0;
    rdata_o  = 'x;

    gather_rvalid = '0;
    gather_rdata  = 'x;

    if (!is_gather) begin // priority 1: normal read requests
      for (int unsigned b_channel = 0; b_channel < VTGNrChannels; b_channel++) begin // channels from the bank side
        if (read_request[b_channel]) begin
          raddr[b_channel] = f_row(raddr_i);
          rdata_o          = rdata[b_channel];
          rvalid_o         = 1'b1;
        end
      end
    end else if (is_gather) begin // priority 2: gather accesses
      for (int unsigned b_channel = 0; b_channel < VTGNrChannels; b_channel++) begin // channels from the bank side
        for (int unsigned g_channel = 0; g_channel < VTGNrChannels; g_channel++) begin // channels from the gather datapath side
          if (gather_read_request[b_channel][g_channel]) begin
            raddr[b_channel]          = f_row(gather_raddr[g_channel]);
            gather_rdata[g_channel]   = rdata[b_channel];
            gather_rvalid[g_channel]  = index_valid_q;
          end
        end
      end
      // assign the gathered data to the output
      rdata_o  = post_gather_rdata;
      rvalid_o = post_gather_rvalid;
    end
  end

  /******************************/
  /*      Scatter DataPath      */
  /******************************/
  ventaglio_scatter #(
    .NarrowDataWidth (NarrowDataWidth),
    .WideDataWidth   (WideDataWidth)
  ) i_vtl_scatter (
    .clk_i       (clk_i),
    .rst_ni      (rst_ni),
    .testmode_i  (testmode_i),
    // narrow ports
    .waddr_i     (waddr_i),
    .wdata_i     (wdata_i),
    .we_i        (we_i && is_scatter),
    .wbe_i       (wbe_i),
    .wvalid_o    (post_scatter_wvalid),
    // wide ports
    .waddr_o     (scatter_waddr),
    .wdata_o     (scatter_wdata),
    .we_o        (scatter_we),
    .wbe_o       (scatter_wbe),
    .wvalid_i    (scatter_wvalid),
    // control
    // TODO: it is better to implement as a counter to track
    .scatter_done_i      (!is_scatter),
    .num_beats_per_op_i  (num_beats_per_op),
    // controls
    .index_i       (index_q                ),
    .vtl_cfg_i     (spatz_req.op_vtl.sp_cfg),
    .index_valid_i (index_valid_q) // meaning a new read data available
  );


  /******************************/
  /*           Buffer           */
  /******************************/

  // Buffer has `VTGNrChannels` channels
  // Each channel is divided into `N_FU` banks, whose width is `ELEN`
  // In this way, each channel can provide the same bandwidth as the VLSU and VPU
  for (genvar channel = 0; channel < VTGNrChannels; channel++) begin : gen_vtg_channels
    for (genvar bank = 0; bank < N_FU; bank++) begin: gen_vtg_banks
      ventaglio_regfile #(
        .NrWords    (VTGNrWordsPerChannel ),
        .WordWidth  (ELEN                 )
      ) i_vtg_vregfile (
        .clk_i     (clk_i                              ),
        .rst_ni    (rst_ni                             ),
        .testmode_i(testmode_i                         ),
        .waddr_i   (waddr[channel]                     ),
        .wdata_i   (wdata[channel][ELEN*bank +: ELEN]  ),
        .we_i      (we[channel]                        ),
        .wbe_i     (wbe[channel][ELENB*bank +: ELENB]  ),
        .raddr_i   (raddr[channel]                     ),
        .rdata_o   (rdata[channel][ELEN*bank +: ELEN]  )
      );
    end
  end

  /******************************/
  /*      Gather  DataPath      */
  /******************************/

  ventaglio_gather #(
    .NarrowDataWidth (NarrowDataWidth),
    .WideDataWidth   (WideDataWidth)
  ) i_vtl_gather (
    .clk_i       (clk_i),
    .rst_ni      (rst_ni),
    .testmode_i  (testmode_i),
    // narrow ports
    .raddr_i     (raddr_i              ),
    .re_i        (re_i && is_gather    ),
    .rdata_o     (post_gather_rdata),
    .rvalid_o    (post_gather_rvalid),
    // wide ports
    .raddr_o     (gather_raddr         ),
    .re_o        (gather_re            ),
    .rdata_i     (gather_rdata         ),
    .rvalid_i    (gather_rvalid        ),
    // control
    .gather_done_i(gather_done          ),
    // index cfg
    .index_i     (index_q              ),
    .vtl_cfg_i   (spatz_req.op_vtl.sp_cfg),
    .load_index_o(vreg_idx_counter_en)
  );

  /******************************/
  /*      Write Requests        */
  /******************************/

  // RESERVED: master VRF write port is held idle.
  assign vrf_we_o    = '0;
  assign vrf_wbe_o   = '0;
  assign vrf_waddr_o = '0;
  assign vrf_wdata_o = '0;

endmodule : ventaglio
