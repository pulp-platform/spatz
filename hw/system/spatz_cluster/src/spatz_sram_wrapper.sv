// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Diyou Shen <dishen@iis.ee.ethz.ch>

`include "mem_interface/typedef.svh"

/// tc_sram wrapper for hybrid cache/spm access
module spatz_sram_wrapper #(
  /// Number of Banks in a Wrapper.
  parameter int unsigned NumBanks              = 32'd0,
  /// Number of Words for a Cache Slice
  parameter int unsigned NumWords              = 32'd0,
  /// Byte Width of a Word
  parameter int unsigned ByteWidth             = 32'd8,
  /// Data size of the interconnect. Only the data portion counts. The offsets
  /// into the address are derived from this.
  parameter int unsigned DataWidth             = 32,
  /// Latency of memory response (in cycles)
  parameter int unsigned MemoryResponseLatency = 1,

  // Dependent parameters, do not overwrite
  parameter int unsigned MemAddrWidth          = (NumWords > 32'd1) ? $clog2(NumWords) : 32'd1,
  /// Address width of each bank
  parameter int unsigned BankAddrWidth         = MemAddrWidth - $clog2(NumBanks),
  /// Address type of input (wrapper bank)
  parameter type mem_addr_t                    = logic [MemAddrWidth-1:0],
  /// Address type of data banks
  parameter type bank_addr_t                   = logic [BankAddrWidth-1:0],
  /// Data type
  parameter type data_t                        = logic [DataWidth-1:0],
  /// Bank select type
  parameter type select_t                      = logic [$clog2(NumBanks)-1:0],
  /// Byte enable type
  parameter type be_t                          = logic [ByteWidth-1:0]
) (
  /// Clock, positive edge triggered.
  input  logic                                 clk_i,
  /// Reset, active low.
  input  logic                                 rst_ni,
  /// SPM Size, used to saperate SPM and Cache in the bank
  input  bank_addr_t                           spm_size_i,
  /// Cache Ports
  input  logic                [NumBanks-1:0]   cache_req_i,
  input  logic                [NumBanks-1:0]   cache_we_i,
  input  bank_addr_t          [NumBanks-1:0]   cache_addr_i,
  input  data_t               [NumBanks-1:0]   cache_wdata_i,
  input  be_t                 [NumBanks-1:0]   cache_be_i,
  output data_t               [NumBanks-1:0]   cache_rdata_o,
  output logic                [NumBanks-1:0]   cache_ready_o,
  /// SPM Ports
  input  logic                                 spm_req_i,
  input  logic                                 spm_we_i,
  input  mem_addr_t                            spm_addr_i,
  input  data_t                                spm_wdata_i,
  input  be_t                                  spm_be_i,
  output data_t                                spm_rdata_o
);
  typedef struct packed {
    logic       we;
    logic [BankAddrWidth:0] addr;
    data_t      data;
    be_t        be;
  } bank_req_t;

  localparam int unsigned SPM   = 1;
  localparam int unsigned CACHE = 0;

  bank_req_t  [NumBanks-1:0] spm_bank_req,   cache_bank_req,   bank_req;
  logic       [NumBanks-1:0] spm_bank_valid, cache_bank_valid, bank_valid;
  data_t      [NumBanks-1:0] bank_rdata;

  // We also need to store the selection for one cycle for rdata forwarding
  logic       [NumBanks-1:0] arb_select, arb_select_d, arb_select_q;
  select_t    bank_select, bank_select_d, bank_select_q;

  // Determine the bank spm requests will be sent to
  assign      bank_select = spm_addr_i[$clog2(NumBanks)-1:0];
  assign      bank_select_d = bank_select;

  always_comb begin
    spm_bank_req   = '0;
    spm_bank_valid = '0;

    // The selected bank will got the input
    spm_bank_valid[bank_select]    = spm_req_i;

    spm_bank_req[bank_select].we   = spm_we_i;
    spm_bank_req[bank_select].addr = {1'b0,spm_addr_i[MemAddrWidth-1:$clog2(NumBanks)]};
    spm_bank_req[bank_select].data = spm_wdata_i;
    spm_bank_req[bank_select].be   = spm_be_i;

    // spm_rdata_o = spm_bank_rdata[bank_select];

    for (int i = 0; i < NumBanks; i++) begin
      cache_bank_valid[i]    = cache_req_i[i];

      cache_bank_req[i].we   = cache_we_i[i];
      // Add address offset to cache requests
      // cache_bank_req[i].addr = cache_addr_i[i] + spm_size_i;
      cache_bank_req[i].addr = {1'b1, cache_addr_i[i]};
      cache_bank_req[i].data = cache_wdata_i[i];
      cache_bank_req[i].be   = cache_be_i[i];
    end
  end

  for (genvar i = 0; i < NumBanks; i ++) begin : gen_banks
    always_comb begin
      arb_select[i]      = SPM;
      cache_ready_o[i]   = 1'b1;

      if (spm_bank_valid[i]) begin
        // SPM always has priority because it assumes a fixed latency access
        // CACHE can stall and wait based on HS
        arb_select[i]    = SPM;
        cache_ready_o[i] = 1'b0;
      end else if (cache_bank_valid[i]) begin
        arb_select[i]    = CACHE;
        cache_ready_o[i] = 1'b1;
      end

      arb_select_d[i] = arb_select[i];
    end

    // Select between SPM and Cache
    rr_arb_tree #(
      .NumIn     ( 2              ),
      .DataType  ( bank_req_t     ),
      .ExtPrio   ( 1'b1           ),
      .AxiVldRdy ( 1'b1           ),
      .LockIn    ( 1'b0           )
    ) i_spm_cache_mux (
      .clk_i   ( clk_i                                    ),
      .rst_ni  ( rst_ni                                   ),
      .flush_i ( '0                                       ),
      .rr_i    ( arb_select[i]                            ),
      .req_i   ( {spm_bank_valid[i], cache_bank_valid[i]} ),
      .gnt_o   (                                          ),
      .data_i  ( {spm_bank_req[i],   cache_bank_req[i]}   ),
      .req_o   ( bank_valid[i]                            ),
      .gnt_i   ( 1'b1                                     ),
      .data_o  ( bank_req[i]                              ),
      .idx_o   ( /* not used */                           )
    );

    // Forward to bank
    tc_sram #(
      .NumWords  (2*NumWords/NumBanks),
      .DataWidth (DataWidth        ),
      .ByteWidth (ByteWidth        ),
      .NumPorts  (1                ),
      .Latency   (MemoryResponseLatency ),
      .SimInit   ("zeros"          )
    ) i_data_bank (
      .clk_i  (clk_i            ),
      .rst_ni (rst_ni           ),
      .req_i  (bank_valid[i]    ),
      .we_i   (bank_req[i].we   ),
      .addr_i (bank_req[i].addr ),
      .wdata_i(bank_req[i].data ),
      .be_i   (bank_req[i].be   ),
      .rdata_o(bank_rdata[i]    )
    );
  end

  // Store ARB info for one cycle to wire response
  shift_reg #(
    .dtype ( logic[NumBanks-1:0]   ),
    .Depth ( MemoryResponseLatency )
  ) i_arb_reg (
    .clk_i,
    .rst_ni,
    .d_i ( arb_select_d ),
    .d_o ( arb_select_q )
  );

  shift_reg #(
    .dtype ( select_t              ),
    .Depth ( MemoryResponseLatency )
  ) i_sel_reg (
    .clk_i,
    .rst_ni,
    .d_i ( bank_select_d ),
    .d_o ( bank_select_q )
  );

  always_comb begin
    cache_rdata_o = '0;
    spm_rdata_o   = '0;

    for (int i = 0; i < NumBanks; i++) begin
      if (arb_select_q[i] == CACHE) begin
        cache_rdata_o[i] = bank_rdata[i];
      end
    end

    spm_rdata_o = bank_rdata[bank_select_q];
  end
endmodule
