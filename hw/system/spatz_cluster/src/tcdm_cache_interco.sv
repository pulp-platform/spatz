// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Diyou Shen <dishen@iis.ee.ethz.ch>

// The cache xbar used to select the cache banks

module tcdm_cache_interco #(
  /// Number of inputs into the interconnect (`> 0`).
  parameter int unsigned NumCore         = 32'd0,
  /// Number of outputs from the interconnect (`> 0`).
  parameter int unsigned NumCache          = 32'd0,
  /// Offset bits based on cacheline: 512b => 6 bits
  parameter int unsigned Offset               = 32'd6,

  /// Port type of the data request ports.
  parameter type         tcdm_req_t           = logic,
  /// Port type of the data response ports.
  parameter type         tcdm_rsp_t           = logic,
  /// Payload type of the data request ports.
  parameter type         tcdm_req_chan_t      = logic,
  /// Payload type of the data response ports.
  parameter type         tcdm_rsp_chan_t      = logic,

  parameter snitch_pkg::topo_e Topology       = snitch_pkg::LogarithmicInterconnect
) (
  /// Clock, positive edge triggered.
  input  logic                                  clk_i,
  /// Reset, active low.
  input  logic                                  rst_ni,
  /// Request port.
  input  tcdm_req_t               [NumCore-1:0] core_req_i,
  /// Response ready in
  input  logic                    [NumCore-1:0] core_rsp_ready_i,
  /// Resposne port.
  output tcdm_rsp_t               [NumCore-1:0] core_rsp_o,
  /// Memory Side
  /// Request.
  output tcdm_req_t              [NumCache-1:0] mem_req_o,
  /// Response ready out
  output logic                   [NumCache-1:0] mem_rsp_ready_o,
  /// Response.
  input  tcdm_rsp_t              [NumCache-1:0] mem_rsp_i
);

  // --------
  // Parameters and Signals
  // --------

  // Selection signal width and types
  localparam int unsigned NumMemSelBits  = $clog2(NumCache);
  localparam int unsigned NumCoreSelBits = $clog2(NumCore);

  typedef logic [NumMemSelBits-1 :0] mem_sel_t;
  typedef logic [NumCoreSelBits-1:0] core_sel_t;

  // core select which cache bank to go
  core_sel_t [NumCore-1:0] core_req_sel;
  mem_sel_t  [NumCache -1:0] mem_rsp_sel;

  // Number of bits used to identify the cache bank
  localparam int unsigned CacheBankBits  = $clog2(NumCore);

  tcdm_req_chan_t [NumCore-1:0] core_req;
  logic           [NumCore-1:0] core_req_valid, core_req_ready;

  tcdm_req_chan_t [NumCache -1:0] mem_req;
  logic           [NumCache -1:0] mem_req_valid, mem_req_ready;

  tcdm_rsp_chan_t [NumCore-1:0] core_rsp;
  logic           [NumCore-1:0] core_rsp_valid, core_rsp_ready;

  tcdm_rsp_chan_t [NumCache -1:0] mem_rsp;
  logic           [NumCache -1:0] mem_rsp_valid, mem_rsp_ready;


  // --------
  // Xbar
  // --------

  stream_xbar #(
    .NumInp      (NumCore      ),
    .NumOut      (NumCache       ),
    .payload_t   (tcdm_req_chan_t   )
  ) i_req_xbar (
    .clk_i  (clk_i            ),
    .rst_ni (rst_ni           ),
    .flush_i(1'b0             ),
    // External priority flag
    .rr_i   ('0               ),
    // Master
    .data_i (core_req         ),
    .valid_i(core_req_valid   ),
    .ready_o(core_req_ready   ),
    .sel_i  (core_req_sel     ),
    // Slave
    .data_o (mem_req          ),
    .valid_o(mem_req_valid    ),
    .ready_i(mem_req_ready    ),
    .idx_o  (/* Unused */     )
  );

  stream_xbar #(
    .NumInp       (NumCache    ),
    .NumOut       (NumCore      ),
    .payload_t    (tcdm_rsp_chan_t  )
  ) i_rsp_xbar (
    .clk_i  (clk_i            ),
    .rst_ni (rst_ni           ),
    .flush_i(1'b0             ),
    // External priority flag
    .rr_i   ('0               ),
    // Master
    .data_i (mem_rsp          ),
    .valid_i(mem_rsp_valid    ),
    .ready_o(mem_rsp_ready    ),
    .sel_i  (mem_rsp_sel      ),
    // Slave
    .data_o (core_rsp         ),
    .valid_o(core_rsp_valid   ),
    .ready_i(core_rsp_ready   ),
    .idx_o  (/* Unused */     )
  );

  // --------
  // Selection Signals
  // --------

  // select the target cache bank based on the `bank` bits
  // Example: 128 KiB total, 4 way, 4 cache banks, 512b cacheline
  // => 128*1024 = 2^17 Byte => 2^(17-6) = 2^11 cachelines
  // => 2^11/4 = 2^9 sets per cache bank => 2^9/4 = 2^7 sets per way per cache bank
  // => 7 bits index; 2 bits cache bank bits;
  // addr: Tag: [31:14]; Index: [13:7]; Cache Bank: [7:6]; Offset: [5:0]
  for (genvar port = 0; port < NumCore; port++) begin : gen_req_sel
    assign core_req_sel[port] = core_req[port].addr[Offset+:CacheBankBits];
  end

  // forward response to the sender core
  for (genvar port = 0; port < NumCache;  port++) begin : gen_rsp_sel
    assign mem_rsp_sel[port] = mem_rsp[port].user.core_id;
  end


  // --------
  // Registers
  // --------

  for (genvar port = 0; port < NumCore; port++) begin : gen_cache_interco_reg
    spill_register #(
      .T      (tcdm_req_chan_t          )
    ) i_tcdm_req_reg (
      .clk_i  (clk_i                    ),
      .rst_ni (rst_ni                   ),
      .data_i (core_req_i[port].q       ),
      .valid_i(core_req_i[port].q_valid ),
      .ready_o(core_rsp_o[port].q_ready ),
      .data_o (core_req[port]           ),
      .valid_o(core_req_valid[port]     ),
      .ready_i(core_req_ready[port]     )
    );

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
      .data_o    (core_rsp_o[port].p        ),
      .valid_o   (core_rsp_o[port].p_valid  ),
      .ready_i   (core_rsp_ready_i[port]    )
    );
  end


  // --------
  // IO Assignment
  // --------
  for (genvar port = 0; port < NumCache; port++) begin : gen_cache_io
    assign mem_req_o[port] = '{
      q:        mem_req[port],
      q_valid:  mem_req_valid[port],
      default:  '0
    };
    assign mem_rsp[port]          = mem_rsp_i[port].p;
    assign mem_rsp_valid[port]    = mem_rsp_i[port].p_valid;
    assign mem_req_ready[port]    = mem_rsp_i[port].q_ready;
  end

  assign mem_rsp_ready_o  = mem_rsp_ready;


endmodule
