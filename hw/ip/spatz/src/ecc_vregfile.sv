// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

module ecc_vregfile import spatz_pkg::*; #(
  parameter int  unsigned NrReadPorts      = 0,
  parameter int  unsigned NrWords          = NRVREG,
  parameter int  unsigned WordWidth        = VRFWordWidth,
  // Derived parameters. Do not change!
  parameter type          addr_t           = logic [$clog2(NrWords)-1:0],
  parameter type          data_t           = logic [WordWidth-1:0],
  parameter type          strb_t           = logic [WordWidth/8-1:0],
  // ECC parameters
  parameter int  unsigned UnprotectedWidth = WordWidth,      // must equal WordWidth
  parameter int  unsigned ProtectedWidth   = WordWidth + 7,  // e.g. 32+7=39 for SECDED
  parameter int  unsigned NumRMWCuts       = 0,              // FF-based: keep 0
  parameter int  unsigned ByteWidth        = 8,
  // Derived ECC parameters. Do not change!
  localparam int unsigned ByteEnWidth      = UnprotectedWidth / ByteWidth,
  localparam int unsigned BankAddrWidth    = $clog2(NrWords)
) (
  input  logic                     clk_i,
  input  logic                     rst_ni,
  input  logic                     testmode_i,
  // Write port
  input  addr_t                    waddr_i,
  input  data_t                    wdata_i,   // unprotected write data
  input  logic                     we_i,
  input  strb_t                    wbe_i,
  // Read ports
  input  addr_t [NrReadPorts-1:0]  raddr_i,
  output data_t [NrReadPorts-1:0]  rdata_o,
  // ECC status outputs
  output logic  [NrReadPorts-1:0]  single_error_o,
  output logic  [NrReadPorts-1:0]  multi_error_o,
  // Write grant (low during RMW stall)
  output logic                     gnt_o
);

  // FI strobe signals----------------------
  // logic  [NrReadPorts-1:0]  single_error_o;
  // logic  [NrReadPorts-1:0]  multi_error_o;
  //-----------------------------------------

  // -------------------------------------------------------------------------
  // Derived localparams
  // -------------------------------------------------------------------------
  localparam int unsigned ProtWidth = ProtectedWidth - UnprotectedWidth;

  // -------------------------------------------------------------------------
  // ECC register file memory (stores encoded words)
  // -------------------------------------------------------------------------
  logic [NrWords-1:0][ProtectedWidth-1:0] mem;

  // -------------------------------------------------------------------------
  // RMW state machine signals (mirrors ecc_sram)
  // -------------------------------------------------------------------------
  typedef enum logic { NORMAL, READ_MODIFY_WRITE } store_state_e;
  store_state_e store_state_d, store_state_q;

  // rmw_count_t: protect against NumRMWCuts==0 giving 0-width type
  localparam int unsigned RMWCountWidth = (NumRMWCuts > 0)
                                          ? cf_math_pkg::idx_width(NumRMWCuts) : 1;
  typedef logic [RMWCountWidth-1:0] rmw_count_t;
  rmw_count_t rmw_count_d, rmw_count_q;

  logic                      internal_we;
  logic                      bank_req;
  logic                      bank_we;
  addr_t                     bank_addr;
  logic [ProtectedWidth-1:0] bank_wdata;
  logic [ProtectedWidth-1:0] bank_rdata;  // combinational read for RMW

  logic [UnprotectedWidth-1:0] input_buffer_d, input_buffer_q;
  addr_t                       addr_buffer_d,  addr_buffer_q;
  strb_t                       be_buffer_d,    be_buffer_q;

  // be_selector: expand byte-enable strobe to bit mask
  logic [UnprotectedWidth-1:0] be_selector;
  for (genvar i = 0; i < ByteEnWidth; i++) begin : gen_be_sel
    assign be_selector[i*ByteWidth +: ByteWidth] = {ByteWidth{be_buffer_q[i]}};
  end

  // RMW pipeline buffer (NumRMWCuts=0 → transparent, depth=0 shift_reg)
  // For FF-based VRF, bank_rdata is combinational so NumRMWCuts should be 0.
  logic [ProtectedWidth-1:0] rmw_buffer_end;
  if (NumRMWCuts == 0) begin : gen_no_rmw_cuts
    assign rmw_buffer_end = bank_rdata;
  end else begin : gen_rmw_cuts
    shift_reg #(
      .dtype (logic [ProtectedWidth-1:0]),
      .Depth (NumRMWCuts)
    ) i_rmw_buffer (
      .clk_i,
      .rst_ni,
      .d_i (bank_rdata   ),
      .d_o (rmw_buffer_end)
    );
  end

  // -------------------------------------------------------------------------
  // ECC encode / decode for write path (RMW)
  // -------------------------------------------------------------------------
  logic [UnprotectedWidth-1:0] to_store;   // input to encoder
  logic [UnprotectedWidth-1:0] loaded;     // decoded old data for RMW merge
  logic [1:0]                  rmw_ecc_error;

  // Decoder: used only for RMW merge; input selects between
  // same-cycle read (NORMAL, for full-word write no RMW needed but
  // decoder is always present) and buffered read (RMW state)
  logic [ProtectedWidth-1:0] rmw_decoder_in;
  assign rmw_decoder_in = (store_state_q == NORMAL) ? bank_rdata : rmw_buffer_end;

  hsiao_ecc_dec #(
    .DataWidth (UnprotectedWidth),
    .ProtWidth (ProtWidth)
  ) i_rmw_ecc_dec (
    .in         (rmw_decoder_in),
    .out        (loaded        ),
    .syndrome_o (              ),
    .err_o      (rmw_ecc_error )
  );

  // Merge: select new or old bytes according to be_buffer_q
  assign to_store = (store_state_q == NORMAL)
                    ? wdata_i                                          // full-word: encode new data directly
                    : (be_selector & input_buffer_q)                  // RMW: new bytes
                    | (~be_selector & loaded);                         //       old bytes

  hsiao_ecc_enc #(
    .DataWidth (UnprotectedWidth),
    .ProtWidth (ProtWidth)
  ) i_ecc_enc (
    .in  (to_store  ),
    .out (bank_wdata)
  );

  // -------------------------------------------------------------------------
  // Write FSM (identical structure to ecc_sram)
  // -------------------------------------------------------------------------
  assign internal_we = we_i & (wbe_i != '0);

  always_comb begin
    store_state_d  = NORMAL;
    gnt_o          = 1'b1;
    bank_addr      = waddr_i;
    bank_we        = internal_we;
    input_buffer_d = wdata_i;
    addr_buffer_d  = waddr_i;
    be_buffer_d    = wbe_i;
    bank_req       = we_i;
    rmw_count_d    = rmw_count_q;

    if (store_state_q == NORMAL) begin
      // Partial-byte write → need RMW
      if (we_i & (wbe_i != {ByteEnWidth{1'b1}}) & internal_we) begin
        store_state_d = READ_MODIFY_WRITE;
        bank_we       = 1'b0;             // suppress write on first cycle
        bank_req      = 1'b0;
        rmw_count_d   = rmw_count_t'(NumRMWCuts);
        gnt_o         = 1'b0;
      end
    end else begin
      // Stall upstream
      gnt_o          = 1'b0;
      bank_addr      = addr_buffer_q;
      bank_we        = 1'b1;
      bank_req       = 1'b0;
      input_buffer_d = input_buffer_q;
      addr_buffer_d  = addr_buffer_q;
      be_buffer_d    = be_buffer_q;

      if (rmw_count_q == '0) begin
        // Merge+write cycle: assert bank_req to trigger the actual write,
        // and raise gnt_o so upstream sees wvalid_o=1 and stops re-issuing we=1.
        // store_state_d stays NORMAL (default), so next cycle we return to NORMAL.
        bank_req = 1'b1;
        gnt_o    = 1'b1;
      end else begin
        bank_req      = 1'b0;
        rmw_count_d   = rmw_count_q - 1;
        store_state_d = READ_MODIFY_WRITE;
      end
    end
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin : proc_rmw_ff
    if (!rst_ni) begin
      store_state_q  <= NORMAL;
      addr_buffer_q  <= '0;
      input_buffer_q <= '0;
      be_buffer_q    <= '0;
      rmw_count_q    <= '0;
    end else begin
      store_state_q  <= store_state_d;
      addr_buffer_q  <= addr_buffer_d;
      input_buffer_q <= input_buffer_d;
      be_buffer_q    <= be_buffer_d;
      rmw_count_q    <= rmw_count_d;
    end
  end

  // -------------------------------------------------------------------------
  // FF memory array – write path
  // bank_rdata is a combinational read used only by the RMW path (write addr)
  // -------------------------------------------------------------------------
  assign bank_rdata = mem[bank_addr];  // combinational; bank_addr tracks addr_buffer_q in RMW

  for (genvar vreg = 0; vreg < NrWords; vreg++) begin : gen_write_mem
    logic wr_en;
    assign wr_en = bank_req & bank_we & (bank_addr == addr_t'(vreg));

    always_ff @(posedge clk_i or negedge rst_ni) begin
      if (!rst_ni)        mem[vreg] <= '0;
      else if (wr_en)     mem[vreg] <= bank_wdata;
    end
  end : gen_write_mem

  // -------------------------------------------------------------------------
  // Read ports – each port gets its own decoder
  // -------------------------------------------------------------------------
  for (genvar port = 0; port < NrReadPorts; port++) begin : gen_read_ports
    logic [ProtectedWidth-1:0] raw_rdata;
    logic [1:0]                port_ecc_err;

    // Combinational read
    assign raw_rdata = mem[raddr_i[port]];

    hsiao_ecc_dec #(
      .DataWidth (UnprotectedWidth),
      .ProtWidth (ProtWidth)
    ) i_rd_ecc_dec (
      .in         (raw_rdata    ),
      .out        (rdata_o[port]),
      .syndrome_o (             ),
      .err_o      (port_ecc_err )
    );

    assign single_error_o[port] = port_ecc_err[0];
    assign multi_error_o [port] = port_ecc_err[1];
  end : gen_read_ports

`ifndef TARGET_SYNTHESIS
  always @(posedge clk_i) begin
    for (int p = 0; p < NrReadPorts; p++) begin
      if (single_error_o[p])
        $display("[ECC vregfile] %t port %0d - single-bit error corrected", $realtime, p);
      if (multi_error_o[p])
        $display("[ECC vregfile] %t port %0d - multi-bit error detected", $realtime, p);
    end
  end
`endif

endmodule : ecc_vregfile