// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE_HW for details.
// SPDX-License-Identifier: SHL-0.51
//
// Combinational helpers for opope_sdotp_multi.
// All pipeline register banks remain in the top module.

// ---------------------------------------------------------------------------
// Product data path: adds implicit bits to the source mantissae and computes
// the two p*p mantissa products (a*b, c*d).
// ---------------------------------------------------------------------------
module opope_sdotp_product #(
  parameter fpnew_pkg::fmt_logic_t SrcDotpFpFmtConfig = '1,
  // Do not change
  localparam fpnew_pkg::fp_encoding_t SUPER_FORMAT = fpnew_pkg::super_format(SrcDotpFpFmtConfig),
  localparam int unsigned SUPER_EXP_BITS = SUPER_FORMAT.exp_bits,
  localparam int unsigned SUPER_MAN_BITS = SUPER_FORMAT.man_bits,
  // Precision bits 'p' include the implicit bit
  localparam int unsigned PRECISION_BITS = SUPER_MAN_BITS + 1
) (
  // fp_src_t operands (sign, exponent, mantissa packed)
  input  logic [SUPER_EXP_BITS+SUPER_MAN_BITS:0] operand_a_i,
  input  logic [SUPER_EXP_BITS+SUPER_MAN_BITS:0] operand_b_i,
  input  logic [SUPER_EXP_BITS+SUPER_MAN_BITS:0] operand_c_i,
  input  logic [SUPER_EXP_BITS+SUPER_MAN_BITS:0] operand_d_i,
  input  fpnew_pkg::fp_info_t                    info_a_i,
  input  fpnew_pkg::fp_info_t                    info_b_i,
  input  fpnew_pkg::fp_info_t                    info_c_i,
  input  fpnew_pkg::fp_info_t                    info_d_i,
  output logic [2*PRECISION_BITS-1:0]            product_x_o,
  output logic [2*PRECISION_BITS-1:0]            product_y_o
);

  typedef struct packed {
    logic                      sign;
    logic [SUPER_EXP_BITS-1:0] exponent;
    logic [SUPER_MAN_BITS-1:0] mantissa;
  } fp_src_t;

  fp_src_t             operand_a_q2, operand_b_q2, operand_c_q2, operand_d_q2;
  fpnew_pkg::fp_info_t info_a_q2, info_b_q2, info_c_q2, info_d_q2;

  assign operand_a_q2 = operand_a_i;
  assign operand_b_q2 = operand_b_i;
  assign operand_c_q2 = operand_c_i;
  assign operand_d_q2 = operand_d_i;
  assign info_a_q2    = info_a_i;
  assign info_b_q2    = info_b_i;
  assign info_c_q2    = info_c_i;
  assign info_d_q2    = info_d_i;

  logic     [PRECISION_BITS-1:0] mantissa_a, mantissa_b, mantissa_c, mantissa_d;
  logic   [2*PRECISION_BITS-1:0] product_x, product_y;  // the p*p product is 2p-bit wide

  // Add implicit bits to mantissae
  assign mantissa_a = {info_a_q2.is_normal, operand_a_q2.mantissa};
  assign mantissa_b = {info_b_q2.is_normal, operand_b_q2.mantissa};
  assign mantissa_c = {info_c_q2.is_normal, operand_c_q2.mantissa};
  assign mantissa_d = {info_d_q2.is_normal, operand_d_q2.mantissa};
  // Mantissa multiplier (a*b)
  assign product_x = mantissa_a * mantissa_b;
  // Mantissa multiplier (c*d)
  assign product_y = mantissa_c * mantissa_d;

  assign product_x_o = product_x;
  assign product_y_o = product_y;

endmodule

// ---------------------------------------------------------------------------
// Normalization 1: build the LZC search vector from the second sum and count
// its leading zeroes (cancellation detection).
// ---------------------------------------------------------------------------
module opope_sdotp_norm_lzc #(
  parameter fpnew_pkg::fmt_logic_t SrcDotpFpFmtConfig = '1,
  parameter fpnew_pkg::fmt_logic_t DstDotpFpFmtConfig = '1,
  // Do not change
  localparam fpnew_pkg::fp_encoding_t SUPER_FORMAT = fpnew_pkg::super_format(SrcDotpFpFmtConfig),
  localparam fpnew_pkg::fp_encoding_t SUPER_DST_FORMAT = fpnew_pkg::super_format(DstDotpFpFmtConfig),
  localparam int unsigned SUPER_MAN_BITS = SUPER_FORMAT.man_bits,
  localparam int unsigned SUPER_DST_MAN_BITS = (fpnew_pkg::maximum(SUPER_DST_FORMAT.man_bits, 2*SUPER_MAN_BITS + 1)
      + ((SrcDotpFpFmtConfig == 6'h02 && DstDotpFpFmtConfig == 6'h20)
          ? 2*SUPER_MAN_BITS : 0)),
  // Precision bits 'p' include the implicit bit
  localparam int unsigned PRECISION_BITS = SUPER_MAN_BITS + 1,
  localparam int unsigned DST_PRECISION_BITS = SUPER_DST_MAN_BITS + 1,
  // The leading-zero counter operates on LZC_SUM_WIDTH bits
  localparam int unsigned LZC_SUM_WIDTH  = 2*DST_PRECISION_BITS + PRECISION_BITS + 5,
  localparam int unsigned LZC_RESULT_WIDTH = $clog2(LZC_SUM_WIDTH)
) (
  input  logic [2*DST_PRECISION_BITS+PRECISION_BITS+3:0] sum_z_i,
  input  logic                                           sum_carry_z_i,
  input  logic                                           effective_subtraction_z_i,
  output logic [LZC_SUM_WIDTH-1:0]                       sum_lower_o,
  output logic [LZC_RESULT_WIDTH-1:0]                    leading_zero_count_o,
  output logic                                           lzc_zeroes_o
);

  logic [2*DST_PRECISION_BITS+PRECISION_BITS+3:0] sum_z_q;
  logic                                           sum_carry_z_q;
  logic                                           effective_subtraction_z_q;

  assign sum_z_q                   = sum_z_i;
  assign sum_carry_z_q             = sum_carry_z_i;
  assign effective_subtraction_z_q = effective_subtraction_z_i;

  logic        [LZC_SUM_WIDTH-1:0]    sum_lower;              // LZC_SUM_WIDTH bits of sum are searched
  logic        [LZC_RESULT_WIDTH-1:0] leading_zero_count;     // the number of leading zeroes
  logic                               lzc_zeroes;             // in case only zeroes found

  assign sum_lower = {(~effective_subtraction_z_q && sum_carry_z_q), sum_z_q};

  // Leading zero counter for cancellations
  lzc #(
    .WIDTH ( LZC_SUM_WIDTH   ),
    .MODE  ( 1               ) // MODE = 1 counts leading zeroes
  ) i_lzc (
    .in_i    ( sum_lower          ),
    .cnt_o   ( leading_zero_count ),
    .empty_o ( lzc_zeroes         )
  );

  assign sum_lower_o          = sum_lower;
  assign leading_zero_count_o = leading_zero_count;
  assign lzc_zeroes_o         = lzc_zeroes;

endmodule

// ---------------------------------------------------------------------------
// Second step of the three-way adder: W + Z with first-sum-zero bypass,
// effective-subtraction detection for Z, and final sign resolution.
// ---------------------------------------------------------------------------
module opope_sdotp_three_way_add2 #(
  parameter fpnew_pkg::fmt_logic_t SrcDotpFpFmtConfig = '1,
  parameter fpnew_pkg::fmt_logic_t DstDotpFpFmtConfig = '1,
  // Do not change
  localparam fpnew_pkg::fp_encoding_t SUPER_FORMAT = fpnew_pkg::super_format(SrcDotpFpFmtConfig),
  localparam fpnew_pkg::fp_encoding_t SUPER_DST_FORMAT = fpnew_pkg::super_format(DstDotpFpFmtConfig),
  localparam int unsigned SUPER_MAN_BITS = SUPER_FORMAT.man_bits,
  localparam int unsigned SUPER_DST_EXP_BITS = SUPER_DST_FORMAT.exp_bits,
  localparam int unsigned SUPER_DST_MAN_BITS = (fpnew_pkg::maximum(SUPER_DST_FORMAT.man_bits, 2*SUPER_MAN_BITS + 1)
      + ((SrcDotpFpFmtConfig == 6'h02 && DstDotpFpFmtConfig == 6'h20)
          ? 2*SUPER_MAN_BITS : 0)),
  // Precision bits 'p' include the implicit bit
  localparam int unsigned PRECISION_BITS = SUPER_MAN_BITS + 1,
  localparam int unsigned DST_PRECISION_BITS = SUPER_DST_MAN_BITS + 1,
  // The leading-zero counter operates on LZC_SUM_WIDTH bits
  localparam int unsigned LZC_SUM_WIDTH  = 2*DST_PRECISION_BITS + PRECISION_BITS + 5,
  localparam int unsigned LZC_RESULT_WIDTH = $clog2(LZC_SUM_WIDTH),
  localparam int unsigned DST_EXP_WIDTH = unsigned'(fpnew_pkg::maximum(SUPER_DST_EXP_BITS + 2, LZC_RESULT_WIDTH))
) (
  input  logic                                           sum_carry_i,
  input  logic                                           bypass_w_i,
  input  logic                                           addend_min_sign_i,
  input  logic                                           effective_subtraction_first_i,
  input  logic signed [DST_EXP_WIDTH-1:0]                exponent_w_i,
  input  logic                                           sticky_before_add_z_i,
  input  logic [2*DST_PRECISION_BITS+PRECISION_BITS+3:0] addend_min_after_shift_i,
  input  logic                                           operand_e_sign_i,
  input  logic                                           product_x_sign_i,
  input  logic                                           product_y_sign_i,
  input  logic [2:0]                                     exponent_cmp_i,
  input  logic signed [DST_EXP_WIDTH-1:0]                exponent_min_i,
  input  logic [2*DST_PRECISION_BITS+2:0]                sum_i,
  input  logic                                           final_sign_i,
  output logic signed [DST_EXP_WIDTH-1:0]                final_tentative_exponent_o,
  output logic                                           effective_subtraction_z_o,
  output logic [2*DST_PRECISION_BITS+PRECISION_BITS+3:0] sum_z_o,
  output logic                                           sum_carry_z_o,
  output logic                                           final_sign_z_o
);

  logic                                           sum_carry_q;
  logic                                           bypass_w_q;
  logic                                           addend_min_sign_q2;
  logic                                           effective_subtraction_first_q2;
  logic signed [DST_EXP_WIDTH-1:0]                exponent_w_q;
  logic                                           sticky_before_add_z_q;
  logic [2*DST_PRECISION_BITS+PRECISION_BITS+3:0] addend_min_after_shift_q;
  logic                                           operand_e_sign_q;
  logic                                           product_x_sign_q;
  logic                                           product_y_sign_q;
  logic [2:0]                                     exponent_cmp_q2;
  logic signed [DST_EXP_WIDTH-1:0]                exponent_min_q2;
  logic [2*DST_PRECISION_BITS+2:0]                sum_q;
  logic                                           final_sign_q;

  assign sum_carry_q                    = sum_carry_i;
  assign bypass_w_q                     = bypass_w_i;
  assign addend_min_sign_q2             = addend_min_sign_i;
  assign effective_subtraction_first_q2 = effective_subtraction_first_i;
  assign exponent_w_q                   = exponent_w_i;
  assign sticky_before_add_z_q          = sticky_before_add_z_i;
  assign addend_min_after_shift_q       = addend_min_after_shift_i;
  assign operand_e_sign_q               = operand_e_sign_i;
  assign product_x_sign_q               = product_x_sign_i;
  assign product_y_sign_q               = product_y_sign_i;
  assign exponent_cmp_q2                = exponent_cmp_i;
  assign exponent_min_q2                = exponent_min_i;
  assign sum_q                          = sum_i;
  assign final_sign_q                   = final_sign_i;

  // ----------------------------------
  // Second Step of the Three-way Adder
  // ----------------------------------
  // Bypass the first addition in the case of result of the first addition equal to zero and
  // minimum addend not equal to zero.
  // Without bypassing, that situation might result in precision loss since the minimum addend is
  // shifted in parallel with the first sum (i.e. if the minimum addend is much smaller than 0,
  // it might have been shifted out before knowning that the result of the first addition was 0)
  // Formed one stage earlier, in first_shift_add, so that the minimum addend's
  // two alignment shifts could be merged into one (see there).
  logic bypass_w;
  assign bypass_w = bypass_w_q;

  // --------------------------------------------------------------------------
  // Two's-complement spine.
  //
  // first_shift_add now hands over {sum_carry_q, sum_q} = sum_raw, the first
  // adder's raw output, plus the sign FRAME `final_sign_q` (the max addend's
  // sign, or the round-mode tie-break on an exact zero) rather than the resolved
  // sign of the first sum.  Writing neg_w = eff_sub_first & ~sum_carry (the flag
  // that says the first sum came out negative in that frame, exactly the term the
  // old final_sign carried), the first sum as a SIGNED value in the frame is
  //
  //     W = {neg_w, sum_carry ^ eff_sub_first, sum_q}          (two's complement)
  //
  // -- pure wiring plus two XORs -- and the old sign-magnitude mantissa_w is |W|.
  //
  // The second addition then runs entirely in the frame: its polarity is
  // e_add = s_min ^ frame_sign, which does not depend on the first adder at all,
  // and the old effective_subtraction_z is e_add ^ neg_w.  The sticky decrement
  // is the one asymmetry: the original subtracts 1 ulp whenever the |W|-frame
  // operation is a subtraction, i.e. when e_add ^ neg_w is set, while the frame
  // adder subtracts when e_add is set.  Writing Q for the frame result without
  // any sticky decrement, the original's value R (in the |W| frame) satisfies
  //
  //     R = Q'      if neg_w = 0        with  Q' = Q + (neg_w & sticky_z)
  //     R = -Q'     if neg_w = 1
  //
  // so |R| = |Q'| and the extra ulp is just an extra carry-in.  Everything the
  // module exports is then a function of Q', its sign bit and its zero flag.
  // --------------------------------------------------------------------------
  // Widths: FRAME_W is one bit wider than the original sum_raw_z, because the
  // frame result is SIGNED where the original was a magnitude plus a carry.
  localparam int unsigned FRAME_W  = 2*DST_PRECISION_BITS+PRECISION_BITS+6; // 65 @ FP16->FP32
  localparam int unsigned MANT_W_W = 2*DST_PRECISION_BITS+5;                // 53 @ FP16->FP32

  logic                            neg_w;      // first sum negative in the frame
  logic [MANT_W_W-1:0]             mantissa_w_signed;
  logic [FRAME_W-1:0]              mantissa_w_shifted;

  logic                            frame_sign;
  logic                            e_add;             // second adder polarity (early)
  logic                            tentative_sign_z;
  logic                            effective_subtraction_z;
  logic signed [DST_EXP_WIDTH-1:0] final_tentative_exponent;

  // (exponent_min >= 0) is exactly the complement of its sign bit -- no need for a
  // DST_EXP_WIDTH magnitude comparator against zero.
  assign final_tentative_exponent = (bypass_w) ? exponent_min_q2   // UNOBSERVABLE clamp
                                              : exponent_w_q;

  assign neg_w = effective_subtraction_first_q2 & ~sum_carry_q;
  assign mantissa_w_signed  = {neg_w, sum_carry_q ^ effective_subtraction_first_q2, sum_q};
  assign mantissa_w_shifted = {{(FRAME_W-PRECISION_BITS-MANT_W_W){mantissa_w_signed[MANT_W_W-1]}},
                               mantissa_w_signed, {PRECISION_BITS{1'b0}}};

  // The exponent_cmp-selected addend sign, hoisted out from BEHIND the frame
  // mux.  It depends only on exponent_cmp and the three addend signs, all of
  // which are ready long before the first adder resolves, so the 8-way select
  // is no longer a serial stage sitting behind bypass_w.
  logic sel_sign;
  always_comb begin
    case (exponent_cmp_q2)
      3'b000  :  sel_sign = product_x_sign_q;
      3'b001  :  sel_sign = product_x_sign_q;
      3'b011  :  sel_sign = operand_e_sign_q;
      3'b100  :  sel_sign = product_y_sign_q;
      3'b110  :  sel_sign = product_y_sign_q;
      3'b111  :  sel_sign = operand_e_sign_q;
      3'b010, 3'b101 : sel_sign = 1'bx;   // transitivity-impossible
      default :  sel_sign = operand_e_sign_q;
    endcase
  end

  // Sign frame of the second addition: the max addend's sign, the minimum
  // addend's sign when the first sum is bypassed.  Free of the first adder.
  assign frame_sign       = (bypass_w) ? addend_min_sign_q2 : final_sign_q;
  // The sign of the first sum, as the original computed it.
  assign tentative_sign_z = frame_sign ^ neg_w;
  assign e_add            = sel_sign ^ frame_sign;

  // ---- ADDER POLARITY, FREE OF bypass_w -----------------------------------
  // When bypass_w is set the frame adder's result is DISCARDED -- sum_raw_z
  // takes the bypass value instead -- so the adder may be built with the
  // NON-BYPASS polarity unconditionally: e_add_nb == e_add on every input for
  // which the adder's output is observed at all.
  //
  // That matters because bypass_w is the design's critical signal: it is two
  // gates plus a module-boundary round trip behind sum_exact_zero, and it used
  // to sit in front of the 63-bit operand inversion AND the 65-bit carry chain.
  // final_sign_q is a single mux behind sum_exact_zero inside first_shift_add,
  // so e_add_nb resolves strictly earlier than e_add ever could, and the whole
  // adder starts that much sooner.
  logic e_add_nb;
  assign e_add_nb = sel_sign ^ final_sign_q;

  assign effective_subtraction_z = e_add ^ neg_w;

  logic [FRAME_W-1:0]              addend_min_shifted;
  logic                            inject_carry_in_z; // inject carry for subtractions if needed

  // In case of a subtraction (in the FRAME), the addend is inverted
  assign addend_min_shifted = (e_add_nb)
      ? ~{{(FRAME_W-(2*DST_PRECISION_BITS+PRECISION_BITS+4)){1'b0}}, addend_min_after_shift_q}
      :  {{(FRAME_W-(2*DST_PRECISION_BITS+PRECISION_BITS+4)){1'b0}}, addend_min_after_shift_q};
  // e_add = 0 : carry in the extra sticky ulp only when the |W|-frame operation was
  //             a subtraction, i.e. when neg_w is set.
  // e_add = 1 : the frame subtracts, so the +1 of the two's complement is present
  //             unless the sticky steals it -- and neg_w gives it back.
  assign inject_carry_in_z = (e_add_nb) ? (~sticky_before_add_z_q | neg_w)
                                        : ( sticky_before_add_z_q & neg_w);

  // ------
  // Adder
  // ------
  // Same evaluation context as the original sum_raw_z, so the bypass shift keeps
  // its width behaviour exactly.
  logic [FRAME_W-1:0]              frame_sum;   // the frame adder's raw output A
  logic [FRAME_W-1:0]              sum_raw_z;   // signed frame result Q'
  logic                            sum_raw_z_neg, sum_raw_z_zero;
  logic                            negate_sel;  // take the magnitude of the frame result
  logic                            result_neg;  // result negative in the |W| frame
  logic [FRAME_W-1:0]              sum_magnitude;
  logic                            sum_carry_z; // observe carry bit from sum for sign fixing
  logic [2*DST_PRECISION_BITS+PRECISION_BITS+3:0] sum_z;       // discard carry as sum won't overflow
  logic                            final_sign_z;

  // The bypass value is no longer built here: first_shift_add produces it on
  // `addend_min_after_shift` itself, because the two shifts of the minimum
  // addend are the same function of it and bypass_w picks which amount is the
  // one that gets observed.  What used to be a 64-bit barrel shifter plus its
  // amount subtract/clamp is now the far side of a 6-bit mux one stage back.
  // The parent zero-extended a 2*p_dst+p+5-bit `bypass_value` whose top bit was
  // structurally 0, so extending the 2*p_dst+p+4-bit field is the same vector.

  // Mantissa adder (W+Z), in the sign frame
  // ------------------------------------------------------------------------
  // THE BYPASS VALUE IS THE FRAME ADDER'S OWN OUTPUT, COMPLEMENTED.
  //
  // bypass_w = sum_exact_zero && sticky_before_add_z, and sum_exact_zero forces
  // the FIRST sum to be exactly 2**(2*p_dst+3): with !int_low_any the
  // intermediate addend has no bit below the max addend's field, so
  // inject_carry_in = eff_sub_first = 1 and
  //     sum_raw = (addend_max << p_dst+3) + 1 + ~(addend_max << p_dst+3)
  //             = 2**(2*p_dst+3),
  // i.e. sum = 0 and sum_carry = 1.  Hence neg_w = 0, mantissa_w_signed = 0 and
  // mantissa_w_shifted = 0 -- the W operand VANISHES on the bypass path.
  // bypass_w also forces sticky_before_add_z = 1, so with neg_w = 0 the
  // carry-in is 0 in BOTH arms of its mux.  What the adder computes there is
  // therefore exactly
  //     e_add_nb = 0 :   Z      -- already the bypass value
  //     e_add_nb = 1 :  ~Z      -- the bypass value, bit-complemented
  // with Z the zero-extended addend_min_after_shift.  So the 65-bit 2:1
  // MULTIPLEXER whose second input is Z is one XOR row on the adder's own
  // output, selected by (bypass_w & e_add_nb):
  //
  //     sum_raw_z == (mantissa_w_shifted + addend_min_shifted + cin)
  //                  ^ {FRAME_W{bypass_w & e_add_nb}}
  //
  // -- bit for bit, on every input, and with the select still resolved AFTER
  // the adder, where bypass_w has the whole carry chain to arrive in.  (Putting
  // bypass_w in FRONT of the adder instead -- on the carry-in or on the
  // inversion polarity -- is measurably worse: see `byfold' in this round's
  // lessons, +116 flops.)  What it buys is the second consumer of the 63-bit
  // addend_min_after_shift bus: it no longer has to be routed to the adder's
  // OUTPUT as well as to its input.
  // ------------------------------------------------------------------------
  logic bypass_flip;
  assign bypass_flip  = bypass_w & e_add_nb;
  // mantissa_w_shifted[PRECISION_BITS-1:0] is structurally 0, so OR-ing the
  // carry-in into bit 0 is the same value as adding it -- and it turns a
  // three-operand sum (two FRAME_W-bit DW01_adds at elaboration) into one.
  // A = the frame adder's raw output; the bypass flip is NOT applied to it.
  assign frame_sum      = (mantissa_w_shifted | FRAME_W'(inject_carry_in_z)) + addend_min_shifted;
  assign sum_raw_z      = frame_sum;                      // name kept for the ports below
  assign sum_raw_z_neg  = frame_sum[FRAME_W-1] ^ bypass_flip;
  assign sum_raw_z_zero = 1'b0;                           // UNOBSERVABLE (see zdet)

  // |Q'| -- the magnitude of the result, which is the magnitude the original
  // produced too (R = +-Q').
  //
  // On the bypass path the original does NOT take a magnitude: it runs its plain
  // `(eff_sub_z && ~sum_carry_z) ? -sum_raw_z` on the bypass value, whose carry bit
  // is structurally 0, so it negates exactly when eff_sub_z is set.  bypass_w implies
  // sum_exact_zero implies neg_w = 0, so eff_sub_z is e_add there; selecting the
  // negate with e_add reproduces that bit for bit -- including the case where the
  // minimum addend's sign port and the exponent_cmp-selected sign disagree, which is
  // unreachable in the top but IS reachable in this cone's free input space.
  assign negate_sel    = sum_raw_z_neg;   // UNOBSERVABLE bypass arm
  // (A ^ {W{f}}) ^ {W{A[W-1]^f}} == A ^ {W{A[W-1]}} : the bypass flip and the
  // magnitude's negate are ONE row, and f survives only as the carry-in.
  assign sum_magnitude = (frame_sum ^ {FRAME_W{frame_sum[FRAME_W-1]}})
                         + FRAME_W'(negate_sel);

  // Sign of the result in the |W| frame: R = -Q' when the first sum was negative
  // in the sign frame, so a zero Q' stays non-negative either way.
  // result_neg has exactly ONE consumer, final_sign_z = tentative_sign_z ^
  // result_neg = frame_sign ^ neg_w ^ result_neg, so only the XOR of the two is
  // observable.  And
  //     neg_w = 0 :  0 ^ negate_sel          = sum_raw_z_neg
  //     neg_w = 1 :  1 ^ (~neg & ~zero)      = sum_raw_z_neg | sum_raw_z_zero
  // i.e. neg_w ^ result_neg == sum_raw_z_neg | (neg_w & sum_raw_z_zero).
  // Carrying THAT instead turns the 2:1 mux plus an XOR into an AND and an OR.
  // The zero term drives a FRAME_W-1-bit OR tree for a bit that can only flip
  // the sign of an exactly-zero magnitude; see this round's lessons.
  assign result_neg   = sum_raw_z_neg;   // UNOBSERVABLE zero term

  // sum_carry_z_o has exactly ONE consumer in the whole design -- norm_lzc's
  //
  //     sum_lower = {(~effective_subtraction_z_q && sum_carry_z_q), sum_z_q}
  //
  // -- and opope_sdotp_multi wires the same effective_subtraction_z through the
  // same bank to both.  The effective-subtraction arm of this mux is therefore
  // ANDed with its own complement and cannot reach an output: dropping it leaves
  // sum_lower bit for bit identical.  What that buys is not the mux, it is the
  // DEPENDENCE: the LZC's MSB no longer waits for result_neg, hence no longer
  // waits for the 65-bit zero-detect of the frame adder's result.
  assign sum_carry_z  = sum_magnitude[2*DST_PRECISION_BITS+PRECISION_BITS+4];

  assign sum_z        = sum_magnitude[2*DST_PRECISION_BITS+PRECISION_BITS+3:0];

  // In case of a mispredicted subtraction result, do a sign flip
  assign final_sign_z = frame_sign ^ result_neg;   // result_neg is neg_w-folded

  assign final_tentative_exponent_o = final_tentative_exponent;
  assign effective_subtraction_z_o  = effective_subtraction_z;
  assign sum_z_o                    = sum_z;
  assign sum_carry_z_o              = sum_carry_z;
  assign final_sign_z_o             = final_sign_z;

endmodule

// ---------------------------------------------------------------------------
// Normalization 2: large normalization shift based on the LZC result, 1-bit
// small normalization, and final sticky-bit update.
// ---------------------------------------------------------------------------
module opope_sdotp_normalization #(
  parameter fpnew_pkg::fmt_logic_t SrcDotpFpFmtConfig = '1,
  parameter fpnew_pkg::fmt_logic_t DstDotpFpFmtConfig = '1,
  // Do not change
  localparam fpnew_pkg::fp_encoding_t SUPER_FORMAT = fpnew_pkg::super_format(SrcDotpFpFmtConfig),
  localparam fpnew_pkg::fp_encoding_t SUPER_DST_FORMAT = fpnew_pkg::super_format(DstDotpFpFmtConfig),
  localparam int unsigned SUPER_MAN_BITS = SUPER_FORMAT.man_bits,
  localparam int unsigned SUPER_DST_EXP_BITS = SUPER_DST_FORMAT.exp_bits,
  localparam int unsigned SUPER_DST_MAN_BITS = (fpnew_pkg::maximum(SUPER_DST_FORMAT.man_bits, 2*SUPER_MAN_BITS + 1)
      + ((SrcDotpFpFmtConfig == 6'h02 && DstDotpFpFmtConfig == 6'h20)
          ? 2*SUPER_MAN_BITS : 0)),
  // Precision bits 'p' include the implicit bit
  localparam int unsigned PRECISION_BITS = SUPER_MAN_BITS + 1,
  localparam int unsigned DST_PRECISION_BITS = SUPER_DST_MAN_BITS + 1,
  // The leading-zero counter operates on LZC_SUM_WIDTH bits
  localparam int unsigned LZC_SUM_WIDTH  = 2*DST_PRECISION_BITS + PRECISION_BITS + 5,
  localparam int unsigned LZC_RESULT_WIDTH = $clog2(LZC_SUM_WIDTH),
  localparam int unsigned DST_EXP_WIDTH = unsigned'(fpnew_pkg::maximum(SUPER_DST_EXP_BITS + 2, LZC_RESULT_WIDTH)),
  // Shift amount width: maximum internal mantissa size is 2*DST_PRECISION_BITS+3 bits
  localparam int unsigned DST_SHIFT_AMOUNT_WIDTH = $clog2(2*DST_PRECISION_BITS+PRECISION_BITS+5)
) (
  input  logic                                         lzc_zeroes_i,
  input  logic [LZC_RESULT_WIDTH-1:0]                  leading_zero_count_i,
  input  logic signed [DST_EXP_WIDTH-1:0]              final_tentative_exponent_i,
  input  logic [LZC_SUM_WIDTH-1:0]                     sum_lower_i,
  input  logic                                         sticky_before_add_z_i,
  input  logic                                         bypass_w_i,
  input  logic                                         sticky_before_add_i,
  input  logic                                         effective_subtraction_first_i,
  input  logic                                         effective_subtraction_z_i,
  input  logic                                         info_min_is_zero_i,
  output logic [DST_PRECISION_BITS:0]                  final_mantissa_o,
  output logic [DST_PRECISION_BITS+PRECISION_BITS+2:0] sum_sticky_bits_o,
  output logic signed [DST_EXP_WIDTH-1:0]              final_exponent_o,
  output logic                                         sticky_after_norm_o
);

  logic                            lzc_zeroes_q;
  logic [LZC_RESULT_WIDTH-1:0]     leading_zero_count_q;
  logic signed [DST_EXP_WIDTH-1:0] final_tentative_exponent_q;
  logic [LZC_SUM_WIDTH-1:0]        sum_lower_q;
  logic                            sticky_before_add_z_q2;
  logic                            bypass_w_q;
  logic                            sticky_before_add_q2;
  logic                            effective_subtraction_first_q3;
  logic                            effective_subtraction_z_q2;
  logic                            info_min_is_zero_q3;

  assign lzc_zeroes_q                   = lzc_zeroes_i;
  assign leading_zero_count_q           = leading_zero_count_i;
  assign final_tentative_exponent_q     = final_tentative_exponent_i;
  assign sum_lower_q                    = sum_lower_i;
  assign sticky_before_add_z_q2         = sticky_before_add_z_i;
  assign bypass_w_q                     = bypass_w_i;
  assign sticky_before_add_q2           = sticky_before_add_i;
  assign effective_subtraction_first_q3 = effective_subtraction_first_i;
  assign effective_subtraction_z_q2     = effective_subtraction_z_i;
  assign info_min_is_zero_q3            = info_min_is_zero_i;

  // ==========================================================================
  // ELABORATION-TIME EXPONENT-RANGE FACT
  //
  //   ADDEND_EXP_POSITIVE == 1  <=>  at EVERY (src_fmt, dst_fmt) pair this
  //   elaboration admits, a NON-ZERO addend's biased destination exponent is
  //   >= 1, while a ZERO product's is 2 - bias_dst < 1.
  //
  // exponent_prep builds the three addend exponents as
  //     product : (a.is_zero | b.is_zero) ? 2 - bias_dst
  //                                       : e_a + sub_a + e_b + sub_b
  //                                         - 2*bias_src + bias_dst + 1
  //     accum   : e_e + ~e.is_normal
  // Each of (e_a + sub_a) and (e_b + sub_b) is >= 1 (a normal has e >= 1 and
  // sub = 0; a sub-normal has e = 0 and sub = 1), so a NON-ZERO product is
  // >= 3 + bias_dst - 2*bias_src, and the accumulator term is >= 1 for every
  // encoding (e = 0 forces is_normal = 0, hence the +1).  The condition below
  // is therefore exactly "3 + bias_dst - 2*bias_src >= 1 for all enabled
  // pairs", evaluated on the worst pair (largest source bias, smallest
  // destination bias).
  //
  // At the measured FP16 -> FP32 build: bias_src = 15, bias_dst = 127, so a
  // non-zero product lands in [100, 160], a zero product at -125 and the
  // accumulator in [1, 256].  ADDEND_EXP_POSITIVE = 1.
  // ==========================================================================
  function automatic bit nonzero_addend_exponents_positive
      (input fpnew_pkg::fmt_logic_t src_cfg, input fpnew_pkg::fmt_logic_t dst_cfg);
    automatic int max_src_bias = 0;
    automatic int min_dst_bias = 0;
    automatic bit have_dst     = 1'b0;
    for (int unsigned f = 0; f < fpnew_pkg::NUM_FP_FORMATS; f++) begin
      if (src_cfg[f] && (int'(fpnew_pkg::bias(fpnew_pkg::fp_format_e'(f))) > max_src_bias))
        max_src_bias = int'(fpnew_pkg::bias(fpnew_pkg::fp_format_e'(f)));
      if (dst_cfg[f] && (!have_dst ||
                         (int'(fpnew_pkg::bias(fpnew_pkg::fp_format_e'(f))) < min_dst_bias))) begin
        min_dst_bias = int'(fpnew_pkg::bias(fpnew_pkg::fp_format_e'(f)));
        have_dst     = 1'b1;
      end
    end
    return have_dst && ((3 + min_dst_bias) >= (2*max_src_bias + 1));
  endfunction

  localparam bit ADDEND_EXP_POSITIVE =
      nonzero_addend_exponents_positive(SrcDotpFpFmtConfig, DstDotpFpFmtConfig);

  // ----------------
  // Normalization 2
  // ----------------
  logic signed [LZC_RESULT_WIDTH:0] leading_zero_count_sgn;  // signed leading-zero count

  logic [DST_SHIFT_AMOUNT_WIDTH-1:0] norm_shamt;  // Normalization shift amount
  logic signed [DST_EXP_WIDTH-1:0] normalized_exponent;

  logic [2*DST_PRECISION_BITS+PRECISION_BITS+4:0] sum_shifted;       // result after first normalization shift
  logic [DST_PRECISION_BITS:0] final_mantissa;  // final mantissa before rounding with round bit
  logic   [DST_PRECISION_BITS+PRECISION_BITS+2:0] sum_sticky_bits;   // remaining p_dst+3 sticky bits after normalization
  logic sticky_after_norm;  // sticky bit after normalization

  logic signed [DST_EXP_WIDTH-1:0] final_exponent;

  assign leading_zero_count_sgn = signed'({1'b0, leading_zero_count_q});

  // Normalization shift amount based on exponents and LZC (unsigned as only left shifts)
  logic large_norm; // large-normalization branch taken (shift distance driven by the LZC)

  // (fte - lzc + 1 > 0) <=> (fte >= lzc).  The original expression is evaluated in
  // 32-bit arithmetic because of the literal 1, so it cannot overflow and the two
  // are equal for every value; the comparison form keeps the shift-amount select
  // off the full-width subtractor.
  // ---- exponent arithmetic, narrowed to the LZC's own width --------------
  // leading_zero_count_q is LZC_RESULT_WIDTH bits and (being a count into a
  // LZC_SUM_WIDTH-bit vector) never exceeds LZC_SUM_WIDTH-1, so
  // leading_zero_count_sgn is a NON-NEGATIVE value that fits in
  // LZC_RESULT_WIDTH bits.  Both the comparison and the subtraction therefore
  // split at bit LZC_RESULT_WIDTH:
  //
  //   fte >= lzc          <=>  fte >= 0 && ( fte[hi] != 0 || fte[lo] >= lzc )
  //   (fte + 1) - lzc     =    { fte_p1[hi] - borrow , fte_p1[lo] - lzc }
  //
  // and every [hi] term is a function of the tentative exponent alone -- ready
  // half a nanosecond before the leading-zero counter.  What is left behind the
  // LZC is one LZC_RESULT_WIDTH-bit compare and one LZC_RESULT_WIDTH-bit
  // subtract, in place of the width=10 DW_cmp and the width=10 `I1-I2+1'
  // datapath block the parent built there.
  //
  // (fte - lzc + 1) == ((fte + 1) - lzc) holds in the parent's 32-bit
  // evaluation and both are truncated to DST_EXP_WIDTH bits on assignment, so
  // the regrouping is exact.
  localparam int unsigned LZC_LO_W = LZC_RESULT_WIDTH;

  logic [DST_EXP_WIDTH-1:0]          fte_p1;         // fte + 1                     EARLY
  logic [DST_EXP_WIDTH-LZC_LO_W-1:0] fte_p1_hi;      // its high slice              EARLY
  logic [DST_EXP_WIDTH-LZC_LO_W-1:0] fte_p1_hi_dec;  // that slice, decremented     EARLY
  logic                              fte_hi_nz;      // fte[hi] != 0 (fte >= 0)     EARLY
  logic [LZC_LO_W-1:0]               fte_lo_sub;     // fte_p1[lo] - lzc            LATE, narrow
  logic                              fte_lo_borrow;  //                             LATE, narrow
  logic                              lzc_le_fte_lo;  // lzc <= fte[lo]              LATE, narrow

  assign fte_p1         = final_tentative_exponent_q + 1;
  assign fte_p1_hi      = fte_p1[DST_EXP_WIDTH-1:LZC_LO_W];
  assign fte_p1_hi_dec  = fte_p1_hi - 1;
  assign fte_hi_nz      = |final_tentative_exponent_q[DST_EXP_WIDTH-2:LZC_LO_W];
  assign {fte_lo_borrow, fte_lo_sub} =
      {1'b0, fte_p1[LZC_LO_W-1:0]} - {1'b0, leading_zero_count_q};
  assign lzc_le_fte_lo  =
      leading_zero_count_q <= final_tentative_exponent_q[LZC_LO_W-1:0];

  // ------------------------------------------------------------------------
  // THE FINAL TENTATIVE EXPONENT IS ALWAYS POSITIVE AT THIS ELABORATION.
  //
  //   final_tentative_exponent = bypass_w ? exponent_min : exponent_max + 1
  //
  // exponent_max >= exponent_addend_z >= 1 on every reachable sort code, and
  // bypass_w implies exponent_min >= 1 by the argument in first_shift_add's
  // gen_bypass_shamt_zero.  Under ADDEND_EXP_POSITIVE the value therefore lies
  // in [1, 2**SUPER_DST_EXP_BITS + 4], which is inside the positive half of the
  // DST_EXP_WIDTH-bit signed container, so its sign bit is constant 0 and the
  // `> 0' test in the sub-normal branch below is constant 1.
  // ------------------------------------------------------------------------
  assign large_norm = !lzc_zeroes_q
                      && (ADDEND_EXP_POSITIVE ? 1'b1
                                              : !final_tentative_exponent_q[DST_EXP_WIDTH-1])
                      && (fte_hi_nz || lzc_le_fte_lo);

  always_comb begin : norm_shift_amount
    if (large_norm) begin
      // Shift by the leading-zero count ITSELF, not by lzc-1.
      //
      // sum_lower_q's leading one sits at bit LZC_SUM_WIDTH-1-lzc, so shifting
      // left by exactly lzc always lands it on the MSB -- and the small
      // normalization step below already right-aligns by one precisely when that
      // MSB is set.  So `<< lzc' followed by the small norm reproduces
      // `<< (lzc-1)' for lzc > 0 and `<< 0' + the right correction for lzc == 0,
      // which are the parent's two cases, bit for bit.
      //
      // What that buys: the `lzc > 0' comparator (resources.rpt gt_x_2, a 6-bit
      // DW_cmp) and the decrement (sub_x_4, a 6-bit DW01_dec) both sat BETWEEN
      // the leading-zero counter and the 64-bit barrel shifter, i.e. in the
      // middle of the critical path.  Both are gone; the LZC result now drives
      // the shifter through a single mux.
      //
      // normalized_exponent is dead code in the parent (final_exponent is built
      // from `fte - lzc + 1' directly, see below); it is kept only so the intent
      // of the branch stays readable.
      norm_shamt          = leading_zero_count_q;
      normalized_exponent = final_tentative_exponent_q - leading_zero_count_sgn + 1; // account for shift
    // Subnormal result
    end else begin
      // Cap the shift distance to align mantissa with minimum exponent
      if (ADDEND_EXP_POSITIVE || (final_tentative_exponent_q > 0))
        norm_shamt = final_tentative_exponent_q - 1;
      else
        norm_shamt = '0;
      normalized_exponent = '0;
    end
  end

  // ------------------------------------------------------------------------
  // THE SMALL-NORMALISATION MUX IS A +1 ON THE SHIFT AMOUNT.
  //
  // Building on the identity proved for norm_carry below
  // (sum_shifted[2*p_dst+p+4] == large_norm under ADDEND_EXP_POSITIVE), the
  // mantissa window the parent selects is
  //
  //     large_norm ? sum_shifted[2*p_dst+p+3 -: p_dst]     (carry branch)
  //                : sum_shifted[2*p_dst+p+2 -: p_dst]     (default branch)
  //
  // i.e. ONE window whose position differs by exactly one bit between the two
  // branches.  Since sum_shifted = sum_lower << norm_shamt and
  //
  //     norm_shamt = large_norm ? lzc : fte-1,
  //
  // shifting by  norm_shamt_m = norm_shamt + !large_norm = large_norm ? lzc : fte
  // moves the default branch's window up by one and makes BOTH branches read
  // the SAME field [2*p_dst+p+2 -: p_dst].  The 24-bit 2:1 mux disappears; what
  // replaces it is one DST_SHIFT_AMOUNT_WIDTH-bit select, and the mantissa path
  // no longer sits behind a mux whose select comes out of the shifter.
  //
  // The subnormal branch's amount becomes fte itself, so its decrement is gone
  // from the shifter's amount (norm_shamt is still built, unchanged, for the
  // sticky mask -- the mask window is stated in the ORIGINAL frame and is
  // untouched by this rewrite).  fte >= 1 under ADDEND_EXP_POSITIVE and
  // fte < 2**LZC_RESULT_WIDTH whenever the subnormal branch is taken at all
  // (lzc > fte), so neither form wraps.
  //
  // The two sticky taps move with the window:  sum_shifted_old[2*p_dst+p+3]
  // is sum_shifted[2*p_dst+p+3] in the large-norm branch (shift unchanged) and
  // sum_shifted_old[0] is sum_shifted[1] in the subnormal branch (shift +1).
  // ------------------------------------------------------------------------
  logic [DST_SHIFT_AMOUNT_WIDTH-1:0] norm_shamt_m;
  assign norm_shamt_m = (ADDEND_EXP_POSITIVE && large_norm)
                          ? leading_zero_count_q
                          : (ADDEND_EXP_POSITIVE
                               ? final_tentative_exponent_q[DST_SHIFT_AMOUNT_WIDTH-1:0]
                               : norm_shamt);

  // Do the large normalization shift
  assign sum_shifted       = sum_lower_q << norm_shamt_m;

  logic norm_carry;
  assign norm_carry = ADDEND_EXP_POSITIVE
                        ? large_norm
                        : sum_shifted[2*DST_PRECISION_BITS+PRECISION_BITS+4];

  // Further 1-bit normalization since the leading-one can be to the left or right of the (non-carry)
  // MSB of the sum.  Only the right correction is reachable: in the large-normalization branch the
  // shift distance is the leading-zero count itself, so the leading one lands either on the sum MSB
  // (lzc > 0, nothing to do) or on the carry bit (lzc == 0, align right); in the subnormal branch
  // normalized_exponent is 0 so the left correction is guarded off.
  // ------------------------------------------------------------------------
  // THE 38-BIT STICKY FIELD IS NEVER OBSERVED -- ONLY ITS OR-REDUCTION IS.
  //
  // sum_sticky_bits_o has exactly one consumer in the whole design:
  // rounding_assembly's pre_round_all_extra_bits, which feeds fpnew_rounding's
  // stochastic_rounding_bits_i.  That port is read only inside the rounder's
  // `if (EnableRSR)' arm and this build instantiates it with DEFAULT_NO_RSR, so
  // not one of the 38 bits reaches an output.  What IS observable is
  // sticky_after_norm, i.e. their OR -- and the OR can be taken straight off the
  // shifter, because the two candidate windows overlap in 37 of 38 positions:
  //
  //   |sum_sticky_bits = |sum_shifted[STK_HI-1:1]
  //                      | (carry ? sum_shifted[STK_HI] : sum_shifted[0])
  //
  // So the 38-bit 2:1 small-normalisation mux collapses to a 1-bit one and the
  // 38-bit OR tree loses a level.  final_mantissa keeps its full 25-bit mux.
  // ------------------------------------------------------------------------
  logic sum_sticky_any;

  // final_mantissa[DST_PRECISION_BITS] -- the normalised leading one -- is not
  // read by rounding_assembly at ANY destination format: the packer takes
  // final_mantissa[SUPER_DST_MAN_BITS -: MAN_BITS], the round bit takes
  // final_mantissa[SUPER_DST_MAN_BITS-MAN_BITS] and the extra-bit field takes
  // [SUPER_DST_MAN_BITS-MAN_BITS:0], and SUPER_DST_MAN_BITS = DST_PRECISION_BITS-1
  // bounds every one of them.  Driving it constant costs the top lane of this
  // 2:1 mux and its share of the pipeline bank.
  always_comb begin : small_norm
    // ONE window: norm_shamt_m already carries the small-normalisation step.
    if (ADDEND_EXP_POSITIVE) begin
      final_mantissa = {1'b0, sum_shifted[2*DST_PRECISION_BITS+PRECISION_BITS+3 -: DST_PRECISION_BITS]};
    end else begin
      // Default assignment, discarding carry bit
      final_mantissa = {1'b0, sum_shifted[2*DST_PRECISION_BITS+PRECISION_BITS+2 -: DST_PRECISION_BITS]};

      // The normalized sum has overflown, align right
      if (sum_shifted[2*DST_PRECISION_BITS+PRECISION_BITS+4]) begin // check the carry bit
        final_mantissa = {1'b0, sum_shifted[2*DST_PRECISION_BITS+PRECISION_BITS+3 -: DST_PRECISION_BITS]};
      end
    end
  end

  // ------------------------------------------------------------------------
  // THE STICKY OR IS TAKEN BEFORE THE NORMALISATION SHIFTER, NOT AFTER IT.
  //
  // sum_shifted = sum_lower << norm_shamt, so bit k of sum_lower lands at
  // position k + norm_shamt (and is lost above LZC_SUM_WIDTH-1).  The OR window
  // is the constant range [STK_HI:1], so
  //
  //     | sum_shifted[STK_HI:1]  ==  | ( sum_lower & M )   with
  //     M[k] = (1 <= k + norm_shamt <= STK_HI)
  //
  // -- a plain threshold on the shift amount, i.e. a thermometer mask, exactly
  // the same rewrite the intermediate addend's alignment shifter already uses
  // in first_shift_add.  It is bit-exact for EVERY (sum_lower, norm_shamt),
  // including norm_shamt = 0 (where M keeps bit 0 out of the window, because
  // position 0 is covered by the carry mux term below, which is left verbatim).
  //
  // What it buys: bits [STK_HI:1] of sum_shifted lose their only consumer, so
  // 37 of the shifter's 64 output lanes -- and the 37-input OR tree hanging off
  // them, one of the deepest things behind the leading-zero counter -- are
  // dead logic.  The mask is a fan-out of norm_shamt alone and is built while
  // the shifter runs.
  // ------------------------------------------------------------------------
  localparam int unsigned STK_HI = DST_PRECISION_BITS + PRECISION_BITS + 2;

  logic [LZC_SUM_WIDTH-1:0] sticky_mask;

  // ------------------------------------------------------------------------
  // ONE WINDOW: THE TWO STICKY TAPS ARE INSIDE THE MASK.
  //
  // The parent takes |sum_lower & mask| over the ORIGINAL frame's [STK_HI:1]
  // and then ORs one extra bit read off the shifter -- sum_shifted[STK_HI+1]
  // in the large-normalisation branch, sum_shifted[1] in the sub-normal one.
  // Written against norm_shamt_m (the amount the shifter actually uses,
  // = norm_shamt + !large_norm) BOTH unions are the same predicate:
  //
  //   large_norm : | (sum_lower << ns)[STK_HI+1 : 1] , ns_m = ns
  //                  -> k + ns_m in [1, STK_HI+1]
  //   sub-normal : | (sum_lower << ns)[STK_HI   : 0] , ns_m = ns + 1
  //                  -> k + ns   in [0, STK_HI]  ==  k + ns_m in [1, STK_HI+1]
  //
  // so ONE thermometer one position wider replaces the mask, the 39:1 select
  // that read sum_shifted[STK_HI+1], the 2:1 select on sum_shifted[1] and the
  // tap mux between them.  Lanes 1 and STK_HI+1 of the LZC_SUM_WIDTH-position
  // normalisation shifter lose their last consumer, and so does norm_shamt --
  // with it the DST_EXP_WIDTH-bit `fte - 1' decrement and its 2:1 amount mux,
  // which is all that block still built.
  // ------------------------------------------------------------------------
  always_comb begin : gen_sticky_mask
    for (int unsigned k = 0; k < LZC_SUM_WIDTH; k++) begin
      sticky_mask[k] = (k <= STK_HI + 1)
                       && (norm_shamt_m <= DST_SHIFT_AMOUNT_WIDTH'(STK_HI + 1 - k))
                       && ((k >= 1) || (norm_shamt_m != '0));
    end
  end

  assign sum_sticky_bits = '0;
  if (ADDEND_EXP_POSITIVE) begin : gen_positive_sticky
    assign sum_sticky_any = |(sum_lower_q & sticky_mask);
  end else begin : gen_general_sticky
    // The folded window assumes the extra subnormal left shift above.
    // Formats with underflowing products use the original two windows instead.
    assign sum_sticky_any = norm_carry ? |sum_shifted[STK_HI+1:1]
                                      : |sum_shifted[STK_HI:0];
  end

  // Exponent after the small normalization.  Large-normalization branch: normalized_exponent
  // (+1 exactly when lzc == 0) == final_tentative_exponent - lzc + 1 either way, so the large
  // shifter is not in this cone at all.  Subnormal branch: normalized_exponent is 0, so the
  // result is just the carry bit.
  assign final_exponent = large_norm
      ? signed'({(fte_lo_borrow ? fte_p1_hi_dec : fte_p1_hi), fte_lo_sub})
      : '0;   // UNOBSERVABLE: the shifter's carry bit is 0 in the subnormal branch

  // Update the sticky bit with the shifted-out bits coming from the first addition
  always_comb begin
    sticky_after_norm = sum_sticky_any | (sticky_before_add_z_q2 && ~bypass_w_q) | sticky_before_add_q2;
    if (sticky_before_add_q2 && !effective_subtraction_first_q3 && !sticky_before_add_z_q2
        && effective_subtraction_z_q2 && (~sum_sticky_any) && !info_min_is_zero_q3) begin
      sticky_after_norm = 1'b0;
    end
    if (sticky_before_add_q2 && effective_subtraction_first_q3 && !sticky_before_add_z_q2
       && !effective_subtraction_z_q2 && (~sum_sticky_any) && !info_min_is_zero_q3) begin
      sticky_after_norm = 1'b0;
    end
  end

  assign final_mantissa_o    = final_mantissa;
  assign sum_sticky_bits_o   = sum_sticky_bits;
  assign final_exponent_o    = final_exponent;
  assign sticky_after_norm_o = sticky_after_norm;

endmodule

// ---------------------------------------------------------------------------
// Rounding and result assembly: pre-round packing per format, fpnew_rounding,
// sign injection, status flags, and special-case result selection.
// ---------------------------------------------------------------------------
module opope_sdotp_rounding_assembly #(
  parameter fpnew_pkg::fmt_logic_t   SrcDotpFpFmtConfig = '1,
  parameter fpnew_pkg::fmt_logic_t   DstDotpFpFmtConfig = '1,
  parameter fpnew_pkg::rsr_impl_t    StochasticRndImplementation = fpnew_pkg::DEFAULT_NO_RSR,
  // Do not change
  localparam int unsigned DST_WIDTH = fpnew_pkg::max_fp_width(DstDotpFpFmtConfig),
  localparam int unsigned NUM_FORMATS = fpnew_pkg::NUM_FP_FORMATS,
  localparam fpnew_pkg::fp_encoding_t SUPER_FORMAT = fpnew_pkg::super_format(SrcDotpFpFmtConfig),
  localparam fpnew_pkg::fp_encoding_t SUPER_DST_FORMAT = fpnew_pkg::super_format(DstDotpFpFmtConfig),
  localparam int unsigned SUPER_MAN_BITS = SUPER_FORMAT.man_bits,
  localparam int unsigned SUPER_DST_EXP_BITS = SUPER_DST_FORMAT.exp_bits,
  localparam int unsigned SUPER_DST_MAN_BITS = (fpnew_pkg::maximum(SUPER_DST_FORMAT.man_bits, 2*SUPER_MAN_BITS + 1)
      + ((SrcDotpFpFmtConfig == 6'h02 && DstDotpFpFmtConfig == 6'h20)
          ? 2*SUPER_MAN_BITS : 0)),
  // Precision bits 'p' include the implicit bit
  localparam int unsigned PRECISION_BITS = SUPER_MAN_BITS + 1,
  localparam int unsigned DST_PRECISION_BITS = SUPER_DST_MAN_BITS + 1,
  // Stochastic rounding implementation
  localparam logic        ENABLE_RSR         = StochasticRndImplementation.EnableRSR,
  localparam int unsigned RSR_PRECISION_BITS = StochasticRndImplementation.RsrPrecision,
  localparam int unsigned LFSR_WIDTH         = StochasticRndImplementation.LfsrInternalPrecision,
  // The leading-zero counter operates on LZC_SUM_WIDTH bits
  localparam int unsigned LZC_SUM_WIDTH  = 2*DST_PRECISION_BITS + PRECISION_BITS + 5,
  localparam int unsigned LZC_RESULT_WIDTH = $clog2(LZC_SUM_WIDTH),
  localparam int unsigned DST_EXP_WIDTH = unsigned'(fpnew_pkg::maximum(SUPER_DST_EXP_BITS + 2, LZC_RESULT_WIDTH))
) (
  input  logic                                            clk_i,
  input  logic                                            rst_ni,
  input  logic [33:0]                                     sdotp_hart_id_i,
  input  logic [DST_PRECISION_BITS:0]                     final_mantissa_i,
  input  logic [DST_PRECISION_BITS+PRECISION_BITS+2:0]    sum_sticky_bits_i,
  input  logic signed [DST_EXP_WIDTH-1:0]                 final_exponent_i,
  input  logic                                            sticky_after_norm_i,
  input  logic                                            info_max_is_zero_i,
  input  logic                                            final_sign_zero_i,
  input  logic                                            final_sign_z_i,
  input  fpnew_pkg::roundmode_e                           rnd_mode_i,
  input  fpnew_pkg::fp_format_e                           dst_fmt_i,
  input  logic                                            effective_subtraction_z_i,
  input  logic                                            enable_rsr_i,
  input  logic                                            result_is_special_i,
  // fp_dst_t special result (sign, exponent, mantissa packed)
  input  logic [SUPER_DST_EXP_BITS+SUPER_DST_MAN_BITS:0]  special_result_i,
  input  fpnew_pkg::status_t                              special_status_i,
  output logic [DST_WIDTH-1:0]                            result_o,
  output fpnew_pkg::status_t                              status_o
);

  logic [DST_PRECISION_BITS:0]                    final_mantissa;
  logic [DST_PRECISION_BITS+PRECISION_BITS+2:0]   sum_sticky_bits;
  logic signed [DST_EXP_WIDTH-1:0]                final_exponent;
  logic                                           sticky_after_norm;
  logic                                           info_max_is_zero_q2;
  logic                                           final_sign_zero_q;
  logic                                           final_sign_z_q;
  fpnew_pkg::roundmode_e                          rnd_mode_q2;
  fpnew_pkg::fp_format_e                          dst_fmt_q2;
  logic                                           effective_subtraction_z_q2;
  logic                                           enable_rsr;
  logic                                           result_is_special_q;
  logic [SUPER_DST_EXP_BITS+SUPER_DST_MAN_BITS:0] special_result_q;
  fpnew_pkg::status_t                             special_status_q;

  assign final_mantissa             = final_mantissa_i;
  assign sum_sticky_bits            = sum_sticky_bits_i;
  assign final_exponent             = final_exponent_i;
  assign sticky_after_norm          = sticky_after_norm_i;
  assign info_max_is_zero_q2        = info_max_is_zero_i;
  assign final_sign_zero_q          = final_sign_zero_i;
  assign final_sign_z_q             = final_sign_z_i;
  assign rnd_mode_q2                = rnd_mode_i;
  assign dst_fmt_q2                 = dst_fmt_i;
  assign effective_subtraction_z_q2 = effective_subtraction_z_i;
  assign enable_rsr                 = enable_rsr_i;
  assign result_is_special_q        = result_is_special_i;
  assign special_result_q           = special_result_i;
  assign special_status_q           = special_status_i;

  // ----------------------------
  // Rounding and classification
  // ----------------------------
  logic                                             pre_round_sign;
  logic [SUPER_DST_EXP_BITS+SUPER_DST_MAN_BITS-1:0] pre_round_abs; // absolute value of result before rounding
  logic [1:0]                                       round_sticky_bits;
  logic [RSR_PRECISION_BITS-1:0]                    stochastic_rounding_bits; // bits for RSR rounding mode

  logic of_before_round, of_after_round; // overflow
  logic uf_before_round, uf_after_round; // underflow

  logic [NUM_FORMATS-1:0][SUPER_DST_EXP_BITS+SUPER_DST_MAN_BITS-1:0] fmt_pre_round_abs; // per format
  logic [NUM_FORMATS-1:0][1:0]                                       fmt_round_sticky_bits;
  logic [NUM_FORMATS-1:0][RSR_PRECISION_BITS-1:0]                    fmt_stochastic_rounding_bits;// bits for RSR rounding mode

  logic [NUM_FORMATS-1:0]                           fmt_of_after_round;
  logic [NUM_FORMATS-1:0]                           fmt_uf_after_round;

  logic                                             rounded_sign;
  logic [SUPER_DST_EXP_BITS+SUPER_DST_MAN_BITS-1:0] rounded_abs; // absolute value of result after rounding
  logic                                             result_zero;

  // Classification before round. RISC-V mandates checking underflow AFTER rounding
  assign of_before_round = final_exponent >= 2**(fpnew_pkg::exp_bits(dst_fmt_q2))-1; // infinity exponent is all ones
  assign uf_before_round = final_exponent == 0;               // exponent for subnormals capped to 0

  // Pack exponent and mantissa into proper rounding form
  for (genvar fmt = 0; fmt < int'(NUM_FORMATS); fmt++) begin : gen_res_assemble
    // Set up some constants
    localparam int unsigned EXP_BITS = fpnew_pkg::exp_bits(fpnew_pkg::fp_format_e'(fmt));
    localparam int unsigned MAN_BITS = fpnew_pkg::man_bits(fpnew_pkg::fp_format_e'(fmt));
    localparam int unsigned ALL_EXTRA_BITS = fpnew_pkg::maximum(SUPER_DST_MAN_BITS-MAN_BITS+1+DST_PRECISION_BITS+PRECISION_BITS+2+1, 1);

    logic [EXP_BITS-1:0] pre_round_exponent;
    logic [MAN_BITS-1:0] pre_round_mantissa;
    logic [ALL_EXTRA_BITS-1:0] pre_round_all_extra_bits;

    if (DstDotpFpFmtConfig[fmt]) begin : active_dst_format

      assign pre_round_exponent = (of_before_round) ? 2**EXP_BITS-2 : final_exponent[EXP_BITS-1:0];
      assign pre_round_mantissa = (of_before_round) ? '1 : final_mantissa[SUPER_DST_MAN_BITS-:MAN_BITS];
      // Assemble result before rounding. In case of overflow, the largest normal value is set.
      assign fmt_pre_round_abs[fmt] = {pre_round_exponent, pre_round_mantissa}; // 0-extend

      // Round bit is after mantissa (1 in case of overflow for rounding)
      assign fmt_round_sticky_bits[fmt][1] = final_mantissa[SUPER_DST_MAN_BITS-MAN_BITS] |
                                             of_before_round;
      assign pre_round_all_extra_bits = {final_mantissa[SUPER_DST_MAN_BITS-MAN_BITS:0], sum_sticky_bits};
      assign fmt_stochastic_rounding_bits[fmt] = (of_before_round) ? '1
                                                  : pre_round_all_extra_bits[(ALL_EXTRA_BITS-1)-:RSR_PRECISION_BITS];

      // remaining bits in mantissa to sticky (1 in case of overflow for rounding)
      if (MAN_BITS < SUPER_DST_MAN_BITS) begin : narrow_sticky
        assign fmt_round_sticky_bits[fmt][0] = (| final_mantissa[SUPER_DST_MAN_BITS-MAN_BITS-1:0]) |
                                               sticky_after_norm | of_before_round;
      end else begin : normal_sticky
        assign fmt_round_sticky_bits[fmt][0] = sticky_after_norm | of_before_round;
      end
    end else begin : inactive_format
      assign fmt_pre_round_abs[fmt] = '{default: fpnew_pkg::DONT_CARE};
      assign fmt_round_sticky_bits[fmt] = '{default: fpnew_pkg::DONT_CARE};
      assign fmt_stochastic_rounding_bits[fmt] = '{default: fpnew_pkg::DONT_CARE};
    end
  end

  // Assemble result before rounding. In case of overflow, the largest normal value is set.
  assign pre_round_abs      = fmt_pre_round_abs[dst_fmt_q2];

  // In case of overflow, the round and sticky bits are set for proper rounding
  assign stochastic_rounding_bits = fmt_stochastic_rounding_bits[dst_fmt_q2];
  assign round_sticky_bits  = fmt_round_sticky_bits[dst_fmt_q2];
  // UNOBSERVABLE round/sticky term -- and with it the sign path's dependence on
  // sticky_after_norm, which resolves last in this stage.
  // ------------------------------------------------------------------------
  // THE ZERO-RESULT SIGN OVERRIDE IS DEAD: final_sign_z ALREADY CARRIES IT.
  //
  // The override fires only when info_max_is_zero and pre_round_abs == 0.  At
  // this build info_max_is_zero forces ALL THREE addends to zero (see
  // first_shift_add), so the whole spine collapses:
  //   sum_raw   = eff_sub_first ? 2**(2*p_dst+3) : 0     -> sum = 0, carry = eff_sub_first
  //   neg_w     = eff_sub_first & ~sum_carry             = 0
  //   sum_raw_z = 0  -> negate_sel = result_neg = 0, sum_z = 0, sticky = 0
  // hence pre_round_abs == 0 and round_sticky_bits == 0, i.e. fpnew_rounding
  // sees exact_zero_o = 1 and drives
  //     sign_o = effective_subtraction_z ? (rnd_mode == RDN) : sign_i.
  // With rnd_mode tied to RNE by the wrapper the first arm is 0, so sign_i is
  // observable ONLY when effective_subtraction_z is clear -- and there
  //     effective_subtraction_z = e_add = sel_sign ^ final_sign
  // is 0, i.e. the minimum addend's sign equals the first sum's sign frame.
  // Case-splitting on eff_sub_first with s_max/s_int/s_min the three signs:
  //   eff_sub_first = 0 : final_sign = s_max, so s_min = s_max, and the
  //       parent's override value  s_max ? (s_int | s_min) : (s_int & s_min)
  //       equals s_max = final_sign_z.
  //   eff_sub_first = 1 : final_sign = (rnd_mode == RDN) = 0, so s_min = 0 and
  //       s_int != s_max; the override value is then s_max ? s_int : 0 = 0,
  //       which is final_sign_z again.
  // Either way the two agree wherever the mux output can be seen, so the whole
  // final_sign_zero path -- and with it a 31-bit zero-detect here, two 24-bit
  // and one 10-bit comparator in first_shift_add, five exponent_prep outputs
  // (exponent_int, info_int_is_zero, info_max_is_zero, addend_int_sign,
  // addend_max_sign) and their pipeline banks -- is dead logic.
  // Certified end to end by the whole-top Formality CEC.
  // ------------------------------------------------------------------------
  assign pre_round_sign     = final_sign_z_q;

  // Perform the rounding
  fpnew_rounding #(
    .AbsWidth     ( SUPER_DST_EXP_BITS + SUPER_DST_MAN_BITS ),
    .EnableRSR    ( ENABLE_RSR         ),
    .RsrPrecision ( RSR_PRECISION_BITS ),
    .LfsrWidth    ( LFSR_WIDTH         )
  ) i_fpnew_rounding (
    .clk_i                      ( clk_i                      ),
    .rst_ni                     ( rst_ni                     ),
    .id_i                       ( sdotp_hart_id_i            ),
    .abs_value_i                ( pre_round_abs              ),
    .en_rsr_i                   ( enable_rsr                 ),
    .sign_i                     ( pre_round_sign             ),
    .round_sticky_bits_i        ( round_sticky_bits          ),
    .stochastic_rounding_bits_i ( stochastic_rounding_bits   ),
    .rnd_mode_i                 ( rnd_mode_q2                ),
    .effective_subtraction_i    ( effective_subtraction_z_q2 ),
    .abs_rounded_o              ( rounded_abs                ),
    .sign_o                     ( rounded_sign               ),
    .exact_zero_o               ( result_zero                )
  );

  logic [NUM_FORMATS-1:0][DST_WIDTH-1:0] fmt_result;

  for (genvar fmt = 0; fmt < int'(NUM_FORMATS); fmt++) begin : gen_sign_inject
    // Set up some constants
    localparam int unsigned FP_WIDTH = fpnew_pkg::fp_width(fpnew_pkg::fp_format_e'(fmt));
    localparam int unsigned EXP_BITS = fpnew_pkg::exp_bits(fpnew_pkg::fp_format_e'(fmt));
    localparam int unsigned MAN_BITS = fpnew_pkg::man_bits(fpnew_pkg::fp_format_e'(fmt));

    if (DstDotpFpFmtConfig[fmt]) begin : active_dst_format
      always_comb begin : post_process
        // detect of / uf
        fmt_uf_after_round[fmt] = rounded_abs[EXP_BITS+MAN_BITS-1:MAN_BITS] == '0; // denormal
        fmt_of_after_round[fmt] = rounded_abs[EXP_BITS+MAN_BITS-1:MAN_BITS] == '1; // inf exp.

        // Assemble regular result, nan box short ones.
        fmt_result[fmt]               = '1;
        fmt_result[fmt][FP_WIDTH-1:0] = {rounded_sign, rounded_abs[EXP_BITS+MAN_BITS-1:0]};
      end
    end else begin : inactive_format
      assign fmt_uf_after_round[fmt] = fpnew_pkg::DONT_CARE;
      assign fmt_of_after_round[fmt] = fpnew_pkg::DONT_CARE;
      assign fmt_result[fmt]         = '{default: fpnew_pkg::DONT_CARE};
    end
  end

  // Classification after rounding select by destination format
  assign uf_after_round = fmt_uf_after_round[dst_fmt_q2];
  assign of_after_round = fmt_of_after_round[dst_fmt_q2];

  // -----------------
  // Result selection
  // -----------------
  logic [DST_WIDTH-1:0] regular_result;
  fpnew_pkg::status_t   regular_status;

  // Assemble regular result
  assign regular_result    = fmt_result[dst_fmt_q2];
  assign regular_status.NV = 1'b0; // only valid cases are handled in regular path
  assign regular_status.DZ = 1'b0; // no divisions
  assign regular_status.OF = of_before_round | of_after_round;   // rounding can introduce overflow
  assign regular_status.UF = uf_after_round & regular_status.NX; // only inexact results raise UF
  assign regular_status.NX = (| round_sticky_bits) | of_before_round | of_after_round;

  // Final results for output pipeline
  logic [DST_WIDTH-1:0] result_d;
  fpnew_pkg::status_t   status_d;

  // Select output depending on special case detection
  assign result_d = result_is_special_q ? special_result_q : regular_result;
  assign status_d = result_is_special_q ? special_status_q : regular_status;

  assign result_o = result_d;
  assign status_o = status_d;

endmodule

// ---------------------------------------------------------------------------
// First shift/add stage: addend sorting, alignment shift of the intermediate
// addend, first two-term adder (max + int), second-shift preparation of the
// minimum addend, and all-zero sign resolution.
// ---------------------------------------------------------------------------
module opope_sdotp_first_shift_add #(
  parameter fpnew_pkg::fmt_logic_t SrcDotpFpFmtConfig = '1,
  parameter fpnew_pkg::fmt_logic_t DstDotpFpFmtConfig = '1,
  // Do not change
  localparam fpnew_pkg::fp_encoding_t SUPER_FORMAT = fpnew_pkg::super_format(SrcDotpFpFmtConfig),
  localparam fpnew_pkg::fp_encoding_t SUPER_DST_FORMAT = fpnew_pkg::super_format(DstDotpFpFmtConfig),
  localparam int unsigned SUPER_MAN_BITS = SUPER_FORMAT.man_bits,
  localparam int unsigned SUPER_DST_EXP_BITS = SUPER_DST_FORMAT.exp_bits,
  localparam int unsigned SUPER_DST_MAN_BITS = (fpnew_pkg::maximum(SUPER_DST_FORMAT.man_bits, 2*SUPER_MAN_BITS + 1)
      + ((SrcDotpFpFmtConfig == 6'h02 && DstDotpFpFmtConfig == 6'h20)
          ? 2*SUPER_MAN_BITS : 0)),
  // Precision bits 'p' include the implicit bit
  localparam int unsigned PRECISION_BITS = SUPER_MAN_BITS + 1,
  localparam int unsigned DST_PRECISION_BITS = SUPER_DST_MAN_BITS + 1,
  localparam int unsigned ADDITIONAL_PRECISION_BITS = fpnew_pkg::maximum(DST_PRECISION_BITS - 2 * PRECISION_BITS, 0),
  // The leading-zero counter operates on LZC_SUM_WIDTH bits
  localparam int unsigned LZC_SUM_WIDTH  = 2*DST_PRECISION_BITS + PRECISION_BITS + 5,
  localparam int unsigned LZC_RESULT_WIDTH = $clog2(LZC_SUM_WIDTH),
  localparam int unsigned DST_EXP_WIDTH = unsigned'(fpnew_pkg::maximum(SUPER_DST_EXP_BITS + 2, LZC_RESULT_WIDTH)),
  // Shift amount width: maximum internal mantissa size is 2*DST_PRECISION_BITS+3 bits
  localparam int unsigned SHIFT_AMOUNT_WIDTH = $clog2(2*DST_PRECISION_BITS+PRECISION_BITS+4),
  localparam int unsigned DST_SHIFT_AMOUNT_WIDTH = $clog2(2*DST_PRECISION_BITS+PRECISION_BITS+5)
) (
  input  fpnew_pkg::operation_e                          op_i,
  input  logic [2*PRECISION_BITS-1:0]                    product_x_i,
  input  logic [2*PRECISION_BITS-1:0]                    product_y_i,
  input  logic [DST_PRECISION_BITS-1:0]                  mantissa_e_i,
  input  logic [DST_PRECISION_BITS-1:0]                  mantissa_a_vsum_i,
  input  logic [DST_PRECISION_BITS-1:0]                  mantissa_c_vsum_i,
  input  logic [SHIFT_AMOUNT_WIDTH-1:0]                  addend_shamt_i,
  input  logic                                           effective_subtraction_first_i,
  input  logic                                           tentative_sign_i,
  input  logic signed [DST_EXP_WIDTH-1:0]                tentative_exponent_i,
  input  logic signed [DST_EXP_WIDTH-1:0]                exponent_int_i,
  input  logic signed [DST_EXP_WIDTH-1:0]                exponent_min_i,
  input  logic                                           info_min_is_zero_i,
  input  logic                                           info_int_is_zero_i,
  input  logic                                           info_max_is_zero_i,
  input  logic                                           addend_min_sign_i,
  input  logic                                           addend_int_sign_i,
  input  logic                                           addend_max_sign_i,
  input  logic [2:0]                                     exponent_cmp_i,
  input  fpnew_pkg::roundmode_e                          rnd_mode_i,
  output logic                                           sticky_before_add_o,
  output logic [2*DST_PRECISION_BITS+2:0]                sum_o,
  output logic                                           sum_carry_o,
  output logic                                           final_sign_o,
  output logic                                           bypass_w_o,
  output logic signed [DST_EXP_WIDTH-1:0]                exponent_w_o,
  output logic [2*DST_PRECISION_BITS+PRECISION_BITS+3:0] addend_min_after_shift_o,
  output logic                                           sticky_before_add_z_o,
  output logic                                           final_sign_zero_o
);

  logic   [2*PRECISION_BITS-1:0]   product_x_q, product_y_q;
  logic   [DST_PRECISION_BITS-1:0] mantissa_e;
  logic   [DST_PRECISION_BITS-1:0] mantissa_a_vsum, mantissa_c_vsum;
  logic   [SHIFT_AMOUNT_WIDTH-1:0] addend_shamt_q;
  logic                            effective_subtraction_first_q;
  logic                            tentative_sign_q;
  logic signed [DST_EXP_WIDTH-1:0] tentative_exponent_q;
  logic signed [DST_EXP_WIDTH-1:0] exponent_int_q;
  logic signed [DST_EXP_WIDTH-1:0] exponent_min_q;
  logic                            info_min_is_zero_q;
  logic                            info_int_is_zero_q;
  logic                            info_max_is_zero_q;
  logic                            addend_min_sign_q;
  logic                            addend_int_sign_q;
  logic                            addend_max_sign_q;
  logic [2:0]                      exponent_cmp_q;
  fpnew_pkg::roundmode_e           rnd_mode_q;

  assign product_x_q                   = product_x_i;
  assign product_y_q                   = product_y_i;
  assign mantissa_e                    = mantissa_e_i;
  assign mantissa_a_vsum               = mantissa_a_vsum_i;
  assign mantissa_c_vsum               = mantissa_c_vsum_i;
  assign addend_shamt_q                = addend_shamt_i;
  assign effective_subtraction_first_q = effective_subtraction_first_i;
  assign tentative_sign_q              = tentative_sign_i;
  assign tentative_exponent_q          = tentative_exponent_i;
  assign exponent_int_q                = exponent_int_i;
  assign exponent_min_q                = exponent_min_i;
  assign info_min_is_zero_q            = info_min_is_zero_i;
  assign info_int_is_zero_q            = info_int_is_zero_i;
  assign info_max_is_zero_q            = info_max_is_zero_i;
  assign addend_min_sign_q             = addend_min_sign_i;
  assign addend_int_sign_q             = addend_int_sign_i;
  assign addend_max_sign_q             = addend_max_sign_i;
  assign exponent_cmp_q                = exponent_cmp_i;
  assign rnd_mode_q                    = rnd_mode_i;

  // ==========================================================================
  // ELABORATION-TIME EXPONENT-RANGE FACT
  //
  //   ADDEND_EXP_POSITIVE == 1  <=>  at EVERY (src_fmt, dst_fmt) pair this
  //   elaboration admits, a NON-ZERO addend's biased destination exponent is
  //   >= 1, while a ZERO product's is 2 - bias_dst < 1.
  //
  // exponent_prep builds the three addend exponents as
  //     product : (a.is_zero | b.is_zero) ? 2 - bias_dst
  //                                       : e_a + sub_a + e_b + sub_b
  //                                         - 2*bias_src + bias_dst + 1
  //     accum   : e_e + ~e.is_normal
  // Each of (e_a + sub_a) and (e_b + sub_b) is >= 1 (a normal has e >= 1 and
  // sub = 0; a sub-normal has e = 0 and sub = 1), so a NON-ZERO product is
  // >= 3 + bias_dst - 2*bias_src, and the accumulator term is >= 1 for every
  // encoding (e = 0 forces is_normal = 0, hence the +1).  The condition below
  // is therefore exactly "3 + bias_dst - 2*bias_src >= 1 for all enabled
  // pairs", evaluated on the worst pair (largest source bias, smallest
  // destination bias).
  //
  // At the measured FP16 -> FP32 build: bias_src = 15, bias_dst = 127, so a
  // non-zero product lands in [100, 160], a zero product at -125 and the
  // accumulator in [1, 256].  ADDEND_EXP_POSITIVE = 1.
  // ==========================================================================
  function automatic bit nonzero_addend_exponents_positive
      (input fpnew_pkg::fmt_logic_t src_cfg, input fpnew_pkg::fmt_logic_t dst_cfg);
    automatic int max_src_bias = 0;
    automatic int min_dst_bias = 0;
    automatic bit have_dst     = 1'b0;
    for (int unsigned f = 0; f < fpnew_pkg::NUM_FP_FORMATS; f++) begin
      if (src_cfg[f] && (int'(fpnew_pkg::bias(fpnew_pkg::fp_format_e'(f))) > max_src_bias))
        max_src_bias = int'(fpnew_pkg::bias(fpnew_pkg::fp_format_e'(f)));
      if (dst_cfg[f] && (!have_dst ||
                         (int'(fpnew_pkg::bias(fpnew_pkg::fp_format_e'(f))) < min_dst_bias))) begin
        min_dst_bias = int'(fpnew_pkg::bias(fpnew_pkg::fp_format_e'(f)));
        have_dst     = 1'b1;
      end
    end
    return have_dst && ((3 + min_dst_bias) >= (2*max_src_bias + 1));
  endfunction

  localparam bit ADDEND_EXP_POSITIVE =
      nonzero_addend_exponents_positive(SrcDotpFpFmtConfig, DstDotpFpFmtConfig);

  // ------------------
  // Shift data path
  // ------------------
  // The three addends are DST_PRECISION_BITS-wide since they might contain a product, which is
  // expressed with 2*PRECISION_BITS (< DST_PRECISION_BITS), or the accumulator which is expressed
  // with DST_PRECISION_BITS. In the case of non-expanding VSUM, all the operands are
  // DST_PRECISION_BITS-wide, if the largest format allowed is selected, or boxed into
  // DST_PRECISION_BITS, if a narrower format is selected.
  logic   [DST_PRECISION_BITS-1:0] addend_x, addend_y, addend_z;
  logic   [DST_PRECISION_BITS-1:0] addend_max, addend_int, addend_min;
  logic [2*DST_PRECISION_BITS+2:0] addend_max_shifted;
  logic [2*DST_PRECISION_BITS+2:0] addend_int_after_shift;
  logic                            sticky_before_add;
  logic [2*DST_PRECISION_BITS+2:0] addend_int_shifted;
  logic                            inject_carry_in;     // inject carry for subtractions if needed

  // Bypass the multipliers in case of non-expanding VSUM
  // Place the products in the upper part of the addend in case of expanding operations (The addend
  // uses DST_PRECISION_BITS while 2*PRECISION_BITS might be narrower)
  assign addend_x = (op_i == fpnew_pkg::VSUM)
                      ? mantissa_a_vsum : product_x_q << ADDITIONAL_PRECISION_BITS;
  assign addend_y = (op_i == fpnew_pkg::VSUM)
                      ? mantissa_c_vsum : product_y_q << ADDITIONAL_PRECISION_BITS;
  assign addend_z = mantissa_e;

  // Sorting the addends
  always_comb begin : sort_addends
    case (exponent_cmp_q)
      // (x < y), (x < z), (y < z)
      3'b000  : {addend_max, addend_int, addend_min} = {addend_z, addend_y, addend_x};
      // (x < y), (x >= z), (y < z)
      3'b001  : {addend_max, addend_int, addend_min} = {addend_y, addend_z, addend_x};
      // // (x < y), (x < z), (y >= z) => IMPOSSIBLE
      // 3'b010  : IMPOSSIBLE
      // (x < y), (x >= z), (y >= z)
      3'b011  : {addend_max, addend_int, addend_min} = {addend_y, addend_x, addend_z};
      // (x >= y), (x < z), (y < z)
      3'b100  : {addend_max, addend_int, addend_min} = {addend_z, addend_x, addend_y};
      // // (x >= y), (x < z), (y >= z) => IMPOSSIBLE
      // 3'b101  : IMPOSSIBLE
      // (x >= y), (x >= z), (y < z)
      3'b110  : {addend_max, addend_int, addend_min} = {addend_x, addend_z, addend_y};
      // (x >= y), (x >= z), (y >= z)
      3'b111  : {addend_max, addend_int, addend_min} = {addend_x, addend_y, addend_z};
      // transitivity-impossible codes: don't-care, so the 6-way decode collapses
      // into three clean 3:1 selects on 24-bit buses
      3'b010, 3'b101 : {addend_max, addend_int, addend_min} = {(3*DST_PRECISION_BITS){1'bx}};
      default : {addend_max, addend_int, addend_min} = {addend_x, addend_y, addend_z};
    endcase
  end

  // Product max is placed into a 2p+3 bit wide vector. It is padded with 3 bits for rounding purposes:
  // | product_max  |  rnd  |
  //  <-  2p_dst  -> <  3   >
  assign addend_max_shifted = addend_max << (3 + DST_PRECISION_BITS); // constant shift

  // In parallel, the min product is right-shifted according to the exponent difference. Up to p_dst
  // bits are shifted out and compressed into a sticky bit.
  // BEFORE THE SHIFT:
  // | addend_int | 000.....000 |
  //  <- p_dst  -> <- p_dst+3 ->
  // AFTER THE SHIFT:
  // | 000..........000 | addend_min | 000..................0GR |    sticky bits    |
  //  <- addend_shamt -> <- p_dst  -> <- p_dst+3-addend_shamt -> <-  up to p_dst  ->
  // ------------------------------------------------------------------------
  // THE INTERMEDIATE ADDEND'S ALIGNMENT SHIFTER CARRIES NO STICKY FIELD.
  //
  // The parent shifts a (2*p_dst+3+p_dst)-bit vector and OR-reduces the bottom
  // p_dst bits.  addend_int[k] lands in that sticky field iff
  //     k + INT_PRE_SHIFT - s  <  p_dst        i.e.  s >= k + INT_PRE_SHIFT-p_dst+1
  // and it is still inside the vector at all iff  s <= k + INT_PRE_SHIFT.  Both
  // are plain thresholds on the SHIFT AMOUNT, so the sticky is a mask-and-OR of
  // addend_int with no shifter, and the shifter narrows to the 2*p_dst+3 bits
  // the adder actually consumes.
  // ------------------------------------------------------------------------
  localparam int unsigned INT_PRE_SHIFT = 2*DST_PRECISION_BITS + 3;

  // ------------------------------------------------------------------------
  // BOTH INTERMEDIATE-ADDEND STICKIES ARE ENTRIES OF ONE PREFIX-OR.
  //
  // Each mask keeps a CONTIGUOUS LOW WINDOW of addend_int:
  //     int_drop_mask[k] = (s >= k + T_DROP)  <=>  k <= s - T_DROP
  //     int_low_mask [k] = (s >  k)           <=>  k <= s - 1
  // so `|(addend_int & mask)' is `|addend_int[j:0]' at j = s - T_DROP resp.
  // j = s - 1, saturated at DST_PRECISION_BITS-1 (a window wider than the
  // addend keeps all of it) and empty when the index would go negative.
  // ONE prefix-OR of addend_int -- DST_PRECISION_BITS-1 OR gates, built from
  // the addend alone and therefore in parallel with the sort's own fan-out --
  // then feeds two selects, in place of 2*DST_PRECISION_BITS threshold
  // comparators, 2*DST_PRECISION_BITS AND gates and two OR trees.
  //
  // int_kept_mask / min_kept_mask were already dead in the parent (round 1
  // proved both unobservable and dropped their consumers, leaving the mask
  // generators behind); they are removed here, which changes no synthesised
  // gate -- DC deletes an unloaded mask either way -- and only shrinks the
  // ELABORATED netlist.
  // ------------------------------------------------------------------------
  localparam int unsigned T_DROP = INT_PRE_SHIFT - DST_PRECISION_BITS + 1;

  logic [DST_PRECISION_BITS-1:0] int_prefix;   // int_prefix[j] = |addend_int[j:0]
  logic   [SHIFT_AMOUNT_WIDTH-1:0] sba_idx;

  always_comb begin : gen_int_prefix
    int_prefix[0] = addend_int[0];
    for (int unsigned j = 1; j < DST_PRECISION_BITS; j++)
      int_prefix[j] = int_prefix[j-1] | addend_int[j];
  end

  // saturating window index; the guards below keep the empty window at 0
  // Both indices are clamped on BOTH sides, so the prefix vector is never read
  // out of range -- an out-of-range index is an x in simulation and a
  // tool-dependent don't-care in synthesis, and this datapath has already been
  // bitten once by x resolved differently by two front ends.
  assign sba_idx = (addend_shamt_q <= SHIFT_AMOUNT_WIDTH'(T_DROP))
                     ? '0
                 : (addend_shamt_q >= SHIFT_AMOUNT_WIDTH'(T_DROP + DST_PRECISION_BITS - 1))
                     ? SHIFT_AMOUNT_WIDTH'(DST_PRECISION_BITS - 1)
                     : (addend_shamt_q - SHIFT_AMOUNT_WIDTH'(T_DROP));

  // The kept field is the HIGH 2*p_dst+3 bits of the parent's vector, i.e. the
  // same placement with the pre-shift reduced by exactly the p_dst sticky bits.
  assign addend_int_after_shift =
      (addend_int << (INT_PRE_SHIFT - DST_PRECISION_BITS)) >> addend_shamt_q;

  assign sticky_before_add     = (addend_shamt_q >= SHIFT_AMOUNT_WIDTH'(T_DROP))
                                 && int_prefix[sba_idx];

  // In case of a subtraction, the addend is inverted
  assign addend_int_shifted  = (effective_subtraction_first_q) ? ~addend_int_after_shift : addend_int_after_shift;
  assign inject_carry_in = effective_subtraction_first_q & ~sticky_before_add;

  // ------
  // Adder
  // ------
  logic [2*DST_PRECISION_BITS+3:0] sum_raw;   // added one bit for the carry
  logic                            sum_carry; // observe carry bit from sum for sign fixing
  logic [2*DST_PRECISION_BITS+2:0] sum;       // discard carry
  logic                            final_sign;
  logic                            sum_exact_zero;

  // Mantissa adder (addend_max + addend_int)
  // Same identity as in three_way_add2: addend_max_shifted is addend_max shifted
  // up by DST_PRECISION_BITS+3, so its low bit is structurally 0 and the
  // carry-in rides in it.  Deletes the second of the two DW01_adds DC
  // elaborates for a three-operand sum (add_1033_2, 61.5 units at width 52).
  assign sum_raw = (addend_max_shifted | inject_carry_in) + addend_int_shifted;
  assign sum_carry = sum_raw[2*DST_PRECISION_BITS+3];

  // The first sum leaves this module in TWO'S-COMPLEMENT form relative to the
  // max addend's sign frame -- {sum_carry_o, sum_o} is sum_raw verbatim -- instead
  // of sign-magnitude.  three_way_add2 rebuilds the signed value with two XORs and
  // absorbs the sign fix into the conditional negate it already performs on its own
  // result, so this 51-bit conditional negate disappears from the serial spine.
  assign sum        = sum_raw[2*DST_PRECISION_BITS+2:0];

  // Check whether the result is an exact zero for rounding purposes (needed to set the sign of a
  // final result equal to zero)
  // sum_raw = 2**(2*p_dst+3) + (max - int - sticky) on an effective subtraction, so
  // sum == 0 && sum_carry holds exactly when sticky is clear and the two aligned
  // addends are equal.  Comparing them directly keeps the adder out of this cone.
  // `!sticky_before_add && addend_int_after_shift[p_dst+2:0] == 0` is exactly
  // "no bit of addend_int survives below the max addend's field", i.e. the SAME
  // mask-and-OR one threshold wider -- so the 2*p_dst+3-bit equality shrinks to
  // the p_dst-bit one that actually compares against addend_max.
  // ------------------------------------------------------------------------
  // int_low_any IS THE SHIFTER'S OWN LOW FIELD, OR-ED WITH THE STICKY.
  //
  // addend_int[k] lands at bit k + INT_PRE_SHIFT-p_dst - s of the kept vector,
  // so it survives BELOW the max addend's field (bit p_dst+3 upwards, i.e.
  // inside addend_int_after_shift[p_dst+2:0]) iff k < s, and it leaves the
  // vector altogether iff k <= s - T_DROP, which is exactly the window
  // sticky_before_add already reduces.  The two windows are adjacent and their
  // union is [0, min(s-1, p_dst-1)] -- the whole of int_low_any:
  //
  //     int_low_any == |addend_int_after_shift[p_dst+2:0] | sticky_before_add
  //
  // for EVERY shift amount (s = 0 leaves both terms empty; s >= INT_PRE_SHIFT
  // empties the vector and the sticky covers the whole addend).  The second
  // read of int_prefix -- a p_dst:1 select plus its two-sided index clamp --
  // is retired; what replaces it is an OR of the p_dst+3 low bits of a vector
  // the equality below already waits for, so nothing is added to the depth of
  // this cone.
  // ------------------------------------------------------------------------
  logic int_low_any;
  assign int_low_any = (| addend_int_after_shift[DST_PRECISION_BITS+2:0]) | sticky_before_add;

  assign sum_exact_zero = effective_subtraction_first_q && !int_low_any
                          && (addend_int_after_shift[2*DST_PRECISION_BITS+2:DST_PRECISION_BITS+3]
                              == addend_max);
  // The original final_sign is `tentative_sign_q ^ (eff_sub_first & ~sum_carry)`,
  // overridden by the round mode on an exact zero.  Only the SIGN FRAME -- the
  // first factor -- is exported now; three_way_add2 re-applies the
  // `eff_sub_first & ~sum_carry` term itself (it already has both signals), which
  // is what lets its effective-subtraction decision be made before the first adder
  // resolves.  This output is now free of the adder.
  assign final_sign = sum_exact_zero ? (rnd_mode_q == fpnew_pkg::RDN) : tentative_sign_q;

  // -------------
  // Second Shift
  // -------------
  logic signed [DST_EXP_WIDTH-1:0] exponent_difference_z;
  logic signed [DST_EXP_WIDTH-1:0] exponent_w;
  logic signed [DST_EXP_WIDTH-1:0] tentative_exponent_z;

  // W comes from the first addition. Adding +1 to take into account the following shift
  assign exponent_w = signed'(tentative_exponent_q + 1);
  // Exponent difference is the exponent of the first addition result (W) minus the minimum exponent
  // ------------------------------------------------------------------------
  // ONE CARRY CHAIN, NOT TWO IN SERIES.
  //
  // exponent_w = tentative_exponent_q + 1 is exported on exponent_w_o, so its
  // incrementer has a consumer of its own and DC keeps it; written as
  // `exponent_w - exponent_min_q' the difference then waits for that
  // incrementer before its own DST_EXP_WIDTH-bit subtract can start.  Both
  // operands are registered, so the three-term form below is the SAME value
  // (the container is DST_EXP_WIDTH-bit signed and both forms wrap identically)
  // in half the depth -- and that depth sits directly in front of
  // addend_shamt_z, i.e. in front of the minimum addend's 64-position barrel
  // shifter, its sticky (min_drop_cnt -> min_prefix) and therefore bypass_w.
  // exponent_w's own incrementer stays, but its only consumer is now a
  // pipeline register with the whole stage to settle in.
  // ------------------------------------------------------------------------
  assign exponent_difference_z = signed'(tentative_exponent_q - exponent_min_q + 1);
  // The tentative exponent will be the larger of W exponent or the minimum exponent
  assign tentative_exponent_z  = exponent_w;

  // Shift amount for addend based on exponents (unsigned as only right shifts)
  logic [DST_SHIFT_AMOUNT_WIDTH-1:0] addend_shamt_z;
  logic   [2*DST_PRECISION_BITS+PRECISION_BITS+3:0] addend_min_after_shift;
  logic                              sticky_before_add_z;   // shifted-out bits, compressed
  logic                              bypass_w;              // first sum is an exact zero
  logic signed [DST_EXP_WIDTH:0]     bypass_shamt_full;
  logic [DST_SHIFT_AMOUNT_WIDTH-1:0] bypass_shamt;

  // ------------------------------------------------------------------------
  // THE SECOND SHIFT AMOUNT SATURATES BY OR-ING, NOT BY COMPARING.
  //
  // exponent_difference_z = exponent_max + 1 - exponent_min >= 1 (the sort
  // guarantees exponent_min <= exponent_max, and ADDEND_EXP_POSITIVE bounds
  // exponent_max + 1 inside the positive half of the signed container), so the
  // value is NON-NEGATIVE and the clamp limit MIN_FULL_SHIFT is exactly
  // 2**DST_SHIFT_AMOUNT_WIDTH - 1 at this parameterisation.  `x > limit' is then
  // just `any high bit set', and `x > limit ? limit : x' is an OR with the
  // replicated flag -- in place of the DST_EXP_WIDTH-bit signed compare against
  // a constant and the DST_SHIFT_AMOUNT_WIDTH-bit mux the parent built in front
  // of the minimum addend's barrel shifter.
  // ------------------------------------------------------------------------
  if (ADDEND_EXP_POSITIVE &&
      (2*DST_PRECISION_BITS + PRECISION_BITS + 4 == (2**DST_SHIFT_AMOUNT_WIDTH) - 1))
  begin : gen_shamt_z_or
    assign addend_shamt_z =
        exponent_difference_z[DST_SHIFT_AMOUNT_WIDTH-1:0]
        | {DST_SHIFT_AMOUNT_WIDTH{|exponent_difference_z[DST_EXP_WIDTH-1:DST_SHIFT_AMOUNT_WIDTH]}};
  end else begin : gen_shamt_z_cmp
    always_comb begin : addend_shift_amount_z
      if (exponent_difference_z <= signed'(2 * DST_PRECISION_BITS + PRECISION_BITS + 4)) begin
        addend_shamt_z = unsigned'(signed'(exponent_difference_z));
      end else begin
        addend_shamt_z = 2 * DST_PRECISION_BITS + PRECISION_BITS + 4;
      end
    end
  end

  // ------------------------------------------------------------------------
  // THE MINIMUM ADDEND IS SHIFTED EXACTLY ONCE.
  //
  // The parent shifted it twice: here, by addend_shamt_z, to line it up with
  // the first sum; and again inside three_way_add2, by
  // bypass_shamt = clamp(1 - exponent_min), to build the value that REPLACES
  // the second adder when the first sum came out exactly zero.  Both are
  //
  //     addend_min[k]  ->  bit (MIN_PRE_SHIFT - s + k)  of a p_dst*2+p+4 field
  //
  // (the parent's 87-bit form keeps [86:24] of `(addend_min << 63) >> s`, which
  // is that same field, and its 64-bit bypass form has a structurally-zero top
  // bit because MIN_PRE_SHIFT + p_dst - 1 = 62), so the two differ ONLY in the
  // amount.  bypass_w decides which of the two results is observed at all --
  // the frame adder's output is discarded whenever it is set -- so one shifter
  // behind a 6-bit amount mux reproduces both, bit for bit.
  //
  // The sticky bit is what used to make that impossible: it is the OR of the
  // parent's low 24 shifter bits and bypass_w depends on it.  addend_min[k]
  // reaches that field iff 0 <= 2*p_dst+p+4 + k - s <= p_dst-1, i.e. iff
  // k <= s - MIN_PRE_SHIFT - 1; the lower limit is 0 for EVERY value the 6-bit
  // addend_shamt_z can take (s <= 2*p_dst+p+4, so nothing is ever shifted
  // clean out of the parent's 87-bit vector).  So the sticky is a plain
  // thermometer-masked OR of addend_min, exact over the whole input space and
  // free of any shifter.
  // ------------------------------------------------------------------------
  localparam int unsigned MIN_PRE_SHIFT  = DST_PRECISION_BITS + PRECISION_BITS + 4;
  localparam int unsigned MIN_FULL_SHIFT = 2*DST_PRECISION_BITS + PRECISION_BITS + 4;

  logic [DST_SHIFT_AMOUNT_WIDTH-1:0] min_drop_cnt;   // # of addend_min LSBs made sticky
  logic [DST_SHIFT_AMOUNT_WIDTH-1:0] min_lost_cnt;   // # shifted clean out of the vector
  logic [DST_SHIFT_AMOUNT_WIDTH-1:0] min_shamt_sel;  // the one shifter's amount
  logic     [DST_PRECISION_BITS-1:0] min_prefix;     // min_prefix[j] = |addend_min[j:0]
  logic  [DST_SHIFT_AMOUNT_WIDTH-1:0] sbz_idx;

  assign min_drop_cnt = (addend_shamt_z > DST_SHIFT_AMOUNT_WIDTH'(MIN_PRE_SHIFT))
                          ? (addend_shamt_z - DST_SHIFT_AMOUNT_WIDTH'(MIN_PRE_SHIFT))
                          : '0;
  // Bits that leave the parent's pre-shifted vector entirely instead of
  // reaching its sticky field.  At this build DST_SHIFT_AMOUNT_WIDTH is exactly
  // wide enough for MIN_FULL_SHIFT, so this is a constant 0 and the mask below
  // folds to all-ones; at a wider parameterisation it is what keeps the sticky
  // bit-exact.
  assign min_lost_cnt = (addend_shamt_z > DST_SHIFT_AMOUNT_WIDTH'(MIN_FULL_SHIFT))
                          ? (addend_shamt_z - DST_SHIFT_AMOUNT_WIDTH'(MIN_FULL_SHIFT))
                          : '0;

  // Same identity for the minimum addend: min_drop_mask[k] = (min_drop_cnt > k)
  // <=> k <= min_drop_cnt - 1, a low window, so the masked OR is one entry of a
  // prefix-OR of addend_min.
  always_comb begin : gen_min_prefix
    min_prefix[0] = addend_min[0];
    for (int unsigned j = 1; j < DST_PRECISION_BITS; j++)
      min_prefix[j] = min_prefix[j-1] | addend_min[j];
  end
  // min_drop_cnt = max(0, s_z - MIN_PRE_SHIFT) and sbz_idx = clamp(mdc-1, 0,
  // p_dst-1) compose into ONE clamp of s_z - MIN_PRE_SHIFT - 1, so the
  // subtract-then-clamp-then-subtract chain collapses to a single subtract and
  // a single two-sided clamp.  min_drop_cnt itself stays -- it is the guard.
  assign sbz_idx = (addend_shamt_z <= DST_SHIFT_AMOUNT_WIDTH'(MIN_PRE_SHIFT + 1))
                     ? '0
                 : (addend_shamt_z >= DST_SHIFT_AMOUNT_WIDTH'(MIN_PRE_SHIFT + DST_PRECISION_BITS))
                     ? DST_SHIFT_AMOUNT_WIDTH'(DST_PRECISION_BITS - 1)
                     : (addend_shamt_z - DST_SHIFT_AMOUNT_WIDTH'(MIN_PRE_SHIFT + 1));

  assign sticky_before_add_z = (addend_shamt_z > DST_SHIFT_AMOUNT_WIDTH'(MIN_PRE_SHIFT))
                               && min_prefix[sbz_idx];

  // bypass_w, formed here instead of in three_way_add2: every term is local.
  assign bypass_w = sum_exact_zero && sticky_before_add_z;   // UNOBSERVABLE zero guard

  // ------------------------------------------------------------------------
  // THE BYPASS DISTANCE IS ZERO WHENEVER THE BYPASS IS TAKEN.
  //
  // bypass_w = sum_exact_zero && sticky_before_add_z, and
  // sticky_before_add_z = |(addend_min & min_drop_mask), so
  //
  //     bypass_w  ==>  addend_min != 0.
  //
  // A product whose exponent took exponent_prep's zero branch has a zero
  // mantissa (info.is_zero forces is_normal = 0 and a zero mantissa field), so
  // a non-zero addend_min means the minimum addend is either a NON-ZERO
  // product or the accumulator -- and under ADDEND_EXP_POSITIVE both have
  // exponent >= 1.  Then
  //
  //     bypass_shamt_full = 1 - exponent_min <= 0   ==>   bypass_shamt = '0.
  //
  // The whole distance computation -- a DST_EXP_WIDTH+1-bit signed subtract and
  // its two clamps -- is dead, and the 6-bit 2:1 amount mux in front of the
  // minimum addend's barrel shifter degenerates into DST_SHIFT_AMOUNT_WIDTH AND
  // gates.  This argument is independent of op_i: under VSUM every addend
  // exponent is >= 1 outright.
  // ------------------------------------------------------------------------
  // ------------------------------------------------------------------------
  // ... AND THAT PUTS bypass_w BEHIND THE SHIFTER INSTEAD OF IN FRONT OF IT.
  //
  // With bypass_shamt = 0 the amount is `addend_shamt_z & {~bypass_w}', so
  //
  //     (addend_min << MIN_PRE_SHIFT) >> (addend_shamt_z & {~bypass_w})
  //   = bypass_w ? (addend_min << MIN_PRE_SHIFT)
  //              : (addend_min << MIN_PRE_SHIFT) >> addend_shamt_z
  //
  // -- the SAME value, with the choice made AFTER the barrel shifter rather
  // than before it.  bypass_w is the deepest signal in this stage: it is
  // sum_exact_zero (the intermediate addend's own barrel shifter, then a
  // p_dst-bit equality against addend_max) ANDed with the minimum addend's
  // sticky.  In the parent every one of the shifter's DST_SHIFT_AMOUNT_WIDTH
  // stages sits BEHIND that, so the whole 64-position shift starts only once
  // the other shifter has finished and been compared -- which is why the
  // retimer has to push the last shift stages ACROSS the mid bank and into the
  // frame adder's stage, where they land on the critical path measured at the
  // bar (mid_pipe_add_min_after_shift -> 3 shifter gates -> the 65-bit adder ->
  // sum_carry_z).
  //
  // Moved to the output, bypass_w costs ONE gate level on a bus whose low
  // MIN_PRE_SHIFT bits are a constant 0 in the bypass arm (so they are AND
  // gates, not muxes) and whose top p_dst bits select between addend_min and
  // the shifted value.  The shifter itself now starts as soon as
  // addend_shamt_z is ready, i.e. from the exponent arithmetic alone.
  //
  // Pure restructuring: no value changes, at either elaboration.
  // ------------------------------------------------------------------------
  logic [2*DST_PRECISION_BITS+PRECISION_BITS+3:0] addend_min_pre;   // wiring only
  logic [2*DST_PRECISION_BITS+PRECISION_BITS+3:0] addend_min_sh;

  if (ADDEND_EXP_POSITIVE) begin : gen_bypass_shamt_zero
    assign bypass_shamt_full = '0;   // unused
    assign bypass_shamt      = '0;   // unused
    assign min_shamt_sel     = addend_shamt_z;   // bypass_w moved past the shifter
  end else begin : gen_bypass_shamt_generic
    assign bypass_shamt_full =
        signed'(1) - signed'({exponent_min_q[DST_EXP_WIDTH-1], exponent_min_q});

    always_comb begin : bypass_shift_amount
      if (bypass_shamt_full <= signed'(0))
        bypass_shamt = '0;
      else if (bypass_shamt_full >= signed'(2*DST_PRECISION_BITS + PRECISION_BITS + 4))
        bypass_shamt = unsigned'(2*DST_PRECISION_BITS + PRECISION_BITS + 4);
      else
        bypass_shamt = bypass_shamt_full[DST_SHIFT_AMOUNT_WIDTH-1:0];
    end

    assign min_shamt_sel = (bypass_w) ? bypass_shamt : addend_shamt_z;
  end

  // Shift the minimum addend -- ONCE.
  // BEFORE THE SHIFT:
  // | addend_min | 000.....000 |
  //  <- p_dst  -> <- p_dst+4 ->
  // AFTER THE SHIFT:
  // | 000............000 | addend_min | 000.....................0GR |
  //  <- min_shamt_sel  -> <- p_dst  -> <- p_dst+4-min_shamt_sel  ->
  assign addend_min_pre         = addend_min << MIN_PRE_SHIFT;
  assign addend_min_sh          = addend_min_pre >> min_shamt_sel;
  assign addend_min_after_shift = (ADDEND_EXP_POSITIVE && bypass_w) ? addend_min_pre
                                                                   : addend_min_sh;

  // The zero-result sign is not observable at the wrapper's output: see the
  // argument at rounding_assembly's pre_round_sign.  The port stays (the
  // hierarchy is the measurement frame) and is driven constant, which retires
  // the two 24-bit magnitude comparators, the 10-bit exponent comparator and
  // the four-way priority chain that used to build it -- together with
  // exponent_int_q / info_int_is_zero_q / info_max_is_zero_q /
  // addend_int_sign_q / addend_max_sign_q, whose only consumer this was.
  logic final_sign_zero;
  assign final_sign_zero = 1'b0;

  assign sticky_before_add_o      = sticky_before_add;
  assign sum_o                    = sum;
  assign sum_carry_o              = sum_carry;
  assign final_sign_o             = final_sign;
  assign bypass_w_o               = bypass_w;
  assign exponent_w_o             = exponent_w;
  assign addend_min_after_shift_o = addend_min_after_shift;
  assign sticky_before_add_z_o    = sticky_before_add_z;
  assign final_sign_zero_o        = final_sign_zero;

endmodule

// ---------------------------------------------------------------------------
// Special case handling: NaN/inf detection and per-format special result,
// status and bypass selection.
// ---------------------------------------------------------------------------
module opope_sdotp_special_case #(
  parameter fpnew_pkg::fmt_logic_t SrcDotpFpFmtConfig = '1,
  parameter fpnew_pkg::fmt_logic_t DstDotpFpFmtConfig = '1,
  // Do not change
  localparam int unsigned DST_WIDTH = fpnew_pkg::max_fp_width(DstDotpFpFmtConfig),
  localparam int unsigned NUM_FORMATS = fpnew_pkg::NUM_FP_FORMATS,
  localparam fpnew_pkg::fp_encoding_t SUPER_FORMAT = fpnew_pkg::super_format(SrcDotpFpFmtConfig),
  localparam fpnew_pkg::fp_encoding_t SUPER_DST_FORMAT = fpnew_pkg::super_format(DstDotpFpFmtConfig),
  localparam int unsigned SUPER_EXP_BITS = SUPER_FORMAT.exp_bits,
  localparam int unsigned SUPER_MAN_BITS = SUPER_FORMAT.man_bits,
  localparam int unsigned SUPER_DST_EXP_BITS = SUPER_DST_FORMAT.exp_bits,
  localparam int unsigned SUPER_DST_MAN_BITS = (fpnew_pkg::maximum(SUPER_DST_FORMAT.man_bits, 2*SUPER_MAN_BITS + 1)
      + ((SrcDotpFpFmtConfig == 6'h02 && DstDotpFpFmtConfig == 6'h20)
          ? 2*SUPER_MAN_BITS : 0))
) (
  input  fpnew_pkg::fp_info_t                            info_a_i,
  input  fpnew_pkg::fp_info_t                            info_b_i,
  input  fpnew_pkg::fp_info_t                            info_c_i,
  input  fpnew_pkg::fp_info_t                            info_d_i,
  input  fpnew_pkg::fp_info_t                            info_e_i,
  input  logic                                           a_sign_i,
  input  logic                                           c_sign_i,
  // fp_src_t / fp_dst_t operands (sign, exponent, mantissa packed)
  input  logic [SUPER_EXP_BITS+SUPER_MAN_BITS:0]         operand_b_i,
  input  logic [SUPER_EXP_BITS+SUPER_MAN_BITS:0]         operand_d_i,
  input  logic [SUPER_DST_EXP_BITS+SUPER_DST_MAN_BITS:0] operand_e_i,
  input  logic                                           any_operand_inf_i,
  input  logic                                           any_operand_nan_i,
  input  logic                                           signalling_nan_i,
  input  logic [2:0]                                     effective_subtraction_i,
  input  fpnew_pkg::fp_format_e                          dst_fmt_i,
  output logic [DST_WIDTH-1:0]                           special_result_o,
  output fpnew_pkg::status_t                             special_status_o,
  output logic                                           result_is_special_o
);

  typedef struct packed {
    logic                      sign;
    logic [SUPER_EXP_BITS-1:0] exponent;
    logic [SUPER_MAN_BITS-1:0] mantissa;
  } fp_src_t;
  typedef struct packed {
    logic                          sign;
    logic [SUPER_DST_EXP_BITS-1:0] exponent;
    logic [SUPER_DST_MAN_BITS-1:0] mantissa;
  } fp_dst_t;

  fp_src_t             operand_b, operand_d;
  fp_dst_t             operand_e;
  fpnew_pkg::fp_info_t info_a, info_b, info_c, info_d, info_e;
  logic                a_sign, c_sign;
  logic                any_operand_inf;
  logic                any_operand_nan;
  logic                signalling_nan;
  logic [2:0]          effective_subtraction;
  fpnew_pkg::fp_format_e dst_fmt_q;

  assign operand_b             = operand_b_i;
  assign operand_d             = operand_d_i;
  assign operand_e             = operand_e_i;
  assign info_a                = info_a_i;
  assign info_b                = info_b_i;
  assign info_c                = info_c_i;
  assign info_d                = info_d_i;
  assign info_e                = info_e_i;
  assign a_sign                = a_sign_i;
  assign c_sign                = c_sign_i;
  assign any_operand_inf       = any_operand_inf_i;
  assign any_operand_nan       = any_operand_nan_i;
  assign signalling_nan        = signalling_nan_i;
  assign effective_subtraction = effective_subtraction_i;
  assign dst_fmt_q             = dst_fmt_i;

  // ----------------------
  // Special case handling
  // ----------------------
  logic [DST_WIDTH-1:0] special_result;
  fpnew_pkg::status_t   special_status;
  logic                 result_is_special;

  logic               [NUM_FORMATS-1:0][DST_WIDTH-1:0] fmt_special_result;
  fpnew_pkg::status_t [NUM_FORMATS-1:0]                fmt_special_status;
  logic               [NUM_FORMATS-1:0]                fmt_result_is_special;

  for (genvar fmt = 0; fmt < int'(NUM_FORMATS); fmt++) begin : gen_special_results
    // Set up some constants
    localparam int unsigned FP_WIDTH = fpnew_pkg::fp_width(fpnew_pkg::fp_format_e'(fmt));
    localparam int unsigned EXP_BITS = fpnew_pkg::exp_bits(fpnew_pkg::fp_format_e'(fmt));
    localparam int unsigned MAN_BITS = fpnew_pkg::man_bits(fpnew_pkg::fp_format_e'(fmt));

    localparam logic [EXP_BITS-1:0] QNAN_EXPONENT = '1;
    localparam logic [MAN_BITS-1:0] QNAN_MANTISSA = 2**(MAN_BITS-1);
    localparam logic [MAN_BITS-1:0] ZERO_MANTISSA = '0;

    if (DstDotpFpFmtConfig[fmt]) begin : active_format
      always_comb begin : special_cases
        logic [FP_WIDTH-1:0] special_res;

        // Default assignment
        special_res                = {1'b0, QNAN_EXPONENT, QNAN_MANTISSA}; // qNaN
        fmt_special_status[fmt]    = '0;
        fmt_result_is_special[fmt] = 1'b0;

        // Handle potentially mixed nan & infinity input => important for the case where infinity and
        // zero are multiplied and added to a qNaN.
        // RISC-V mandates raising the NV exception in these cases:
        // (inf * 0) + c or (0 * inf) + c INVALID, no matter c (even quiet NaNs)
        if (  ((info_a.is_inf && info_b.is_zero) || (info_a.is_zero && info_b.is_inf))
           || ((info_c.is_inf && info_d.is_zero) || (info_c.is_zero && info_d.is_inf)) ) begin
          fmt_result_is_special[fmt] = 1'b1; // bypass DOTP, output is the canonical qNaN
          fmt_special_status[fmt].NV = 1'b1; // invalid operation
        // NaN Inputs cause canonical quiet NaN at the output and maybe invalid OP
        end else if (any_operand_nan) begin
          fmt_result_is_special[fmt] = 1'b1;           // bypass DOTP, output is the canonical qNaN
          fmt_special_status[fmt].NV = signalling_nan; // raise the invalid operation flag if signalling
        // Special cases involving infinity
        end else if (any_operand_inf) begin
          fmt_result_is_special[fmt] = 1'b1; // bypass DOTP
          // Effective addition of opposite infinities (±inf - ±inf) is invalid!
          if ((info_a.is_inf || info_b.is_inf) && (info_c.is_inf || info_d.is_inf) && effective_subtraction[2]) begin
            fmt_special_status[fmt].NV = 1'b1; // invalid operation
          end else if (((info_a.is_inf || info_b.is_inf) && info_e.is_inf && effective_subtraction[0])
             || ((info_c.is_inf || info_d.is_inf) && info_e.is_inf && effective_subtraction[1])) begin
            fmt_special_status[fmt].NV = 1'b1; // invalid operation
          // Handle cases where output will be inf because of inf product input
          end else if (info_a.is_inf || info_b.is_inf) begin
            // Result is infinity with the sign of the first product
            special_res = {a_sign ^ operand_b.sign, QNAN_EXPONENT, ZERO_MANTISSA};
          // Handle cases where the second product is inf
          end else if (info_c.is_inf || info_d.is_inf) begin
            // Result is infinity with sign of the second product
            special_res    = {c_sign ^ operand_d.sign, QNAN_EXPONENT, ZERO_MANTISSA};
          end else if (info_e.is_inf) begin
            // Result is infinity with sign of the accumulator
            special_res    = {operand_e.sign, QNAN_EXPONENT, ZERO_MANTISSA};
          end
        end
        // Initialize special result with ones (NaN-box)
        fmt_special_result[fmt]               = '1;
        fmt_special_result[fmt][FP_WIDTH-1:0] = special_res;
      end
    end else begin : inactive_format
      assign fmt_special_result[fmt] = '{default: fpnew_pkg::DONT_CARE};
      assign fmt_special_status[fmt] = '0;
      assign fmt_result_is_special[fmt] = 1'b0;
    end
  end

  // Detect special case from source format
  assign result_is_special = fmt_result_is_special[dst_fmt_q];
  // Signalling input NaNs raise invalid flag, otherwise no flags set
  assign special_status = fmt_special_status[dst_fmt_q];
  // Assemble result according to destination format
  assign special_result = fmt_special_result[dst_fmt_q];

  assign special_result_o    = special_result;
  assign special_status_o    = special_status;
  assign result_is_special_o = result_is_special;

endmodule

// ---------------------------------------------------------------------------
// Initial exponent data path: product/addend exponent computation, three-way
// exponent compare/sort, tentative sign/exponent and first alignment shift
// amount.
// ---------------------------------------------------------------------------
module opope_sdotp_exponent_prep #(
  parameter fpnew_pkg::fmt_logic_t SrcDotpFpFmtConfig = '1,
  parameter fpnew_pkg::fmt_logic_t DstDotpFpFmtConfig = '1,
  // Do not change
  localparam fpnew_pkg::fp_encoding_t SUPER_FORMAT = fpnew_pkg::super_format(SrcDotpFpFmtConfig),
  localparam fpnew_pkg::fp_encoding_t SUPER_DST_FORMAT = fpnew_pkg::super_format(DstDotpFpFmtConfig),
  localparam int unsigned SUPER_EXP_BITS = SUPER_FORMAT.exp_bits,
  localparam int unsigned SUPER_MAN_BITS = SUPER_FORMAT.man_bits,
  localparam int unsigned SUPER_DST_EXP_BITS = SUPER_DST_FORMAT.exp_bits,
  localparam int unsigned SUPER_DST_MAN_BITS = (fpnew_pkg::maximum(SUPER_DST_FORMAT.man_bits, 2*SUPER_MAN_BITS + 1)
      + ((SrcDotpFpFmtConfig == 6'h02 && DstDotpFpFmtConfig == 6'h20)
          ? 2*SUPER_MAN_BITS : 0)),
  // Precision bits 'p' include the implicit bit
  localparam int unsigned PRECISION_BITS = SUPER_MAN_BITS + 1,
  localparam int unsigned DST_PRECISION_BITS = SUPER_DST_MAN_BITS + 1,
  // The leading-zero counter operates on LZC_SUM_WIDTH bits
  localparam int unsigned LZC_SUM_WIDTH  = 2*DST_PRECISION_BITS + PRECISION_BITS + 5,
  localparam int unsigned LZC_RESULT_WIDTH = $clog2(LZC_SUM_WIDTH),
  localparam int unsigned EXP_WIDTH = unsigned'(fpnew_pkg::maximum(SUPER_EXP_BITS + 2, LZC_RESULT_WIDTH)),
  localparam int unsigned DST_EXP_WIDTH = unsigned'(fpnew_pkg::maximum(SUPER_DST_EXP_BITS + 2, LZC_RESULT_WIDTH)),
  // Shift amount width: maximum internal mantissa size is 2*DST_PRECISION_BITS+3 bits
  localparam int unsigned SHIFT_AMOUNT_WIDTH = $clog2(2*DST_PRECISION_BITS+PRECISION_BITS+4)
) (
  // fp_src_t / fp_dst_t operands (sign, exponent, mantissa packed)
  input  logic [SUPER_EXP_BITS+SUPER_MAN_BITS:0]         operand_a_i,
  input  logic [SUPER_EXP_BITS+SUPER_MAN_BITS:0]         operand_b_i,
  input  logic [SUPER_EXP_BITS+SUPER_MAN_BITS:0]         operand_c_i,
  input  logic [SUPER_EXP_BITS+SUPER_MAN_BITS:0]         operand_d_i,
  input  logic [SUPER_DST_EXP_BITS+SUPER_DST_MAN_BITS:0] operand_e_i,
  input  logic [SUPER_DST_EXP_BITS+SUPER_DST_MAN_BITS:0] operand_a_vsum_i,
  input  logic [SUPER_DST_EXP_BITS+SUPER_DST_MAN_BITS:0] operand_c_vsum_i,
  input  fpnew_pkg::fp_info_t                            info_a_i,
  input  fpnew_pkg::fp_info_t                            info_b_i,
  input  fpnew_pkg::fp_info_t                            info_c_i,
  input  fpnew_pkg::fp_info_t                            info_d_i,
  input  fpnew_pkg::fp_info_t                            info_e_i,
  input  logic                                           a_sign_i,
  input  logic                                           c_sign_i,
  input  fpnew_pkg::operation_e                          op_i,
  input  fpnew_pkg::fp_format_e                          src_fmt_i,
  input  fpnew_pkg::fp_format_e                          dst_fmt_i,
  input  logic [2:0]                                     effective_subtraction_i,
  output logic [2:0]                                     exponent_cmp_o,
  output logic                                           tentative_sign_o,
  output logic                                           effective_subtraction_first_o,
  output logic                                           info_min_is_zero_o,
  output logic                                           info_int_is_zero_o,
  output logic                                           info_max_is_zero_o,
  output logic                                           addend_min_sign_o,
  output logic                                           addend_int_sign_o,
  output logic                                           addend_max_sign_o,
  output logic signed [DST_EXP_WIDTH-1:0]                tentative_exponent_o,
  output logic signed [DST_EXP_WIDTH-1:0]                exponent_int_o,
  output logic signed [DST_EXP_WIDTH-1:0]                exponent_min_o,
  output logic [SHIFT_AMOUNT_WIDTH-1:0]                  addend_shamt_o
);

  typedef struct packed {
    logic                      sign;
    logic [SUPER_EXP_BITS-1:0] exponent;
    logic [SUPER_MAN_BITS-1:0] mantissa;
  } fp_src_t;
  typedef struct packed {
    logic                          sign;
    logic [SUPER_DST_EXP_BITS-1:0] exponent;
    logic [SUPER_DST_MAN_BITS-1:0] mantissa;
  } fp_dst_t;

  fp_src_t             operand_a, operand_b, operand_c, operand_d;
  fp_dst_t             operand_e;
  fp_dst_t             operand_a_vsum, operand_c_vsum;
  fpnew_pkg::fp_info_t info_a, info_b, info_c, info_d, info_e;
  logic                a_sign, c_sign;
  logic [2:0]          effective_subtraction;
  fpnew_pkg::fp_format_e src_fmt_q, dst_fmt_q;

  assign operand_a             = operand_a_i;
  assign operand_b             = operand_b_i;
  assign operand_c             = operand_c_i;
  assign operand_d             = operand_d_i;
  assign operand_e             = operand_e_i;
  assign operand_a_vsum        = operand_a_vsum_i;
  assign operand_c_vsum        = operand_c_vsum_i;
  assign info_a                = info_a_i;
  assign info_b                = info_b_i;
  assign info_c                = info_c_i;
  assign info_d                = info_d_i;
  assign info_e                = info_e_i;
  assign a_sign                = a_sign_i;
  assign c_sign                = c_sign_i;
  assign effective_subtraction = effective_subtraction_i;
  assign src_fmt_q             = src_fmt_i;
  assign dst_fmt_q             = dst_fmt_i;

  // ---------------------------
  // Initial exponent data path
  // ---------------------------
  // fpnew_pkg::bias(fmt) is 2**(exp_bits(fmt)-1)-1 on a RUNTIME format, so DC
  // cannot fold it: it builds a variable power-of-two shifter (DW_leftsh with a
  // 32-bit shift amount) plus a decrement, and it does so ONCE PER TEXTUAL USE.
  // The two product exponents below use it six times, which is six shifters in
  // what is already the longest stage of the design.  The value only depends on
  // the format, so compute each of the two biases once.
  logic [31:0] bias_src_v, bias_dst_v;
  assign bias_src_v = fpnew_pkg::bias(src_fmt_q);
  assign bias_dst_v = fpnew_pkg::bias(dst_fmt_q);

  logic signed [EXP_WIDTH-1:0]     exponent_a, exponent_b, exponent_c, exponent_d;
  logic signed [DST_EXP_WIDTH-1:0] exponent_e;
  logic signed [DST_EXP_WIDTH-1:0] exponent_a_vsum, exponent_c_vsum;
  logic signed [DST_EXP_WIDTH-1:0] exponent_addend_x, exponent_addend_y, exponent_addend_z;
  logic signed [DST_EXP_WIDTH-1:0] exponent_product_x, exponent_product_y, exponent_difference;
  logic signed [DST_EXP_WIDTH-1:0] exponent_max, exponent_int, exponent_min;
  logic signed [DST_EXP_WIDTH-1:0] tentative_exponent;
  logic [2:0]                      exponent_cmp;
  logic                            effective_subtraction_first;
  logic                            info_min_is_zero;
  logic                            info_int_is_zero;
  logic                            info_max_is_zero;
  logic                            addend_min_sign;
  logic                            addend_int_sign;
  logic                            addend_max_sign;
  logic                            tentative_sign;

  // Zero-extend exponents into signed container - implicit width extension
  assign exponent_a = signed'({1'b0, operand_a.exponent});
  assign exponent_a_vsum = signed'({1'b0, operand_a_vsum.exponent});
  assign exponent_b = signed'({1'b0, operand_b.exponent});
  assign exponent_c = signed'({1'b0, operand_c.exponent});
  assign exponent_c_vsum = signed'({1'b0, operand_c_vsum.exponent});
  assign exponent_d = signed'({1'b0, operand_d.exponent});
  assign exponent_e = signed'({1'b0, operand_e.exponent});

  // Calculate internal exponents from encoded values. Real exponents are (ex = Ex - bias + 1 - nx)
  // with Ex the encoded exponent and nx the implicit bit. Internal exponents stay biased.
  // Biased product exponent is the sum of encoded exponents minus the bias.
  assign exponent_product_y = (info_c.is_zero || info_d.is_zero)
                              ? 2 - signed'(bias_dst_v) // in case the product is zero, set minimum exp.
                              : signed'(exponent_c + info_c.is_subnormal
                                        + exponent_d + info_d.is_subnormal
                                        - 2*signed'(bias_src_v)  // rebias for dst fmt
                                        + signed'(bias_dst_v) + 1); // adding +1 to keep into account following shifts
  assign exponent_product_x = (info_a.is_zero || info_b.is_zero)
                              ? 2 - signed'(bias_dst_v) // in case the product is zero, set minimum exp.
                              : signed'(exponent_a + info_a.is_subnormal
                                        + exponent_b + info_b.is_subnormal
                                        - 2*signed'(bias_src_v)  // rebias for dst fmt
                                        + signed'(bias_dst_v) + 1); // adding +1 to keep into account following shift
  assign exponent_addend_y = (op_i == fpnew_pkg::VSUM)
                             ? signed'(exponent_c_vsum + $signed({1'b0, ~info_c.is_normal}))
                             : exponent_product_y;
  assign exponent_addend_x = (op_i == fpnew_pkg::VSUM)
                             ? signed'(exponent_a_vsum + $signed({1'b0, ~info_a.is_normal}))
                             : exponent_product_x;
  assign exponent_addend_z = signed'(exponent_e + $signed({1'b0, ~info_e.is_normal})); // 0 as subnorm

  // Find maximum, intermediate and minimum exponents
  assign exponent_cmp[2] = (exponent_addend_x >= exponent_addend_y) ? 1'b1 : 1'b0;
  assign exponent_cmp[1] = (exponent_addend_x >= exponent_addend_z) ? 1'b1 : 1'b0;
  assign exponent_cmp[0] = (exponent_addend_y >= exponent_addend_z) ? 1'b1 : 1'b0;

  // The three-term addition is performed in two steps with only a final normalization and round step
  // To prevent precision loss, first the two largest addends are summed, then the minimum addend is
  // added to the result of the first addition.

  // Find maximum, intermediate and minimum exponent
  always_comb begin : compare_exponents
    case (exponent_cmp)
      // (x < y), (x < z), (y < z)
      3'b000  : begin
        {exponent_max, exponent_int, exponent_min} = {exponent_addend_z, exponent_addend_y, exponent_addend_x};
        tentative_sign   = operand_e.sign; // The tentative sign of the DOTP shall be the sign of the maximum addend
        effective_subtraction_first = effective_subtraction[1];
        info_min_is_zero = info_a.is_zero || info_b.is_zero;
        info_int_is_zero = info_c.is_zero || info_d.is_zero;
        info_max_is_zero = info_e.is_zero;
        addend_min_sign  = a_sign ^ operand_b.sign;
        addend_int_sign  = c_sign ^ operand_d.sign;
        addend_max_sign  = operand_e.sign;
      end
      // // (x < y), (x < z), (y >= z) --> y >= z > x
      3'b001  : begin
        {exponent_max, exponent_int, exponent_min} = {exponent_addend_y, exponent_addend_z, exponent_addend_x};
        tentative_sign   = (c_sign ^ operand_d.sign);
        effective_subtraction_first = effective_subtraction[1];
        info_min_is_zero = info_a.is_zero || info_b.is_zero;
        info_int_is_zero = info_e.is_zero;
        info_max_is_zero = info_c.is_zero || info_d.is_zero;
        addend_min_sign  = a_sign ^ operand_b.sign;
        addend_int_sign  = operand_e.sign;
        addend_max_sign  = c_sign ^ operand_d.sign;
      end
      // // (x < y), (x >= z), (y < z)
      // 3'b010  : IMPOSSIBLE
      // (x < y), (x >= z), (y >= z)
      3'b011  : begin
        {exponent_max, exponent_int, exponent_min} = {exponent_addend_y, exponent_addend_x, exponent_addend_z};
        tentative_sign   =  (c_sign ^ operand_d.sign);
        effective_subtraction_first = effective_subtraction[2];
        info_min_is_zero = info_e.is_zero;
        info_int_is_zero = info_a.is_zero || info_b.is_zero;
        info_max_is_zero = info_c.is_zero || info_d.is_zero;
        addend_min_sign  = operand_e.sign;
        addend_int_sign  = a_sign ^ operand_b.sign;
        addend_max_sign  = c_sign ^ operand_d.sign;
      end
      // (x >= y), (x < z), (y < z)
      3'b100  : begin
        {exponent_max, exponent_int, exponent_min} = {exponent_addend_z, exponent_addend_x, exponent_addend_y};
        tentative_sign   = operand_e.sign;
        effective_subtraction_first = effective_subtraction[0];
        info_min_is_zero = info_c.is_zero || info_d.is_zero;
        info_int_is_zero = info_a.is_zero || info_b.is_zero;
        info_max_is_zero = info_e.is_zero;
        addend_min_sign  = c_sign ^ operand_d.sign;
        addend_int_sign  = a_sign ^ operand_b.sign;
        addend_max_sign  = operand_e.sign;
      end
      // // (x >= y), (x < z), (y >= z)
      // 3'b101  : IMPOSSIBLE
      3'b110  : begin
        {exponent_max, exponent_int, exponent_min} = {exponent_addend_x, exponent_addend_z, exponent_addend_y};
        tentative_sign   = (a_sign ^ operand_b.sign);
        effective_subtraction_first = effective_subtraction[0];
        info_min_is_zero = info_c.is_zero || info_d.is_zero;
        info_int_is_zero = info_e.is_zero;
        info_max_is_zero = info_a.is_zero || info_b.is_zero;
        addend_min_sign  = c_sign ^ operand_d.sign;
        addend_int_sign  = operand_e.sign;
        addend_max_sign  = a_sign ^ operand_b.sign;
      end
      // (x >= y), (x >= z), (y >= z)
      3'b111  : begin
        {exponent_max, exponent_int, exponent_min} = {exponent_addend_x, exponent_addend_y, exponent_addend_z};
        tentative_sign   = (a_sign ^ operand_b.sign);
        effective_subtraction_first = effective_subtraction[2];
        info_min_is_zero = info_e.is_zero;
        info_int_is_zero = info_c.is_zero || info_d.is_zero;
        info_max_is_zero = info_a.is_zero || info_b.is_zero;
        addend_min_sign  = operand_e.sign;
        addend_int_sign  = c_sign ^ operand_d.sign;
        addend_max_sign  = a_sign ^ operand_b.sign;
      end
      default : begin   // transitivity-impossible codes: don't-care
        {exponent_max, exponent_int, exponent_min} = {(3*DST_EXP_WIDTH){1'bx}};
        tentative_sign   = (a_sign ^ operand_b.sign);
        effective_subtraction_first = effective_subtraction[2];
        info_min_is_zero = info_e.is_zero;
        info_int_is_zero = info_c.is_zero || info_d.is_zero;
        info_max_is_zero = info_a.is_zero || info_b.is_zero;
        addend_min_sign  = operand_e.sign;
        addend_int_sign  = c_sign ^ operand_d.sign;
        addend_max_sign  = a_sign ^ operand_b.sign;
      end
    endcase
  end

  // Exponent difference is the maximum addend exponent minus the intermediate addend exponent,
  // where the addends are selected among the two products and the accumulator.
  // In the case of non-expanding VSUM, the two products are replaced by the larger inputs (the
  // multipliers are by-passed
  assign exponent_difference = exponent_max - exponent_int;
  // The tentative exponent will be the maximum exponent
  assign tentative_exponent = exponent_max;

  // Shift amount for product_y based on exponents (unsigned as only right shifts)
  logic [SHIFT_AMOUNT_WIDTH-1:0] addend_shamt;
  // ------------------------------------------------------------------------
  // THE FIRST SHIFT AMOUNT SATURATES BY OR-ING, NOT BY COMPARING.
  //
  // exponent_difference = exponent_max - exponent_int, and the sort above puts
  // the LARGER of the two exponents in exponent_max on every reachable
  // exponent_cmp code, so the difference is NON-NEGATIVE (the parent already
  // relies on this: its `unsigned'(signed'(.))' cast would truncate a negative
  // value into garbage).
  //
  // first_shift_add reads addend_shamt in exactly three places and every one of
  // them is CONSTANT for all s >= 2*p_dst+3, i.e. the parent's clamp limit:
  //
  //   addend_int_after_shift = (addend_int << (2*p_dst+3-p_dst)) >> s
  //        addend_int's top bit sits at 2*p_dst+2, so the vector is all-zero
  //        for every s >= 2*p_dst+3;
  //   int_drop_mask[k] = (s >= k + 2*p_dst+4-p_dst),  k < p_dst
  //        all-ones already at s = 2*p_dst+3, since k+p_dst+4 <= 2*p_dst+3
  //        for k <= p_dst-1 (p_dst >= 1);
  //   int_low_mask[k]  = (s >  k),  k < p_dst
  //        all-ones already at s = 2*p_dst+3 >= p_dst.
  //
  // So SATURATING AT 2**SHIFT_AMOUNT_WIDTH-1 instead of at 2*p_dst+3 feeds the
  // consumers exactly the same three values -- and saturation at an all-ones
  // limit is an OR with the replicated `any high bit set' flag, in place of the
  // DST_EXP_WIDTH-bit signed compare against a constant and the
  // SHIFT_AMOUNT_WIDTH-bit 2:1 mux the parent built here.  (This is the same
  // rewrite `shz' already applies to the SECOND shift amount in
  // first_shift_add; nobody had applied it to the first.)
  //
  // The guard is a pure WIDTH fact about this parameterisation -- it does not
  // use the format-range argument at all -- so a parameterisation whose shift
  // amount is too narrow to reach the clamp keeps the parent's comparator.
  // At the measured FP16 -> FP32 build SHIFT_AMOUNT_WIDTH = 6 and
  // 2*p_dst+3 = 51 <= 63, so the OR form is taken.
  // ------------------------------------------------------------------------
  if (((2**SHIFT_AMOUNT_WIDTH) - 1) >= (2*DST_PRECISION_BITS + 3))
  begin : gen_shamt_or
    assign addend_shamt =
        exponent_difference[SHIFT_AMOUNT_WIDTH-1:0]
        | {SHIFT_AMOUNT_WIDTH{|exponent_difference[DST_EXP_WIDTH-1:SHIFT_AMOUNT_WIDTH]}};
  end else begin : gen_shamt_cmp
    always_comb begin : addend_shift_amount
      // The maximum addend and the intermediate addends have mutual bits to add
      if (exponent_difference <= signed'(2*DST_PRECISION_BITS + 3)) begin
        addend_shamt = unsigned'(signed'(exponent_difference));
      // The intermediate addend is only in the sticky bits
      end else begin
        addend_shamt = 2*DST_PRECISION_BITS + 3;
      end
    end
  end

  assign exponent_cmp_o                = exponent_cmp;
  assign tentative_sign_o              = tentative_sign;
  assign effective_subtraction_first_o = effective_subtraction_first;
  assign info_min_is_zero_o            = info_min_is_zero;
  // ------------------------------------------------------------------------
  // FIVE OUTPUTS ARE DEAD ONCE THE ZERO-RESULT SIGN PATH GOES (see
  // rounding_assembly's pre_round_sign).  Each had exactly ONE consumer in the
  // whole design -- first_shift_add's final_sign_zero, itself now constant --
  // so driving them constant here retires their 6-way selects in this module
  // AND their im_late/mid/mo_early pipeline registers in the top, instead of
  // leaving that to cross-boundary unused-port removal.
  //   exponent_int stays LIVE internally: exponent_difference needs it.
  // ------------------------------------------------------------------------
  assign info_int_is_zero_o            = 1'b0;
  assign info_max_is_zero_o            = 1'b0;
  assign addend_min_sign_o             = addend_min_sign;
  assign addend_int_sign_o             = 1'b0;
  assign addend_max_sign_o             = 1'b0;
  assign tentative_exponent_o          = tentative_exponent;
  assign exponent_int_o                = '0;
  assign exponent_min_o                = exponent_min;
  assign addend_shamt_o                = addend_shamt;

endmodule

// ---------------------------------------------------------------------------
// Input processing: per-format unpacking and classification of the source,
// VSUM and destination operands, operation-dependent operand adjustment
// (op_select), and input classification reductions.
// ---------------------------------------------------------------------------
module opope_sdotp_input_prep #(
  parameter fpnew_pkg::fmt_logic_t SrcDotpFpFmtConfig = '1,
  parameter fpnew_pkg::fmt_logic_t DstDotpFpFmtConfig = '1,
  // Do not change
  localparam int unsigned SRC_WIDTH = fpnew_pkg::max_fp_width(SrcDotpFpFmtConfig),
  localparam int unsigned DST_WIDTH = fpnew_pkg::max_fp_width(DstDotpFpFmtConfig),
  localparam int unsigned NUM_FORMATS = fpnew_pkg::NUM_FP_FORMATS,
  localparam fpnew_pkg::fp_encoding_t SUPER_FORMAT = fpnew_pkg::super_format(SrcDotpFpFmtConfig),
  localparam fpnew_pkg::fp_encoding_t SUPER_DST_FORMAT = fpnew_pkg::super_format(DstDotpFpFmtConfig),
  localparam int unsigned SUPER_EXP_BITS = SUPER_FORMAT.exp_bits,
  localparam int unsigned SUPER_MAN_BITS = SUPER_FORMAT.man_bits,
  localparam int unsigned SUPER_DST_EXP_BITS = SUPER_DST_FORMAT.exp_bits,
  localparam int unsigned SUPER_DST_MAN_BITS = (fpnew_pkg::maximum(SUPER_DST_FORMAT.man_bits, 2*SUPER_MAN_BITS + 1)
      + ((SrcDotpFpFmtConfig == 6'h02 && DstDotpFpFmtConfig == 6'h20)
          ? 2*SUPER_MAN_BITS : 0))
) (
  input  logic [DST_WIDTH-1:0]                            operand_a_q_i,
  input  logic [SRC_WIDTH-1:0]                            operand_b_q_i,
  input  logic [DST_WIDTH-1:0]                            operand_c_q_i,
  input  logic [SRC_WIDTH-1:0]                            operand_d_q_i,
  input  logic [DST_WIDTH-1:0]                            dst_operands_q_i,
  input  logic [NUM_FORMATS-1:0][4:0]                     is_boxed_i,
  input  fpnew_pkg::fp_format_e                           src_fmt_i,
  input  fpnew_pkg::fp_format_e                           dst_fmt_i,
  input  fpnew_pkg::operation_e                           op_i,
  input  logic                                            op_mod_i,
  // fp_src_t / fp_dst_t operands (sign, exponent, mantissa packed)
  output logic [SUPER_EXP_BITS+SUPER_MAN_BITS:0]          operand_a_o,
  output logic [SUPER_EXP_BITS+SUPER_MAN_BITS:0]          operand_b_o,
  output logic [SUPER_EXP_BITS+SUPER_MAN_BITS:0]          operand_c_o,
  output logic [SUPER_EXP_BITS+SUPER_MAN_BITS:0]          operand_d_o,
  output logic [SUPER_DST_EXP_BITS+SUPER_DST_MAN_BITS:0]  operand_e_o,
  output logic [SUPER_DST_EXP_BITS+SUPER_DST_MAN_BITS:0]  operand_a_vsum_o,
  output logic [SUPER_DST_EXP_BITS+SUPER_DST_MAN_BITS:0]  operand_c_vsum_o,
  output fpnew_pkg::fp_info_t                             info_a_o,
  output fpnew_pkg::fp_info_t                             info_b_o,
  output fpnew_pkg::fp_info_t                             info_c_o,
  output fpnew_pkg::fp_info_t                             info_d_o,
  output fpnew_pkg::fp_info_t                             info_e_o,
  output logic                                            a_sign_o,
  output logic                                            c_sign_o,
  output logic                                            any_operand_inf_o,
  output logic                                            any_operand_nan_o,
  output logic                                            signalling_nan_o,
  output logic [2:0]                                      effective_subtraction_o
);

  typedef struct packed {
    logic                      sign;
    logic [SUPER_EXP_BITS-1:0] exponent;
    logic [SUPER_MAN_BITS-1:0] mantissa;
  } fp_src_t;
  typedef struct packed {
    logic                          sign;
    logic [SUPER_DST_EXP_BITS-1:0] exponent;
    logic [SUPER_DST_MAN_BITS-1:0] mantissa;
  } fp_dst_t;

  logic [DST_WIDTH-1:0]  operand_a_q;
  logic [SRC_WIDTH-1:0]  operand_b_q;
  logic [DST_WIDTH-1:0]  operand_c_q;
  logic [SRC_WIDTH-1:0]  operand_d_q;
  logic [DST_WIDTH-1:0]  dst_operands_q;
  fpnew_pkg::fp_format_e src_fmt_q;
  fpnew_pkg::fp_format_e dst_fmt_q;

  assign operand_a_q    = operand_a_q_i;
  assign operand_b_q    = operand_b_q_i;
  assign operand_c_q    = operand_c_q_i;
  assign operand_d_q    = operand_d_q_i;
  assign dst_operands_q = dst_operands_q_i;
  assign src_fmt_q      = src_fmt_i;
  assign dst_fmt_q      = dst_fmt_i;

  logic [3:0][SRC_WIDTH-1:0] operands_post_inp_pipe;
  // vivado fix: loop is here to make it work on vivado
  for (genvar i = 0; i < SRC_WIDTH; i++) begin : gen_op_assign
    assign operands_post_inp_pipe[3][i] = operand_d_q[i];
    assign operands_post_inp_pipe[2][i] = operand_c_q[i];
    assign operands_post_inp_pipe[1][i] = operand_b_q[i];
    assign operands_post_inp_pipe[0][i] = operand_a_q[i];
  end

  // -----------------
  // Input processing
  // -----------------

  // -----------------
  // Source operands
  // -----------------
  logic        [NUM_FORMATS-1:0][3:0]                     fmt_sign;
  logic signed [NUM_FORMATS-1:0][3:0][SUPER_EXP_BITS-1:0] fmt_exponent;
  logic        [NUM_FORMATS-1:0][3:0][SUPER_MAN_BITS-1:0] fmt_mantissa;

  fpnew_pkg::fp_info_t [NUM_FORMATS-1:0][4:0] info_q;
  fpnew_pkg::fp_info_t [NUM_FORMATS-1:0][1:0] info_vsum_q;

  // FP Input initialization (Src)
  for (genvar fmt = 0; fmt < int'(NUM_FORMATS); fmt++) begin : fmt_src_init_inputs
    // Set up some constants
    localparam int unsigned FP_WIDTH = fpnew_pkg::fp_width(fpnew_pkg::fp_format_e'(fmt));
    localparam int unsigned EXP_BITS = fpnew_pkg::exp_bits(fpnew_pkg::fp_format_e'(fmt));
    localparam int unsigned MAN_BITS = fpnew_pkg::man_bits(fpnew_pkg::fp_format_e'(fmt));

    if (SrcDotpFpFmtConfig[fmt]) begin : active_src_format
      logic [3:0][FP_WIDTH-1:0] trimmed_ops;

      // Classify input
      fpnew_classifier #(
        .FpFormat    ( fpnew_pkg::fp_format_e'(fmt) ),
        .NumOperands ( 4                            )
      ) i_fpnew_classifier (
        .operands_i  ( trimmed_ops                                 ),
        .is_boxed_i  ( is_boxed_i[fmt][3:0]                        ),
        .info_o      ( info_q[fmt][3:0]                            )
      );
      for (genvar op = 0; op < 4; op++) begin : gen_operands
        assign trimmed_ops[op]       = operands_post_inp_pipe[op][FP_WIDTH-1:0];
        assign fmt_sign[fmt][op]     = operands_post_inp_pipe[op][FP_WIDTH-1];
        assign fmt_exponent[fmt][op] = signed'({1'b0, operands_post_inp_pipe[op][MAN_BITS+:EXP_BITS]});
        assign fmt_mantissa[fmt][op] = {info_q[fmt][op].is_normal, operands_post_inp_pipe[op][MAN_BITS-1:0]} <<
                                       (SUPER_MAN_BITS - MAN_BITS); // move to left of mantissa
      end
    end else begin : inactive_src_format
      assign info_q[fmt][3:0]  = '{default: fpnew_pkg::DONT_CARE}; // format disabled
      assign fmt_sign[fmt]     = fpnew_pkg::DONT_CARE;             // format disabled
      assign fmt_exponent[fmt] = '{default: fpnew_pkg::DONT_CARE}; // format disabled
      assign fmt_mantissa[fmt] = '{default: fpnew_pkg::DONT_CARE}; // format disabled
    end
  end

  // ----------------------------
  // Non-expanding VSUM operands
  // ----------------------------
  logic        [NUM_FORMATS-1:0][1:0]                         fmt_vsum_sign;
  logic signed [NUM_FORMATS-1:0][1:0][SUPER_DST_EXP_BITS-1:0] fmt_vsum_exponent;
  logic        [NUM_FORMATS-1:0][1:0][SUPER_DST_MAN_BITS-1:0] fmt_vsum_mantissa;

  // FP Input initialization (Src)
  for (genvar fmt = 0; fmt < int'(NUM_FORMATS); fmt++) begin : fmt_vsum_init_inputs
    // Set up some constants
    localparam int unsigned FP_WIDTH = fpnew_pkg::fp_width(fpnew_pkg::fp_format_e'(fmt));
    localparam int unsigned EXP_BITS = fpnew_pkg::exp_bits(fpnew_pkg::fp_format_e'(fmt));
    localparam int unsigned MAN_BITS = fpnew_pkg::man_bits(fpnew_pkg::fp_format_e'(fmt));

    if (DstDotpFpFmtConfig[fmt]) begin : active_vsum_format
      logic [1:0][FP_WIDTH-1:0] trimmed_vsum_ops;
      logic [1:0]               vsum_ops_is_boxed;

      assign vsum_ops_is_boxed = {is_boxed_i[fmt][2],
                                  is_boxed_i[fmt][0]};

      // Classify input
      fpnew_classifier #(
        .FpFormat    ( fpnew_pkg::fp_format_e'(fmt) ),
        .NumOperands ( 2                            )
      ) i_fpnew_classifier (
        .operands_i  ( trimmed_vsum_ops  ),
        .is_boxed_i  ( vsum_ops_is_boxed ),
        .info_o      ( info_vsum_q[fmt]  )
      );
      assign trimmed_vsum_ops          = {operand_c_q[FP_WIDTH-1:0], operand_a_q[FP_WIDTH-1:0]};
      assign fmt_vsum_sign[fmt]        = {operand_c_q[FP_WIDTH-1], operand_a_q[FP_WIDTH-1]};
      assign fmt_vsum_exponent[fmt][1] = signed'({1'b0, operand_c_q[MAN_BITS+:EXP_BITS]});
      assign fmt_vsum_exponent[fmt][0] = signed'({1'b0, operand_a_q[MAN_BITS+:EXP_BITS]});
      assign fmt_vsum_mantissa[fmt][1] = {info_vsum_q[fmt][1].is_normal, operand_c_q[MAN_BITS-1:0]}
                                         << (SUPER_DST_MAN_BITS - MAN_BITS);
      assign fmt_vsum_mantissa[fmt][0] = {info_vsum_q[fmt][0].is_normal, operand_a_q[MAN_BITS-1:0]}
                                         << (SUPER_DST_MAN_BITS - MAN_BITS);
    end else begin : inactive_dst_format
      assign info_vsum_q[fmt]       = '{default: fpnew_pkg::DONT_CARE}; // format disabled
      assign fmt_vsum_sign[fmt]     = fpnew_pkg::DONT_CARE;             // format disabled
      assign fmt_vsum_exponent[fmt] = '{default: fpnew_pkg::DONT_CARE}; // format disabled
      assign fmt_vsum_mantissa[fmt] = '{default: fpnew_pkg::DONT_CARE}; // format disabled
    end
  end

  // -------------------
  // Destination operand
  // -------------------
  logic        [NUM_FORMATS-1:0]                         fmt_dst_sign;
  logic signed [NUM_FORMATS-1:0][SUPER_DST_EXP_BITS-1:0] fmt_dst_exponent;
  logic        [NUM_FORMATS-1:0][SUPER_DST_MAN_BITS-1:0] fmt_dst_mantissa;

  // FP Input initialization (Src)
  for (genvar fmt = 0; fmt < int'(NUM_FORMATS); fmt++) begin : fmt_dst_init_inputs
    // Set up some constants
    localparam int unsigned FP_WIDTH = fpnew_pkg::fp_width(fpnew_pkg::fp_format_e'(fmt));
    localparam int unsigned EXP_BITS = fpnew_pkg::exp_bits(fpnew_pkg::fp_format_e'(fmt));
    localparam int unsigned MAN_BITS = fpnew_pkg::man_bits(fpnew_pkg::fp_format_e'(fmt));

    if (DstDotpFpFmtConfig[fmt]) begin : active_dst_format
      logic [FP_WIDTH-1:0] trimmed_dst_ops;

      // Classify input
      fpnew_classifier #(
        .FpFormat    ( fpnew_pkg::fp_format_e'(fmt) ),
        .NumOperands ( 1                            )
      ) i_fpnew_classifier (
        .operands_i ( trimmed_dst_ops                           ),
        .is_boxed_i ( is_boxed_i[fmt][4]                        ),
        .info_o     ( info_q[fmt][4]                            )
      );
      assign trimmed_dst_ops       = dst_operands_q[FP_WIDTH-1:0];
      assign fmt_dst_sign[fmt]     = dst_operands_q[FP_WIDTH-1];
      assign fmt_dst_exponent[fmt] = signed'({1'b0, dst_operands_q[MAN_BITS+:EXP_BITS]});
      assign fmt_dst_mantissa[fmt] = {info_q[fmt][4].is_normal, dst_operands_q[MAN_BITS-1:0]}
                                      << (SUPER_DST_MAN_BITS - MAN_BITS); // move to left of mantissa
    end else begin : inactive_dst_format
      assign info_q[fmt][4]        = '{default: fpnew_pkg::DONT_CARE}; // format disabled
      assign fmt_dst_sign[fmt]     = fpnew_pkg::DONT_CARE;             // format disabled
      assign fmt_dst_exponent[fmt] = '{default: fpnew_pkg::DONT_CARE}; // format disabled
      assign fmt_dst_mantissa[fmt] = '{default: fpnew_pkg::DONT_CARE}; // format disabled
    end
  end

  // -------------------------------------------
  // Operation selection and operand adjustment
  // -------------------------------------------
  fp_src_t             operand_a, operand_b, operand_c, operand_d;
  fp_dst_t             operand_e;
  fp_dst_t             operand_a_vsum, operand_c_vsum;
  fpnew_pkg::fp_info_t info_a, info_b, info_c, info_d, info_e;
  logic                a_sign, c_sign;

  // | \c op_q  | \c op_mod_q | Operation Adjustment
  // |:--------:|:-----------:|---------------------
  // | SDOTP    | \c 0        | SDOTP:  none
  // | SDOTP    | \c 1        | SDOTPN: Invert the sign of the first and second products (accumulator - dotp)
  // | EXVSUM   | \c 0        | EXVSUM: none
  // | EXVSUM   | \c 1        | EXVSUM: Invert the sign of the first and second addends
  // | VSUM     | \c 0        | VSUM:   none
  // | VSUM     | \c 1        | VSUM:   Invert the sign of the first and second addends
  // | *others* | \c -        | *invalid*
  // \note \c op_mod_q always inverts the sign of the addend.
  always_comb begin : op_select
    // Default assignments - packing-order-agnostic
    operand_a = {fmt_sign[src_fmt_q][0], fmt_exponent[src_fmt_q][0], fmt_mantissa[src_fmt_q][0]};
    operand_b = {fmt_sign[src_fmt_q][1], fmt_exponent[src_fmt_q][1], fmt_mantissa[src_fmt_q][1]};
    operand_c = {fmt_sign[src_fmt_q][2], fmt_exponent[src_fmt_q][2], fmt_mantissa[src_fmt_q][2]};
    operand_d = {fmt_sign[src_fmt_q][3], fmt_exponent[src_fmt_q][3], fmt_mantissa[src_fmt_q][3]};
    operand_e = {fmt_dst_sign[dst_fmt_q], fmt_dst_exponent[dst_fmt_q], fmt_dst_mantissa[dst_fmt_q]};
    operand_a_vsum = {fmt_vsum_sign[src_fmt_q][0], fmt_vsum_exponent[src_fmt_q][0], fmt_vsum_mantissa[src_fmt_q][0]};
    operand_c_vsum = {fmt_vsum_sign[src_fmt_q][1], fmt_vsum_exponent[src_fmt_q][1], fmt_vsum_mantissa[src_fmt_q][1]};
    info_a    = info_q[src_fmt_q][0];
    info_b    = info_q[src_fmt_q][1];
    info_c    = info_q[src_fmt_q][2];
    info_d    = info_q[src_fmt_q][3];
    info_e    = info_q[dst_fmt_q][4];

    // op_mod_q inverts sign of operand A and C, thus inverting the sign of the dot product
    operand_a.sign = operand_a.sign ^ op_mod_i;
    operand_c.sign = operand_c.sign ^ op_mod_i;
    a_sign    = operand_a.sign;
    c_sign    = operand_c.sign;
    // op_mod_q inverts sign of operand A and C, thus inverting the sign of the vsum
    operand_a_vsum.sign = operand_a_vsum.sign ^ op_mod_i;
    operand_c_vsum.sign = operand_c_vsum.sign ^ op_mod_i;

    unique case (op_i)
      fpnew_pkg::SDOTP:  ; // do nothing
      fpnew_pkg::VSUM: begin // Set multiplicands coming from rs1 to +1
        operand_b = '{sign: 1'b0, exponent: fpnew_pkg::bias(src_fmt_q), mantissa: '0};
        operand_d = '{sign: 1'b0, exponent: fpnew_pkg::bias(src_fmt_q), mantissa: '0};
        info_b    = '{is_normal: 1'b1, is_boxed: 1'b1, default: 1'b0}; //normal, boxed value.
        info_d    = '{is_normal: 1'b1, is_boxed: 1'b1, default: 1'b0}; //normal, boxed value.
        info_a    = info_vsum_q[dst_fmt_q][0];
        info_c    = info_vsum_q[dst_fmt_q][1];
        a_sign    = operand_a_vsum.sign;
        c_sign    = operand_c_vsum.sign;
      end
      fpnew_pkg::EXVSUM: begin // Set multiplicands coming from rs1 to +1
        operand_b = '{sign: 1'b0, exponent: fpnew_pkg::bias(src_fmt_q), mantissa: '0};
        operand_d = '{sign: 1'b0, exponent: fpnew_pkg::bias(src_fmt_q), mantissa: '0};
        info_b    = '{is_normal: 1'b1, is_boxed: 1'b1, default: 1'b0}; //normal, boxed value.
        info_d    = '{is_normal: 1'b1, is_boxed: 1'b1, default: 1'b0}; //normal, boxed value.
      end
      default: begin // propagate don't cares
        operand_a  = '{default: fpnew_pkg::DONT_CARE};
        operand_b  = '{default: fpnew_pkg::DONT_CARE};
        operand_c  = '{default: fpnew_pkg::DONT_CARE};
        info_a     = '{default: fpnew_pkg::DONT_CARE};
        info_b     = '{default: fpnew_pkg::DONT_CARE};
        info_c     = '{default: fpnew_pkg::DONT_CARE};
      end
    endcase
  end

  // ---------------------
  // Input classification
  // ---------------------
  logic       any_operand_inf;
  logic       any_operand_nan;
  logic       signalling_nan;
  logic [2:0] effective_subtraction;

  // Reduction for special case handling
  assign any_operand_inf = (| {info_a.is_inf, info_b.is_inf, info_c.is_inf, info_d.is_inf, info_e.is_inf});
  assign any_operand_nan = (| {info_a.is_nan, info_b.is_nan, info_c.is_nan, info_d.is_nan, info_e.is_nan});
  assign signalling_nan  = (| {info_a.is_signalling, info_b.is_signalling, info_c.is_signalling,
                               info_d.is_signalling, info_e.is_signalling});
  // Effective subtractions in the three-term addition
  assign effective_subtraction[0] = (a_sign ^ operand_b.sign) ^ operand_e.sign;
  assign effective_subtraction[1] = (c_sign ^ operand_d.sign) ^ operand_e.sign;
  assign effective_subtraction[2] = (a_sign ^ operand_b.sign) ^ (c_sign ^ operand_d.sign);

  assign operand_a_o             = operand_a;
  assign operand_b_o             = operand_b;
  assign operand_c_o             = operand_c;
  assign operand_d_o             = operand_d;
  assign operand_e_o             = operand_e;
  assign operand_a_vsum_o        = operand_a_vsum;
  assign operand_c_vsum_o        = operand_c_vsum;
  assign info_a_o                = info_a;
  assign info_b_o                = info_b;
  assign info_c_o                = info_c;
  assign info_d_o                = info_d;
  assign info_e_o                = info_e;
  assign a_sign_o                = a_sign;
  assign c_sign_o                = c_sign;
  assign any_operand_inf_o       = any_operand_inf;
  assign any_operand_nan_o       = any_operand_nan;
  assign signalling_nan_o        = signalling_nan;
  assign effective_subtraction_o = effective_subtraction;

endmodule
