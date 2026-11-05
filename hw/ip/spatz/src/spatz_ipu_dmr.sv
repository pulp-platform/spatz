// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Maoyuan Cai (maoyuan.cai@unibo.it)

module spatz_ipu_dmr import spatz_pkg::*; import rvv_pkg::vew_e; #(
    parameter type tag_t    = logic
  ) (
    input  logic   clk_i,
    input  logic   rst_ni,
    // Operation Signals
    input  op_e    operation_i,
    input  logic   operation_valid_i,
    output logic   operation_ready_o,
    input  elen_t  op_s1_i,
    input  elen_t  op_s2_i,
    input  elen_t  op_d_i,
    input  tag_t   tag_i,
    input  elenb_t carry_i,
    input  vew_e   sew_i,
    // Result Output
    output elenb_t be_o,
    output elen_t  result_o,
    output elenb_t result_valid_o,
    input  logic   result_ready_i,
    output tag_t   tag_o,
    output logic   busy_o,
    // redundancy fault output
    output logic   dup_fault_o
  );

// Include FF
`include "common_cells/registers.svh"


  // ------------------------------------------------------------------
  // The two IPU copies -- lockstep by construction (identical config,
  // identical inputs, identical accept timing), so any difference in
  // their outputs can only be explained by a fault.
  // ------------------------------------------------------------------
  elen_t              a_result,  b_result;
  tag_t               a_tag,     b_tag;
  logic               a_operation_ready, b_operation_ready;
  logic               a_busy,      b_busy;
  elenb_t             a_out_valid, b_out_valid;

  spatz_ipu #(
      .tag_t(tag_t)
    ) i_ipu_a (
      .clk_i            (clk_i                                                                                           ),
      .rst_ni           (rst_ni                                                                                          ),
      .operation_i      (operation_i                                                                                    ),
      // Only the IPU0 executes scalar instructions
      .operation_valid_i(operation_valid_i),
      .operation_ready_o(a_operation_ready                                                                                       ),
      .op_s1_i          (op_s1_i                                                              ),
      .op_s2_i          (op_s2_i                                                              ),
      .op_d_i           (op_d_i                                                              ),
      .tag_i            (tag_i                                                                                       ),
      .carry_i          (carry_i                                                                                              ),
      .sew_i            (sew_i                                                                                             ),
      .be_o             (/* Unused */                                                                                    ),
      .result_o         (a_result                                                                ),
      .result_valid_o   (a_out_valid                                                        ),
      .result_ready_i   (result_ready_i                                                                            ),
      .tag_o            (a_tag                                                                         ),
      .busy_o           (a_busy                                                                               )
    );

  spatz_ipu #(
      .tag_t(tag_t)
    ) i_ipu_b (
      .clk_i            (clk_i                                                                                           ),
      .rst_ni           (rst_ni                                                                                          ),
      .operation_i      (operation_i                                                                                    ),
      // Only the IPU0 executes scalar instructions
      .operation_valid_i(operation_valid_i),
      .operation_ready_o(b_operation_ready                                                                                       ),
      .op_s1_i          (op_s1_i                                                              ),
      .op_s2_i          (op_s2_i                                                              ),
      .op_d_i           (op_d_i                                                              ),
      .tag_i            (tag_i                                                                                       ),
      .carry_i          (carry_i                                                                                              ),
      .sew_i            (sew_i                                                                                             ),
      .be_o             (/* Unused */                                                                                    ),
      .result_o         (b_result                                                           ),
      .result_valid_o   (b_out_valid                                                        ),
      .result_ready_i   (result_ready_i                                                                            ),
      .tag_o            (b_tag                                                                         ),
      .busy_o           (b_busy                                                                               )
    );

  assign operation_ready_o    = a_operation_ready && b_operation_ready;

  // ------------------------------------------------------------------
  // Forward (functional path). No retry: best-effort forward of
  // Primary's (i_ipu_a's) result on a mismatch, same-cycle, straight off
  // the live a_result/b_result -- this MUST stay same-cycle.
  // ------------------------------------------------------------------
  assign result_valid_o    = a_out_valid;
  assign result_o          = a_result;
  assign tag_o             = a_tag;

  assign busy_o = a_busy | b_busy;

  // ------------------------------------------------------------------
  // Compare (diagnostic path, deliberately pipelined -- see the header
  // comment's "Timing note").
  // ------------------------------------------------------------------
  // typedef struct packed {
  //   elenb_t           a_out_valid;
  //   elenb_t           b_out_valid;
  //   elen_t            a_result;
  //   elen_t            b_result;
  //   tag_t             a_tag;
  //   tag_t             b_tag;
  //   logic             a_operation_ready;
  //   logic             b_operation_ready;
  // } dmr_cmp_entry_t;

  // dmr_cmp_entry_t cmp_d, cmp_q;

  // assign cmp_d = dmr_cmp_entry_t'{
  //   a_out_valid:  a_out_valid,
  //   b_out_valid:  b_out_valid,
  //   a_operation_ready:  a_operation_ready,
  //   b_operation_ready:  b_operation_ready,
  //   a_result: a_result,
  //   b_result: b_result,
  //   a_tag:    a_tag,
  //   b_tag:    b_tag
  // };

  // `FF(cmp_q, cmp_d, '0)

  // Stage 2: independent sub-compares, registered in parallel -- see the
  // comment above for why result_match gets a cycle to itself.
  logic any_valid_d,    any_valid_q;
  logic both_valid_d,   both_valid_q;
  logic result_match_d, result_match_q;
  logic tag_match_d,    tag_match_q;
  logic ready_match_d,  ready_match_q;
  logic busy_match_d,   busy_match_q;

  assign any_valid_d    = &a_out_valid || &b_out_valid;
  assign both_valid_d   = &a_out_valid && &b_out_valid;
  assign result_match_d = (a_result == b_result);
  assign tag_match_d    = (a_tag    == b_tag);
  assign ready_match_d  = (a_operation_ready == b_operation_ready);
  assign busy_match_d   = (a_busy == b_busy);

  `FF(any_valid_q,    any_valid_d,    1'b0)
  `FF(both_valid_q,   both_valid_d,   1'b0)
  `FF(result_match_q, result_match_d, 1'b0)
  `FF(tag_match_q,    tag_match_d,    1'b0)
  `FF(ready_match_q,  ready_match_d,  1'b0)
  `FF(busy_match_q,   busy_match_d,  1'b0)
  
  // Stage 3: trivial reduction of already-registered 1-bit flags,
  // edge-detected so one fault yields one pulse even across VFU stalls.
  logic mismatch_comb, mismatch_q, dup_fault_q;

  assign mismatch_comb = any_valid_q
                         && !(both_valid_q && result_match_q && tag_match_q && ready_match_q)
                         || !busy_match_q;

  `FF(mismatch_q,  mismatch_comb,                1'b0)
  `FF(dup_fault_q, mismatch_comb && !mismatch_q, 1'b0)

  assign dup_fault_o = dup_fault_q;

endmodule
