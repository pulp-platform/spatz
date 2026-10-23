// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Authors: Sergio Mazzola <smazzola@iis.ee.ethz.ch>
//          Cyrill Durrer <cdurrer@iis.ee.ethz.ch>
//
// Adapted from `konark_tcdm_aligner` for the Spatz cluster.


module spatz_tcdm_aligner
  import hci_package::*;
#(
  parameter type         tcdm_req_t    = logic,
  parameter type         tcdm_rsp_t    = logic,
  parameter int unsigned DATA_WIDTH     = 512,   // Data width of the (HCI) access to align [bits]
  parameter int unsigned SB_WIDTH       = 256,   // Superbank data width [bits]
  parameter int unsigned WORD_WIDTH     = 64,
  parameter int unsigned ADDR_WIDTH     = 48,
  parameter int unsigned MISALIGNED_ACCESSES = 1,
  // Dependent parameters: do not modify!
  localparam int unsigned ByteWidth = 8,
  localparam int unsigned DataWidthActual = (MISALIGNED_ACCESSES == 1) ? DATA_WIDTH - WORD_WIDTH : DATA_WIDTH,
  // The HWPE accesses are word-aligned (WORD_WIDTH): the access can start at most
  // (SbBytes - WordBytes) bytes into a superbank, hence it covers at most
  // DataBytes + SbBytes - WordBytes bytes counted from the superbank boundary.
  localparam int unsigned DataBytes = DATA_WIDTH / ByteWidth,
  localparam int unsigned SbBytes   = SB_WIDTH / ByteWidth,
  localparam int unsigned WordBytes = WORD_WIDTH / ByteWidth,
  localparam int unsigned NumPlugs = (MISALIGNED_ACCESSES == 1) ? ((DataBytes + SbBytes - WordBytes) + SbBytes - 1) / SbBytes
                                                                : (DataBytes + SbBytes - 1) / SbBytes,
  localparam int unsigned RemainingBytes = (DATA_WIDTH / ByteWidth) - ((NumPlugs - 1) * (SB_WIDTH / ByteWidth)),
  localparam int unsigned SbAddrBits = $clog2(SB_WIDTH / ByteWidth),    // SB_WIDTH has to be power of 2!
  localparam logic [ADDR_WIDTH-1:0] SbAddrMask = {
    {ADDR_WIDTH-SbAddrBits{1'b1}},
    {SbAddrBits{1'b0}}
  }
) (
  input  logic          clk_i,
  input  logic          rst_ni,
  hci_core_intf         tcdm_misaligned,
  output tcdm_req_t [NumPlugs-1:0]  tcdm_req_aligned_o,
  input  tcdm_rsp_t [NumPlugs-1:0]  tcdm_rsp_aligned_i
);

  logic [SbAddrBits-1:0] sb_offset_d, sb_offset_q;
  logic [SbAddrBits-1:0] req_offset_bytes;
  logic [ADDR_WIDTH-1:0] req_base_addr;
  logic [NumPlugs * (SB_WIDTH / ByteWidth)-1:0] req_strb_aligned;
  logic [NumPlugs * SB_WIDTH-1:0] req_data_aligned;
  logic [NumPlugs*SB_WIDTH-1:0] aligned_rsp_data;
  logic [NumPlugs-1:0] aligned_q_ready;


  assign sb_offset_d = (tcdm_misaligned.req && tcdm_misaligned.gnt) ? tcdm_misaligned.add[SbAddrBits-1:0] : sb_offset_q;
  assign req_base_addr = (MISALIGNED_ACCESSES == 1) ? (tcdm_misaligned.add & SbAddrMask)
                                                     : tcdm_misaligned.add;

  generate
    for (genvar plug = 0; plug < NumPlugs; plug++) begin : gen_aligner_tcdm_if
      localparam int unsigned PlugByteOffset = plug * (SB_WIDTH / ByteWidth);
      localparam int unsigned PlugBitOffset  = plug * SB_WIDTH;

      assign tcdm_req_aligned_o[plug].q_valid = tcdm_misaligned.req;
      assign tcdm_req_aligned_o[plug].q.addr  = req_base_addr + PlugByteOffset;
      assign tcdm_req_aligned_o[plug].q.write = ~tcdm_misaligned.wen;
      assign tcdm_req_aligned_o[plug].q.strb  = req_strb_aligned[PlugByteOffset +: (SB_WIDTH / ByteWidth)];
      assign tcdm_req_aligned_o[plug].q.data  = req_data_aligned[PlugBitOffset +: SB_WIDTH];
      assign tcdm_req_aligned_o[plug].q.amo   = reqrsp_pkg::AMONone;
      assign tcdm_req_aligned_o[plug].q.user  = '0;
    end
  endgenerate

  assign req_offset_bytes = (MISALIGNED_ACCESSES == 1) ? sb_offset_d : '0;

  always_comb begin
    req_strb_aligned = '0;
    req_data_aligned = '0;

    for (int unsigned src_byte = 0; src_byte < (DATA_WIDTH / ByteWidth); src_byte++) begin
      if ((req_offset_bytes + src_byte) < (NumPlugs * (SB_WIDTH / ByteWidth))) begin
        req_strb_aligned[req_offset_bytes + src_byte] = tcdm_misaligned.be[src_byte];
        req_data_aligned[((req_offset_bytes + src_byte) * ByteWidth) +: ByteWidth] =
          tcdm_misaligned.data[(src_byte * ByteWidth) +: ByteWidth];
      end
    end
  end

  generate
    for (genvar PlugIdxCnt = 0; PlugIdxCnt < NumPlugs; PlugIdxCnt++) begin : gen_aligned_rsp_data
      assign aligned_rsp_data[(PlugIdxCnt * SB_WIDTH) +: SB_WIDTH] =
        tcdm_rsp_aligned_i[PlugIdxCnt].p.data[SB_WIDTH-1:0];
      assign aligned_q_ready[PlugIdxCnt] = tcdm_rsp_aligned_i[PlugIdxCnt].q_ready;
    end

    for (genvar ByteIdx = 0; ByteIdx < (DATA_WIDTH / ByteWidth); ByteIdx++) begin : gen_r_data_bytes
      if (MISALIGNED_ACCESSES == 0) begin : gen_r_data_aligned
        assign tcdm_misaligned.r_data[(ByteIdx * ByteWidth) +: ByteWidth] =
          aligned_rsp_data[(ByteIdx * ByteWidth) +: ByteWidth];
      end else begin : gen_r_data_misaligned
        assign tcdm_misaligned.r_data[(ByteIdx * ByteWidth) +: ByteWidth] =
          aligned_rsp_data[((ByteIdx + sb_offset_q) * ByteWidth) +: ByteWidth];
      end
    end
  endgenerate

  // Request can get granted only when all plugs are ready at the same time.
  assign tcdm_misaligned.gnt = &aligned_q_ready;
  // p_valid should come at the next cycle after the handshake, at the same time on all plugs
  // if this does not happen, something is wrong somewhere else, and this will break
  assign tcdm_misaligned.r_valid  = tcdm_rsp_aligned_i[0].p_valid;
  assign tcdm_misaligned.r_opc    = '0;
  assign tcdm_misaligned.r_user   = '0;
  assign tcdm_misaligned.r_id     = '0;
  assign tcdm_misaligned.r_ecc    = '0;
  assign tcdm_misaligned.egnt     = '1;
  assign tcdm_misaligned.r_evalid = '0;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni)
      sb_offset_q <= '0;
    else
      sb_offset_q <= sb_offset_d;
  end

endmodule
