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
  // EXTERNAL id width. 0 = derive from NumWords, which is what this module did unconditionally
  // until 2026-08-28 and which makes the id space exactly the entry count. That is safe only
  // while every ROB in the design is the same depth. Once ROB1-3 were shrunk to 16 entries the
  // id narrowed to 4 bits with it, so an id was recycled after 16 allocations instead of 64 --
  // and a late DUPLICATE response from the memory side (the MSHR can emit one) then landed in
  // whatever request had since taken that number. Measured: orphan=37 dup_alloc=37 on the
  // shallow ports, 0/0 with 64-deep ports, and the kernel wedges.
  //
  // Set this to the id width the rest of the design already carries -- snitch_pkg::MetaIdWidth
  // is idx_width(RobDepth) and does NOT shrink with RobNDepth, so the wire is already 6-7 bits.
  // The extra high bits become a GENERATION tag: storage stays NumWords, but an id is not
  // reused until the generation also wraps, and a stale response is detected and dropped.
  parameter int unsigned IdWidthExt = 0,
  // Dependant parameters. Do not change!
  parameter IdWidth                 = (IdWidthExt > idx_width(NumWords))
                                        ? IdWidthExt : idx_width(NumWords),
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

  // Split point of every id for the window compare: i = {i_hi, i_lo} at bit log2(BlockWords).
  // (IdWidth-1 coincides with idx_width(BlockWords) only when BlockWords == NumWords/2; at
  // NumWords=64/BlockWords=16 the window is a QUARTER of the ring and the split moves to bit 4.)
  // Held at 1 when the feature is absent so the slice stays legal in the off branch.
  localparam int unsigned BlkLoW = (BlockWords > 1) ? idx_width(BlockWords) : 1;
  // Width of the high ("quadrant") part of an id above the BlkLoW split.
  localparam int unsigned QSelW  = (BlockWords > 1) ? (IdWidth - BlkLoW) : 1;

  /*************
   *  Signals  *
   *************/

  // Entry addressing is SEPARATE from the id now. entry_t indexes the storage (always
  // NumWords); id_t is what crosses the port boundary and may carry GenBits more.
  localparam int unsigned EntryAw = idx_width(NumWords);
  localparam int unsigned GenBits = IdWidth - EntryAw;
  localparam int unsigned GenW    = (GenBits > 0) ? GenBits : 1;   // 0-width regs are awkward
  typedef logic [EntryAw-1:0] entry_t;

  entry_t           read_pointer_d, read_pointer_q;
  entry_t           read_next_ptr;
  entry_t           write_pointer_d, write_pointer_q, write_next_ptr;

  // Generation counter, and the generation each live entry was allocated in. Both collapse to
  // nothing when GenBits == 0, which is the default and keeps the legacy build bit-identical.
  logic [GenW-1:0]                gen_d, gen_q;
  logic [NumWords-1:0][GenW-1:0]  entry_gen_d, entry_gen_q;
  entry_t                         push_entry, push2_entry;
  logic                           push_gen_ok, push2_gen_ok;
  logic [31:0]                    stale_drop_d, stale_drop_q;   // visibility only

  // Used to see which ID is available (legacy form only: not instantiated under CntIdValid)
  logic [NumWords-1:0] id_valid_d, id_valid_q;
  // Keep track of the ROB utilization
  logic [EntryAw:0] status_cnt_d, status_cnt_q;

  // Block reservation: the granted window as a bitmap, and the single fire condition that
  // both the pointer update and the counter fixups are built from.
  logic                  block_fire;
  logic [NumWords-1:0]   block_mask;
  // Shared thermometer decode for the window mask: blk_lt[j] = (j < write_pointer_q low bits).
  logic [BlockWords-1:0] blk_lt;
  // Quadrant decomposition of the write pointer and the per-quadrant equality one-hots.
  // Declared at module level (project convention: no signals inside generate loops).
  logic [BlkLoW-1:0]   wp_lo;
  logic [QSelW-1:0]    wp_hi, wp_hi1;
  logic [2**QSelW-1:0] eq_wp, eq_wp1;

  // Memory
  data_t [NumWords-1:0] mem_d, mem_q;
  logic  [NumWords-1:0] valid_d, valid_q;

  // Status flags
  assign full_o    = (status_cnt_q == NumWords);
  assign empty_o   = (status_cnt_q == 'd0);
  // The id handed out, and the id reported for the read head, both carry the generation so a
  // response can be matched back to the exact allocation rather than merely to the entry.
  if (GenBits > 0) begin : gen_id_with_generation
    assign id_o      = {gen_q, write_pointer_q};
    assign id_read_o = {entry_gen_q[read_pointer_q][GenBits-1:0], read_pointer_q};
  end else begin : gen_id_legacy
    assign id_o      = write_pointer_q;
    assign id_read_o = read_pointer_q;
  end

  // Decompose an incoming id: which entry, and is it the generation that entry currently holds?
  //
  // The compare MUST sit inside a generate. Written as `(GenBits == 0) || (id_i[IdWidth-1:EntryAw]
  // == ...)` it looks safe -- the short circuit can never evaluate the slice when GenBits is 0 --
  // but SystemVerilog ELABORATES both operands regardless of the logical short circuit, and with
  // GenBits an int unsigned, `GenBits-1` underflows to 4294967295. VCS rejects it:
  // Error-[TCF-CVTL] Constant value too large. The `||` guards the runtime, not the elaboration.
  assign push_entry   = id_i[EntryAw-1:0];
  assign push2_entry  = id2_i[EntryAw-1:0];
  // GENERATION FORWARDING -- required, not an optimisation.
  //
  // A STORE allocates and pushes in the SAME cycle (spatz_vlsu.sv:1816-1818 reads the VRF straight
  // into the entry it is allocating). The stamp for that allocation is in entry_gen_d and does not
  // reach entry_gen_q until the next edge, so comparing the incoming id against entry_gen_q
  // compares the NEW generation with the PREVIOUS lap's stored one. On the first store after the
  // ring wraps that is carried_gen=1 vs entry_gen=0 -- which is exactly the 256 identical drops
  // measured (id=16 -> entry=0, carried 1, stored 0, all on a shallow port). Loads never hit this:
  // their push arrives many cycles after allocation.
  //
  // So forward the generation being stamped this cycle. alloc_gen_fwd is the value the entry WILL
  // hold; comparing against it makes a same-cycle allocate+push match, while a genuine late
  // duplicate (older generation, no allocation this cycle) still mismatches and is still dropped.
  logic                alloc_fire;
  logic [NumWords-1:0] alloc_win_mask;
  logic [GenW-1:0]     gen_of_push, gen_of_push2;
  assign alloc_fire = block_fire || (id_req_i && !full_o);
  // The window being allocated THIS cycle, as a bitmap over entries. Deliberately an always_comb
  // and not a function called from a continuous assign: the first version of this fix used
  // `function automatic logic in_alloc_window(entry_t e)` inside `assign gen_of_push = ...`, and
  // the drops SURVIVED it -- with the allocation trace and the drop trace on the SAME edge for the
  // SAME entry, which is precisely the case the window test exists to accept. A continuous
  // assignment builds its sensitivity from the operands of the RHS; the signals a called function
  // reads but does not take as arguments (alloc_fire, block_fire, write_pointer_q) are not
  // reliably among them, so gen_of_push kept the value it had when push_entry last changed --
  // computed while alloc_fire was still 0. always_comb has guaranteed implicit sensitivity to
  // everything it reads, and block_mask is the window the allocator itself uses, so the two can
  // no longer disagree.
  always_comb begin
    alloc_win_mask = '0;
    if (alloc_fire) begin
      if (block_fire) alloc_win_mask = block_mask;
      else            alloc_win_mask[write_pointer_q] = 1'b1;
    end
  end
  if (GenBits > 0) begin : gen_push_generation_check
    assign gen_of_push  = alloc_win_mask[push_entry ] ? gen_q : entry_gen_q[push_entry ][GenBits-1:0];
    assign gen_of_push2 = alloc_win_mask[push2_entry] ? gen_q : entry_gen_q[push2_entry][GenBits-1:0];
    assign push_gen_ok  = (id_i [IdWidth-1:EntryAw] == gen_of_push);
    assign push2_gen_ok = (id2_i[IdWidth-1:EntryAw] == gen_of_push2);
  end else begin : gen_push_generation_none
    // No generation bits: the id IS the entry, exactly as before this change.
    assign push_gen_ok  = 1'b1;
    assign push2_gen_ok = 1'b1;
  end
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

  // Window mask: block_mask[i] = 1 iff i is in [wp, wp+BlockWords) mod NumWords. Split every
  // id at the BlkLoW bit into {i_hi, i_lo}; then
  //   (i - wp) mod NumWords < BlockWords
  //     <=> (i_hi == wp_hi && i_lo >= wp_lo) || (i_hi == wp_hi+1 && i_lo < wp_lo)
  // (wp_hi1 wraps mod 2^QSelW in the QSelW-bit sum, so the window crosses the ring top for
  // free). blk_lt is the shared "i_lo < wp_lo" thermometer decode; its complement gives
  // "i_lo >= wp_lo". At QSelW == 1 (BlockWords == NumWords/2, the legacy 32/16 shape) wp_hi1
  // == ~wp_hi and this reduces BIT-IDENTICALLY to the shipped msb-XOR form:
  //   q=0: (eq?~blk_lt:blk_lt) with eq=~wp4, eq1=wp4 -> ~(wp4 ^ blk_lt)
  //   q=1: (eq?~blk_lt:blk_lt) with eq=wp4, eq1=~wp4 ->  (wp4 ^ blk_lt)
  // i.e. ~(i_hi ^ wp_hi ^ lt) per id. ~3 levels, and OFF the request cone (only consumers are
  // burst_odd_expected bookkeeping and an assertion -- grep-verified). Deliberately NOT a
  // variable left shift: that is a 4-stage barrel shifter, ~160 GE / 4 levels (correction C1).
  if (BlockWords > 1) begin : gen_block_mask
    assign wp_lo  = write_pointer_q[BlkLoW-1:0];
    assign wp_hi  = write_pointer_q[IdWidth-1:BlkLoW];
    assign wp_hi1 = wp_hi + QSelW'(1);
    for (genvar q = 0; q < 2**QSelW; q++) begin : gen_quad_eq
      assign eq_wp[q]  = (wp_hi  == QSelW'(q));
      assign eq_wp1[q] = (wp_hi1 == QSelW'(q));
    end : gen_quad_eq
    for (genvar j = 0; j < BlockWords; j++) begin : gen_block_mask_bit
      assign blk_lt[j] = (BlkLoW'(j) < wp_lo);
    end : gen_block_mask_bit
    for (genvar q = 0; q < 2**QSelW; q++) begin : gen_quad_mask
      for (genvar j = 0; j < BlockWords; j++) begin : gen_quad_mask_bit
        assign block_mask[q*BlockWords + j] = (eq_wp[q]  & ~blk_lt[j]) |
                                              (eq_wp1[q] &  blk_lt[j]);
      end : gen_quad_mask_bit
    end : gen_quad_mask
  end else begin : gen_no_block_mask
    assign wp_lo      = '0;
    assign wp_hi      = '0;
    assign wp_hi1     = '0;
    assign eq_wp      = '0;
    assign eq_wp1     = '0;
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
    gen_d           = gen_q;
    entry_gen_d     = entry_gen_q;
    stale_drop_d    = stale_drop_q;

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
      write_pointer_d = entry_t'(write_pointer_q + BlockWords);
      // The whole window becomes allocated at once (bitmap removed under CntIdValid).
      if (!CntIdValid) id_valid_d = id_valid_q & ~block_mask;
      status_cnt_d = status_cnt_q + BlockWords;
      // Stamp every entry the block just took, and advance the generation if the ring wrapped.
      if (GenBits > 0) begin
        for (int unsigned b = 0; b < BlockWords; b++)
          entry_gen_d[entry_t'(write_pointer_q + b)] = gen_q;
        if ((write_pointer_q + BlockWords) >= NumWords) gen_d = gen_q + 1;
      end
    // Request an ID.
    end else if (id_req_i && !full_o) begin
      // Stamp this entry with the generation it is being allocated in, BEFORE the pointer moves.
      if (GenBits > 0) entry_gen_d[write_pointer_q] = gen_q;
      // Increment the write pointer, advancing the generation on wrap so the next lap's ids are
      // distinguishable from this lap's.
      if (write_pointer_q == NumWords-1) begin
        write_pointer_d = 0;
        if (GenBits > 0) gen_d = gen_q + 1;
      end else begin
        write_pointer_d = write_pointer_q + 1;
      end
      // Bitmap decoder (write side): removed under CntIdValid.
      if (!CntIdValid) id_valid_d[write_pointer_q] = 1'b0;
      // Increment the overall counter
      status_cnt_d = status_cnt_q + 1;
    end

    // Push data. Indexed by the ENTRY, and accepted only if the id's generation still matches
    // the one that entry was allocated in. A mismatch is a late duplicate for an allocation that
    // has already retired -- dropping it is the whole point of this scheme, because writing it
    // would corrupt whatever request now owns the entry.
    if (push_i) begin
      if (push_gen_ok) begin
        mem_d[push_entry]   = data_i;
        valid_d[push_entry] = 1'b1;
      end else begin
        stale_drop_d = stale_drop_q + 1;
      end
    end

    // Second slot-addressed write port
    if ((NumWrPorts > 1) && push2_i) begin
      if (push2_gen_ok) begin
        mem_d[push2_entry]   = data2_i;
        valid_d[push2_entry] = 1'b1;
      end else begin
        stale_drop_d = stale_drop_q + 1;
      end
    end

    // ROB is in fall-through mode -> do not change the pointers
    if (FallThrough && push_i && push_gen_ok && (push_entry == read_pointer_q)) begin
      data_o  = data_i;
      valid_o = 1'b1;
      if (pop_i) begin
        valid_d[push_entry] = 1'b0;
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
      gen_q           <= '0;
      entry_gen_q     <= '0;
      stale_drop_q    <= '0;
    end else begin
      read_pointer_q  <= read_pointer_d;
      write_pointer_q <= write_pointer_d;
      status_cnt_q    <= status_cnt_d;
      mem_q           <= mem_d;
      valid_q         <= valid_d;
      gen_q           <= gen_d;
      entry_gen_q     <= entry_gen_d;
      stale_drop_q    <= stale_drop_d;
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
  // BlockWords must be a power-of-two PROPER divisor of NumWords (16/32 and 16/64 verified):
  // the window mask splits each id at log2(BlockWords) and reduces bit-identically to the
  // legacy msb-XOR form when BlockWords == NumWords/2.
  if ((BlockWords != 1) &&
      !((BlockWords > 1) && (NumWords % BlockWords == 0) && (BlockWords == 2**$clog2(BlockWords))))
    $error("BlockWords must be 1 (off) or a power-of-two divisor of NumWords.");
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

`ifndef TARGET_SYNTHESIS
  // PER-DROP TRACE. The end-of-simulation count told us 86 responses went missing but not WHICH,
  // and a starved run never reaches that final block at all. Print the id, the entry it decodes
  // to, the generation carried against the generation the entry holds, and the cycle -- so a
  // dropped response can be matched to the allocation it belonged to instead of inferred.
  //
  // GENERATE, not a runtime `if`. The first version of this block read
  //     if (rst_ni && (GenBits > 0)) ... id_i[IdWidth-1:EntryAw] ... [GenBits-1:0]
  // which looks guarded and is not: a runtime condition does not stop ELABORATION, so the slices
  // are still built, and port 0 always has GenBits == 0 (its NumWords == NrOutstandingLoads), so
  // GenBits-1 underflows to 4294967295 and VCS rejects it -- Error-[TCF-CVTL]. This is the SAME
  // trap already documented at the push_gen_ok assigns above; it was walked into a second time in
  // this file. Only a generate `if` removes the code from elaboration.
  // ALLOCATION TRACE. The drop trace says a response arrived carrying generation 1 for an entry
  // holding generation 0 -- i.e. the entry was NOT re-stamped when it was reallocated on the second
  // lap. Reasoning about the allocation path has produced two wrong answers already; this logs every
  // event that can move entry_gen or the pointer, so a drop can be matched against the exact
  // allocation history of its entry instead of inferred from the RTL.
  // Verbose: every allocation, pop and generation advance. Opt in with
  // extra_vlog_defs=-DROB_GEN_TRACE when a drop needs to be matched against the exact
  // allocation history of its entry. Off by default -- it emitted 13,312 lines in a
  // 30k-cycle 256-core run.
`ifdef ROB_GEN_TRACE
  if (GenBits > 0) begin : gen_alloc_trace
    // pragma translate_off
    always_ff @(posedge clk_i) begin
      if (rst_ni) begin
        if (id_req_i && !full_o && !block_fire)
          $display("[rob_alloc] t=%0t %m SINGLE entry=%0d stamp_gen=%0d -> id_o=%0d  (wp=%0d gen=%0d cnt=%0d full=%0b)",
                   $time, write_pointer_q, gen_q, {gen_q, write_pointer_q}, write_pointer_q, gen_q, status_cnt_q, full_o);
        if (block_fire)
          $display("[rob_alloc] t=%0t %m BLOCK  base=%0d words=%0d stamp_gen=%0d  (gen=%0d cnt=%0d)",
                   $time, write_pointer_q, BlockWords, gen_q, gen_q, status_cnt_q);
        // id_req asserted but REFUSED -- the consumer may still be sampling id_o combinationally
        if (id_req_i && full_o)
          $display("[rob_alloc] t=%0t %m REFUSED(full) wp=%0d gen=%0d id_o would be %0d",
                   $time, write_pointer_q, gen_q, {gen_q, write_pointer_q});
        if (pop_i && valid_o)
          $display("[rob_pop] t=%0t %m entry=%0d gen_held=%0d cnt=%0d",
                   $time, read_pointer_q, entry_gen_q[read_pointer_q][GenBits-1:0], status_cnt_q);
        if (GenBits > 0 && (gen_d != gen_q))
          $display("[rob_gen] t=%0t %m generation %0d -> %0d (wp %0d -> %0d)",
                   $time, gen_q, gen_d, write_pointer_q, write_pointer_d);
      end
    end
    // pragma translate_on
  end

`endif

  if (GenBits > 0) begin : gen_drop_trace
    // pragma translate_off
    always_ff @(posedge clk_i) begin
      if (rst_ni) begin
        if (push_i && !push_gen_ok)
          $display({"[rob_drop] t=%0t %m id=%0d -> entry=%0d carried_gen=%0d entry_gen=%0d ",
                    "(NumWords=%0d) | why: alloc_fire=%0b block_fire=%0b id_req=%0b full=%0b ",
                    "wp=%0d gen_q=%0d win=%0b gen_of_push=%0d"},
                   $time, id_i, push_entry, id_i[IdWidth-1:EntryAw],
                   entry_gen_q[push_entry][GenBits-1:0], NumWords,
                   alloc_fire, block_fire, id_req_i, full_o, write_pointer_q, gen_q,
                   alloc_win_mask[push_entry], gen_of_push);
        if ((NumWrPorts > 1) && push2_i && !push2_gen_ok)
          $display("[rob_drop2] t=%0t %m id=%0d -> entry=%0d carried_gen=%0d entry_gen=%0d",
                   $time, id2_i, push2_entry, id2_i[IdWidth-1:EntryAw],
                   entry_gen_q[push2_entry][GenBits-1:0]);
      end
    end
    // pragma translate_on
  end


  // Visibility. A non-zero count here means the generation tag EARNED its keep: a late duplicate
  // response arrived for an allocation that had already retired, and was dropped instead of
  // corrupting whatever request now owns the entry. Silence means it never happened -- which is
  // the expected reading whenever IdWidthExt leaves GenBits at 0.
  // pragma translate_off
  final begin
    if (stale_drop_q != 0)
      $display("[reorder_buffer] %m: dropped %0d stale response(s) on generation mismatch (NumWords=%0d IdWidth=%0d GenBits=%0d)",
               stale_drop_q, NumWords, IdWidth, GenBits);
  end
  // pragma translate_on
`endif

endmodule: reorder_buffer
