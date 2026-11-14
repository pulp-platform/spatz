// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Low-area shadow verification checker for one Spatz fpnew lane.
//
// Protection strategy in this cleaned version:
//   ADDMUL : reduced-precision fpnew datapath
//            FP32 -> FP16ALT (E8M7), FP16 -> FP8 (E5M2), FP8 -> FP8.
//   DOTP   : reduced fpnew SDOTP for the common FP16xFP16->FP32 case,
//            mapped to FP8xFP8->FP16ALT and checked on exponent.
//   DIVSQRT: reduced fpnew DIVSQRT (C910): FP32 -> FP16ALT (ULP tolerance),
//            FP16 -> FP16 and FP16ALT -> FP16ALT (exact). Only elaborated
//            where FPUImplementation enables DIVSQRT (shared: FPU0 only).
//   NONCOMP: exact lightweight functional replica (SGNJ/MINMAX/CMP/CLASSIFY).
//   CONV   : low-area semantic checker.  F2F/I2F compare FP exponent class;
//            F2I compares integer leading-one position against FP exponent.
//            Numerically fragile/ambiguous cases are bypassed deliberately.
//
// The legacy hand-written fixed-point ADDMUL/DOTP datapath and its large
// delay-line/opnd/cmp register structures have been removed completely.

module spatz_fpu_shadow_checker
  import fpnew_pkg::*;
#(
  parameter int unsigned ElenWidth      = 32,
  parameter int unsigned AddmulLatency  = 1,
  parameter int unsigned DotpLatency    = 2,
  parameter int unsigned NoncompLatency = 1,
  // Retained for drop-in parameter compatibility with the previous checker.
  // It is no longer used by the reduced-fpnew arithmetic checkers.
  parameter int unsigned SlackUlp       = 10,
  // Width of the per-instruction id threaded alongside the real FPU's own
  // tag_i/tag_o (see expected_id_i/result_id_i below).
  parameter int unsigned IdWidth        = 1,
  // Upper bound on how many accepted instructions the real FPU can ever
  // have outstanding (accepted but not yet retired) at once, across all of
  // its opgroups combined -- used only as a plausibility bound for the
  // handshake-level check below, not to model any specific opgroup's exact
  // latency. Size to comfortably exceed the real FPUImplementation's total
  // pipeline depth; the default is a conservative round number, not a
  // measured value for any particular configuration.
  parameter int unsigned MaxOutstanding = 16,
  parameter fpu_features_t       FPUFeatures       = fpu_features_t'(0),
  parameter fpu_implementation_t FPUImplementation = fpu_implementation_t'(0)
) (
  input  logic                 clk_i,
  input  logic                 rst_ni,

  input  logic [ElenWidth-1:0] operand1_i,
  input  logic [ElenWidth-1:0] operand2_i,
  input  logic [ElenWidth-1:0] operand3_i,
  input  operation_e           op_i,
  input  logic                 op_mod_i,
  input  roundmode_e           rnd_mode_i,
  input  fp_format_e           src_fmt_i,
  input  fp_format_e           dst_fmt_i,
  input  int_format_e          int_fmt_i,
  input  logic                 vectorial_op_i,
  input  logic                 in_valid_i,
  input  logic                 in_ready_i,
  // Id of the instruction being accepted this cycle (mirrors what's fed to
  // the real FPU's own tag_i) and the id attached to whatever the real FPU
  // is currently returning on result_i (mirrors its tag_o). Used purely to
  // detect real/shadow pipeline misalignment -- see reduced_addmul_fault_comb/
  // reduced_dotp_fault_comb below -- not for any arithmetic purpose.
  input  logic [IdWidth-1:0]   expected_id_i,
  input  logic [IdWidth-1:0]   result_id_i,

  // Pure-passthrough tag bit (vfu_tag_t.narrowing), threaded in/out solely to
  // catch a corrupted tag-pipeline register inside any opgroup (e.g. a
  // divsqrt/noncomp inp_pipe_tag_q.narrowing flop) -- see the tag-passthrough
  // check below. Not used by any arithmetic/tolerance logic above.
  input  logic                 tag_narrowing_i,
  input  logic                 result_tag_narrowing_i,

  input  logic [ElenWidth-1:0] result_i,
  input  logic                 result_valid_i,
  input  logic                 result_ready_i,

  output logic                 fault_o
);

  localparam int unsigned MaxLanes     = ElenWidth / 8;
  localparam int unsigned LaneCntWidth = $clog2(MaxLanes + 1);

  logic is_addmul_op, is_dotp_op, is_noncomp_op, is_conv_op;
  assign is_addmul_op = (op_i inside {FMADD, FNMSUB, ADD, MUL}) && (src_fmt_i == dst_fmt_i);
  assign is_dotp_op   = (op_i == SDOTP);
  assign is_noncomp_op = (op_i inside {SGNJ, MINMAX, CMP, CLASSIFY}) && (src_fmt_i == dst_fmt_i);
  assign is_conv_op   = (op_i inside {F2F, F2I, I2F});


  // =====================================================================
  // NONCOMP: exact low-cost replica
  // =====================================================================
  typedef struct {
    logic                     valid;
    logic [LaneCntWidth-1:0]  num_lanes;
    fp_format_e               dst_fmt;
    logic [ElenWidth-1:0]     expected [MaxLanes];
    // Identifies which in-flight instruction this entry's expected value
    // belongs to. result_i/result_valid_i/result_id_i are the FPU's single
    // shared output bus across ALL opgroups, so this id is matched (not
    // merely compared) against result_id_i to recognize the one retiring
    // result that actually corresponds to this entry -- see noncomp_consumed
    // below. A non-matching id simply means an unrelated instruction (any
    // other opgroup, or NONCOMP with the id not yet reused) retired this
    // cycle, not a fault.
    logic [IdWidth-1:0]       id;
  } noncomp_entry_t;

  // Exact bit-level duplicate-and-compare for SGNJ/MINMAX/CMP/CLASSIFY,
  // replicating fpnew_noncomp.sv precisely (not the tolerance-based shadow
  // used above). Full precision throughout -- no 7-bit truncation, no adaptive-error
  // tracking, no cancellation tolerance; every field below is exact.
  //
  // full_decode zero-extends exp/man into fixed 8-/23-bit containers so
  // the same comparisons work uniformly across formats; exp_zero/exp_ones
  // flag the *exponent field*, not zero/inf themselves (e.g. exp_zero is
  // true for both true-zero and subnormal -- callers derive the
  // distinction from man where needed, as CLASSIFY does).
  //
  // Returns a struct (not output ports) specifically so callers can share
  // ONE decode per operand across all four NONCOMP sub-ops below, instead
  // of each sub-op decoding both operands itself: the SGNJ/MINMAX/CMP
  // hardware all exist per lane regardless of which is actually selected
  // at runtime (the case statement below just muxes their outputs), so
  // decoding raw_a/raw_b once and passing the result in cuts full_decode
  // from 7 instances/lane (2+2+2+1) down to 2 -- a real area win with zero
  // effect on the result or on timing (NONCOMP was never part of this
  // checker's timing violations).
  // ---------------------------------------------------------------------
  typedef struct packed {
    logic         sign;
    logic [7:0]   exp;
    logic [22:0]  man;
    logic         exp_zero;
    logic         exp_ones;
    logic         is_snan;
  } noncomp_decoded_t;

  function automatic noncomp_decoded_t full_decode(
    input logic [ElenWidth-1:0] raw, input fp_format_e fmt
  );
    automatic noncomp_decoded_t d;
    automatic int unsigned eb = exp_bits(fmt);
    automatic int unsigned mb = man_bits(fmt);
    automatic int unsigned w  = fp_width(fmt);
    automatic logic [7:0]  raw_exp;
    automatic logic [31:0] raw_man;

    d.sign  = raw[w-1];
    raw_exp = 8'((raw >> mb) & ((32'd1 << eb) - 32'd1));
    raw_man = raw & ((32'd1 << mb) - 32'd1);

    d.exp_zero = (raw_exp == 8'd0);
    d.exp_ones = (raw_exp == 8'((32'd1 << eb) - 32'd1));
    d.exp      = raw_exp;
    d.man      = 23'(raw_man);
    // Signalling NaN: exponent all-1, mantissa nonzero, quiet bit (MSB of
    // the format's *actual* mb-bit mantissa field, not bit 22) clear.
    d.is_snan  = d.exp_ones && (raw_man != 32'd0) && !raw_man[mb-1];
    return d;
  endfunction

  function automatic logic [ElenWidth-1:0] canonical_qnan(input fp_format_e fmt);
    automatic int unsigned eb = exp_bits(fmt);
    automatic int unsigned mb = man_bits(fmt);
    return ElenWidth'(((ElenWidth'(1) << eb) - 1) << mb) | ElenWidth'(ElenWidth'(1) << (mb - 1));
  endfunction

  // rnd_mode_i selects the sub-op exactly as fpnew_noncomp.sv does: RNE ->
  // SGNJ (sign = sign_b), RTZ -> SGNJN (sign = ~sign_b), RDN -> SGNJX
  // (sign = sign_a^sign_b). Magnitude always comes from operand a. RUP
  // (raw operand_a passthrough) is fpnew's own encoding, never emitted by
  // Spatz's decoder for VFSGNJ -- not modeled.
  function automatic logic [ElenWidth-1:0] compute_sgnj(
    input logic [ElenWidth-1:0] raw_a, input noncomp_decoded_t da, input noncomp_decoded_t db,
    input fp_format_e fmt, input roundmode_e rm
  );
    automatic logic result_sign;
    automatic int unsigned w;
    automatic logic [ElenWidth-1:0] mag_mask;

    w = fp_width(fmt);

    unique case (rm)
      RNE: result_sign = db.sign;
      RTZ: result_sign = ~db.sign;
      RDN: result_sign = da.sign ^ db.sign;
      default: result_sign = da.sign;
    endcase

    mag_mask = (ElenWidth'(1) << (w - 1)) - ElenWidth'(1);
    return (raw_a & mag_mask) | (result_sign ? (ElenWidth'(1) << (w - 1)) : '0);
  endfunction

  // rnd_mode_i: RNE -> MIN, RTZ -> MAX. Matches fpnew_noncomp.sv exactly:
  // both-NaN -> canonical qNaN; exactly-one-NaN -> return the other
  // operand unconditionally; else compare the FULL raw bit pattern
  // (including sign) as an unsigned integer, XOR-corrected by sign --
  // this specific formula (not a magnitude-only compare) is what makes
  // MIN always return -0 and MAX always return +0 for mixed-sign-zero
  // inputs, order-independent (verified by hand against both orderings).
  function automatic logic [ElenWidth-1:0] compute_minmax(
    input logic [ElenWidth-1:0] raw_a, input logic [ElenWidth-1:0] raw_b,
    input noncomp_decoded_t da, input noncomp_decoded_t db,
    input fp_format_e fmt, input roundmode_e rm
  );
    automatic logic is_nan_a, is_nan_b, a_smaller;
    automatic int unsigned w;
    automatic logic [ElenWidth-1:0] mask_w;

    w = fp_width(fmt);
    is_nan_a = da.exp_ones && (da.man != 23'd0);
    is_nan_b = db.exp_ones && (db.man != 23'd0);

    if (is_nan_a && is_nan_b) return canonical_qnan(fmt);
    if (is_nan_a) return raw_b;
    if (is_nan_b) return raw_a;

    mask_w    = (w == ElenWidth) ? '1 : ((ElenWidth'(1) << w) - ElenWidth'(1));
    a_smaller = ((raw_a & mask_w) < (raw_b & mask_w)) ^ (da.sign || db.sign);

    if (rm == RNE) return a_smaller ? raw_a : raw_b; // MIN
    else           return a_smaller ? raw_b : raw_a; // MAX
  endfunction

  // rnd_mode_i base behaviors (RNE=LE, RTZ=LT, RDN=EQ), op_mod_i XORs the
  // boolean -- but NaN handling is NOT a uniform post-hoc XOR, it differs
  // per predicate exactly as fpnew_noncomp.sv implements it: any sNaN
  // forces false unconditionally (op_mod_i never applied, matching the
  // real "result stays default" short-circuit); LE/LT force false for
  // ANY NaN (quiet included) without applying op_mod_i; EQ/NE (RDN) is
  // the one case that DOES apply op_mod_i to a NaN operand, since
  // "equal ^ op_mod_i" degenerates to exactly "op_mod_i" when NaN forces
  // equal=0 -- i.e. NE(NaN)=true, EQ(NaN)=false, both correct without a
  // separate branch. operands_equal true for +-0 either way.
  function automatic logic compute_cmp(
    input logic [ElenWidth-1:0] raw_a, input logic [ElenWidth-1:0] raw_b,
    input noncomp_decoded_t da, input noncomp_decoded_t db,
    input fp_format_e fmt, input roundmode_e rm, input logic op_mod
  );
    automatic logic is_nan_a, is_nan_b, any_nan, any_snan;
    automatic logic a_smaller, equal;
    automatic int unsigned w;
    automatic logic [ElenWidth-1:0] mask_w;

    w = fp_width(fmt);
    is_nan_a = da.exp_ones && (da.man != 23'd0);
    is_nan_b = db.exp_ones && (db.man != 23'd0);
    any_nan  = is_nan_a || is_nan_b;
    any_snan = (is_nan_a && da.is_snan) || (is_nan_b && db.is_snan);

    mask_w = (w == ElenWidth) ? '1 : ((ElenWidth'(1) << w) - ElenWidth'(1));
    // fpnew_classifier's is_zero requires exponent==0 AND mantissa==0 --
    // exp_zero alone (exponent field zero) is also true for subnormals, so
    // using it directly here would wrongly treat two different
    // subnormals as "equal".
    equal     = ((raw_a & mask_w) == (raw_b & mask_w)) ||
                ((da.exp_zero && da.man == 23'd0) && (db.exp_zero && db.man == 23'd0));
    a_smaller = ((raw_a & mask_w) < (raw_b & mask_w)) ^ (da.sign || db.sign);

    if (any_snan) return 1'b0;

    unique case (rm)
      RNE: return any_nan ? 1'b0 : ((a_smaller || equal) ^ op_mod);      // LE
      RTZ: return any_nan ? 1'b0 : ((a_smaller && !equal) ^ op_mod);     // LT
      RDN: return any_nan ? op_mod : (equal ^ op_mod);                   // EQ
      default: return 1'b0;
    endcase
  endfunction

  // Single-operand: sign/exponent/mantissa decode into fpnew_pkg's 10-bit
  // one-hot class mask, priority chain matching fpnew_classifier.sv. Not
  // NaN-boxing aware -- confirmed moot for Spatz's actual configuration
  // (ELEN=32, RVD=0): fpnew_top's is_boxed is unconditionally 1 whenever
  // either the format fills the full lane width or the op is vectorial,
  // and both are always true here.
  function automatic logic [ElenWidth-1:0] compute_classify(input noncomp_decoded_t da);
    automatic logic [9:0] mask10;

    mask10 = 10'd0;

    if (!da.exp_zero && !da.exp_ones) begin // normal
      mask10 = da.sign ? (10'd1 << 1) : (10'd1 << 6);
    end else if (da.exp_zero && da.man != 23'd0) begin // subnormal
      mask10 = da.sign ? (10'd1 << 2) : (10'd1 << 5);
    end else if (da.exp_zero && da.man == 23'd0) begin // zero
      mask10 = da.sign ? (10'd1 << 3) : (10'd1 << 4);
    end else if (da.exp_ones && da.man == 23'd0) begin // inf
      mask10 = da.sign ? (10'd1 << 0) : (10'd1 << 7);
    end else begin // nan
      mask10 = da.is_snan ? (10'd1 << 8) : (10'd1 << 9);
    end
    return ElenWidth'(mask10);
  endfunction

  // ---------------------------------------------------------------------
  // Capture-time NONCOMP computation (combinational). Per-lane vectorial
  // splitting mirrors the ADDMUL case above (same lane count/offset
  // formula); CLASSIFY only reads operand1_i.
  // ---------------------------------------------------------------------
  logic [ElenWidth-1:0]    noncomp_expected_comb [MaxLanes];
  logic [LaneCntWidth-1:0] noncomp_num_lanes_comb;

  always_comb begin
    noncomp_num_lanes_comb = '0;
    for (int lane = 0; lane < MaxLanes; lane++) noncomp_expected_comb[lane] = '0;

    if (is_noncomp_op) begin
      automatic int unsigned w = fp_width(dst_fmt_i);
      noncomp_num_lanes_comb = LaneCntWidth'(ElenWidth / w);
      for (int lane = 0; lane < MaxLanes; lane++) begin
        if (lane < noncomp_num_lanes_comb) begin
          automatic logic [ElenWidth-1:0] raw_a, raw_b, lane_mask, result_raw;
          automatic noncomp_decoded_t da, db;
          raw_a = operand1_i >> (lane * w);
          raw_b = operand2_i >> (lane * w);
          // Decoded ONCE per operand, shared across all four sub-ops below
          // -- see full_decode's own comment for why (all four exist in
          // hardware per lane regardless of which op_i actually selects).
          da = full_decode(raw_a, dst_fmt_i);
          db = full_decode(raw_b, dst_fmt_i);
          // Mask to this lane's own w bits: compute_minmax/compute_classify
          // can return stray upper bits carried over from an adjacent
          // lane's data (unlike compute_sgnj, which already masks
          // internally); comparing those against result_i's corresponding
          // (unrelated) adjacent-lane bits would spuriously mismatch.
          lane_mask = (w == ElenWidth) ? '1 : ((ElenWidth'(1) << w) - ElenWidth'(1));
          unique case (op_i)
            SGNJ:     result_raw = compute_sgnj(raw_a, da, db, dst_fmt_i, rnd_mode_i);
            MINMAX:   result_raw = compute_minmax(raw_a, raw_b, da, db, dst_fmt_i, rnd_mode_i);
            CMP:      result_raw = {{(ElenWidth-1){1'b0}}, compute_cmp(raw_a, raw_b, da, db, dst_fmt_i, rnd_mode_i, op_mod_i)};
            CLASSIFY: result_raw = compute_classify(da);
            default:  result_raw = '0;
          endcase
          noncomp_expected_comb[lane] = result_raw & lane_mask;
        end
      end
    end
  end



  noncomp_entry_t noncomp_dline [NoncompLatency];

  // Fires only when the entry this shadow is tracking is actually the one
  // retiring this cycle. result_i/result_valid_i/result_id_i are the FPU's
  // single shared output bus across ALL opgroups (ADDMUL/DOTP/DIVSQRT/CONV
  // too), so an id mismatch here just means "some other instruction retired
  // this cycle" -- it must never by itself be treated as a fault.
  logic noncomp_consumed;
  assign noncomp_consumed = noncomp_dline[NoncompLatency-1].valid && result_valid_i &&
                            (noncomp_dline[NoncompLatency-1].id == result_id_i);

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      for (int i = 0; i < NoncompLatency; i++)
        noncomp_dline[i] <= '{valid: 1'b0, num_lanes: '0,
                              dst_fmt: fpnew_pkg::FP32,
                              expected: '{default: '0},
                              id: '0};
    end else if (in_valid_i && is_noncomp_op && in_ready_i) begin // pushes only on an actual new NONCOMP accept
      for (int i = NoncompLatency - 1; i > 0; i--)
        noncomp_dline[i] <= noncomp_dline[i-1];
      noncomp_dline[0] <= '{
        valid:     in_valid_i && is_noncomp_op,
        num_lanes: noncomp_num_lanes_comb,
        dst_fmt:   dst_fmt_i,
        expected:  noncomp_expected_comb,
        id:        expected_id_i
      };
    end else if (noncomp_consumed) begin
      // Just compared against its matching retiring result -- invalidate so
      // a LATER, unrelated instruction retiring with a reused/wrapped-around
      // id can't spuriously re-match this stale entry.
      noncomp_dline[NoncompLatency-1].valid <= 1'b0;
    end
  end

  logic noncomp_fault_comb;
  always_comb begin
    noncomp_fault_comb = 1'b0;
    if (noncomp_consumed) begin
      automatic fp_format_e fmt = noncomp_dline[NoncompLatency-1].dst_fmt;
      automatic int unsigned w = fp_width(fmt);
      automatic logic [ElenWidth-1:0] lane_mask =
        (w == ElenWidth) ? '1 : ((ElenWidth'(1) << w) - ElenWidth'(1));
      for (int lane = 0; lane < MaxLanes; lane++) begin
        if (lane < noncomp_dline[NoncompLatency-1].num_lanes) begin
          automatic logic [ElenWidth-1:0] real_raw;
          real_raw = (result_i >> (lane * w)) & lane_mask;
          if (real_raw != noncomp_dline[NoncompLatency-1].expected[lane])
            noncomp_fault_comb = 1'b1;
        end
      end
    end
  end

// =====================================================================
// ADDMUL
// =================================
  // Tightening to 2 (5 of 7 shadow mantissa bits checked) was tried and
  // reverted: a fault-free kernel run failed under it, so even FP32's much
  // gentler quantization (~0.8% per-operand error) doesn't have enough
  // headroom below tol=4. Back to the last known-good value.
  localparam int unsigned ReducedFp32Ulp          = 4;
  // Tightening to 1 was tried and reverted: a fault-free kernel run failed
  // under it, confirming the baseline-quantization-noise risk -- FP16's
  // shadow (FP16->FP8, real quantization loss, ~25% relative error per
  // operand on only a 2-bit mantissa) needs tol=2 to absorb ordinary,
  // non-cancellation ADDMUL arithmetic without false-flagging it. Back to
  // the last known-good value.
  localparam int unsigned ReducedFp16Ulp          = 2;
  // Per-format cancellation-risk exponent window.  Not shared: FP32->FP16ALT
  // quantization keeps 7/23 mantissa bits (~0.8% per-operand error), while
  // FP16->FP8 keeps only 2/10 (~25% per-operand error), so the same window
  // that is safe for FP32 lets FP16 cancellation blow-up cross the ULP
  // tolerance -- confirmed empirically: 2 is correct for FP32, 3 for FP16.
  // (FP8 no longer has an entry here: native FP8 ADDMUL dropped bypass/
  // cancel-risk classification entirely -- see ReducedFp8ExactTol.)
  localparam int unsigned ReducedCancelExpWindowFp32 = 2;
  localparam int unsigned ReducedCancelExpWindowFp16 = 3;
  // Native FP8 ADDMUL (real src_fmt_i==FP8, shadow at the SAME E5M2
  // precision -- exact, like FP16ALT below). Also tried: a bypass-lane
  // split (tight-but-not-exact ReducedFp8Ulp on ordinary lanes, exponent-
  // only on cancel-risk-flagged ones); a uniform exponent+top-mantissa-bit
  // compare (exp+man[1], dropping only man[0] -- empirically found to
  // behave identically to this exact compare on the tested fault set, so
  // wasn't buying any margin in practice); a sign+exponent-only compare
  // (no mantissa bits at all). Back to the genuinely exact compare here --
  // sign+exp+both mantissa bits, via reduced_mismatch8 at tol=0, which
  // correctly folds sign into its ordered-code comparison rather than a
  // hand-rolled bit-slice that would miss a sign-only fault. No cancel-
  // risk classification, one uniform compare on every lane, same as
  // FP16ALT below -- exact precision on both sides means there is no
  // legitimate reason for shadow and real to ever disagree at all, ULP=0.
  // (spatz_vfu.sv's operand-parity check still excludes FP8 for now --
  // see the TEST ONLY note on input_dup_mismatch_d there.)
  localparam int unsigned ReducedFp8ExactTol = 0;
  // Native FP16ALT ADDMUL (real src_fmt_i==FP16ALT, shadow computed at the
  // SAME E8M7 precision -- no quantization step at all, unlike every other
  // ADDMUL mapping in this file). Since there is no reduction, there is no
  // cancellation-driven quantization-noise mechanism to protect against
  // (that's what the bypass_lane/cancel-risk machinery exists for
  // elsewhere) -- so FP16ALT skips that classification entirely, same as
  // FP8 above. Also matching FP8's own progression (tried exponent-only,
  // then dialed back to a genuinely exact compare), this now reuses
  // reduced_mismatch16 at tol=0 directly -- sign+exp+both mantissa bits,
  // one uniform compare on every lane, bypass or not. No cancel-risk
  // classification needed: exact precision on both sides means there is no
  // legitimate reason for shadow and real to ever disagree at all, same
  // rationale as ReducedFp8ExactTol above. Unvalidated against a fault-free
  // FP16ALT kernel (bp-fmatmul) yet.
  //
  // (reduced_fp16alt_exp_mismatch below is now DOTP-only -- see its own
  // comment and ReducedDotpFp8ExpSlack -- since this ADDMUL mapping no
  // longer calls it.)
  localparam int unsigned ReducedFp16AltExactTol = 0;

  typedef struct packed {
    fp_format_e real_fmt;
    logic       vectorial;
    logic [3:0] bypass_lane;
    // Carried through purely to detect real/shadow misalignment at compare
    // time -- see expected_id_i/result_id_i above.
    logic [IdWidth-1:0] id;
    // Per-lane conservative upper bound on the FP16 result's biased exponent
    // field, derived from the trusted full-precision input operands at
    // accept time -- see reduced_fp16_exp_bound below. Only FP16's 2 lanes
    // are covered (FP8 bypass-lane handling is unchanged); sized [1:0]
    // to keep this at the minimum width the FP16 case actually needs.
    logic [1:0][4:0] exp_bound;
    // Same technique for the FP32 case (see reduced_fp32_exp_bound below).
    // FP32 ADDMUL is always scalar (vectorial forced to 0), so this needs
    // only a single 8-bit value, not a per-lane array.
    logic [7:0] exp_bound32;
  } reduced_addmul_tag_t;

  function automatic fmt_logic_t reduced_addmul_fmt_mask();
    automatic fmt_logic_t m;
    m = '0;
    m[FP16ALT] = 1'b1;
    m[FP8]     = 1'b1;
    return m;
  endfunction

  function automatic fmt_unsigned_t reduced_addmul_pipe_regs();
    automatic fmt_unsigned_t p;
    p = '{default: '0};
    p[FP16ALT] = 32'(AddmulLatency);
    p[FP8]     = 32'(AddmulLatency);
    return p;
  endfunction

  function automatic fmt_unit_types_t reduced_addmul_unit_types();
    automatic fmt_unit_types_t u;
    u = '{default: DISABLED};
    u[FP16ALT] = MERGED;
    u[FP8]     = MERGED;
    return u;
  endfunction

  localparam fmt_logic_t      ReducedAddmulFmtMask   = reduced_addmul_fmt_mask();
  localparam fmt_unsigned_t   ReducedAddmulPipeRegs  = reduced_addmul_pipe_regs();
  localparam fmt_unit_types_t ReducedAddmulUnitTypes = reduced_addmul_unit_types();

  // Same-exponent-range quantizers.  Only the mantissa is reduced.
  // If NaN payload truncation would produce Inf, force a quiet-NaN payload.
  function automatic logic [15:0] reduced_fp32_to_fp16alt(input logic [31:0] x);
    automatic logic       s;
    automatic logic [7:0] e;
    automatic logic [22:0] m;
    automatic logic [6:0] mr;
    s  = x[31];
    e  = x[30:23];
    m  = x[22:0];
    mr = m[22 -: 7];
    if ((e == 8'hff) && (m != 23'd0) && (mr == 7'd0)) mr = 7'b1000000;
    return {s, e, mr};
  endfunction

  function automatic logic [7:0] reduced_fp16_to_fp8(input logic [15:0] x);
    automatic logic       s;
    automatic logic [4:0] e;
    automatic logic [9:0] m;
    automatic logic [1:0] mr;
    s  = x[15];
    e  = x[14:10];
    m  = x[9:0];
    mr = m[9 -: 2];
    if ((e == 5'h1f) && (m != 10'd0) && (mr == 2'd0)) mr = 2'b10;
    return {s, e, mr};
  endfunction

  function automatic logic reduced_fp32_subnormal(input logic [31:0] x);
    return (x[30:23] == 8'd0) && (x[22:0] != 23'd0);
  endfunction

  function automatic logic reduced_fp16_subnormal(input logic [15:0] x);
    return (x[14:10] == 5'd0) && (x[9:0] != 10'd0);
  endfunction

  function automatic logic reduced_fp8_subnormal(input logic [7:0] x);
    return (x[6:2] == 5'd0) && (x[1:0] != 2'd0);
  endfunction

  // FP16ALT (E8M7): sign=[15], exp=[14:7] (8 bits, bias 127 -- same field
  // width/bias as FP32), man=[6:0] (7 bits).
  function automatic logic reduced_fp16alt_subnormal(input logic [15:0] x);
    return (x[14:7] == 8'd0) && (x[6:0] != 7'd0);
  endfunction

  function automatic logic reduced_fp32_zero(input logic [31:0] x);
    return (x[30:23] == 8'd0) && (x[22:0] == 23'd0);
  endfunction

  function automatic logic reduced_fp16_zero(input logic [15:0] x);
    return (x[14:10] == 5'd0) && (x[9:0] == 10'd0);
  endfunction

  function automatic logic reduced_fp8_zero(input logic [7:0] x);
    return (x[6:2] == 5'd0) && (x[1:0] == 2'd0);
  endfunction

  function automatic logic reduced_fp32_special(input logic [31:0] x);
    return x[30:23] == 8'hff;
  endfunction

  function automatic logic reduced_fp16_special(input logic [15:0] x);
    return x[14:10] == 5'h1f;
  endfunction

  function automatic logic reduced_fp8_special(input logic [7:0] x);
    return x[6:2] == 5'h1f;
  endfunction

  // FP8's own exact-compare needs no new function: it reuses
  // reduced_mismatch8 (below) at ReducedFp8ExactTol==0 directly on every
  // lane -- see that localparam's comment for why exact precision on both
  // sides means an unconditional equality check (via the existing sign-
  // aware ordered-code comparator) is the right tool here, not a bespoke
  // bit-slice.

  // Exponent-only compare, now used only by DOTP's FP8->FP16ALT mapping
  // (see ReducedDotpFp8ExpSlack below) -- native FP16ALT ADDMUL above has
  // moved to a full exact compare (reduced_mismatch16 at
  // ReducedFp16AltExactTol) and no longer calls this. No zero/subnormal
  // exemption of any kind here (unlike reduced_mismatch8/16's standard
  // both-zero equivalence): FP16ALT->FP16ALT (and, for the DOTP case,
  // FP8->FP16ALT) has no quantization step to cause the shadow's own
  // result to legitimately collapse toward zero under cancellation the way
  // a reduced-mantissa shadow can, so that specific carve-out has no known
  // legitimate case to protect here. real_val's own subnormal range is
  // still excluded at each call site (matching every other format's
  // pattern in this file).
  function automatic logic reduced_fp16alt_exp_mismatch(
    input logic [15:0] shadow, input logic [15:0] real_val, input int unsigned slack
  );
    automatic logic [7:0] sh_exp, real_exp;
    automatic logic [8:0] diff;
    sh_exp   = shadow[14:7];
    real_exp = real_val[14:7];
    diff = (sh_exp >= real_exp) ? (sh_exp - real_exp) : (real_exp - sh_exp);
    return diff > 9'(slack);
  endfunction

  // Plausibility bound for cancellation-risk-flagged native FP16 ADDMUL
  // lanes, used instead of a full skip. Unlike FP8's exact compare (or
  // FP16ALT's/DOTP's exponent-only ones), this is NOT a shadow-vs-real
  // exponent compare: FP16's shadow quantizes
  // 10 mantissa bits down to FP8's 2, so under real cancellation the
  // shadow's own result is dominated by ~25% quantization noise and its
  // exponent can legitimately diverge from the real one by an unbounded
  // number of bins -- no fixed slack on that compare can avoid false
  // positives (confirmed empirically: slack=1 and slack=4 both fired on
  // fault-free cancellation cases). Instead, bound the real result's
  // exponent using only the trusted, full-precision INPUT operands
  // (already available, and already partly computed for cancellation
  // classification) -- a result can't legitimately land far above the
  // largest input magnitude, regardless of how precisely it cancels.
  function automatic logic [4:0] reduced_fp16_exp_bound(
    input operation_e op, input logic [15:0] a, input logic [15:0] b, input logic [15:0] c
  );
    automatic int signed e_prod, e_c, e_max;
    // Mirrors reduced_cancel_risk_fp16's own e_prod/e_c derivation, including
    // its MUL special case: a pure multiply's addend is architecturally
    // forced to zero, so operand3_i/c carries no meaningful exponent for
    // this op and must not be folded into the bound (reduced_cancel_risk_fp16
    // itself returns early on MUL without ever touching c, for the same
    // reason).
    if (op == ADD) begin
      e_prod = int'(b[14:10]);
      e_c    = int'(c[14:10]);
    end else if (op == MUL) begin
      e_prod = int'(a[14:10]) + int'(b[14:10]) - 15;
      e_c    = e_prod;
    end else begin
      e_prod = int'(a[14:10]) + int'(b[14:10]) - 15;
      e_c    = int'(c[14:10]);
    end
    e_max  = (e_prod > e_c) ? e_prod : e_c;
    // Small margin for legitimate exponent growth (e.g. a carry out of an
    // addition), not for numerical noise -- the compare on the other end
    // is against the real, full-precision result, so no quantization slack
    // is needed here.
    e_max += 2;
    if (e_max < 0)  e_max = 0;
    if (e_max > 30) e_max = 30; // stay below the all-ones (inf/nan) encoding
    return 5'(e_max);
  endfunction

  function automatic logic reduced_fp16_exp_bound_violation(
    input logic [4:0] bound, input logic [15:0] real_val
  );
    automatic logic [4:0] real_exp;
    real_exp = real_val[14:10];
    if (real_exp == 5'h1f) return 1'b0; // inf/nan/special -- not this check's job
    return real_exp > bound;
  endfunction

  // Same technique as reduced_fp16_exp_bound, for FP32 ADDMUL bypass-lane
  // instructions -- see that function's own comment for the full rationale
  // (this closes the exact same "cancel-risk lanes get a full skip" gap,
  // just for FP32/FP16ALT instead of FP16/FP8; FP32 never got any bound at
  // all before this, not even a directional one). No rebias is needed here:
  // unlike the FP16/FP8 pair, FP32 and its FP16ALT shadow share the same
  // 8-bit, bias-127 exponent field (reduced_fp32_to_fp16alt keeps the
  // exponent bits unchanged), so operand exponents are used directly.
  function automatic logic [7:0] reduced_fp32_exp_bound(
    input operation_e op, input logic [31:0] a, input logic [31:0] b, input logic [31:0] c
  );
    automatic int signed e_prod, e_c, e_max;
    // Mirrors reduced_cancel_risk_fp32's own e_prod/e_c derivation, including
    // its MUL special case: a pure multiply's addend is architecturally
    // forced to zero, so operand3_i/c carries no meaningful exponent for
    // this op and must not be folded into the bound.
    if (op == ADD) begin
      e_prod = int'(b[30:23]);
      e_c    = int'(c[30:23]);
    end else if (op == MUL) begin
      e_prod = int'(a[30:23]) + int'(b[30:23]) - 127;
      e_c    = e_prod;
    end else begin
      e_prod = int'(a[30:23]) + int'(b[30:23]) - 127;
      e_c    = int'(c[30:23]);
    end
    e_max  = (e_prod > e_c) ? e_prod : e_c;
    // Same margin rationale as reduced_fp16_exp_bound: legitimate exponent
    // growth from summing a bounded number of terms, not numerical noise.
    e_max += 2;
    if (e_max < 0)   e_max = 0;
    if (e_max > 254) e_max = 254; // stay below the all-ones (inf/nan) encoding
    return 8'(e_max);
  endfunction

  function automatic logic reduced_fp32_exp_bound_violation(
    input logic [7:0] bound, input logic [31:0] real_val
  );
    automatic logic [7:0] real_exp;
    real_exp = real_val[30:23];
    if (real_exp == 8'hff) return 1'b0; // inf/nan/special -- not this check's job
    return real_exp > bound;
  endfunction

  // Conservative input-side cancellation predictor.  It is used only to
  // bypass numerically fragile cases, never to predict a numerical result.
  function automatic logic reduced_cancel_risk_fp32(
    input operation_e op, input logic op_mod,
    input logic [31:0] a, input logic [31:0] b, input logic [31:0] c
  );
    automatic int signed e_prod, e_c, d;
    automatic logic sign_prod, sign_c, prod_zero, c_zero;
    reduced_cancel_risk_fp32 = 1'b0;
    if (op == MUL) return reduced_cancel_risk_fp32;
    if (reduced_fp32_special(a) || reduced_fp32_special(b) || reduced_fp32_special(c))
      return reduced_cancel_risk_fp32;

    sign_c = c[31] ^ op_mod;
    c_zero = reduced_fp32_zero(c);
    if (op == ADD) begin
      sign_prod = b[31];
      prod_zero = reduced_fp32_zero(b);
      e_prod    = int'(b[30:23]);
    end else begin
      sign_prod = a[31] ^ b[31];
      if (op == FNMSUB) sign_prod = ~sign_prod;
      prod_zero = reduced_fp32_zero(a) || reduced_fp32_zero(b);
      e_prod    = int'(a[30:23]) + int'(b[30:23]) - 127;
    end
    e_c = int'(c[30:23]);
    if (!prod_zero && !c_zero && (sign_prod != sign_c)) begin
      d = e_prod - e_c;
      if (d < 0) d = -d;
      if (d <= int'(ReducedCancelExpWindowFp32)) reduced_cancel_risk_fp32 = 1'b1;
    end
  endfunction

  function automatic logic reduced_cancel_risk_fp16(
    input operation_e op, input logic op_mod,
    input logic [15:0] a, input logic [15:0] b, input logic [15:0] c
  );
    automatic int signed e_prod, e_c, d;
    automatic logic sign_prod, sign_c, prod_zero, c_zero;
    reduced_cancel_risk_fp16 = 1'b0;
    if (op == MUL) return reduced_cancel_risk_fp16;
    if (reduced_fp16_special(a) || reduced_fp16_special(b) || reduced_fp16_special(c))
      return reduced_cancel_risk_fp16;

    sign_c = c[15] ^ op_mod;
    c_zero = reduced_fp16_zero(c);
    if (op == ADD) begin
      sign_prod = b[15];
      prod_zero = reduced_fp16_zero(b);
      e_prod    = int'(b[14:10]);
    end else begin
      sign_prod = a[15] ^ b[15];
      if (op == FNMSUB) sign_prod = ~sign_prod;
      prod_zero = reduced_fp16_zero(a) || reduced_fp16_zero(b);
      e_prod    = int'(a[14:10]) + int'(b[14:10]) - 15;
    end
    e_c = int'(c[14:10]);
    if (!prod_zero && !c_zero && (sign_prod != sign_c)) begin
      d = e_prod - e_c;
      if (d < 0) d = -d;
      if (d <= int'(ReducedCancelExpWindowFp16)) reduced_cancel_risk_fp16 = 1'b1;
    end
  endfunction

  function automatic logic [15:0] reduced_ordered16(input logic [15:0] x);
    return x[15] ? ~x : (x | 16'h8000);
  endfunction

  function automatic logic [7:0] reduced_ordered8(input logic [7:0] x);
    return x[7] ? ~x : (x | 8'h80);
  endfunction

  function automatic logic reduced_nan16(input logic [15:0] x);
    return (x[14:7] == 8'hff) && (x[6:0] != 7'd0);
  endfunction

  function automatic logic reduced_zero16(input logic [15:0] x);
    return x[14:0] == 15'd0;
  endfunction

  function automatic logic reduced_nan8(input logic [7:0] x);
    return (x[6:2] == 5'h1f) && (x[1:0] != 2'd0);
  endfunction

  function automatic logic reduced_zero8(input logic [7:0] x);
    return x[6:0] == 7'd0;
  endfunction

  function automatic logic reduced_mismatch16(
    input logic [15:0] a, input logic [15:0] b, input int unsigned tol
  );
    automatic logic [15:0] oa, ob, diff;
    if (reduced_nan16(a) || reduced_nan16(b))
      return !(reduced_nan16(a) && reduced_nan16(b));
    if (reduced_zero16(a) && reduced_zero16(b)) return 1'b0;
    oa   = reduced_ordered16(a);
    ob   = reduced_ordered16(b);
    diff = (oa >= ob) ? (oa - ob) : (ob - oa);
    return diff > 16'(tol);
  endfunction

  function automatic logic reduced_mismatch8(
    input logic [7:0] a, input logic [7:0] b, input int unsigned tol
  );
    automatic logic [7:0] oa, ob, diff;
    if (reduced_nan8(a) || reduced_nan8(b))
      return !(reduced_nan8(a) && reduced_nan8(b));
    if (reduced_zero8(a) && reduced_zero8(b)) return 1'b0;
    oa   = reduced_ordered8(a);
    ob   = reduced_ordered8(b);
    diff = (oa >= ob) ? (oa - ob) : (ob - oa);
    return diff > 8'(tol);
  endfunction

  logic [2:0][31:0] reduced_addmul_operands;
  fp_format_e       reduced_addmul_fmt;
  logic             reduced_addmul_vectorial;
  reduced_addmul_tag_t reduced_addmul_tag_i, reduced_addmul_tag_o;
  logic [31:0]      reduced_addmul_result;
  logic             reduced_addmul_in_ready;
  logic             reduced_addmul_out_valid;
  logic             reduced_addmul_busy;
  logic             reduced_addmul_supported;

  always_comb begin
    reduced_addmul_operands = '0;
    reduced_addmul_fmt      = FP8;
    reduced_addmul_vectorial = vectorial_op_i;
    reduced_addmul_supported = 1'b0;
    reduced_addmul_tag_i.real_fmt   = src_fmt_i;
    reduced_addmul_tag_i.vectorial  = vectorial_op_i;
    reduced_addmul_tag_i.bypass_lane = '0;
    reduced_addmul_tag_i.id          = expected_id_i;
    reduced_addmul_tag_i.exp_bound   = '0;
    reduced_addmul_tag_i.exp_bound32 = '0;

    unique case (src_fmt_i)
      FP32: begin
        reduced_addmul_supported  = 1'b1;
        reduced_addmul_fmt        = FP16ALT;
        // A real FP32 element maps to one scalar E8M7 element.  Upper bits
        // are don't-care because is_boxed_i is forced valid below.
        reduced_addmul_vectorial  = 1'b0;
        reduced_addmul_operands[0][15:0] = reduced_fp32_to_fp16alt(operand1_i[31:0]);
        reduced_addmul_operands[1][15:0] = reduced_fp32_to_fp16alt(operand2_i[31:0]);
        reduced_addmul_operands[2][15:0] = reduced_fp32_to_fp16alt(operand3_i[31:0]);

        reduced_addmul_tag_i.bypass_lane[0] =
          reduced_cancel_risk_fp32(op_i, op_mod_i, operand1_i[31:0], operand2_i[31:0], operand3_i[31:0]);
        reduced_addmul_tag_i.exp_bound32 =
          reduced_fp32_exp_bound(op_i, operand1_i[31:0], operand2_i[31:0], operand3_i[31:0]);
        unique case (op_i)
          ADD: reduced_addmul_tag_i.bypass_lane[0] |=
                 reduced_fp32_subnormal(operand2_i[31:0]) || reduced_fp32_subnormal(operand3_i[31:0]);
          MUL: reduced_addmul_tag_i.bypass_lane[0] |=
                 reduced_fp32_subnormal(operand1_i[31:0]) || reduced_fp32_subnormal(operand2_i[31:0]);
          default: reduced_addmul_tag_i.bypass_lane[0] |=
                 reduced_fp32_subnormal(operand1_i[31:0]) || reduced_fp32_subnormal(operand2_i[31:0]) ||
                 reduced_fp32_subnormal(operand3_i[31:0]);
        endcase
      end

      FP16: begin
        reduced_addmul_supported = 1'b1;
        reduced_addmul_fmt       = FP8;
        for (int lane = 0; lane < 2; lane++) begin
          automatic logic [15:0] a, b, c;
          a = operand1_i >> (16 * lane);
          b = operand2_i >> (16 * lane);
          c = operand3_i >> (16 * lane);
          reduced_addmul_operands[0][8*lane +: 8] = reduced_fp16_to_fp8(a);
          reduced_addmul_operands[1][8*lane +: 8] = reduced_fp16_to_fp8(b);
          reduced_addmul_operands[2][8*lane +: 8] = reduced_fp16_to_fp8(c);
          reduced_addmul_tag_i.bypass_lane[lane] = reduced_cancel_risk_fp16(op_i, op_mod_i, a, b, c);
          reduced_addmul_tag_i.exp_bound[lane]   = reduced_fp16_exp_bound(op_i, a, b, c);
          unique case (op_i)
            ADD: reduced_addmul_tag_i.bypass_lane[lane] |= reduced_fp16_subnormal(b) || reduced_fp16_subnormal(c);
            MUL: reduced_addmul_tag_i.bypass_lane[lane] |= reduced_fp16_subnormal(a) || reduced_fp16_subnormal(b);
            default: reduced_addmul_tag_i.bypass_lane[lane] |=
                       reduced_fp16_subnormal(a) || reduced_fp16_subnormal(b) || reduced_fp16_subnormal(c);
          endcase
        end
      end

      FP8: begin
        reduced_addmul_supported = 1'b1;
        reduced_addmul_fmt       = FP8;
        reduced_addmul_operands[0] = operand1_i;
        reduced_addmul_operands[1] = operand2_i;
        reduced_addmul_operands[2] = operand3_i;
        // No bypass_lane classification (stays at its default '0 from the
        // top of this block) -- see ReducedFp8ExactTol's comment:
        // exact precision means there is no cancellation-driven
        // quantization-noise mechanism here to classify against.
      end

      FP16ALT: begin
        reduced_addmul_supported = 1'b1;
        reduced_addmul_fmt       = FP16ALT;
        // No quantization: FP16ALT->FP16ALT is exact precision, and the
        // 32-bit operand words already pack up to 2 native 16-bit FP16ALT
        // elements exactly where fpnew expects them for a vectorial op --
        // same passthrough as the native FP8 case above, just at 16 bits.
        reduced_addmul_operands[0] = operand1_i;
        reduced_addmul_operands[1] = operand2_i;
        reduced_addmul_operands[2] = operand3_i;
        // Deliberately no bypass_lane classification (stays at its default
        // '0 from the top of this block) -- see ReducedFp16AltExactTol's
        // comment: exact precision means there is no cancellation-driven
        // quantization-noise mechanism here to classify against.
      end

      default: begin
        reduced_addmul_supported = 1'b0;
      end
    endcase
  end

  fpnew_opgroup_block #(
    .OpGroup       ( ADDMUL                 ),
    .Width         ( ElenWidth              ),
    .EnableVectors ( 1'b1                   ),
    .FpFmtMask     ( ReducedAddmulFmtMask   ),
    .IntFmtMask    ( FPUFeatures.IntFmtMask ),
    .FmtPipeRegs   ( ReducedAddmulPipeRegs  ),
    .FmtUnitTypes  ( ReducedAddmulUnitTypes ),
    .PipeConfig    ( FPUImplementation.PipeConfig ),
    .TagType       ( reduced_addmul_tag_t   )
  ) i_reduced_addmul_shadow (
    .clk_i,
    .rst_ni,
    .hart_id_i      ( '0 ),
    .operands_i     ( reduced_addmul_operands ),
    .is_boxed_i     ( '1 ),
    .rnd_mode_i,
    .op_i,
    .op_mod_i,
    .src_fmt_i      ( reduced_addmul_fmt ),
    .dst_fmt_i      ( reduced_addmul_fmt ),
    .int_fmt_i,
    .vectorial_op_i ( reduced_addmul_vectorial ),
    .tag_i          ( reduced_addmul_tag_i ),
    .simd_mask_i    ( '1 ),
    // Inject only transactions already accepted by the real FPU.  With the
    // same pipeline depth/backpressure the reduced block remains in lockstep.
    .in_valid_i     ( in_valid_i && in_ready_i && is_addmul_op && reduced_addmul_supported ),
    .in_ready_o     ( reduced_addmul_in_ready ),
    .flush_i        ( 1'b0 ),
    .result_o       ( reduced_addmul_result ),
    .status_o       ( /* unused */ ),
    .extension_bit_o( /* unused */ ),
    .tag_o          ( reduced_addmul_tag_o ),
    .out_valid_o    ( reduced_addmul_out_valid ),
    .out_ready_i    ( result_ready_i ),
    .busy_o         ( reduced_addmul_busy )
  );

  // ---------------------------------------------------------------------
  // Deferred compare stage (ADDMUL/DOTP only).
  //
  // result_i is NOT a fresh, full-budget signal by the time it reaches this
  // module: fpnew_top's own final result mux (the round-robin arbiter
  // combining per-opgroup outputs, fpnew_opgroup_block.sv/fpnew_top.sv) is
  // combinational, not registered, so result_i already carries that tail
  // delay before this module even sees it -- confirmed historically in this
  // exact file (see the "Two deferred compare-pipeline stages" / "Output
  // register" comments this module used to carry, back when the old
  // hand-written ADDMUL/DOTP datapath needed two full deferred stages to
  // fit its own decode+compare logic in the remaining budget: "fpnew's own
  // remaining tail logic at ~0.7ns... against a ~1.1ns per-cycle budget").
  //
  // CONV's and NONCOMP's comparators stay combinational on result_i
  // directly: both are a bare bit-pattern equality, the specific case
  // that history already validated as cheap enough to share a cycle with
  // fpnew's tail. reduced_mismatch16/8 (decode + two sign-conditional
  // ordered-code transforms + subtract + threshold compare) and
  // reduced_dotp_exp_mismatch (field extraction + 4 special-case checks +
  // subtract + compare) are NOT that cheap -- real, un-measured risk of
  // repeating the same violation, so their result_i-side work is deferred
  // one cycle: capture result_i/result_valid_i and each shadow's own
  // (transient) result_o/tag_o together, then run the actual comparison
  // off the registered copies next cycle. Safe to add a cycle of latency
  // here for the same reason the old two-stage split was: fault_o never
  // gates a VRF write or a retirement handshake, only a diagnostic counter
  // and a sticky, software-masked interrupt bit.
  // ---------------------------------------------------------------------
  logic [ElenWidth-1:0] result_i_q;
  logic                 result_valid_q;
  logic [IdWidth-1:0]   result_id_i_q;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      result_i_q     <= '0;
      result_valid_q <= 1'b0;
      result_id_i_q  <= '0;
    end else begin
      result_i_q     <= result_i;
      result_valid_q <= result_valid_i;
      result_id_i_q  <= result_id_i;
    end
  end

  logic                 reduced_addmul_cmp_valid_q;
  reduced_addmul_tag_t  reduced_addmul_tag_q;
  logic [31:0]          reduced_addmul_result_q;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      reduced_addmul_cmp_valid_q <= 1'b0;
      reduced_addmul_tag_q.real_fmt    <= fpnew_pkg::FP32;
      reduced_addmul_tag_q.vectorial   <= 1'b0;
      reduced_addmul_tag_q.bypass_lane <= '0;
      reduced_addmul_tag_q.id          <= '0;
      reduced_addmul_tag_q.exp_bound   <= '0;
      reduced_addmul_tag_q.exp_bound32 <= '0;
      reduced_addmul_result_q    <= '0;
    end else begin
      // Captured alongside result_i_q above: the same condition that used
      // to gate the (now-deferred) comparison directly, just latched a
      // cycle earlier instead of re-sampled a cycle later.
      reduced_addmul_cmp_valid_q <= reduced_addmul_out_valid && result_ready_i;
      reduced_addmul_tag_q       <= reduced_addmul_tag_o;
      reduced_addmul_result_q    <= reduced_addmul_result;
    end
  end

  logic reduced_addmul_fault_comb;
  always_comb begin
    reduced_addmul_fault_comb = 1'b0;

    if (reduced_addmul_cmp_valid_q) begin
      // The shadow's own tracked instruction id must match the id attached
      // to whatever real result is being sampled this cycle. If it doesn't,
      // the two pipelines have desynced -- e.g. a corrupted op/format on the
      // real side routed this instruction through a different-latency
      // opgroup than ADDMUL, so the fixed-delay result_i_q/result_valid_q
      // sample below is no longer guaranteed to hold *this* instruction's
      // result. Comparing shadow-vs-real under that condition would silently
      // check unrelated instructions against each other (can pass OR fail
      // by coincidence) rather than actually verifying anything -- flag it
      // directly instead.
      if (reduced_addmul_tag_q.id != result_id_i_q) begin
        reduced_addmul_fault_comb = 1'b1;
      // A shadow result without the corresponding real result indicates a
      // control/latency mismatch and is conservatively reported.
      end else if (!result_valid_q) begin
        reduced_addmul_fault_comb = 1'b1;
      end else begin
        unique case (reduced_addmul_tag_q.real_fmt)
          FP32: begin
            automatic logic [15:0] real_q;
            real_q = reduced_fp32_to_fp16alt(result_i_q[31:0]);
            if (!reduced_fp32_subnormal(result_i_q[31:0])) begin
              // Bypass-classified instructions get the input-derived
              // plausibility bound instead of a full skip -- same as the
              // FP16/DOTP treatment above (see reduced_fp32_exp_bound).
              if (reduced_addmul_tag_q.bypass_lane[0]) begin
                if (reduced_fp32_exp_bound_violation(reduced_addmul_tag_q.exp_bound32, result_i_q[31:0]))
                  reduced_addmul_fault_comb = 1'b1;
              end else if (reduced_mismatch16(reduced_addmul_result_q[15:0], real_q, ReducedFp32Ulp)) begin
                reduced_addmul_fault_comb = 1'b1;
              end
            end
          end

          FP16: begin
            automatic int unsigned lanes;
            lanes = reduced_addmul_tag_q.vectorial ? 2 : 1;
            for (int lane = 0; lane < 2; lane++) begin
              if (lane < lanes) begin
                automatic logic [15:0] real_lane;
                automatic logic [7:0]  real_q;
                automatic logic        lane_mismatch;
                real_lane = result_i_q >> (16 * lane);
                real_q    = reduced_fp16_to_fp8(real_lane);
                if (!reduced_fp16_subnormal(real_lane)) begin
                  // Cancellation/subnormal-risk lanes get a plausibility
                  // bound on the real result's own exponent (derived from
                  // the trusted input operands, see reduced_fp16_exp_bound)
                  // instead of a full skip; other lanes keep the tight
                  // ordered-code compare against ReducedFp16Ulp.
                  if (reduced_addmul_tag_q.bypass_lane[lane])
                    lane_mismatch = reduced_fp16_exp_bound_violation(
                      reduced_addmul_tag_q.exp_bound[lane], real_lane);
                  else
                    lane_mismatch = reduced_mismatch8(reduced_addmul_result_q[8*lane +: 8], real_q, ReducedFp16Ulp);
                end else begin
                  lane_mismatch = 1'b0;
                end
                if (lane_mismatch) begin
                  reduced_addmul_fault_comb = 1'b1;
                  `ifndef SYNTHESIS
                    $display(
                    "[REDUCED_ADDMUL_FP16] lane=%0d shadow=%h real16=%h real_q=%h real_fmt=%0d vectorial=%0d result_valid=%0d result_ready=%0d bypass=%0d exp_bound=%0d",
                    lane,
                    reduced_addmul_result[8*lane +: 8],
                    real_lane,
                    real_q,
                    reduced_addmul_tag_o.real_fmt,
                    reduced_addmul_tag_o.vectorial,
                    result_valid_i,
                    result_ready_i,
                    reduced_addmul_tag_q.bypass_lane[lane],
                    reduced_addmul_tag_q.exp_bound[lane]
                    );
                  `endif
                end
              end
            end
          end

          FP8: begin
            automatic int unsigned lanes;
            lanes = reduced_addmul_tag_q.vectorial ? 4 : 1;
            for (int lane = 0; lane < 4; lane++) begin
              if (lane < lanes) begin
                automatic logic [7:0] real_lane;
                automatic logic [7:0] shadow_byte;
                real_lane   = result_i_q >> (8 * lane);
                shadow_byte = reduced_addmul_result_q[8*lane +: 8];
                // Uniform EXACT compare (sign+exp+both mantissa bits), every
                // lane -- no bypass classification (see ReducedFp8ExactTol's
                // comment for why exact precision doesn't need one).
                if (!reduced_fp8_subnormal(real_lane) &&
                    reduced_mismatch8(shadow_byte, real_lane, ReducedFp8ExactTol)) begin
                  reduced_addmul_fault_comb = 1'b1;
                //   `ifndef SYNTHESIS
                //     $display(
                //         "[REDUCED_ADDMUL_FP8] lane=%0d shadow=%h real=%h op=%0d vectorial=%0d result_valid=%0d result_ready=%0d",
                //         lane,
                //         shadow_byte,
                //         real_lane,
                //         op_i,
                //         reduced_addmul_tag_o.vectorial,
                //         result_valid_i,
                //         result_ready_i);
                // `endif
                end
              end
            end
          end

          FP16ALT: begin
            automatic int unsigned lanes;
            lanes = reduced_addmul_tag_q.vectorial ? 2 : 1;
            for (int lane = 0; lane < 2; lane++) begin
              if (lane < lanes) begin
                automatic logic [15:0] real_lane;
                automatic logic [15:0] shadow_lane;
                real_lane   = result_i_q >> (16 * lane);
                shadow_lane = reduced_addmul_result_q[16*lane +: 16];
                // Uniform EXACT compare (sign+exp+both mantissa bits), every
                // lane -- no bypass classification (see
                // ReducedFp16AltExactTol's comment for why exact precision
                // doesn't need one).
                if (!reduced_fp16alt_subnormal(real_lane) &&
                    reduced_mismatch16(shadow_lane, real_lane, ReducedFp16AltExactTol)) begin
                  reduced_addmul_fault_comb = 1'b1;
                  `ifndef SYNTHESIS
                    $display(
                    "[REDUCED_ADDMUL_FP16ALT] lane=%0d shadow=%h real=%h vectorial=%0d result_valid=%0d result_ready=%0d",
                    lane,
                    shadow_lane,
                    real_lane,
                    reduced_addmul_tag_o.vectorial,
                    result_valid_i,
                    result_ready_i
                    );
                  `endif
                end
              end
            end
          end

          default: begin
            // No reduced ADDMUL format is defined for other real formats.
            // Such an operation is not injected into this shadow block.
          end
        endcase
      end
    end
  end

`ifndef SYNTHESIS
  // Integration diagnostics.  These should never print in a fault-free run.
  always_ff @(posedge clk_i) begin
    if (rst_ni) begin
      if (in_valid_i && in_ready_i && is_addmul_op && reduced_addmul_supported && !reduced_addmul_in_ready)
        $display("[REDUCED_ADDMUL] shadow not ready when real FPU accepted an operation");
      if (reduced_addmul_out_valid && result_ready_i && !result_valid_i)
        $display("[REDUCED_ADDMUL] shadow output valid without matching real result_valid");
    end
  end
`endif

  // FAULT-COVERAGE FIX: the real FPU can accept an ADDMUL op on a cycle the
  // shadow's own reduced block isn't ready for (lockstep desync) -- when
  // that happens, this operation's real result is never injected into the
  // shadow pipeline and reduced_addmul_fault_comb above never runs a
  // comparison for it at all (that block only fires reactively once the
  // shadow itself produces a result). That silent gap is exactly how a
  // fault that desyncs the two pipelines (e.g. by corrupting src_fmt_i so
  // the real FPU takes an unsupported-format path with different timing)
  // can commit an uncompared, arbitrarily wrong result. Promote the
  // existing (previously debug-only) drop condition into a real fault
  // instead of just a $display.
  logic reduced_addmul_dropped_op;
  assign reduced_addmul_dropped_op =
    in_valid_i && in_ready_i && is_addmul_op &&
    reduced_addmul_supported && !reduced_addmul_in_ready;


  // =====================================================================
  // Reduced-precision FPnew DOTP shadow (replacement for legacy DOTP)
  // =====================================================================
  // First reduced DOTP target:
  //   real   : FP16 x FP16 + FP32 accumulator -> FP32
  //   shadow : FP8  x FP8  + FP16ALT accumulator -> FP16ALT
  //
  // This mapping preserves the exponent widths of BOTH source and destination:
  //   FP16     E5M10 -> FP8     E5M2
  //   FP32     E8M23 -> FP16ALT E8M7
  // while reusing fpnew's native SDOTP datapath and rounding/normalization.
  //
  // A direct fpnew_sdotp_multi_wrapper is used with LaneWidth=16 instead of a
  // 32-bit fpnew_opgroup_block.  This is important for area: the generic DOTP
  // opgroup requires FP32 to be present in its format mask and would therefore
  // also elaborate the wider FP16/FP16ALT -> FP32 path.  The direct 16-bit
  // wrapper can be configured only for FP8 source + FP16ALT destination.
  //
  // Other real SDOTP format pairs are intentionally bypassed in this first
  // version.  That is a coverage trade-off, not a functional error: the user
  // requirement is to prioritize essentially-zero false positives.

  localparam int unsigned ReducedDotpExpSlack = 1;
  // Second real SDOTP mapping: src_fmt_i==FP8, dst_fmt_i==FP16ALT (a real
  // FP8xFP8 widening dot-product, e.g. sdotp-bp-fmatmul with the kernel set
  // to fm.dst=1 -- see main.c). Unlike the FP16->FP32 mapping above, this
  // one needs NO quantization at all: the shadow's own fpnew_sdotp_multi_
  // wrapper is already fixed at FP8xFP8->FP16ALT, which is an EXACT match
  // to both the real multiplicands (FP8) and the real accumulator
  // (FP16ALT) -- same reasoning as ADDMUL's native FP16ALT case above
  // (which now uses a full exact compare instead; this DOTP mapping keeps
  // the older exponent-only reduced_fp16alt_exp_mismatch for now, with its
  // own slack, uniformly on every instruction -- no bypass/cancel-risk
  // classification, matching the same "exact precision needs no
  // cancellation-noise protection" argument).
  localparam int unsigned ReducedDotpFp8ExpSlack = 1;

  typedef struct packed {
    logic bypass;
    // Carried through purely to detect real/shadow misalignment at compare
    // time -- see expected_id_i/result_id_i above.
    logic [IdWidth-1:0] id;
    // Conservative upper bound on the result's biased FP32/FP16ALT (shared
    // E8, bias-127) exponent field, derived from the trusted full-precision
    // input operands at accept time -- see reduced_dotp_exp_bound below.
    // Same technique as reduced_addmul_tag_t.exp_bound, ported here for the
    // same reason: reduced_dotp_exp_mismatch's shadow-vs-real value compare
    // is skipped entirely on bypass-classified (cancellation-risk/subnormal)
    // instructions, which must not mean skipping verification altogether.
    logic [7:0] exp_bound;
    // Which real mapping produced this shadow result -- FP16->FP32 (the
    // above bypass/exp_bound/exp_mismatch machinery) or FP8->FP16ALT (a
    // direct, uniform reduced_fp16alt_exp_mismatch compare, no bypass).
    // Needed because result_i_q's width/layout and the compare function
    // both differ between the two mappings, and both share this one
    // fpnew_sdotp_multi_wrapper instance.
    logic real_is_fp8;
  } reduced_dotp_tag_t;

  function automatic fmt_logic_t reduced_dotp_fmt_mask();
    automatic fmt_logic_t m;
    m = '0;
    m[FP8]     = 1'b1;
    m[FP16ALT] = 1'b1;
    return m;
  endfunction

  localparam fmt_logic_t ReducedDotpFmtMask = reduced_dotp_fmt_mask();

  // Conservative cancellation detector for a0*b0 + a1*b1 + c.
  // We deliberately bypass every mixed-sign non-zero sum.  This is much more
  // conservative than the legacy dynamic-range heuristic, but it removes the
  // hardest source of approximation false positives.  Coverage can be widened
  // later after fault-free regression is clean.
  function automatic logic reduced_dotp_bypass_fp16(
    input logic [15:0] a0,
    input logic [15:0] b0,
    input logic [15:0] a1,
    input logic [15:0] b1,
    input logic [31:0] c
  );
    automatic logic p0_zero, p1_zero, c_zero;
    automatic logic p0_sign, p1_sign, c_sign;
    automatic logic have_pos, have_neg;

    reduced_dotp_bypass_fp16 = 1'b0;

    // Reduced input conversion is deliberately not trusted for subnormals or
    // specials.  Passing these cases is preferable to generating a false alarm.
    if (reduced_fp16_subnormal(a0) || reduced_fp16_subnormal(b0) ||
        reduced_fp16_subnormal(a1) || reduced_fp16_subnormal(b1) ||
        reduced_fp32_subnormal(c)  ||
        reduced_fp16_special(a0) || reduced_fp16_special(b0) ||
        reduced_fp16_special(a1) || reduced_fp16_special(b1) ||
        reduced_fp32_special(c))
      return 1'b1;

    p0_zero = reduced_fp16_zero(a0) || reduced_fp16_zero(b0);
    p1_zero = reduced_fp16_zero(a1) || reduced_fp16_zero(b1);
    c_zero  = reduced_fp32_zero(c);

    p0_sign = a0[15] ^ b0[15];
    p1_sign = a1[15] ^ b1[15];
    c_sign  = c[31];

    have_pos = (!p0_zero && !p0_sign) ||
               (!p1_zero && !p1_sign) ||
               (!c_zero  && !c_sign);
    have_neg = (!p0_zero &&  p0_sign) ||
               (!p1_zero &&  p1_sign) ||
               (!c_zero  &&  c_sign);

    if (have_pos && have_neg)
      reduced_dotp_bypass_fp16 = 1'b1;
  endfunction

  // Exponent-only comparison requested for the reduced shadow.  Since FP32
  // and FP16ALT both use an 8-bit exponent, no rebiasing is necessary.
  // +/- ReducedDotpExpSlack raw exponent steps are accepted to absorb the
  // occasional one-bin shift caused by severe mantissa reduction/rounding.
  function automatic logic reduced_dotp_exp_mismatch(
    input logic [15:0] shadow,
    input logic [31:0] real_result
  );
    automatic logic [7:0] sh_exp, real_exp;
    automatic logic [8:0] diff;

    sh_exp   = shadow[14:7];
    real_exp = real_result[30:23];

    // Only exempt on the SHADOW's own zero/subnormal boundary (sh_exp==0) --
    // its arithmetic legitimately collapsing toward zero under real
    // cancellation/underflow is a known, benign artifact of its own reduced
    // precision (this mapping DOES have real quantization loss, unlike
    // FP8's/FP16ALT's exact ADDMUL compares above, which is why only this
    // one and reduced_dotp_exp_bound_violation below still carry this kind
    // of carve-out at all). Deliberately NOT exempting on real_exp at either
    // boundary (or
    // on sh_exp==0xff): real_result is the untrusted value under test, and a
    // fault pushing it to 0/0x1f -- e.g. a corrupted output-routing tag that
    // silently swaps in an unrelated all-zero result register -- is exactly
    // the gross corruption this check exists to catch, not wave through.
    if (sh_exp == 8'h00)
      return 1'b0;

    diff = (sh_exp >= real_exp) ? (sh_exp - real_exp) : (real_exp - sh_exp);
    return diff > 9'(ReducedDotpExpSlack);
  endfunction

  // Plausibility bound for bypass-classified (cancellation-risk/subnormal)
  // DOTP instructions, used instead of a full skip -- same rationale and
  // technique as reduced_fp16_exp_bound above (ADDMUL): reduced_dotp_result
  // is quantized down to FP8xFP8->FP16ALT, so under real cancellation its
  // own exponent can diverge from the real FP16xFP16->FP32 result by an
  // unbounded number of bins with no fixed slack able to separate that from
  // an actual fault. Bound the real result's exponent instead using only
  // the trusted, full-precision FP16 input operands (already available):
  // a result of a0*b0 + a1*b1 + c can't legitimately land far above the
  // largest of the two products' or the accumulator's own magnitude,
  // regardless of how precisely the terms cancel.
  function automatic logic [7:0] reduced_dotp_exp_bound(
    input logic [15:0] a0, input logic [15:0] b0,
    input logic [15:0] a1, input logic [15:0] b1,
    input logic [31:0] c
  );
    automatic int signed e_prod0, e_prod1, e_c, e_max;
    // Each FP16 product's mathematical exponent, re-biased from FP16's E5
    // (bias 15) field into the FP32/FP16ALT shared E8 (bias 127) field:
    // true_unbiased = ea+eb-30 (two bias-15 terms combined), then
    // +127 to rebias -- folds to a flat +97 offset.
    e_prod0 = int'(a0[14:10]) + int'(b0[14:10]) + 97;
    e_prod1 = int'(a1[14:10]) + int'(b1[14:10]) + 97;
    e_c     = int'(c[30:23]);
    e_max   = e_prod0;
    if (e_prod1 > e_max) e_max = e_prod1;
    if (e_c     > e_max) e_max = e_c;
    // Margin for legitimate exponent growth from summing three terms
    // (two products plus the accumulator), not for numerical noise -- the
    // compare on the other end is against the real, full-precision result.
    e_max += 3;
    if (e_max < 0)   e_max = 0;
    if (e_max > 254) e_max = 254; // stay below the all-ones (inf/nan) encoding
    return 8'(e_max);
  endfunction

  function automatic logic reduced_dotp_exp_bound_violation(
    input logic [7:0] bound, input logic [31:0] real_val
  );
    automatic logic [7:0] real_exp;
    real_exp = real_val[30:23];
    if (real_exp == 8'hff) return 1'b0; // inf/nan/special -- not this check's job
    return real_exp > bound;
  endfunction

  logic [2:0][15:0] reduced_dotp_operands;
  reduced_dotp_tag_t reduced_dotp_tag_i, reduced_dotp_tag_o;
  logic [15:0] reduced_dotp_result;
  logic        reduced_dotp_in_ready;
  logic        reduced_dotp_out_valid;
  logic        reduced_dotp_busy;
  logic        reduced_dotp_supported;

  always_comb begin
    automatic logic [15:0] a0, a1, b0, b1;

    reduced_dotp_operands  = '0;
    reduced_dotp_supported = 1'b0;
    reduced_dotp_tag_i     = '{default: '0};
    reduced_dotp_tag_i.id  = expected_id_i;

    // Native SDOTP packing in fpnew is:
    //   operand1 = {a1, a0}, operand2 = {b1, b0}, operand3 = wide accumulator.
    a0 = operand1_i[15:0];
    a1 = operand1_i[31:16];
    b0 = operand2_i[15:0];
    b1 = operand2_i[31:16];

    if (is_dotp_op && (src_fmt_i == FP16) && (dst_fmt_i == FP32)) begin
      reduced_dotp_supported = 1'b1;

      // Two real FP16 multiplicands are quantized and packed as two FP8 values.
      reduced_dotp_operands[0][7:0]  = reduced_fp16_to_fp8(a0);
      reduced_dotp_operands[0][15:8] = reduced_fp16_to_fp8(a1);
      reduced_dotp_operands[1][7:0]  = reduced_fp16_to_fp8(b0);
      reduced_dotp_operands[1][15:8] = reduced_fp16_to_fp8(b1);

      // FP32 accumulator -> FP16ALT accumulator, preserving E8 exponent range.
      reduced_dotp_operands[2] = reduced_fp32_to_fp16alt(operand3_i[31:0]);

      reduced_dotp_tag_i.bypass     = reduced_dotp_bypass_fp16(a0, b0, a1, b1, operand3_i[31:0]);
      reduced_dotp_tag_i.exp_bound  = reduced_dotp_exp_bound(a0, b0, a1, b1, operand3_i[31:0]);
      reduced_dotp_tag_i.real_is_fp8 = 1'b0;
    end else if (is_dotp_op && (src_fmt_i == FP8) && (dst_fmt_i == FP16ALT)) begin
      reduced_dotp_supported = 1'b1;

      // Real multiplicands are already FP8 -- exact match to the shadow's
      // own fixed FP8 source format, no quantization. Same a0/a1/b0/b1
      // packing convention as the FP16 case above (fpnew_sdotp_multi's own
      // operand extraction is width-parametrized: a0=operand1_i[FP_WIDTH-1:0],
      // a1=operand1_i[2*FP_WIDTH-1:FP_WIDTH], same for b0/b1 on operand2_i),
      // just at FP_WIDTH=8 instead of 16.
      reduced_dotp_operands[0][7:0]  = operand1_i[7:0];
      reduced_dotp_operands[0][15:8] = operand1_i[15:8];
      reduced_dotp_operands[1][7:0]  = operand2_i[7:0];
      reduced_dotp_operands[1][15:8] = operand2_i[15:8];

      // Real accumulator is already FP16ALT -- exact match, no quantization.
      reduced_dotp_operands[2] = operand3_i[15:0];

      // No bypass/cancel-risk classification -- see ReducedDotpFp8ExpSlack.
      reduced_dotp_tag_i.bypass       = 1'b0;
      reduced_dotp_tag_i.real_is_fp8  = 1'b1;
    end
  end

  fpnew_sdotp_multi_wrapper #(
    .LaneWidth   ( 16                    ),
    .FpFmtConfig ( ReducedDotpFmtMask    ),
    .NumPipeRegs ( DotpLatency           ),
    .PipeConfig  ( FPUImplementation.PipeConfig ),
    .TagType     ( reduced_dotp_tag_t    ),
    .AuxType     ( logic                 ),
    .StochasticRndImplementation ( fpnew_pkg::DEFAULT_NO_RSR )
  ) i_reduced_dotp_shadow (
    .clk_i,
    .rst_ni,
    .sdotp_hart_id_i ( '0 ),
    .operands_i      ( reduced_dotp_operands ),
    .is_boxed_i      ( '1 ),
    .rnd_mode_i,
    .op_i            ( fpnew_pkg::SDOTP ),
    .op_mod_i        ( op_mod_i ),
    .src_fmt_i       ( fpnew_pkg::FP8 ),
    .dst_fmt_i       ( fpnew_pkg::FP16ALT ),
    .tag_i           ( reduced_dotp_tag_i ),
    .mask_i          ( 1'b1 ),
    .aux_i           ( 1'b0 ),
    // Only mirror operations that the real FPU has already accepted.
    .in_valid_i      ( in_valid_i && in_ready_i && reduced_dotp_supported ),
    .in_ready_o      ( reduced_dotp_in_ready ),
    .flush_i         ( 1'b0 ),
    .result_o        ( reduced_dotp_result ),
    .status_o        ( /* unused */ ),
    .extension_bit_o ( /* unused */ ),
    .tag_o           ( reduced_dotp_tag_o ),
    .mask_o          ( /* unused */ ),
    .aux_o           ( /* unused */ ),
    .out_valid_o     ( reduced_dotp_out_valid ),
    .out_ready_i     ( result_ready_i ),
    .busy_o          ( reduced_dotp_busy )
  );

  // Same deferred-compare rationale as the ADDMUL stage above -- shares
  // this module's result_i_q/result_valid_q, adds only its own
  // result/tag/cmp_valid capture.
  logic               reduced_dotp_cmp_valid_q;
  reduced_dotp_tag_t  reduced_dotp_tag_q;
  logic [15:0]        reduced_dotp_result_q;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      reduced_dotp_cmp_valid_q <= 1'b0;
      reduced_dotp_tag_q       <= '{default: '0};
      reduced_dotp_result_q    <= '0;
    end else begin
      reduced_dotp_cmp_valid_q <= reduced_dotp_out_valid && result_ready_i;
      reduced_dotp_tag_q       <= reduced_dotp_tag_o;
      reduced_dotp_result_q    <= reduced_dotp_result;
    end
  end

  logic reduced_dotp_fault_comb;
  always_comb begin
    reduced_dotp_fault_comb = 1'b0;

    if (reduced_dotp_cmp_valid_q) begin
      // Same id-based misalignment check as reduced_addmul_fault_comb above.
      if (reduced_dotp_tag_q.id != result_id_i_q) begin
        reduced_dotp_fault_comb = 1'b1;
      end else if (!result_valid_q) begin
        // Shadow/real control misalignment is still a hard fault.
        reduced_dotp_fault_comb = 1'b1;
      // FP8->FP16ALT mapping: exact precision on both sides, so a single
      // uniform exponent-only compare (no bypass) -- see
      // ReducedDotpFp8ExpSlack. result_i_q's low 16 bits hold the real
      // FP16ALT accumulator directly, same placement convention as every
      // other narrow-format real result in this module.
      end else if (reduced_dotp_tag_q.real_is_fp8) begin
        if (!reduced_fp16alt_subnormal(result_i_q[15:0]) &&
            reduced_fp16alt_exp_mismatch(reduced_dotp_result_q, result_i_q[15:0], ReducedDotpFp8ExpSlack))
          reduced_dotp_fault_comb = 1'b1;
      // Bypass-classified instructions get the input-derived plausibility
      // bound instead of a full skip -- see reduced_dotp_exp_bound above.
      end else if (reduced_dotp_tag_q.bypass) begin
        if (reduced_dotp_exp_bound_violation(reduced_dotp_tag_q.exp_bound, result_i_q[31:0]))
          reduced_dotp_fault_comb = 1'b1;
      end else if (reduced_dotp_exp_mismatch(reduced_dotp_result_q, result_i_q[31:0])) begin
        reduced_dotp_fault_comb = 1'b1;
      end
    end
  end

`ifndef SYNTHESIS
  always_ff @(posedge clk_i) begin
    if (rst_ni) begin
      if (in_valid_i && in_ready_i && reduced_dotp_supported && !reduced_dotp_in_ready)
        $display("[REDUCED_DOTP] shadow not ready when real FPU accepted an operation");
      if (reduced_dotp_out_valid && result_ready_i && !result_valid_i)
        $display("[REDUCED_DOTP] shadow output valid without matching real result_valid");
    end
  end
`endif

  // FAULT-COVERAGE FIX: same lockstep-desync gap as reduced_addmul_dropped_op
  // above, for the DOTP shadow.
  logic reduced_dotp_dropped_op;
  assign reduced_dotp_dropped_op =
    in_valid_i && in_ready_i && reduced_dotp_supported && !reduced_dotp_in_ready;



  // ---------------------------------------------------------------------
  // CONV duplicate-and-compare (see the FPUFeatures/FPUImplementation
  // parameter comment above for why this group is protected by literal
  // duplication rather than a hand-written replica). Unlike the other
  // three mechanisms, no lane-iteration/width-masking/delay-line is
  // needed here: fpnew_opgroup_block handles vectorial sub-lane packing
  // internally (from vectorial_op_i), exactly like the real unit does --
  // comparing the full result_o against result_i directly is sufficient.
  //
  // is_boxed_i tied to '1: confirmed moot for Spatz's actual
  // configuration (ELEN=32, RVD=0) during the NONCOMP work above --
  // fpnew_top's own is_boxed is unconditionally 1 whenever the format
  // fills the full lane width or the op is vectorial, and both are
  // always true here. hart_id_i tied to '0: only affects stochastic
  // rounding, which Spatz's decoder never selects for any op (RISC-V frm
  // only ever encodes RNE/RTZ/RDN/RUP/RMM) -- a dead path, not an
  // unverified shortcut.
  // ---------------------------------------------------------------------
  logic [ElenWidth-1:0] conv_result;
  logic                 conv_out_valid, conv_in_ready, conv_busy;

  fpnew_opgroup_block #(
    .OpGroup      (CONV),
    .Width        (ElenWidth),
    .EnableVectors(FPUFeatures.EnableVectors),
    .FpFmtMask    (FPUFeatures.FpFmtMask),
    .IntFmtMask   (FPUFeatures.IntFmtMask),
    .FmtPipeRegs  (FPUImplementation.PipeRegs[CONV]),
    .FmtUnitTypes (FPUImplementation.UnitTypes[CONV]),
    .PipeConfig   (FPUImplementation.PipeConfig)
  ) i_conv_shadow (
    .clk_i          (clk_i),
    .rst_ni         (rst_ni),
    .hart_id_i      ('0),
    .operands_i     ({operand3_i, operand2_i, operand1_i}),
    .is_boxed_i     ('1),
    .rnd_mode_i     (rnd_mode_i),
    .op_i           (op_i),
    .op_mod_i       (op_mod_i),
    .src_fmt_i      (src_fmt_i),
    .dst_fmt_i      (dst_fmt_i),
    .int_fmt_i      (int_fmt_i),
    .vectorial_op_i (vectorial_op_i),
    .tag_i          ('0),
    .simd_mask_i    ('1),
    .in_valid_i     (in_valid_i && is_conv_op),
    .in_ready_o     (conv_in_ready),
    .flush_i        (1'b0),
    .result_o       (conv_result),
    .status_o       (/* unused */),
    .extension_bit_o(/* unused */),
    .tag_o          (/* unused */),
    .out_valid_o    (conv_out_valid),
    .out_ready_i    (result_ready_i),
    .busy_o         (conv_busy)
  );

  logic conv_fault_comb;
  assign conv_fault_comb = conv_out_valid && ((!result_valid_i) || (conv_result != result_i));


  // =====================================================================
  // Reduced-precision FPnew DIVSQRT shadow
  // =====================================================================
  // Same methodology as the ADDMUL/DOTP shadows: a real fpnew opgroup block
  // (OpGroup=DIVSQRT, the same C910/THMULTI divider type as the real FPU)
  // fed only with operations the real FPU has accepted, at reduced precision:
  //   real FP32    -> shadow FP16ALT (E8M7): mantissa truncated, exponent
  //                   field unchanged (reduced_fp32_to_fp16alt), checked with
  //                   an ordered-code ULP tolerance.
  //   real FP16    -> shadow FP16 (E5M10): same precision, EXACT compare.
  //   real FP16ALT -> shadow FP16ALT: same precision, EXACT compare (free:
  //                   the shadow needs FP16ALT for the FP32 mapping anyway).
  //   FP8/FP8ALT are not supported by the C910 divider at all.
  //
  // Difference to ADDMUL/DOTP: the divider is iterative and its latency
  // depends on the format (C910 SRT: ~15 rounds for single, fewer for half),
  // so the FP16ALT shadow of an FP32 op finishes EARLIER than the real one
  // and the two are not in lockstep. The real FPU holds at most one div/sqrt
  // in flight (the C910 unit is not pipelined; spatz_vfu additionally gates
  // FPU0 with divsqrt_inflight_q), so a single pending slot -- keyed by the
  // instruction id, like the ADDMUL/DOTP tag.id -- holds whichever result
  // arrives first until the other one is there. The compare itself is then
  // deferred one cycle off registered copies, as for ADDMUL/DOTP.
  //
  // Only elaborated where the given FPUImplementation actually enables the
  // DIVSQRT opgroup (with a shared divider: FPU0 only).

  // FP32 -> FP16ALT: each operand loses up to 1 ULP(E8M7) of relative
  // precision to truncation; a quotient/root of perturbed operands deviates
  // by at most ~2 ULP, plus the shadow's own rounding and the real result's
  // truncation. No cancellation mechanism exists for div/sqrt, so no
  // bypass/exp-bound classification is needed beyond subnormal operands.
  // Same value as ReducedFp32Ulp (ADDMUL). Unvalidated against a fault-free
  // div/sqrt kernel yet.
  localparam int unsigned ReducedDivsqrtFp32Ulp  = 4;
  // FP16/FP16ALT: same format and same divider on both sides -> bit-exact.
  localparam int unsigned ReducedDivsqrtExactTol = 0;

  localparam logic DivsqrtEnabled = fpnew_pkg::any_enabled_multi(
    FPUImplementation.UnitTypes[DIVSQRT], FPUFeatures.FpFmtMask);
  localparam int unsigned DivsqrtPipeRegs = fpnew_pkg::get_num_regs_multi(
    FPUImplementation.PipeRegs[DIVSQRT], FPUImplementation.UnitTypes[DIVSQRT], FPUFeatures.FpFmtMask);

  function automatic fmt_logic_t reduced_divsqrt_fmt_mask();
    automatic fmt_logic_t m;
    m = '0;
    m[FP16]    = 1'b1;
    m[FP16ALT] = 1'b1;
    return m;
  endfunction

  function automatic fmt_unsigned_t reduced_divsqrt_pipe_regs();
    automatic fmt_unsigned_t p;
    p = '{default: '0};
    // Same (merged) pipe depth as the real divider.
    p[FP16]    = 32'(DivsqrtPipeRegs);
    p[FP16ALT] = 32'(DivsqrtPipeRegs);
    return p;
  endfunction

  function automatic fmt_unit_types_t reduced_divsqrt_unit_types();
    automatic fmt_unit_types_t u;
    u = '{default: DISABLED};
    u[FP16]    = MERGED;
    u[FP16ALT] = MERGED;
    return u;
  endfunction

  // FP16 (E5M10) counterpart of reduced_mismatch16 (which decodes NaN with
  // the FP16ALT E8M7 layout). Same ordered-code comparison.
  function automatic logic reduced_mismatch_fp16(
    input logic [15:0] a, input logic [15:0] b, input int unsigned tol
  );
    automatic logic [15:0] oa, ob, diff;
    automatic logic nan_a, nan_b;
    nan_a = (a[14:10] == 5'h1f) && (a[9:0] != 10'd0);
    nan_b = (b[14:10] == 5'h1f) && (b[9:0] != 10'd0);
    if (nan_a || nan_b) return !(nan_a && nan_b);
    if (reduced_zero16(a) && reduced_zero16(b)) return 1'b0;
    oa   = reduced_ordered16(a);
    ob   = reduced_ordered16(b);
    diff = (oa >= ob) ? (oa - ob) : (ob - oa);
    return diff > 16'(tol);
  endfunction

  typedef struct packed {
    fp_format_e         real_fmt;
    logic               vectorial;
    // FP32 only: an operand is subnormal, so its FP16ALT truncation can
    // collapse to zero (e.g. x/0 = inf vs. a real huge value) -- skip.
    logic               bypass;
    logic [IdWidth-1:0] id;
  } reduced_divsqrt_tag_t;

  logic is_divsqrt_op;
  assign is_divsqrt_op = (op_i inside {DIV, SQRT}) && (src_fmt_i == dst_fmt_i);

  logic reduced_divsqrt_fault_comb;
  logic reduced_divsqrt_dropped_op;

  if (DivsqrtEnabled) begin : gen_reduced_divsqrt
    logic [1:0][31:0]     ds_operands;
    fp_format_e           ds_fmt;
    logic                 ds_vectorial;
    logic                 ds_supported;
    reduced_divsqrt_tag_t ds_tag_i;
    logic [31:0]          ds_result;
    logic                 ds_in_ready, ds_out_valid;

    always_comb begin
      ds_operands  = '0;
      ds_fmt       = FP16;
      ds_vectorial = vectorial_op_i;
      ds_supported = 1'b0;
      ds_tag_i.real_fmt  = src_fmt_i;
      ds_tag_i.vectorial = vectorial_op_i;
      ds_tag_i.bypass    = 1'b0;
      ds_tag_i.id        = expected_id_i;

      unique case (src_fmt_i)
        FP32: begin
          ds_supported = 1'b1;
          ds_fmt       = FP16ALT;
          // One real FP32 element maps to one scalar E8M7 element.
          ds_vectorial = 1'b0;
          ds_tag_i.vectorial = 1'b0;
          ds_operands[0][15:0] = reduced_fp32_to_fp16alt(operand1_i[31:0]);
          ds_operands[1][15:0] = reduced_fp32_to_fp16alt(operand2_i[31:0]);
          // DIV = op[0]/op[1]; SQRT reads op[0] only.
          ds_tag_i.bypass = reduced_fp32_subnormal(operand1_i[31:0]) ||
                            ((op_i == DIV) && reduced_fp32_subnormal(operand2_i[31:0]));
        end
        FP16: begin
          ds_supported   = 1'b1;
          ds_fmt         = FP16;
          ds_operands[0] = operand1_i;
          ds_operands[1] = operand2_i;
        end
        FP16ALT: begin
          ds_supported   = 1'b1;
          ds_fmt         = FP16ALT;
          ds_operands[0] = operand1_i;
          ds_operands[1] = operand2_i;
        end
        default: ds_supported = 1'b0;
      endcase
    end

    logic ds_accept;
    assign ds_accept = in_valid_i && in_ready_i && is_divsqrt_op && ds_supported;

    fpnew_opgroup_block #(
      .OpGroup       ( DIVSQRT                       ),
      .Width         ( ElenWidth                     ),
      .EnableVectors ( 1'b1                          ),
      .DivSqrtSel    ( fpnew_pkg::THMULTI            ),
      .FpFmtMask     ( reduced_divsqrt_fmt_mask()    ),
      .IntFmtMask    ( FPUFeatures.IntFmtMask        ),
      .FmtPipeRegs   ( reduced_divsqrt_pipe_regs()   ),
      .FmtUnitTypes  ( reduced_divsqrt_unit_types()  ),
      .PipeConfig    ( FPUImplementation.PipeConfig  ),
      .TagType       ( logic                         )
    ) i_reduced_divsqrt_shadow (
      .clk_i,
      .rst_ni,
      .hart_id_i      ( '0 ),
      .operands_i     ( ds_operands ),
      .is_boxed_i     ( '1 ),
      .rnd_mode_i,
      .op_i,
      .op_mod_i,
      .src_fmt_i      ( ds_fmt ),
      .dst_fmt_i      ( ds_fmt ),
      .int_fmt_i,
      .vectorial_op_i ( ds_vectorial ),
      .tag_i          ( 1'b0 ),
      .simd_mask_i    ( '1 ),
      .in_valid_i     ( ds_accept ),
      .in_ready_o     ( ds_in_ready ),
      .flush_i        ( 1'b0 ),
      .result_o       ( ds_result ),
      .status_o       ( /* unused */ ),
      .extension_bit_o( /* unused */ ),
      .tag_o          ( /* unused */ ),
      .out_valid_o    ( ds_out_valid ),
      // Never backpressured: the result is parked in the pending slot below.
      .out_ready_i    ( 1'b1 ),
      .busy_o         ( /* unused */ )
    );

    // -------------------------------------------------------------------
    // Pending slot: one accepted div/sqrt waiting for both results.
    // -------------------------------------------------------------------
    logic                 pend_q, pend_d;
    reduced_divsqrt_tag_t pend_tag_q, pend_tag_d;
    logic                 sh_done_q, sh_done_d, real_done_q, real_done_d;
    logic [31:0]          sh_res_q, sh_res_d;
    logic [ElenWidth-1:0] real_res_q, real_res_d;

    logic                 sh_now, real_now, sh_have, real_have, complete;
    logic [31:0]          sh_val;
    logic [ElenWidth-1:0] real_val;

    assign sh_now   = pend_q && !sh_done_q && ds_out_valid;
    assign real_now = pend_q && !real_done_q && result_valid_i && result_ready_i &&
                      (result_id_i == pend_tag_q.id);
    assign sh_have  = sh_done_q   || sh_now;
    assign real_have = real_done_q || real_now;
    assign sh_val   = sh_done_q   ? sh_res_q   : ds_result;
    assign real_val = real_done_q ? real_res_q : result_i;
    assign complete = pend_q && sh_have && real_have;

    always_comb begin
      pend_d      = pend_q;
      pend_tag_d  = pend_tag_q;
      sh_done_d   = sh_done_q   || sh_now;
      real_done_d = real_done_q || real_now;
      sh_res_d    = sh_now   ? ds_result : sh_res_q;
      real_res_d  = real_now ? result_i  : real_res_q;

      if (complete) begin
        pend_d      = 1'b0;
        sh_done_d   = 1'b0;
        real_done_d = 1'b0;
      end
      if (ds_accept && ds_in_ready) begin
        pend_d      = 1'b1;
        pend_tag_d  = ds_tag_i;
        sh_done_d   = 1'b0;
        real_done_d = 1'b0;
      end
    end

    always_ff @(posedge clk_i or negedge rst_ni) begin
      if (!rst_ni) begin
        pend_q      <= 1'b0;
        pend_tag_q  <= '{real_fmt: FP32, default: '0};
        sh_done_q   <= 1'b0;
        real_done_q <= 1'b0;
        sh_res_q    <= '0;
        real_res_q  <= '0;
      end else begin
        pend_q      <= pend_d;
        pend_tag_q  <= pend_tag_d;
        sh_done_q   <= sh_done_d;
        real_done_q <= real_done_d;
        sh_res_q    <= sh_res_d;
        real_res_q  <= real_res_d;
      end
    end

    // Deferred compare stage (registered copies, as for ADDMUL/DOTP).
    logic                 cmp_valid_q;
    reduced_divsqrt_tag_t cmp_tag_q;
    logic [31:0]          cmp_sh_q;
    logic [ElenWidth-1:0] cmp_real_q;
    // Control-level desync, reported one cycle later with the compare.
    logic                 desync_q;

    // Real result before the shadow's: the shadow runs the same or a
    // shorter iteration count with the same pipe depth and is never
    // backpressured, so this can only be a real/shadow desync. A new accept
    // while the previous op is still pending likewise means the one-in-
    // flight invariant was broken (the slot is overwritten).
    logic desync_comb;
    assign desync_comb = (real_now && !sh_have) ||
                         (ds_accept && pend_q && !complete);

    always_ff @(posedge clk_i or negedge rst_ni) begin
      if (!rst_ni) begin
        cmp_valid_q <= 1'b0;
        cmp_tag_q   <= '{real_fmt: FP32, default: '0};
        cmp_sh_q    <= '0;
        cmp_real_q  <= '0;
        desync_q    <= 1'b0;
      end else begin
        cmp_valid_q <= complete;
        cmp_tag_q   <= pend_tag_q;
        cmp_sh_q    <= sh_val;
        cmp_real_q  <= real_val;
        desync_q    <= desync_comb;
      end
    end

    always_comb begin
      reduced_divsqrt_fault_comb = desync_q;

      if (cmp_valid_q) begin
        unique case (cmp_tag_q.real_fmt)
          FP32: begin
            automatic logic [15:0] real_q;
            real_q = reduced_fp32_to_fp16alt(cmp_real_q[31:0]);
            // was: !reduced_fp32_subnormal(cmp_real_q[31:0]) -- self-referential,
            // skipped the check whenever the REAL (possibly fault-corrupted)
            // result itself looked subnormal, letting a fault that drives an
            // otherwise-normal result into subnormal range mask itself. Use the
            // shadow's own (fault-independent) result instead: it genuinely
            // reflects whether this is a degenerate FP16ALT case.
            //
            // Known residual gap (previously analyzed, not yet retested): if
            // the shadow's own division underflows all the way to exact zero
            // on an otherwise-legitimate (fault-free) subnormal result,
            // reduced_fp16alt_subnormal(cmp_sh_q) is false (it requires a
            // nonzero mantissa) so this bypass won't fire, and
            // reduced_mismatch16 only suppresses a mismatch when BOTH sides
            // are zero -- a possible false-positive path distinct from the
            // NONCOMP id-matching bug that caused the previously-observed
            // false positives (see noncomp_consumed above). Re-applied as-is
            // to isolate and retest against real fault-campaign data now that
            // the NONCOMP bug is fixed.
            // if (!cmp_tag_q.bypass && !reduced_fp32_subnormal(cmp_real_q[31:0]) &&
            if (!cmp_tag_q.bypass && !reduced_fp16alt_subnormal(cmp_sh_q[15:0]) &&
                reduced_mismatch16(cmp_sh_q[15:0], real_q, ReducedDivsqrtFp32Ulp)) begin
              reduced_divsqrt_fault_comb = 1'b1;
              `ifndef SYNTHESIS
                $display("[REDUCED_DIVSQRT_FP32] shadow=%h real32=%h real_q=%h",
                         cmp_sh_q[15:0], cmp_real_q[31:0], real_q);
              `endif
            end
          end

          FP16: begin
            automatic int unsigned lanes;
            lanes = cmp_tag_q.vectorial ? 2 : 1;
            for (int lane = 0; lane < 2; lane++) begin
              if (lane < lanes) begin
                automatic logic [15:0] real_lane, shadow_lane;
                real_lane   = cmp_real_q >> (16 * lane);
                shadow_lane = cmp_sh_q[16*lane +: 16];
                if (reduced_mismatch_fp16(shadow_lane, real_lane, ReducedDivsqrtExactTol)) begin
                  reduced_divsqrt_fault_comb = 1'b1;
                  `ifndef SYNTHESIS
                    $display("[REDUCED_DIVSQRT_FP16] lane=%0d shadow=%h real=%h",
                             lane, shadow_lane, real_lane);
                  `endif
                end
              end
            end
          end

          FP16ALT: begin
            automatic int unsigned lanes;
            lanes = cmp_tag_q.vectorial ? 2 : 1;
            for (int lane = 0; lane < 2; lane++) begin
              if (lane < lanes) begin
                automatic logic [15:0] real_lane, shadow_lane;
                real_lane   = cmp_real_q >> (16 * lane);
                shadow_lane = cmp_sh_q[16*lane +: 16];
                if (reduced_mismatch16(shadow_lane, real_lane, ReducedDivsqrtExactTol)) begin
                  reduced_divsqrt_fault_comb = 1'b1;
                  `ifndef SYNTHESIS
                    $display("[REDUCED_DIVSQRT_FP16ALT] lane=%0d shadow=%h real=%h",
                             lane, shadow_lane, real_lane);
                  `endif
                end
              end
            end
          end

          default: ;
        endcase
      end
    end

    // Same rule as reduced_addmul_dropped_op: the real FPU accepted a div/sqrt
    // the shadow could not take, so it would otherwise go unchecked.
    assign reduced_divsqrt_dropped_op = ds_accept && !ds_in_ready;

`ifndef SYNTHESIS
    always_ff @(posedge clk_i) begin
      if (rst_ni) begin
        if (ds_accept && !ds_in_ready)
          $display("[REDUCED_DIVSQRT] shadow not ready when real FPU accepted an operation");
        if (desync_comb)
          $display("[REDUCED_DIVSQRT] real/shadow desync (real before shadow, or overlapping accept)");
      end
    end
`endif
  end : gen_reduced_divsqrt
  else begin : gen_no_reduced_divsqrt
    assign reduced_divsqrt_fault_comb = 1'b0;
    assign reduced_divsqrt_dropped_op = 1'b0;
  end : gen_no_reduced_divsqrt

  // =====================================================================
  // Handshake-level plausibility check
  // =====================================================================
  // in_valid_i/expected_id_i now source from an independently-registered
  // accept-valid/tag copy in spatz_vfu.sv (not the real FPU's own
  // registers), so this counter can actually diverge from the real FPU's
  // own accept/retire behavior instead of silently tracking it in lockstep.
  // That closes a class of fault none of the arithmetic checks above can
  // ever see: a corrupted accept-valid or instruction-tag register that
  // makes the real FPU silently accept an extra (or drop an) instruction,
  // with no result_i value-mismatch to compare at all since the affected
  // instruction never enters this module's own reduced units in the first
  // place. Rather than reconstruct each opgroup's exact latency (DIVSQRT in
  // particular is iterative, not a fixed pipeline depth), this only tracks
  // the aggregate count of accepted-but-not-yet-retired instructions and
  // flags the two ways that count can go wrong: a retire with nothing
  // outstanding (spurious result), or more outstanding than the real
  // datapath could ever physically hold (an accept that never retires).
  localparam int unsigned OutCntWidth = $clog2(MaxOutstanding + 1);

  logic accept_event, retire_event;
  assign accept_event  = in_valid_i && in_ready_i;
  assign retire_event  = result_valid_i && result_ready_i;

  logic [OutCntWidth-1:0] outstanding_q;
  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      outstanding_q <= '0;
    end else begin
      unique case ({accept_event, retire_event})
        2'b10:   if (outstanding_q != OutCntWidth'(MaxOutstanding)) outstanding_q <= outstanding_q + 1'b1;
        2'b01:   if (outstanding_q != '0) outstanding_q <= outstanding_q - 1'b1;
        default: outstanding_q <= outstanding_q;
      endcase
    end
  end

  logic handshake_fault_comb;
  always_comb begin
    handshake_fault_comb = 1'b0;
    // A retire with nothing outstanding and no offsetting accept this same
    // cycle can only be a spurious result the checker's independent accept
    // tracking never requested.
    if (retire_event && !accept_event && (outstanding_q == '0))
      handshake_fault_comb = 1'b1;
    // Outstanding pegged at the configured max while still accepting (and
    // not simultaneously retiring) means accepts are piling up that never
    // come back -- consistent with the real FPU dropping an accept the
    // checker's independent copy saw.
    if (accept_event && !retire_event && (outstanding_q == OutCntWidth'(MaxOutstanding)))
      handshake_fault_comb = 1'b1;
  end

  // =====================================================================
  // Tag-passthrough check (narrowing bit)
  // =====================================================================
  // vfu_tag_t.narrowing is pure passthrough metadata carried alongside the
  // operands through every opgroup's own tag pipeline, used only downstream
  // (in spatz_vfu.sv) to route/pack the FPU's numeric result -- it never
  // enters any arithmetic/tolerance check above, so a corrupted tag-pipeline
  // register (e.g. a divsqrt/noncomp inp_pipe_tag_q.narrowing flop) would
  // otherwise go completely unchecked even though it can silently misroute
  // an otherwise-correct result. Tracked the same way as the id itself: one
  // entry per outstanding instruction slot, written at accept and checked
  // against the real tag retiring alongside result_i.
  logic narrowing_q [2**IdWidth];

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      for (int i = 0; i < 2**IdWidth; i++) narrowing_q[i] <= 1'b0;
    end else if (accept_event) begin
      narrowing_q[expected_id_i] <= tag_narrowing_i;
    end
  end

  logic tag_fault_comb;
  assign tag_fault_comb = retire_event && (narrowing_q[result_id_i] != result_tag_narrowing_i);

  // =====================================================================
  // Final fault output
  // =====================================================================
  logic fault_comb, fault_q;
  assign fault_comb = reduced_addmul_fault_comb || reduced_dotp_fault_comb ||
                      noncomp_fault_comb || conv_fault_comb ||
                      reduced_divsqrt_fault_comb ||
                      reduced_addmul_dropped_op || reduced_dotp_dropped_op ||
                      reduced_divsqrt_dropped_op ||
                      handshake_fault_comb ||
                      tag_fault_comb;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) fault_q <= 1'b0;
    else         fault_q <= fault_comb;
  end

  assign fault_o = fault_q;

endmodule : spatz_fpu_shadow_checker
