// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich
//
// This generic module provides an interface through which responses can
// be read in order, despite being written out of order. The responses
// must be indexed with an ID that identifies it within the ROB.
//
// TwinROB0 extensions (both default OFF -> bit-identical legacy elaboration):
// - NumWrPorts=2: a second slot-addressed write port (data2/id2/push2), so two
//   response lanes can fill the SAME id space concurrently (2-wide burst receive).
//   The two pushes of one cycle must target different ids (asserted).
// - NumRdPorts=2: a second in-order read head (read_pointer+1) and a dual pop
//   (pop_dual_i pops both heads in one cycle), so a consumer can drain
//   2 words/cycle while reads stay strictly pointer-ordered.
// - BlockWords>1: BLOCK ID RESERVATION (docs/spatz_mlp_design_plan.md §5.1). One
//   id_req_block_i reserves BlockWords consecutive ids in a SINGLE cycle instead of
//   walking them one per cycle. The ROB owns the room check (room_block_o) so a
//   requester bug cannot over-allocate the id space, and block_mask_o exports the
//   granted window for the requester's own per-id bookkeeping. BlockWords==1 is the
//   feature-absent default: every added statement const-folds out.
//
// SPATZ_ROB_CNT_IDVALID (default OFF, docs/spatz_mlp_design_plan.md §5.2 / R1):
// derive id_valid_o from the occupancy counter instead of the id_valid_q free-id
// bitmap. OFF elaborates the legacy bitmap unchanged (bit-identical). ON is a
// deliberate netlist change: it deletes the NumWords bitmap flops, their write-
// pointer decoder, the two NumWords:1 read muxes and write_next_ptr.

module reorder_buffer
  import cf_math_pkg::idx_width;
#(
  parameter int unsigned DataWidth  = 0,
  parameter int unsigned NumWords   = 0,
  parameter bit FallThrough         = 1'b0,
  parameter int unsigned NumWrPorts = 1,
  parameter int unsigned NumRdPorts = 1,
  // Block id reservation width. 1 = feature absent (bit-identical legacy elaboration).
  // When > 1 it must be exactly NumWords/2: the window mask below exploits that identity
  // to reduce "(i - wp) mod NumWords < BlockWords" to a single msb test (checked below).
  parameter int unsigned BlockWords = 1,
  // Dependant parameters. Do not change!
  parameter IdWidth                 = idx_width(NumWords),
  parameter type data_t             = logic [DataWidth-1:0],
  parameter type id_t               = logic [IdWidth-1:0]
) (
  input  logic  clk_i,
  input  logic  rst_ni,
  // Data write
  input  data_t data_i,
  input  id_t   id_i,
  input  logic  push_i,
  // Second data write port (used when NumWrPorts > 1; tie off otherwise)
  input  data_t data2_i,
  input  id_t   id2_i,
  input  logic  push2_i,
  // Data read
  output data_t data_o,
  output logic  valid_o,
  output id_t   id_read_o,
  input  logic  pop_i,
  // Second in-order read head + dual pop (used when NumRdPorts > 1)
  output data_t data2_o,
  output logic  valid2_o,
  input  logic  pop_dual_i,
  // ID request
  input  logic  id_req_i,
  output id_t   id_o,
  output logic  id_valid_o,  // is the next id valid?
  output logic  full_o,
  output logic  empty_o,
  // Block ID reservation (used when BlockWords > 1; tie off otherwise). One cycle reserves
  // the whole window [id_o, id_o+BlockWords). room_block_o is the ROB's OWN room guard --
  // block_fire is built from it internally, so the requester cannot bypass it.
  input  logic                id_req_block_i,
  output logic                room_block_o,
  output logic [NumWords-1:0] block_mask_o
);

  /****************
   *  Parameters  *
   ****************/

  // R1: free-id tracking from status_cnt_q instead of the id_valid_q bitmap.
  // 0 = legacy bitmap (default, bit-identical); 1 = counter compare.
  localparam int unsigned CntIdValid =
    `ifdef SPATZ_ROB_CNT_IDVALID `SPATZ_ROB_CNT_IDVALID
    `else 0 `endif;

  // Low-order write-pointer bits used by the block-window compare. With BlockWords ==
  // NumWords/2 this is IdWidth-1. Held at 1 when the feature is absent so the slice stays
  // legal in the (unelaborated) off branch.
  localparam int unsigned BlkLoW = (BlockWords > 1) ? (IdWidth - 1) : 1;

  /*************
   *  Signals  *
   *************/

  id_t              read_pointer_d, read_pointer_q;
  id_t              read_next_ptr;
  id_t              write_pointer_d, write_pointer_q, write_next_ptr;

  // Used to see which ID is available (legacy form only: not instantiated under CntIdValid)
  logic [NumWords-1:0] id_valid_d, id_valid_q;
  // Keep track of the ROB utilization
  logic [IdWidth:0] status_cnt_d, status_cnt_q;

  // Block reservation: the granted window as a bitmap, and the single fire condition that
  // both the pointer update and the counter fixups are built from.
  logic                  block_fire;
  logic [NumWords-1:0]   block_mask;
  // Shared thermometer decode for the window mask: blk_lt[j] = (j < write_pointer_q low bits).
  logic [BlockWords-1:0] blk_lt;

  // Memory
  data_t [NumWords-1:0] mem_d, mem_q;
  logic  [NumWords-1:0] valid_d, valid_q;

  // Status flags
  assign full_o    = (status_cnt_q == NumWords);
  assign empty_o   = (status_cnt_q == 'd0);
  assign id_o      = write_pointer_q;
  assign id_read_o = read_pointer_q;
  assign read_next_ptr  = read_pointer_q + 1;

  // "Are the next two ids free?" (the VLSU burst allocator demands two, see its
  // rob_id_valid use). Ids are allocated and freed strictly in order, so the allocated
  // set is always the contiguous ring [read_pointer_q, write_pointer_q) whose cardinality
  // *is* status_cnt_q (checked by cnt_ptr_coherent below). The bitmap lookup therefore
  // equals the compare below at both boundaries: at cnt == NumWords-1 the only free slot
  // is write_pointer_q itself, so write_next_ptr == read_pointer_q is still allocated -> 0,
  // and (NumWords-1 <= NumWords-2) == 0; at cnt == NumWords (full) both are 0.
  // write_next_ptr has no other consumer, so it is tied off (folds away) under CntIdValid.
  if (CntIdValid) begin : gen_id_valid_cnt
    assign write_next_ptr = '0;
    assign id_valid_o     = (status_cnt_q <= (NumWords - 2));
  end else begin : gen_id_valid_map
    assign write_next_ptr = write_pointer_q + 1;
    assign id_valid_o     = id_valid_q[write_pointer_q] & id_valid_q[write_next_ptr];
  end

  // Room for a WHOLE block. The NON-STRICT <= is load-bearing, not stylistic
  // (docs/spatz_mlp_design_plan.md §4/T3): after the first burst of a two-burst load
  // handshakes, status_cnt_q is EXACTLY NumWords-BlockWords, so a strict < would silently
  // re-serialise the second burst -- and the symptom is "the change did nothing", not a
  // failure. Note this compares against a COMPILE-TIME CONSTANT, never against a requested
  // length: keeping the burst-length cone out of full_o/room_block_o is what holds this at
  // ~2 levels from a flop instead of ~28 (T3).
  assign room_block_o = (BlockWords > 1) ? (status_cnt_q <= (NumWords - BlockWords)) : 1'b0;
  assign block_fire   = (BlockWords > 1) && id_req_block_i && room_block_o;
  assign block_mask_o = block_mask;

  // Window mask: block_mask[i] = 1 iff i is in [wp, wp+BlockWords) mod NumWords. With
  // BlockWords == NumWords/2 that test is exactly "the msb of (i - wp) is 0", i.e.
  //     i[IdWidth-1] ^ wp[IdWidth-1] ^ (i[BlkLoW-1:0] < wp[BlkLoW-1:0]) == 0,
  // so ONE shared thermometer decode of the low pointer bits plus one XOR per bit covers
  // both halves (~2 levels). Deliberately NOT ~({BlockWords{1'b1}} << wp[BlkLoW-1:0]): a
  // variable left shift is a 4-stage barrel shifter, ~160 GE / 4 levels -- 8x the area of
  // this form (timing-reviewer correction C1).
  if (BlockWords > 1) begin : gen_block_mask
    for (genvar j = 0; j < BlockWords; j++) begin : gen_block_mask_bit
      assign blk_lt[j]                  = (BlkLoW'(j) < write_pointer_q[BlkLoW-1:0]);
      assign block_mask[j]              = ~(write_pointer_q[IdWidth-1] ^ blk_lt[j]);
      assign block_mask[j + BlockWords] =  (write_pointer_q[IdWidth-1] ^ blk_lt[j]);
    end : gen_block_mask_bit
  end else begin : gen_no_block_mask
    assign blk_lt     = '0;
    assign block_mask = '0;
  end

  // Read and Write logic
  always_comb begin: read_write_comb
    // Maintain state
    read_pointer_d  = read_pointer_q;
    write_pointer_d = write_pointer_q;
    status_cnt_d    = status_cnt_q;
    mem_d           = mem_q;
    valid_d         = valid_q;
    id_valid_d      = id_valid_q;

    // Output data
    data_o  = mem_q[read_pointer_q];
    valid_o = valid_q[read_pointer_q];
    // Second read head (structurally tied off when NumRdPorts == 1)
    data2_o  = (NumRdPorts > 1) ? mem_q[read_next_ptr]   : '0;
    valid2_o = (NumRdPorts > 1) ? valid_q[read_next_ptr] : 1'b0;

    // Reserve a whole block of ids in ONE cycle. Written as the head of an if / else-if with
    // the single-id request so the two are mutually exclusive BY CONSTRUCTION rather than by
    // an external promise (A4): if a requester ever asserts both, the block wins here and in
    // every counter fixup below, consistently.
    if (block_fire) begin
      // NumWords is a power of two whenever BlockWords > 1 (checked below), so id_t
      // arithmetic wraps naturally; with BlockWords == NumWords/2 this is one inverter on
      // the pointer msb, cheaper than the +1 incrementer it parallels.
      write_pointer_d = id_t'(write_pointer_q + BlockWords);
      // The whole window becomes allocated at once (bitmap removed under CntIdValid).
      if (!CntIdValid) id_valid_d = id_valid_q & ~block_mask;
      status_cnt_d = status_cnt_q + BlockWords;
    // Request an ID.
    end else if (id_req_i && !full_o) begin
      // Increment the write pointer
      if (write_pointer_q == NumWords-1) begin
        write_pointer_d = 0;
      end else begin
        write_pointer_d = write_pointer_q + 1;
      end
      // Bitmap decoder (write side): removed under CntIdValid.
      if (!CntIdValid) id_valid_d[write_pointer_q] = 1'b0;
      // Increment the overall counter
      status_cnt_d = status_cnt_q + 1;
    end

    // Push data
    if (push_i) begin
      mem_d[id_i]   = data_i;
      valid_d[id_i] = 1'b1;
    end

    // Second slot-addressed write port
    if ((NumWrPorts > 1) && push2_i) begin
      mem_d[id2_i]   = data2_i;
      valid_d[id2_i] = 1'b1;
    end

    // ROB is in fall-through mode -> do not change the pointers
    if (FallThrough && push_i && (id_i == read_pointer_q)) begin
      data_o  = data_i;
      valid_o = 1'b1;
      if (pop_i) begin
        valid_d[id_i] = 1'b0;
      end
    end

    // Pop data
    if (pop_i && valid_o) begin
      // Word was consumed
      valid_d[read_pointer_q] = 1'b0;
      // Mark ID as available (bitmap decoder, read side: removed under CntIdValid)
      if (!CntIdValid) id_valid_d[read_pointer_q] = 1'b1;

      // Increment the read pointer
      if (read_pointer_q == NumWords-1)
        read_pointer_d = '0;
      else
        read_pointer_d = read_pointer_q + 1;
      // Decrement the overall counter
      status_cnt_d = status_cnt_q - 1;
    end

    // Keep the overall counter stable if we request new ID and pop at the same time
    if ((id_req_i && !full_o) && (pop_i && valid_o)) begin
      status_cnt_d = status_cnt_q;
    end

    // Same fixup for a block reservation coincident with a pop: +BlockWords from the block,
    // -1 from the pop. Placed AFTER the single-id fixup so the block wins if both were
    // somehow requested -- the same priority the allocation above uses. The popped id can
    // never lie inside the window (room_block_o bounds the occupancy so read_pointer_q sits
    // at or beyond wp+BlockWords), and the pop's id_valid_d set below runs later anyway.
    if (block_fire && (pop_i && valid_o)) begin
      status_cnt_d = status_cnt_q + BlockWords - 1;
    end

    // Dual pop: consume both read heads in one cycle (strictly pointer-ordered).
    // id_t arithmetic wraps naturally (power-of-two NumWords enforced below).
    if ((NumRdPorts > 1) && pop_dual_i && valid_o && valid2_o) begin
      valid_d[read_pointer_q]    = 1'b0;
      valid_d[read_next_ptr]     = 1'b0;
      // read_next_ptr itself stays: it also feeds valid_d / data2_o above.
      if (!CntIdValid) begin
        id_valid_d[read_pointer_q] = 1'b1;
        id_valid_d[read_next_ptr]  = 1'b1;
      end
      read_pointer_d             = id_t'(read_pointer_q + 2);
      status_cnt_d               = status_cnt_q - 2;
      if (id_req_i && !full_o) begin
        status_cnt_d = status_cnt_q - 1;
      end
      // Block reservation coincident with a dual pop: +BlockWords - 2. Last, so it wins over
      // the single-id variant above, matching the allocation priority.
      if (block_fire) begin
        status_cnt_d = status_cnt_q + BlockWords - 2;
      end
    end
  end: read_write_comb

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      read_pointer_q  <= '0;
      write_pointer_q <= '0;
      status_cnt_q    <= '0;
      mem_q           <= '0;
      valid_q         <= '0;
    end else begin
      read_pointer_q  <= read_pointer_d;
      write_pointer_q <= write_pointer_d;
      status_cnt_q    <= status_cnt_d;
      mem_q           <= mem_d;
      valid_q         <= valid_d;
    end
  end

  // Legacy free-id bitmap flops. Under CntIdValid nothing reads id_valid_q (id_valid_o
  // is derived from status_cnt_q), so the NumWords flops are not instantiated at all;
  // the constant tie-off keeps the net driven for lint/waveforms and const-folds away.
  if (CntIdValid) begin : gen_id_valid_ff_none
    assign id_valid_q = '1;
  end else begin : gen_id_valid_ff
    always_ff @(posedge clk_i or negedge rst_ni) begin
      if (!rst_ni) begin
        // By default, all IDs are available
        id_valid_q <= '1;
      end else begin
        id_valid_q <= id_valid_d;
      end
    end
  end

  /****************
   *  Assertions  *
   ****************/

  if (NumWords == 0)
    $error("NumWords cannot be 0.");
  if ((NumWrPorts != 1) && (NumWrPorts != 2))
    $error("NumWrPorts must be 1 or 2.");
  if ((NumRdPorts != 1) && (NumRdPorts != 2))
    $error("NumRdPorts must be 1 or 2.");
  if ((NumRdPorts > 1) && (NumWords != 2**IdWidth))
    $error("NumRdPorts=2 requires power-of-two NumWords (pointer +2 wrap).");
  if (((NumWrPorts > 1) || (NumRdPorts > 1)) && FallThrough)
    $error("FallThrough is not supported with the TwinROB0 extensions.");
  if ((BlockWords != 1) && (BlockWords != NumWords/2))
    $error("BlockWords must be 1 (off) or NumWords/2: the window mask uses that identity.");
  if ((BlockWords > 1) && (NumWords != 2**IdWidth))
    $error("BlockWords > 1 requires power-of-two NumWords (write pointer + BlockWords wrap).");
  if ((BlockWords > 1) && FallThrough)
    $error("FallThrough is not supported with block ID reservation.");

  `ifndef VERILATOR
  // pragma translate_off
  full_write : assert property(
      @(posedge clk_i) disable iff (!rst_ni) (full_o |-> !id_req_i))
  else $fatal (1, "Trying to request an ID although the ROB is full.");

  empty_read : assert property(
      @(posedge clk_i) disable iff (!rst_ni) (!valid_o |-> !pop_i))
  else $fatal (1, "Trying to pop data although the top of the ROB is not valid.");

  // A pop must never underflow status_cnt_q. valid_o is valid_q[read_pointer_q],
  // which is NOT the same as !empty_o: a valid head with status_cnt_q == 0 (a push
  // landing outside the allocated [read_pointer_q, write_pointer_q) window) makes
  // status_cnt_d wrap to 2**(IdWidth+1)-1, after which full_o and empty_o never
  // assert again and every later id request / drain misbehaves. empty_read above
  // does not cover this case (it only relates pop_i to valid_o).
  pop_no_underflow : assert property(
      @(posedge clk_i) disable iff (!rst_ni) ((pop_i || pop_dual_i) |-> !empty_o))
  else $fatal (1, "ROB pop while empty: status_cnt_q would underflow.");

  // A7: the invariant the CntIdValid form of id_valid_o rests on -- ids are allocated
  // and freed strictly in order, so the occupancy counter and the (write - read) ring
  // distance always agree mod NumWords, and the counter never exceeds NumWords. Armed
  // in both elaborations (sim-only): it validates the legacy build before the knob is
  // turned on, and it also catches the status_cnt_q wrap of pop_no_underflow above.
  if (NumWords == 2**IdWidth) begin : gen_cnt_ptr_assert
    cnt_ptr_coherent : assert property(
        @(posedge clk_i) disable iff (!rst_ni)
        ((status_cnt_q <= NumWords) &&
         (id_t'(status_cnt_q) == id_t'(write_pointer_q - read_pointer_q))))
    else $fatal (1, "status_cnt_q incoherent with the (write - read) pointer distance.");
  end

  if (BlockWords > 1) begin : gen_block_asserts
    // A3: the block may only ever fire on the ROOM check (cnt <= NumWords-BlockWords), never
    // on full_o (cnt <= NumWords-1). Gating it on full_o instead would let status_cnt_d reach
    // NumWords+BlockWords-1 WITHOUT wrapping, double-allocating BlockWords-1 slots -- wrong
    // data with no other symptom. Tautological against the assign above BY DESIGN: it is the
    // tripwire for a future edit that rewrites block_fire.
    blk_room : assert property(
        @(posedge clk_i) disable iff (!rst_ni) (block_fire |-> room_block_o))
    else $fatal (1, "Block reservation fired without room for a whole block.");
    blk_no_overflow : assert property(
        @(posedge clk_i) disable iff (!rst_ni) (block_fire |-> (status_cnt_d <= NumWords)))
    else $fatal (1, "Block reservation overflows the ROB occupancy counter.");
    // A4: block and single id request are mutually exclusive. The allocation gives the block
    // priority, so a coincident single request is silently dropped -- and its requester then
    // uses an id the ROB never handed out.
    blk_single_exclusive : assert property(
        @(posedge clk_i) disable iff (!rst_ni) (!(id_req_block_i && id_req_i)))
    else $fatal (1, "Block and single ID request asserted in the same cycle.");
  end

  if (NumWrPorts > 1) begin : gen_wr2_asserts
    wr2_id_collision : assert property(
        @(posedge clk_i) disable iff (!rst_ni) ((push_i && push2_i) |-> (id_i != id2_i)))
    else $fatal (1, "Both write ports pushing the same id in one cycle.");
  end

  if (NumRdPorts > 1) begin : gen_rd2_asserts
    dual_pop_valid : assert property(
        @(posedge clk_i) disable iff (!rst_ni) (pop_dual_i |-> (valid_o && valid2_o)))
    else $fatal (1, "Dual pop without both read heads valid.");
    pop_exclusive : assert property(
        @(posedge clk_i) disable iff (!rst_ni) (!(pop_i && pop_dual_i)))
    else $fatal (1, "pop_i and pop_dual_i asserted together.");
  end
  // pragma translate_on
  `endif

endmodule: reorder_buffer
