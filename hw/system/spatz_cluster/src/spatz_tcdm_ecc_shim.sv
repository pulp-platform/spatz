// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

`include "common_cells/registers.svh"

/// ECC encode/decode shim for a TCDM port, with RMW support for partial writes.
///
/// Sits between a plain DataWidth-bit TCDM initiator (e.g. axi_to_tcdm) and an
/// ECC-protected ProtDataWidth-bit TCDM interconnect.
///
///   Reads:          Forward request; decode ProtDataWidth-bit response to DataWidth bits.
///   Full writes:    Encode DataWidth-bit data to ProtDataWidth codeword; forward with strb='1.
///   Partial writes: RMW — read existing codeword → decode → byte-merge → re-encode → write full.
///
/// FSM:
///   NORMAL    — transparent pass-through
///   RMW_REQ   — issuing internal read at the write address until TCDM grants it
///   RMW_WAIT  — read granted; waiting for SRAM response
///   RMW_WRITE — issuing encoded full-word write-back
module spatz_tcdm_ecc_shim #(
  /// Unprotected data width (e.g. 32).
  parameter int unsigned DataWidth     = 32,
  /// ECC-protected codeword width (e.g. 39 = 32 + 7 Hsiao parity bits).
  parameter int unsigned ProtDataWidth = 39,
  /// TCDM request type (must carry a ProtDataWidth-bit data field).
  parameter type         tcdm_req_t    = logic,
  /// TCDM response type.
  parameter type         tcdm_rsp_t    = logic
) (
  input  logic      clk_i,
  input  logic      rst_ni,
  /// Upstream port — plain DataWidth-bit initiator (data field valid in [DataWidth-1:0]).
  input  tcdm_req_t in_req_i,
  output tcdm_rsp_t in_rsp_o,
  /// Downstream port — ECC-protected interconnect (ProtDataWidth-bit codewords).
  output tcdm_req_t out_req_o,
  input  tcdm_rsp_t out_rsp_i
);

  localparam int unsigned StrbWidth = DataWidth / 8;
  localparam int unsigned ProtWidth = ProtDataWidth - DataWidth; // parity-bit count (e.g. 7)

  // ---------------------------------------------------------------------------
  // ECC instances (combinational, always active)
  // ---------------------------------------------------------------------------
  logic [DataWidth-1:0]     enc_in;
  logic [ProtDataWidth-1:0] enc_out;
  logic [DataWidth-1:0]     dec_out;
  logic [DataWidth-1:0]     rmw_merged;
  logic [ProtDataWidth-1:0] rmw_enc_out;

  // Encoder for full-word writes (enc_in driven from upstream write data)
  hsiao_ecc_enc #(.DataWidth(DataWidth), .ProtWidth(ProtWidth)) i_enc (
    .in  (enc_in ),
    .out (enc_out)
  );

  // Decoder for downstream read responses
  hsiao_ecc_dec #(.DataWidth(DataWidth), .ProtWidth(ProtWidth)) i_dec (
    .in        (out_rsp_i.p.data[ProtDataWidth-1:0]),
    .out       (dec_out                            ),
    .syndrome_o(/* unused */                       ),
    .err_o     (/* unused */                       )
  );

  // Encoder for the RMW write-back (input is merged data)
  hsiao_ecc_enc #(.DataWidth(DataWidth), .ProtWidth(ProtWidth)) i_rmw_enc (
    .in  (rmw_merged ),
    .out (rmw_enc_out)
  );

  // ---------------------------------------------------------------------------
  // RMW state
  // ---------------------------------------------------------------------------
  logic [DataWidth-1:0] rmw_old_data_q, rmw_old_data_d; // decoded existing SRAM word
  tcdm_req_t            rmw_req_q,      rmw_req_d;       // saved partial-write request

  // Byte-merge: substitute bytes selected by the saved strobe into the old SRAM data
  always_comb begin
    rmw_merged = rmw_old_data_q;
    for (int b = 0; b < StrbWidth; b++) begin
      if (rmw_req_q.q.strb[b])
        rmw_merged[8*b +: 8] = rmw_req_q.q.data[8*b +: 8];
    end
  end

  // ---------------------------------------------------------------------------
  // FSM
  // ---------------------------------------------------------------------------
  typedef enum logic [1:0] {
    NORMAL,    // transparent pass-through
    RMW_REQ,   // issuing internal read for partial-write RMW
    RMW_WAIT,  // read granted; waiting for SRAM response
    RMW_WRITE  // issuing encoded full-word write-back
  } state_t;

  state_t state_q, state_d;

  always_comb begin
    // Defaults
    state_d        = state_q;
    rmw_req_d      = rmw_req_q;
    rmw_old_data_d = rmw_old_data_q;
    enc_in         = in_req_i.q.data[DataWidth-1:0];

    // Transparent pass-through by default
    out_req_o = in_req_i;
    in_rsp_o  = out_rsp_i;

    // Always present decoded read data to upstream on the response channel
    in_rsp_o.p.data = ProtDataWidth'(dec_out);

    unique case (state_q)

      // ----------------------------------------------------------------------
      NORMAL: begin
        if (in_req_i.q_valid && in_req_i.q.write) begin
          if (&in_req_i.q.strb[StrbWidth-1:0]) begin
            // Full-word write: encode and forward
            out_req_o.q.data = enc_out;
          end else begin
            // Partial write: begin RMW — stall upstream, save request
            rmw_req_d         = in_req_i;
            in_rsp_o.q_ready  = 1'b0;
            out_req_o.q_valid = 1'b0;
            state_d           = RMW_REQ;
          end
        end
        // Reads: transparent; response decoded by the default assignment above
      end

      // ----------------------------------------------------------------------
      RMW_REQ: begin
        // Issue internal read at write address; stall upstream
        in_rsp_o.q_ready  = 1'b0;
        in_rsp_o.p_valid  = 1'b0;
        out_req_o         = rmw_req_q;
        out_req_o.q.write = 1'b0;
        out_req_o.q.amo   = reqrsp_pkg::AMONone;
        out_req_o.q.data  = '0;
        out_req_o.q.strb  = '0;
        out_req_o.q_valid = 1'b1;
        if (out_rsp_i.q_ready)
          state_d = RMW_WAIT;
      end

      // ----------------------------------------------------------------------
      RMW_WAIT: begin
        // Read granted; no new request; wait for SRAM response
        in_rsp_o.q_ready  = 1'b0;
        in_rsp_o.p_valid  = 1'b0;
        out_req_o.q_valid = 1'b0;
        if (out_rsp_i.p_valid) begin
          rmw_old_data_d = dec_out; // capture decoded existing SRAM word
          state_d        = RMW_WRITE;
        end
      end

      // ----------------------------------------------------------------------
      RMW_WRITE: begin
        // Issue encoded full-word write-back; stall upstream
        in_rsp_o.q_ready  = 1'b0;
        in_rsp_o.p_valid  = 1'b0;
        out_req_o         = rmw_req_q;
        out_req_o.q.data  = rmw_enc_out;
        out_req_o.q.strb  = '1;
        out_req_o.q.write = 1'b1;
        out_req_o.q_valid = 1'b1;
        if (out_rsp_i.q_ready) begin
          // Write granted — release upstream for one cycle
          in_rsp_o.q_ready = 1'b1;
          state_d          = NORMAL;
        end
      end

      default: state_d = NORMAL;

    endcase
  end

  `FF(state_q,        state_d,        NORMAL)
  `FF(rmw_req_q,      rmw_req_d,      '0    )
  `FF(rmw_old_data_q, rmw_old_data_d, '0    )

endmodule
