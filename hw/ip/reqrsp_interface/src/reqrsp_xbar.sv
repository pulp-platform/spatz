// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Diyou Shen <dishen@iis.ee.ethz.ch>

// The cache xbar used to select the cache banks

module reqrsp_xbar #(
  /// Number of inputs into the interconnect (`> 0`).
  parameter int unsigned NumInp               = 32'd0,
  /// Number of outputs from the interconnect (`> 0`).
  parameter int unsigned NumOut               = 32'd0,
  /// Generate Register
  parameter int unsigned PipeReg              = 1'b1,
  /// Rsp path Register
  parameter int unsigned RspReg               = 1'b0,
  /// Do we need to provide external priority for Xbar?
  parameter int unsigned ExtReqPrio           = 1'b0,
  parameter int unsigned ExtRspPrio           = 1'b0,
  /// Payload type of the data request ports.
  parameter type         tcdm_req_chan_t      = logic,
  /// Payload type of the data response ports.
  parameter type         tcdm_rsp_chan_t      = logic,

  parameter snitch_pkg::topo_e Topology       = snitch_pkg::LogarithmicInterconnect,

  parameter type         slv_sel_t            = logic [((NumOut > 1) ? $clog2(NumOut) : 1)-1 :0],

  parameter type         mst_sel_t            = logic [((NumInp > 1) ? $clog2(NumInp) : 1)-1 :0]
) (
  /// Clock, positive edge triggered.
  input  logic                                clk_i,
  /// Reset, active low.
  input  logic                                rst_ni,
  /// Request port.
  input  tcdm_req_chan_t         [NumInp-1:0] slv_req_i,
  /// Only used when use external priority
  input  mst_sel_t               [NumOut-1:0] slv_rr_i,
  input  logic                   [NumInp-1:0] slv_req_valid_i,
  output logic                   [NumInp-1:0] slv_req_ready_o,

  output tcdm_rsp_chan_t         [NumInp-1:0] slv_rsp_o,
  output logic                   [NumInp-1:0] slv_rsp_valid_o,
  input  logic                   [NumInp-1:0] slv_rsp_ready_i,
  // Slave (req input) selects which master (req output)
  input  slv_sel_t               [NumInp-1:0] slv_sel_i,

  // Which slave (req input) selected for routing
  output mst_sel_t               [NumOut-1:0] slv_selected_o,

  /// Memory Side
  /// Request.
  output tcdm_req_chan_t         [NumOut-1:0] mst_req_o,
  output logic                   [NumOut-1:0] mst_req_valid_o,
  input  logic                   [NumOut-1:0] mst_req_ready_i,

  input  tcdm_rsp_chan_t         [NumOut-1:0] mst_rsp_i,
  /// Only used when use external priority
  input  slv_sel_t               [NumInp-1:0] mst_rr_i,
  input  logic                   [NumOut-1:0] mst_rsp_valid_i,
  output logic                   [NumOut-1:0] mst_rsp_ready_o,

  // Master (rsp input) select which slave (rsp output)
  input  mst_sel_t               [NumOut-1:0] mst_sel_i
);

  // --------
  // Parameters and Signals
  // --------

  tcdm_req_chan_t [NumInp-1:0] core_req;
  logic           [NumInp-1:0] core_req_valid,  core_req_ready;

  tcdm_req_chan_t [NumOut-1:0] mem_req;
  logic           [NumOut-1:0] mem_req_valid,   mem_req_ready;

  tcdm_rsp_chan_t [NumInp-1:0] core_rsp;
  logic           [NumInp-1:0] core_rsp_valid,  core_rsp_ready;

  tcdm_rsp_chan_t [NumOut-1:0] mem_rsp;
  logic           [NumOut-1:0] mem_rsp_valid,   mem_rsp_ready;

  slv_sel_t [NumInp-1:0] slv_sel;
  mst_sel_t [NumOut-1:0] core_rr;

  typedef struct packed {
    tcdm_req_chan_t payload;
    slv_sel_t select;
  } reg_data_t;

  // --------
  // Xbar
  // --------

  stream_xbar #(
    .NumInp      (NumInp          ),
    .NumOut      (NumOut          ),
    // LockIn cannot be set when using external priority
    .ExtPrio     (ExtReqPrio      ),
    .AxiVldRdy   (~ExtReqPrio     ),
    .LockIn      (~ExtReqPrio     ),
    .payload_t   (tcdm_req_chan_t )
  ) i_req_xbar (
    .clk_i  (clk_i            ),
    .rst_ni (rst_ni           ),
    .flush_i(1'b0             ),
    // External priority flag
    .rr_i   (core_rr          ),
    // Master
    .data_i (core_req         ),
    .valid_i(core_req_valid   ),
    .ready_o(core_req_ready   ),
    .sel_i  (slv_sel          ),
    // Slave
    .data_o (mem_req          ),
    .valid_o(mem_req_valid    ),
    .ready_i(mem_req_ready    ),
    .idx_o  (slv_selected_o   )
  );

  stream_xbar #(
    .NumInp       (NumOut           ),
    .NumOut       (NumInp           ),
    // LockIn cannot be set when using external priority
    .ExtPrio      (ExtRspPrio       ),
    .AxiVldRdy    (~ExtRspPrio      ),
    .LockIn       (~ExtRspPrio      ),
    .payload_t    (tcdm_rsp_chan_t  )
  ) i_rsp_xbar (
    .clk_i  (clk_i            ),
    .rst_ni (rst_ni           ),
    .flush_i(1'b0             ),
    // External priority flag
    .rr_i   (mst_rr_i         ),
    // Master
    .data_i (mem_rsp          ),
    .valid_i(mem_rsp_valid    ),
    .ready_o(mem_rsp_ready    ),
    .sel_i  (mst_sel_i        ),
    // Slave
    .data_o (core_rsp         ),
    .valid_o(core_rsp_valid   ),
    .ready_i(core_rsp_ready   ),
    .idx_o  (/* Unused */     )
  );


  // --------
  // Registers
  // --------

  for (genvar port = 0; port < NumInp; port++) begin : gen_cache_interco_reg
    if (PipeReg == 1) begin : gen_regs
      reg_data_t prereg_data, postreg_data;
      assign prereg_data = '{
        payload: slv_req_i[port],
        select : slv_sel_i[port]
      };
      spill_register #(
        .T      (reg_data_t          )
      ) i_tcdm_req_reg (
        .clk_i  (clk_i                    ),
        .rst_ni (rst_ni                   ),
        .data_i (prereg_data              ),
        .valid_i(slv_req_valid_i[port]    ),
        .ready_o(slv_req_ready_o[port]    ),
        .data_o (postreg_data             ),
        .valid_o(core_req_valid[port]     ),
        .ready_i(core_req_ready[port]     )
      );

      assign core_req[port] = postreg_data.payload;
      assign slv_sel[port] = postreg_data.select;

      if (RspReg) begin : gen_rsp_reg
        spill_register #(
          .T         (tcdm_rsp_chan_t       )
        ) i_tcdm_rsp_reg (
          .clk_i     (clk_i                 ),
          .rst_ni    (rst_ni                ),
          .data_i    (core_rsp[port]        ),
          .valid_i   (core_rsp_valid[port]  ),
          .ready_o   (core_rsp_ready[port]  ),
          .data_o    (slv_rsp_o[port]       ),
          .valid_o   (slv_rsp_valid_o[port] ),
          .ready_i   (slv_rsp_ready_i[port] )
        );
      end else begin : gen_rsp_bypass
        fall_through_register #(
          .T         (tcdm_rsp_chan_t           )
        ) i_tcdm_rsp_reg (
          .clk_i     (clk_i                     ),
          .rst_ni    (rst_ni                    ),
          .clr_i     (1'b0                      ),
          .testmode_i(1'b0                      ),
          .data_i    (core_rsp[port]            ),
          .valid_i   (core_rsp_valid[port]      ),
          .ready_o   (core_rsp_ready[port]      ),
          .data_o    (slv_rsp_o[port]           ),
          .valid_o   (slv_rsp_valid_o[port]     ),
          .ready_i   (slv_rsp_ready_i[port]     )
        );
      end
    end else begin : bypass_reg
      assign core_req[port]        = slv_req_i[port];
      assign core_req_valid[port]  = slv_req_valid_i[port];
      assign slv_req_ready_o[port] = core_req_ready[port];
      assign slv_sel[port]         = slv_sel_i[port];

      assign slv_rsp_o[port]       = core_rsp_valid[port] ? core_rsp[port] : '0;
      assign slv_rsp_valid_o[port] = core_rsp_valid[port];
      assign core_rsp_ready[port]  = slv_rsp_ready_i[port];
    end
  end

  for (genvar port = 0; port < NumOut; port++) begin : gen_rr_reg
    if (PipeReg == 1) begin : gen_regs
      shift_reg #(
        .dtype(mst_sel_t         ),
        .Depth(1                 )
      ) i_rr_pipe (
        .clk_i (clk_i            ),
        .rst_ni(rst_ni           ),
        .d_i   (slv_rr_i[port]   ),
        .d_o   (core_rr[port]    )
      );
    end else begin : bypass_reg
      assign core_rr[port] = slv_rr_i[port];
    end
  end

  // --------
  // IO Assignment
  // --------

  for (genvar port = 0; port < NumOut; port++) begin
    // Mute the req/rsp channel if not valid => easier to debug
    assign mst_req_o      [port] = mem_req_valid  [port] ? mem_req[port] : '0;
    assign mst_req_valid_o[port] = mem_req_valid  [port];
    assign mem_req_ready  [port] = mst_req_ready_i[port];

    assign mem_rsp        [port] = mst_rsp_valid_i[port] ? mst_rsp_i[port] : '0;
    assign mem_rsp_valid  [port] = mst_rsp_valid_i[port];
    assign mst_rsp_ready_o[port] = mem_rsp_ready  [port];
  end

endmodule
