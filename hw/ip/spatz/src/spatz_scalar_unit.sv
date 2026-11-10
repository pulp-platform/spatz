// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich
//
// The controller contains all of the CSR registers, the instruction decoder,
// the operation issuer, the register scoreboard, and the result write back to
// the main core.

module spatz_scalar_unit
  import spatz_pkg::*;
  import rvv_pkg::*;
  import cf_math_pkg::idx_width;
  import fpnew_pkg::*;
  #(
    parameter bit           RegisterRsp       = 0,
    parameter type          spatz_issue_req_t = logic,
    parameter type          spatz_issue_rsp_t = logic,
    parameter type          spatz_rsp_t       = logic,

    parameter int unsigned N_IPU = 1,
    parameter int unsigned N_FPU = 1,
    parameter int unsigned ELEN  = 32,
    parameter fpu_implementation_t FPUImplementation = fpu_implementation_t'(0),

    /// Do not change
    localparam int unsigned N_FU  = N_IPU > N_FPU ? N_IPU : N_FPU,
    localparam int unsigned ELENB = ELEN / 8
  ) (
    input  logic                                   clk_i,
    input  logic                                   rst_ni,
    input  logic [31:0]                            hart_id_i,
    // Snitch Issue
    input  logic                                   issue_valid_i,
    output logic                                   scalar_correctable_fault_o,
    output logic                                   scalar_uncorrectable_fault_o,
    output logic                                   issue_ready_o,
    input  spatz_issue_req_t                       issue_req_i,
    output spatz_issue_rsp_t                       issue_rsp_o,
    output logic                                   rsp_valid_o,
    input  logic                                   rsp_ready_i,
    output spatz_rsp_t                             rsp_o,
    // FPU untimed sidechannel
    input  roundmode_e                             fpu_rnd_mode_i,
    input  fmt_mode_t                              fpu_fmt_mode_i,
    output status_t                                fpu_status_o
  );

  // TMR fault flags of the triplicated FFs
  logic [9-1:0] ff_tmr_err_0;
  logic [12*(N_FPU)-1:0] ff_tmr_err_1;
  logic ff_tmr_fault_any;
  assign ff_tmr_fault_any = (|ff_tmr_err_0) | (|ff_tmr_err_1);

// Include FF
`include "common_cells/registers.svh"

  /////////////
  // Signals //
  /////////////

  // Spatz request
  spatz_req_t vfu_req;
  logic       vfu_req_valid;
  logic       vfu_req_illegal;
  logic       vfu_req_ready;
  logic       vfu_rsp_valid;
  // logic       vfu_rsp_ready;
  vfu_rsp_t   spatz_rsp;
  vfu_rsp_t vfu_rsp;
  logic     vfu_rsp_ready;
  logic       rsp_valid_d;
  logic       rsp_ready_d;
  spatz_rsp_t rsp_d;
  //////////////
  // Decoding //
  //////////////

  // Decoder req
  decoder_req_t decoder_req;
  logic         decoder_req_valid;
  // Decoder rsp
  decoder_rsp_t decoder_rsp;
  logic         decoder_rsp_valid;

  spatz_decoder  #(
    .RVV(1'b0)
  ) i_decoder (
    .clk_i              (clk_i            ),
    .rst_ni             (rst_ni           ),
    // Request
    .decoder_req_i      (decoder_req      ),
    .decoder_req_valid_i(decoder_req_valid),
    // Response
    .decoder_rsp_o      (decoder_rsp      ),
    .decoder_rsp_valid_o(decoder_rsp_valid),
    // FPU untimed sidechannel
    .fpu_rnd_mode_i     (fpu_rnd_mode_i   ),
    .fpu_fmt_mode_i     (fpu_fmt_mode_i   )
  );

  // Decode new instruction if new request arrives
  always_comb begin : proc_decode
    decoder_req       = '0;
    decoder_req_valid = 1'b0;

    // Decode new instruction if one is received and spatz is ready
    if (issue_valid_i && issue_ready_o) begin
      decoder_req.instr = issue_req_i.data_op;
      decoder_req.rs1   = issue_req_i.data_arga;
      decoder_req.rs2   = issue_req_i.data_argb;
      decoder_req.rsd   = issue_req_i.data_argc;
      decoder_req.rd    = issue_req_i.id;
      decoder_req.vtype =   '0;
      decoder_req_valid = 1'b1;
    end
  end // proc_decode

  ////////////////////
  // Request Buffer //
  ////////////////////

  // Spatz request
  spatz_req_t buffer_spatz_req;
  // Buffer state signals
  logic       req_buffer_ready, req_buffer_valid, req_buffer_pop;

  // One element wide instruction buffer
  fall_through_register #(
    .T(spatz_req_t)
  ) i_req_buffer (
    .clk_i     (clk_i                ),
    .rst_ni    (rst_ni               ),
    .clr_i     (1'b0                 ),
    .testmode_i(1'b0                 ),
    .ready_o   (req_buffer_ready     ),
    .valid_o   (req_buffer_valid     ),
    .data_i    (decoder_rsp.spatz_req),
    .valid_i   (decoder_rsp_valid    ),
    .data_o    (buffer_spatz_req     ),
    .ready_i   (req_buffer_pop       )
  );

  /////////////
  // Issuing //
  /////////////

  // We stall issuing a new instruction if the corresponding execution unit is
  // not ready yet. Or we have a change in LMUL, for which we need to let all the
  // units finish first before scheduling a new operation (to avoid running into
  // issues with the socreboard).
  logic vfu_stall;
  assign vfu_stall   = ~vfu_req_ready & req_buffer_valid;

  // Running instructions
  logic      [NrParallelInstructions-1:0] running_insn_d, running_insn_q;
  spatz_id_t                              next_insn_id;
  logic                                   running_insn_full;
  // TMR: running_insn_q (voted value in running_insn_q, copies running_insn_q0..2)
  logic      [NrParallelInstructions-1:0] running_insn_q0, running_insn_q1, running_insn_q2;
  `FF(running_insn_q0, running_insn_d, '0)
  `FF(running_insn_q1, running_insn_d, '0)
  `FF(running_insn_q2, running_insn_d, '0)
  bitwise_TMR_voter_fail #(.DataWidth($bits(running_insn_q)), .VoterType(1)) i_running_insn_q_voter (
    .a_i(running_insn_q0), .b_i(running_insn_q1), .c_i(running_insn_q2), .majority_o(running_insn_q), .fault_detected_o(ff_tmr_err_0[0]));

  find_first_one #(
    .WIDTH(NrParallelInstructions)
  ) i_ffo_next_insn_id (
    .in_i       (~running_insn_q  ),
    .first_one_o(next_insn_id     ),
    .no_ones_o  (running_insn_full)
  );

  // Pop the buffer if we do not have a unit stall
  assign req_buffer_pop = ~vfu_stall & req_buffer_valid && !running_insn_full;

  // Issue new operation to execution units
  always_comb begin : ex_issue
    // Define new spatz request
    vfu_req         = buffer_spatz_req;
    vfu_req.id      = next_insn_id;
    vfu_req_illegal = decoder_rsp_valid ? decoder_rsp.instr_illegal : 1'b0;
    vfu_req_valid   = req_buffer_pop && !vfu_req_illegal && !running_insn_full;

    // We have a new instruction and there is no stall.
    if (vfu_req_valid) begin
      vfu_req.vtype  = buffer_spatz_req.vtype;
      vfu_req.vl     = 1;
      vfu_req.vstart = '0;
    end
  end // ex_issue

  always_comb begin: proc_next_insn_id
    // Maintain state
    running_insn_d = running_insn_q;

    // New instruction!
    if (vfu_req_valid && vfu_req.ex_unit != CON)
      running_insn_d[next_insn_id] = 1'b1;

    // Finished a instruction
    if (vfu_rsp_valid)
      running_insn_d[vfu_rsp.id] = 1'b0;
  end: proc_next_insn_id

  // Respond to core about the decoded instruction.
  always_comb begin : acc_issue_resp
    issue_rsp_o = '0;

    // Is there something running on Spatz? If so, prevent Snitch from reading the fcsr register
    issue_rsp_o.isfloat = |running_insn_q;

    // We have a new valid instruction
    issue_rsp_o.accept = decoder_rsp_valid && !decoder_rsp.instr_illegal;
  end // acc_issue_resp

  //////////////
  // Retiring //
  //////////////


  logic [1:0] vfu_scalar_response_err;
  rel_spill_register #(
    .TmrHandshake(1'b0),
    .T(vfu_rsp_t)
  ) i_vfu_scalar_response (
    .clk_i  (clk_i                          ),
    .rst_ni (rst_ni                         ),
    .data_i (vfu_rsp                        ),
    .valid_i(vfu_rsp_valid && vfu_rsp.wb    ),
    .ready_o(vfu_rsp_ready                  ),
    .data_o (spatz_rsp                      ),
    .valid_o(rsp_valid_d                    ),
    .ready_i(rsp_ready_d                    ),
    .err_o            (vfu_scalar_response_err),
    .data_corrector_o (),
    .data_corrected_i ('0)
  );

  logic [1:0] spatz_rsp_register_err;
  rel_spill_register #(
    .TmrHandshake(1'b0),
    .T     (spatz_rsp_t ),
    .Bypass(!RegisterRsp)
  ) i_spatz_rsp_register (
    .clk_i  (clk_i       ),
    .rst_ni (rst_ni      ),
    .data_i (rsp_d       ),
    .valid_i(rsp_valid_d ),
    .ready_o(rsp_ready_d ),
    .data_o (rsp_o       ),
    .valid_o(rsp_valid_o ),
    .ready_i(rsp_ready_i ),
    .err_o            (spatz_rsp_register_err),
    .data_corrector_o (),
    .data_corrected_i ('0)
  );

  // Retire an operation/instruction and write back result to core
  // if necessary.
  assign rsp_d.id      = spatz_rsp.rd;
  assign rsp_d.data    = spatz_rsp.result;

  // Signal to core that Spatz is ready for a new instruction
  assign issue_ready_o = req_buffer_ready;

  // Instruction tag (propagated together with the operands through the pipelines)
  typedef struct packed {
    spatz_id_t id;

    vew_e vsew;
    vlen_t vstart;

    // Encodes both the scalar RD and the VD address in the VRF
    vfu_rsp_addr_t vd_addr;
    logic wb;
    logic last;

    // Is this a narrowing instruction?
    logic narrowing;
    logic narrowing_upper;

    // Is this a reduction?
    logic reduction;
  } vfu_tag_t;

  ///////////////////////
  //  Operation queue  //
  ///////////////////////

  spatz_req_t spatz_req;
  logic       spatz_req_valid;
  logic       spatz_req_ready;

  logic [1:0] operation_queue_err;
  rel_spill_register #(
    .TmrHandshake(1'b0),
    .T(spatz_req_t)
  ) i_operation_queue (
    .clk_i  (clk_i          ),
    .rst_ni (rst_ni         ),
    .data_i (vfu_req        ),
    .valid_i(vfu_req_valid  ),
    .ready_o(vfu_req_ready  ),
    .data_o (spatz_req      ),
    .valid_o(spatz_req_valid),
    .ready_i(spatz_req_ready),
    .err_o            (operation_queue_err),
    .data_corrector_o (),
    .data_corrected_i ('0)
  );

  ///////////////
  //  Control  //
  ///////////////

  // Vector length counter
  vlen_t vl_q, vl_d;
  // TMR: vl_q (voted value in vl_q, copies vl_q0..2)
  vlen_t vl_q0, vl_q1, vl_q2;
  `FF(vl_q0, vl_d, '0)
  `FF(vl_q1, vl_d, '0)
  `FF(vl_q2, vl_d, '0)
  logic [$bits(vl_q)-1:0] vl_q_vote;
  bitwise_TMR_voter_fail #(.DataWidth($bits(vl_q)), .VoterType(1)) i_vl_q_voter (
    .a_i(vl_q0), .b_i(vl_q1), .c_i(vl_q2), .majority_o(vl_q_vote), .fault_detected_o(ff_tmr_err_0[1]));
  assign vl_q = vlen_t'(vl_q_vote);

  // Are we busy?
  logic busy_q, busy_d;
  // TMR: busy_q (voted value in busy_q, copies busy_q0..2)
  logic busy_q0, busy_q1, busy_q2;
  `FF(busy_q0, busy_d, 1'b0)
  `FF(busy_q1, busy_d, 1'b0)
  `FF(busy_q2, busy_d, 1'b0)
  bitwise_TMR_voter_fail #(.DataWidth($bits(busy_q)), .VoterType(1)) i_busy_q_voter (
    .a_i(busy_q0), .b_i(busy_q1), .c_i(busy_q2), .majority_o(busy_q), .fault_detected_o(ff_tmr_err_0[2]));

  // Number of elements in one VRF word
  logic [$clog2(N_FU*(ELEN/8)):0] nr_elem_word;
  assign nr_elem_word = (N_FU * (1 << (MAXEW - spatz_req.vtype.vsew))) >> spatz_req.op_arith.is_narrowing;

  // Are we running integer or floating-point instructions?
  typedef enum logic {
    VFU_RunningIPU, VFU_RunningFPU
   } state_t;
   state_t state_d, state_q;
  // TMR: state_q (voted value in state_q, copies state_q0..2)
  state_t state_q0, state_q1, state_q2;
  `FF(state_q0, state_d, VFU_RunningFPU)
  `FF(state_q1, state_d, VFU_RunningFPU)
  `FF(state_q2, state_d, VFU_RunningFPU)
  logic [$bits(state_q)-1:0] state_q_vote;
  bitwise_TMR_voter_fail #(.DataWidth($bits(state_q)), .VoterType(1)) i_state_q_voter (
    .a_i(state_q0), .b_i(state_q1), .c_i(state_q2), .majority_o(state_q_vote), .fault_detected_o(ff_tmr_err_0[3]));
  assign state_q = state_t'(state_q_vote);

  // Propagate the tags through the functional units
  vfu_tag_t ipu_result_tag, fpu_result_tag, result_tag;
  vfu_tag_t input_tag;

  assign result_tag = (state_q == VFU_RunningIPU ? ipu_result_tag : fpu_result_tag);

  // Number of words advanced by vstart
  vlen_t vstart;
  assign vstart = ((spatz_req.vstart / N_FU) >> (MAXEW - spatz_req.vtype.vsew)) << (MAXEW - spatz_req.vtype.vsew);

  // Should we stall?
  logic stall;

  // IPU results
  logic [N_FU*ELEN-1:0]  ipu_result;
  logic [N_FU*ELENB-1:0] ipu_result_valid;
  logic [N_FU*ELENB-1:0] ipu_in_ready;

  // FPU results
  logic [N_FU*ELEN-1:0]  fpu_result;
  logic [N_FU*ELENB-1:0] fpu_result_valid;
  logic [N_FU*ELENB-1:0] fpu_in_ready;

  // Operands and result signals
  logic [N_FU*ELEN-1:0]  operand1, operand2, operand3;
  logic [N_FU*ELENB-1:0] in_ready;

  // Are the VFU operands ready?
  logic operands_ready;
  assign operands_ready = spatz_req_valid && vfu_rsp_ready && !stall;

  // Valid operations
  logic [N_FU*ELENB-1:0] valid_operations;
  assign valid_operations = ELEN == 32 ? 4'hf : 8'hff;

  // Pending results
  logic [N_FU*ELENB-1:0] pending_results;
  assign pending_results = result_tag.wb ? (ELEN == 32 ? 4'hf : 8'hff) : '1;

  // Did we issue a microoperation?
  logic word_issued;
  assign word_issued = spatz_req_valid && &(in_ready | ~valid_operations) && operands_ready && !stall;

  // Currently running instructions
  logic [NrParallelInstructions-1:0] running_d, running_q;
  // TMR: running_q (voted value in running_q, copies running_q0..2)
  logic [NrParallelInstructions-1:0] running_q0, running_q1, running_q2;
  `FF(running_q0, running_d, '0)
  `FF(running_q1, running_d, '0)
  `FF(running_q2, running_d, '0)
  bitwise_TMR_voter_fail #(.DataWidth($bits(running_q)), .VoterType(1)) i_running_q_voter (
    .a_i(running_q0), .b_i(running_q1), .c_i(running_q2), .majority_o(running_q), .fault_detected_o(ff_tmr_err_0[4]));

  // Is this a FPU instruction
  logic is_fpu_insn;
  assign is_fpu_insn = FPU && spatz_req.op inside {[VFADD:VSDOTP]};

  // Is the FPU busy?
  logic is_fpu_busy;

  // Is the IPU busy?
  logic is_ipu_busy;

  // Scalar results (sent back to Snitch)
  elen_t scalar_result;

  // Is this the last request?
  logic last_request;

  // Are we producing the upper or lower part of the results of a narrowing instruction?
  logic narrowing_upper_d, narrowing_upper_q;
  // TMR: narrowing_upper_q (voted value in narrowing_upper_q, copies narrowing_upper_q0..2)
  logic narrowing_upper_q0, narrowing_upper_q1, narrowing_upper_q2;
  `FF(narrowing_upper_q0, narrowing_upper_d, 1'b0)
  `FF(narrowing_upper_q1, narrowing_upper_d, 1'b0)
  `FF(narrowing_upper_q2, narrowing_upper_d, 1'b0)
  bitwise_TMR_voter_fail #(.DataWidth($bits(narrowing_upper_q)), .VoterType(1)) i_narrowing_upper_q_voter (
    .a_i(narrowing_upper_q0), .b_i(narrowing_upper_q1), .c_i(narrowing_upper_q2), .majority_o(narrowing_upper_q), .fault_detected_o(ff_tmr_err_0[5]));

  // Are we reading the upper or lower part of the operands of a widening instruction?
  logic widening_upper_d, widening_upper_q;
  // TMR: widening_upper_q (voted value in widening_upper_q, copies widening_upper_q0..2)
  logic widening_upper_q0, widening_upper_q1, widening_upper_q2;
  `FF(widening_upper_q0, widening_upper_d, 1'b0)
  `FF(widening_upper_q1, widening_upper_d, 1'b0)
  `FF(widening_upper_q2, widening_upper_d, 1'b0)
  bitwise_TMR_voter_fail #(.DataWidth($bits(widening_upper_q)), .VoterType(1)) i_widening_upper_q_voter (
    .a_i(widening_upper_q0), .b_i(widening_upper_q1), .c_i(widening_upper_q2), .majority_o(widening_upper_q), .fault_detected_o(ff_tmr_err_0[6]));

  // Are any results valid?
  logic [N_FU*ELEN-1:0]  result;
  logic [N_FU*ELENB-1:0] result_valid;
  logic                  result_ready;
  assign result_ready = &(result_valid | ~pending_results) && ((result_tag.wb && vfu_rsp_ready));

  always_comb begin: control_proc
    // Maintain state
    vl_d              = vl_q;
    busy_d            = busy_q;
    running_d         = running_q;
    state_d           = state_q;
    narrowing_upper_d = narrowing_upper_q;
    widening_upper_d  = widening_upper_q;

    // We are not stalling
    stall = 1'b0;

    // This is not the last request
    last_request = 1'b0;

    // We are handling an instruction
    spatz_req_ready = 1'b0;

    // Do not ack anything
    vfu_rsp_valid = 1'b0;
    vfu_rsp       = '0;

    // Change number of remaining elements
    if (word_issued) begin
      vl_d              = vl_q + nr_elem_word;
      // Update narrowing information
      narrowing_upper_d = narrowing_upper_q ^ spatz_req.op_arith.is_narrowing;
      widening_upper_d  = widening_upper_q ^ (spatz_req.op_arith.widen_vs1 || spatz_req.op_arith.widen_vs2);
    end

    // Current state of the VFU
    if (spatz_req_valid)
      unique case (state_q)
        VFU_RunningIPU: begin
          // Only go to the FPU state once the IPUs are no longer busy
          if (is_fpu_insn) begin
            if (is_ipu_busy)
              stall = 1'b1;
            else begin
              state_d = VFU_RunningFPU;
              stall   = 1'b1;
            end
          end
        end
        VFU_RunningFPU: begin
          // Only go back to the IPU state once the FPUs are no longer busy
          if (!is_fpu_insn)
            if (is_fpu_busy)
              stall = 1'b1;
            else begin
              state_d = VFU_RunningIPU;
              stall   = 1'b1;
            end
        end
        default:;
      endcase

    // Finished the execution!
    if (spatz_req_valid && ((vl_d >= spatz_req.vl && !spatz_req.op_arith.is_reduction))) begin
      spatz_req_ready         = spatz_req_valid;
      busy_d                  = 1'b0;
      vl_d                    = '0;
      last_request            = 1'b1;
      running_d[spatz_req.id] = 1'b0;
      widening_upper_d        = 1'b0;
      narrowing_upper_d       = 1'b0;
    end
    // Do we have a new instruction?
    else if (spatz_req_valid && !running_d[spatz_req.id]) begin
      // Start at vstart
      vl_d                    = vstart;
      busy_d                  = 1'b1;
      running_d[spatz_req.id] = 1'b1;

      // Change number of remaining elements
      if (word_issued)
        vl_d = vl_q + nr_elem_word;
    end

    // An instruction finished execution
    if (result_tag.last && &(result_valid | ~pending_results)) begin
      vfu_rsp.id      = result_tag.id;
      vfu_rsp.rd      = result_tag.vd_addr[GPRWidth-1:0];
      vfu_rsp.wb      = result_tag.wb;
      vfu_rsp.result  = result_tag.wb ? scalar_result : '0;
      vfu_rsp_valid   = 1'b1;
    end
  end: control_proc

  //////////////
  // Operands //
  //////////////

  always_comb begin: operand_proc
    operand1 = {1*N_FU{spatz_req.rs1}};
    operand3 = {1*N_FU{spatz_req.rsd}};

    // Replicate scalar operands
    unique case (spatz_req.vtype.vsew)
      EW_8 : operand2   = MAXEW == EW_32 ? {4*N_FU{spatz_req.rs2[7:0]}}  : {8*N_FU{spatz_req.rs2[7:0]}};
      EW_16: operand2   = MAXEW == EW_32 ? {2*N_FU{spatz_req.rs2[15:0]}} : {4*N_FU{spatz_req.rs2[15:0]}};
      EW_32: operand2   = MAXEW == EW_32 ? {1*N_FU{spatz_req.rs2[31:0]}} : {2*N_FU{spatz_req.rs2[31:0]}};
      default: operand2 = {1*N_FU{spatz_req.rs2}};
    endcase
  end: operand_proc

  assign in_ready     = state_q == VFU_RunningIPU ? ipu_in_ready     : fpu_in_ready;
  assign result       = state_q == VFU_RunningIPU ? ipu_result       : fpu_result;
  assign result_valid = state_q == VFU_RunningIPU ? ipu_result_valid : fpu_result_valid;

  assign scalar_result = result[ELEN-1:0];

  assign input_tag = '{
      id             : spatz_req.id,
      vsew           : spatz_req.vtype.vsew,
      vstart         : spatz_req.vstart,
      vd_addr        : vfu_rsp_addr_t'(spatz_req.rd),
      wb             : 1'b1,
      last           : last_request,
      narrowing      : spatz_req.op_arith.is_narrowing,
      narrowing_upper: narrowing_upper_q,
      reduction      : 1'b0
    };

  //////////
  // IPUs //
  //////////

  logic     [N_IPU*ELENB-1:0] int_ipu_in_ready;
  logic     [N_IPU*ELEN-1:0]  int_ipu_operand1;
  logic     [N_IPU*ELEN-1:0]  int_ipu_operand2;
  logic     [N_IPU*ELEN-1:0]  int_ipu_operand3;
  logic     [N_IPU*ELEN-1:0]  int_ipu_result;
  vfu_tag_t [N_IPU-1:0]       int_ipu_result_tag;
  logic     [N_IPU*ELENB-1:0] int_ipu_result_valid;
  logic                       int_ipu_result_ready;
  logic     [N_IPU-1:0]       int_ipu_busy;

  assign is_ipu_busy = |int_ipu_busy;

  logic [N_FU*ELEN-1:0] ipu_wide_operand1, ipu_wide_operand2, ipu_wide_operand3;
  always_comb begin: gen_ipu_widening
    automatic logic [N_FU*ELEN/2-1:0] shift_operand1 = !widening_upper_q ? operand1[N_FU*ELEN/2-1:0] : operand1[N_FU*ELEN-1:N_FU*ELEN/2];
    automatic logic [N_FU*ELEN/2-1:0] shift_operand2 = !widening_upper_q ? operand2[N_FU*ELEN/2-1:0] : operand2[N_FU*ELEN-1:N_FU*ELEN/2];

    ipu_wide_operand1 = operand1;
    ipu_wide_operand2 = operand2;
    ipu_wide_operand3 = operand3;

    case (spatz_req.vtype.vsew)
      EW_32: begin
        for (int el = 0; el < N_FU; el++) begin
          if (spatz_req.op_arith.widen_vs1 && MAXEW == EW_64)
            ipu_wide_operand1[64*el +: 64] = spatz_req.op_arith.signed_vs1 ? {{32{shift_operand1[32*el+31]}}, shift_operand1[32*el +: 32]} : {32'b0, shift_operand1[32*el +: 32]};

          if (spatz_req.op_arith.widen_vs2 && MAXEW == EW_64)
            ipu_wide_operand2[64*el +: 64] = spatz_req.op_arith.signed_vs2 ? {{32{shift_operand2[32*el+31]}}, shift_operand2[32*el +: 32]} : {32'b0, shift_operand2[32*el +: 32]};
        end
      end
      EW_16: begin
        for (int el = 0; el < (MAXEW == EW_64 ? 2*N_FU : N_FU); el++) begin
          if (spatz_req.op_arith.widen_vs1)
            ipu_wide_operand1[32*el +: 32] = spatz_req.op_arith.signed_vs1 ? {{16{shift_operand1[16*el+15]}}, shift_operand1[16*el +: 16]} : {16'b0, shift_operand1[16*el +: 16]};

          if (spatz_req.op_arith.widen_vs2)
            ipu_wide_operand2[32*el +: 32] = spatz_req.op_arith.signed_vs2 ? {{16{shift_operand2[16*el+15]}}, shift_operand2[16*el +: 16]} : {16'b0, shift_operand2[16*el +: 16]};
        end
      end
      EW_8: begin
        for (int el = 0; el < (MAXEW == EW_64 ? 4*N_FU : 2*N_FU); el++) begin
          if (spatz_req.op_arith.widen_vs1)
            ipu_wide_operand1[16*el +: 16] = spatz_req.op_arith.signed_vs1 ? {{8{shift_operand1[8*el+7]}}, shift_operand1[8*el +: 8]} : {8'b0, shift_operand1[8*el +: 8]};

          if (spatz_req.op_arith.widen_vs2)
            ipu_wide_operand2[16*el +: 16] = spatz_req.op_arith.signed_vs2 ? {{8{shift_operand2[8*el+7]}}, shift_operand2[8*el +: 8]} : {8'b0, shift_operand2[8*el +: 8]};
        end
      end
      default:;
    endcase
  end: gen_ipu_widening

  assign ipu_in_ready         = int_ipu_in_ready;
  assign int_ipu_operand1     = ipu_wide_operand1;
  assign int_ipu_operand2     = ipu_wide_operand2;
  assign int_ipu_operand3     = ipu_wide_operand3;
  assign ipu_result           = int_ipu_result;
  assign ipu_result_valid     = int_ipu_result_valid;
  assign int_ipu_result_ready = result_ready;
  assign ipu_result_tag       = int_ipu_result_tag[0];

  for (genvar ipu = 0; unsigned'(ipu) < N_IPU; ipu++) begin : gen_ipus
    logic ipu_ready;
    assign int_ipu_in_ready[ipu*ELENB +: ELENB] = {ELENB{ipu_ready}};

    logic is_widening;
    assign is_widening = spatz_req.op_arith.widen_vs1 || spatz_req.op_arith.widen_vs2;

    vew_e sew;
    assign sew = vew_e'(int'(spatz_req.vtype.vsew) + is_widening);

    spatz_ipu #(
      .tag_t(vfu_tag_t)
    ) i_ipu (
      .clk_i            (clk_i                                                                                           ),
      .rst_ni           (rst_ni                                                                                          ),
      .operation_i      (spatz_req.op                                                                                    ),
      // Only the IPU0 executes scalar instructions
      .operation_valid_i(spatz_req_valid && operands_ready && (!spatz_req.op_arith.is_scalar || ipu == 0) && !is_fpu_insn),
      .operation_ready_o(ipu_ready                                                                                       ),
      .op_s1_i          (int_ipu_operand1[ipu*ELEN +: ELEN]                                                              ),
      .op_s2_i          (int_ipu_operand2[ipu*ELEN +: ELEN]                                                              ),
      .op_d_i           (int_ipu_operand3[ipu*ELEN +: ELEN]                                                              ),
      .tag_i            (input_tag                                                                                       ),
      .carry_i          ('0                                                                                              ),
      .sew_i            (sew                                                                                             ),
      .be_o             (/* Unused */                                                                                    ),
      .result_o         (int_ipu_result[ipu*ELEN +: ELEN]                                                                ),
      .result_valid_o   (int_ipu_result_valid[ipu*ELENB +: ELENB]                                                        ),
      .result_ready_i   (int_ipu_result_ready                                                                            ),
      .tag_o            (int_ipu_result_tag[ipu]                                                                         ),
      .busy_o           (int_ipu_busy[ipu]                                                                               )
    );
  end : gen_ipus

  ////////////
  //  FPUs  //
  ////////////

  if (FPU) begin: gen_fpu
    operation_e fpu_op;
    fp_format_e fpu_src_fmt, fpu_dst_fmt;
    int_format_e fpu_int_fmt;
    logic fpu_op_mode;
    logic fpu_vectorial_op;

    logic [N_FPU-1:0] fpu_busy_d, fpu_busy_q;
    // TMR: fpu_busy_q (voted value in fpu_busy_q, copies fpu_busy_q0..2)
    logic [N_FPU-1:0] fpu_busy_q0, fpu_busy_q1, fpu_busy_q2;
    `FF(fpu_busy_q0, fpu_busy_d, '0)
    `FF(fpu_busy_q1, fpu_busy_d, '0)
    `FF(fpu_busy_q2, fpu_busy_d, '0)
    bitwise_TMR_voter_fail #(.DataWidth($bits(fpu_busy_q)), .VoterType(1)) i_fpu_busy_q_voter (
      .a_i(fpu_busy_q0), .b_i(fpu_busy_q1), .c_i(fpu_busy_q2), .majority_o(fpu_busy_q), .fault_detected_o(ff_tmr_err_0[7]));

    status_t [N_FPU-1:0] fpu_status_d, fpu_status_q;
    // TMR: fpu_status_q (voted value in fpu_status_q, copies fpu_status_q0..2)
    status_t [N_FPU-1:0] fpu_status_q0, fpu_status_q1, fpu_status_q2;
    `FF(fpu_status_q0, fpu_status_d, '0)
    `FF(fpu_status_q1, fpu_status_d, '0)
    `FF(fpu_status_q2, fpu_status_d, '0)
    logic [$bits(fpu_status_q)-1:0] fpu_status_q_vote;
    bitwise_TMR_voter_fail #(.DataWidth($bits(fpu_status_q)), .VoterType(1)) i_fpu_status_q_voter (
      .a_i(fpu_status_q0), .b_i(fpu_status_q1), .c_i(fpu_status_q2), .majority_o(fpu_status_q_vote), .fault_detected_o(ff_tmr_err_0[8]));
    assign fpu_status_q = (fpu_status_q_vote);

    always_comb begin: gen_decoder
      fpu_op           = fpnew_pkg::FMADD;
      fpu_op_mode      = 1'b0;
      fpu_vectorial_op = 1'b0;
      is_fpu_busy      = |fpu_busy_q;
      fpu_src_fmt      = fpnew_pkg::FP32;
      fpu_dst_fmt      = fpnew_pkg::FP32;
      fpu_int_fmt      = fpnew_pkg::INT32;

      fpu_status_o = '0;
      for (int fpu = 0; fpu < N_FPU; fpu++)
        fpu_status_o |= fpu_status_q[fpu];

      if (FPU) begin
        unique case (spatz_req.vtype.vsew)
          EW_64: begin
            if (RVD) begin
              fpu_src_fmt = fpnew_pkg::FP64;
              fpu_dst_fmt = fpnew_pkg::FP64;
              fpu_int_fmt = fpnew_pkg::INT64;
            end
          end
          EW_32: begin
            fpu_src_fmt      = spatz_req.op_arith.is_narrowing || spatz_req.op_arith.widen_vs1 || spatz_req.op_arith.widen_vs2 ? fpnew_pkg::FP64 : fpnew_pkg::FP32;
            fpu_dst_fmt      = spatz_req.op_arith.widen_vs1 || spatz_req.op_arith.widen_vs2 || spatz_req.op == VSDOTP ? fpnew_pkg::FP64          : fpnew_pkg::FP32;
            fpu_int_fmt      = spatz_req.op_arith.is_narrowing && spatz_req.op inside {VI2F, VU2F} ? fpnew_pkg::INT64                            : fpnew_pkg::INT32;
            fpu_vectorial_op = FLEN > 32;
          end
          EW_16: begin
            fpu_src_fmt      = spatz_req.op_arith.is_narrowing || spatz_req.op_arith.widen_vs1 || spatz_req.op_arith.widen_vs2 ? fpnew_pkg::FP32 : (spatz_req.fm.src ? fpnew_pkg::FP16ALT : fpnew_pkg::FP16);
            fpu_dst_fmt      = spatz_req.op_arith.widen_vs1 || spatz_req.op_arith.widen_vs2 || spatz_req.op == VSDOTP          ? fpnew_pkg::FP32 : (spatz_req.fm.dst ? fpnew_pkg::FP16ALT : fpnew_pkg::FP16);
            fpu_int_fmt      = spatz_req.op_arith.is_narrowing && spatz_req.op inside {VI2F, VU2F}                             ? fpnew_pkg::INT32 : fpnew_pkg::INT16;
            fpu_vectorial_op = 1'b1;
          end
          EW_8: begin
            fpu_src_fmt      = spatz_req.op_arith.is_narrowing || spatz_req.op_arith.widen_vs1 || spatz_req.op_arith.widen_vs2 ? (spatz_req.fm.src ? fpnew_pkg::FP16ALT : fpnew_pkg::FP16) : (spatz_req.fm.src ? fpnew_pkg::FP8ALT : fpnew_pkg::FP8);
            fpu_dst_fmt      = spatz_req.op_arith.widen_vs1 || spatz_req.op_arith.widen_vs2 || spatz_req.op == VSDOTP          ? (spatz_req.fm.dst ? fpnew_pkg::FP16ALT : fpnew_pkg::FP16) : (spatz_req.fm.dst ? fpnew_pkg::FP8ALT : fpnew_pkg::FP8);
            fpu_int_fmt      = spatz_req.op_arith.is_narrowing && spatz_req.op inside {VI2F, VU2F}                             ? fpnew_pkg::INT16 : fpnew_pkg::INT8;
            fpu_vectorial_op = 1'b1;
          end
          default:;
        endcase

        unique case (spatz_req.op)
          VFADD: fpu_op = fpnew_pkg::ADD;
          VFSUB: begin
            fpu_op      = fpnew_pkg::ADD;
            fpu_op_mode = 1'b1;
          end
          VFMUL  : fpu_op = fpnew_pkg::MUL;
          VFMADD : fpu_op = fpnew_pkg::FMADD;
          VFMSUB : begin
            fpu_op      = fpnew_pkg::FMADD;
            fpu_op_mode = 1'b1;
          end
          VFNMSUB: fpu_op = fpnew_pkg::FNMSUB;
          VFNMADD: begin
            fpu_op      = fpnew_pkg::FNMSUB;
            fpu_op_mode = 1'b1;
          end

          VFMINMAX: begin
            fpu_op = fpnew_pkg::MINMAX;
            fpu_dst_fmt = fpu_src_fmt;
          end


          VFSGNJ : begin
            fpu_op = fpnew_pkg::SGNJ;
            fpu_dst_fmt = fpu_src_fmt;
          end
          VFCLASS: begin
            fpu_op = fpnew_pkg::CLASSIFY;
            fpu_dst_fmt = fpu_src_fmt;
          end
          VFCMP  : begin
            fpu_op = fpnew_pkg::CMP;
            fpu_dst_fmt = fpu_src_fmt;
          end

          VF2F: fpu_op = fpnew_pkg::F2F;
          VF2I: fpu_op = fpnew_pkg::F2I;
          VF2U: begin
            fpu_op      = fpnew_pkg::F2I;
            fpu_op_mode = 1'b1;
          end
          VI2F: fpu_op = fpnew_pkg::I2F;
          VU2F: begin
            fpu_op      = fpnew_pkg::I2F;
            fpu_op_mode = 1'b1;
          end

          VSDOTP: fpu_op = fpnew_pkg::SDOTP;

          default:;
        endcase
      end
    end: gen_decoder

    logic [N_FPU*ELEN-1:0] wide_operand1, wide_operand2, wide_operand3;
    always_comb begin: gen_widening
      automatic logic [N_FPU*ELEN/2-1:0] shift_operand1 = !widening_upper_q ? operand1[N_FPU*ELEN/2-1:0] : operand1[N_FPU*ELEN-1:N_FPU*ELEN/2];
      automatic logic [N_FPU*ELEN/2-1:0] shift_operand2 = !widening_upper_q ? operand2[N_FPU*ELEN/2-1:0] : operand2[N_FPU*ELEN-1:N_FPU*ELEN/2];

      wide_operand1 = operand1;
      wide_operand2 = operand2;
      wide_operand3 = operand3;

      case (spatz_req.vtype.vsew)
        EW_32: begin
          for (int el = 0; el < N_FPU; el++) begin
            if (spatz_req.op_arith.widen_vs1 && MAXEW == EW_64)
              wide_operand1[64*el +: 64] = widen_fp32_to_fp64(shift_operand1[32*el +: 32]);

            if (spatz_req.op_arith.widen_vs2 && MAXEW == EW_64)
              wide_operand2[64*el +: 64] = widen_fp32_to_fp64(shift_operand2[32*el +: 32]);
          end
        end
        EW_16: begin
          for (int el = 0; el < (MAXEW == EW_64 ? 2*N_FPU : N_FPU); el++) begin
            if (spatz_req.op_arith.widen_vs1)
              wide_operand1[32*el +: 32] = widen_fp16_to_fp32(shift_operand1[16*el +: 16]);

            if (spatz_req.op_arith.widen_vs2)
              wide_operand2[32*el +: 32] = widen_fp16_to_fp32(shift_operand2[16*el +: 16]);
          end
        end
        EW_8: begin
          for (int el = 0; el < (MAXEW == EW_64 ? 4*N_FPU : 2*N_FPU); el++) begin
            if (spatz_req.op_arith.widen_vs1)
              wide_operand1[16*el +: 16] = widen_fp8_to_fp16(shift_operand1[8*el +: 8]);

            if (spatz_req.op_arith.widen_vs2)
              wide_operand2[16*el +: 16] = widen_fp8_to_fp16(shift_operand2[8*el +: 8]);
          end
        end
        default:;
      endcase
    end: gen_widening

    for (genvar fpu = 0; unsigned'(fpu) < N_FPU; fpu++) begin : gen_fpnew
      logic int_fpu_result_valid;
      logic int_fpu_in_ready;
      vfu_tag_t tag;

      assign fpu_in_ready[fpu*ELENB +: ELENB]     = {ELENB{int_fpu_in_ready}};
      assign fpu_result_valid[fpu*ELENB +: ELENB] = {ELENB{int_fpu_result_valid}};

      elen_t fpu_operand1, fpu_operand2, fpu_operand3;
      assign fpu_operand1 = spatz_req.op_arith.switch_rs1_rd ? wide_operand3[fpu*ELEN +: ELEN] : wide_operand1[fpu*ELEN +: ELEN];
      assign fpu_operand2 = wide_operand2[fpu*ELEN +: ELEN];
      assign fpu_operand3 = (fpu_op == fpnew_pkg::ADD || spatz_req.op_arith.switch_rs1_rd) ? wide_operand1[fpu*ELEN +: ELEN] : wide_operand3[fpu*ELEN +: ELEN];

      logic int_fpu_in_valid;
      assign int_fpu_in_valid = spatz_req_valid && operands_ready && (!spatz_req.op_arith.is_scalar || fpu == 0) && is_fpu_insn;

      // Generate an FPU pipeline
      elen_t fpu_operand1_q, fpu_operand2_q, fpu_operand3_q;
      operation_e fpu_op_q;
      fp_format_e fpu_src_fmt_q, fpu_dst_fmt_q;
      int_format_e fpu_int_fmt_q;
      logic fpu_op_mode_q;
      logic fpu_vectorial_op_q;
      roundmode_e rm_q;
      vfu_tag_t input_tag_q;
      logic fpu_in_valid_q;
      logic fpu_in_ready_d;

      // TMR: fpu_operand1_q (voted value in fpu_operand1_q, copies fpu_operand1_q0..2)
      elen_t fpu_operand1_q0, fpu_operand1_q1, fpu_operand1_q2;
      `FFL(fpu_operand1_q0, fpu_operand1, int_fpu_in_valid && int_fpu_in_ready, '0)
      `FFL(fpu_operand1_q1, fpu_operand1, int_fpu_in_valid && int_fpu_in_ready, '0)
      `FFL(fpu_operand1_q2, fpu_operand1, int_fpu_in_valid && int_fpu_in_ready, '0)
      logic [$bits(fpu_operand1_q)-1:0] fpu_operand1_q_vote;
      bitwise_TMR_voter_fail #(.DataWidth($bits(fpu_operand1_q)), .VoterType(1)) i_fpu_operand1_q_voter (
        .a_i(fpu_operand1_q0), .b_i(fpu_operand1_q1), .c_i(fpu_operand1_q2), .majority_o(fpu_operand1_q_vote), .fault_detected_o(ff_tmr_err_1[((0)*(N_FPU)+fpu)*12+0]));
      assign fpu_operand1_q = elen_t'(fpu_operand1_q_vote);
      // TMR: fpu_operand2_q (voted value in fpu_operand2_q, copies fpu_operand2_q0..2)
      elen_t fpu_operand2_q0, fpu_operand2_q1, fpu_operand2_q2;
      `FFL(fpu_operand2_q0, fpu_operand2, int_fpu_in_valid && int_fpu_in_ready, '0)
      `FFL(fpu_operand2_q1, fpu_operand2, int_fpu_in_valid && int_fpu_in_ready, '0)
      `FFL(fpu_operand2_q2, fpu_operand2, int_fpu_in_valid && int_fpu_in_ready, '0)
      logic [$bits(fpu_operand2_q)-1:0] fpu_operand2_q_vote;
      bitwise_TMR_voter_fail #(.DataWidth($bits(fpu_operand2_q)), .VoterType(1)) i_fpu_operand2_q_voter (
        .a_i(fpu_operand2_q0), .b_i(fpu_operand2_q1), .c_i(fpu_operand2_q2), .majority_o(fpu_operand2_q_vote), .fault_detected_o(ff_tmr_err_1[((0)*(N_FPU)+fpu)*12+1]));
      assign fpu_operand2_q = elen_t'(fpu_operand2_q_vote);
      // TMR: fpu_operand3_q (voted value in fpu_operand3_q, copies fpu_operand3_q0..2)
      elen_t fpu_operand3_q0, fpu_operand3_q1, fpu_operand3_q2;
      `FFL(fpu_operand3_q0, fpu_operand3, int_fpu_in_valid && int_fpu_in_ready, '0)
      `FFL(fpu_operand3_q1, fpu_operand3, int_fpu_in_valid && int_fpu_in_ready, '0)
      `FFL(fpu_operand3_q2, fpu_operand3, int_fpu_in_valid && int_fpu_in_ready, '0)
      logic [$bits(fpu_operand3_q)-1:0] fpu_operand3_q_vote;
      bitwise_TMR_voter_fail #(.DataWidth($bits(fpu_operand3_q)), .VoterType(1)) i_fpu_operand3_q_voter (
        .a_i(fpu_operand3_q0), .b_i(fpu_operand3_q1), .c_i(fpu_operand3_q2), .majority_o(fpu_operand3_q_vote), .fault_detected_o(ff_tmr_err_1[((0)*(N_FPU)+fpu)*12+2]));
      assign fpu_operand3_q = elen_t'(fpu_operand3_q_vote);
      // TMR: fpu_op_q (voted value in fpu_op_q, copies fpu_op_q0..2)
      operation_e fpu_op_q0, fpu_op_q1, fpu_op_q2;
      `FFL(fpu_op_q0, fpu_op, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::FMADD)
      `FFL(fpu_op_q1, fpu_op, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::FMADD)
      `FFL(fpu_op_q2, fpu_op, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::FMADD)
      logic [$bits(fpu_op_q)-1:0] fpu_op_q_vote;
      bitwise_TMR_voter_fail #(.DataWidth($bits(fpu_op_q)), .VoterType(1)) i_fpu_op_q_voter (
        .a_i(fpu_op_q0), .b_i(fpu_op_q1), .c_i(fpu_op_q2), .majority_o(fpu_op_q_vote), .fault_detected_o(ff_tmr_err_1[((0)*(N_FPU)+fpu)*12+3]));
      assign fpu_op_q = operation_e'(fpu_op_q_vote);
      // TMR: fpu_src_fmt_q (voted value in fpu_src_fmt_q, copies fpu_src_fmt_q0..2)
      fp_format_e fpu_src_fmt_q0, fpu_src_fmt_q1, fpu_src_fmt_q2;
      `FFL(fpu_src_fmt_q0, fpu_src_fmt, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::FP32)
      `FFL(fpu_src_fmt_q1, fpu_src_fmt, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::FP32)
      `FFL(fpu_src_fmt_q2, fpu_src_fmt, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::FP32)
      logic [$bits(fpu_src_fmt_q)-1:0] fpu_src_fmt_q_vote;
      bitwise_TMR_voter_fail #(.DataWidth($bits(fpu_src_fmt_q)), .VoterType(1)) i_fpu_src_fmt_q_voter (
        .a_i(fpu_src_fmt_q0), .b_i(fpu_src_fmt_q1), .c_i(fpu_src_fmt_q2), .majority_o(fpu_src_fmt_q_vote), .fault_detected_o(ff_tmr_err_1[((0)*(N_FPU)+fpu)*12+4]));
      assign fpu_src_fmt_q = fp_format_e'(fpu_src_fmt_q_vote);
      // TMR: fpu_dst_fmt_q (voted value in fpu_dst_fmt_q, copies fpu_dst_fmt_q0..2)
      fp_format_e fpu_dst_fmt_q0, fpu_dst_fmt_q1, fpu_dst_fmt_q2;
      `FFL(fpu_dst_fmt_q0, fpu_dst_fmt, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::FP32)
      `FFL(fpu_dst_fmt_q1, fpu_dst_fmt, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::FP32)
      `FFL(fpu_dst_fmt_q2, fpu_dst_fmt, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::FP32)
      logic [$bits(fpu_dst_fmt_q)-1:0] fpu_dst_fmt_q_vote;
      bitwise_TMR_voter_fail #(.DataWidth($bits(fpu_dst_fmt_q)), .VoterType(1)) i_fpu_dst_fmt_q_voter (
        .a_i(fpu_dst_fmt_q0), .b_i(fpu_dst_fmt_q1), .c_i(fpu_dst_fmt_q2), .majority_o(fpu_dst_fmt_q_vote), .fault_detected_o(ff_tmr_err_1[((0)*(N_FPU)+fpu)*12+5]));
      assign fpu_dst_fmt_q = fp_format_e'(fpu_dst_fmt_q_vote);
      // TMR: fpu_int_fmt_q (voted value in fpu_int_fmt_q, copies fpu_int_fmt_q0..2)
      int_format_e fpu_int_fmt_q0, fpu_int_fmt_q1, fpu_int_fmt_q2;
      `FFL(fpu_int_fmt_q0, fpu_int_fmt, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::INT8)
      `FFL(fpu_int_fmt_q1, fpu_int_fmt, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::INT8)
      `FFL(fpu_int_fmt_q2, fpu_int_fmt, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::INT8)
      logic [$bits(fpu_int_fmt_q)-1:0] fpu_int_fmt_q_vote;
      bitwise_TMR_voter_fail #(.DataWidth($bits(fpu_int_fmt_q)), .VoterType(1)) i_fpu_int_fmt_q_voter (
        .a_i(fpu_int_fmt_q0), .b_i(fpu_int_fmt_q1), .c_i(fpu_int_fmt_q2), .majority_o(fpu_int_fmt_q_vote), .fault_detected_o(ff_tmr_err_1[((0)*(N_FPU)+fpu)*12+6]));
      assign fpu_int_fmt_q = int_format_e'(fpu_int_fmt_q_vote);
      // TMR: fpu_op_mode_q (voted value in fpu_op_mode_q, copies fpu_op_mode_q0..2)
      logic fpu_op_mode_q0, fpu_op_mode_q1, fpu_op_mode_q2;
      `FFL(fpu_op_mode_q0, fpu_op_mode, int_fpu_in_valid && int_fpu_in_ready, 1'b0)
      `FFL(fpu_op_mode_q1, fpu_op_mode, int_fpu_in_valid && int_fpu_in_ready, 1'b0)
      `FFL(fpu_op_mode_q2, fpu_op_mode, int_fpu_in_valid && int_fpu_in_ready, 1'b0)
      bitwise_TMR_voter_fail #(.DataWidth($bits(fpu_op_mode_q)), .VoterType(1)) i_fpu_op_mode_q_voter (
        .a_i(fpu_op_mode_q0), .b_i(fpu_op_mode_q1), .c_i(fpu_op_mode_q2), .majority_o(fpu_op_mode_q), .fault_detected_o(ff_tmr_err_1[((0)*(N_FPU)+fpu)*12+7]));
      // TMR: fpu_vectorial_op_q (voted value in fpu_vectorial_op_q, copies fpu_vectorial_op_q0..2)
      logic fpu_vectorial_op_q0, fpu_vectorial_op_q1, fpu_vectorial_op_q2;
      `FFL(fpu_vectorial_op_q0, fpu_vectorial_op, int_fpu_in_valid && int_fpu_in_ready, 1'b0)
      `FFL(fpu_vectorial_op_q1, fpu_vectorial_op, int_fpu_in_valid && int_fpu_in_ready, 1'b0)
      `FFL(fpu_vectorial_op_q2, fpu_vectorial_op, int_fpu_in_valid && int_fpu_in_ready, 1'b0)
      bitwise_TMR_voter_fail #(.DataWidth($bits(fpu_vectorial_op_q)), .VoterType(1)) i_fpu_vectorial_op_q_voter (
        .a_i(fpu_vectorial_op_q0), .b_i(fpu_vectorial_op_q1), .c_i(fpu_vectorial_op_q2), .majority_o(fpu_vectorial_op_q), .fault_detected_o(ff_tmr_err_1[((0)*(N_FPU)+fpu)*12+8]));
      // TMR: rm_q (voted value in rm_q, copies rm_q0..2)
      roundmode_e rm_q0, rm_q1, rm_q2;
      `FFL(rm_q0, spatz_req.rm, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::RNE)
      `FFL(rm_q1, spatz_req.rm, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::RNE)
      `FFL(rm_q2, spatz_req.rm, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::RNE)
      logic [$bits(rm_q)-1:0] rm_q_vote;
      bitwise_TMR_voter_fail #(.DataWidth($bits(rm_q)), .VoterType(1)) i_rm_q_voter (
        .a_i(rm_q0), .b_i(rm_q1), .c_i(rm_q2), .majority_o(rm_q_vote), .fault_detected_o(ff_tmr_err_1[((0)*(N_FPU)+fpu)*12+9]));
      assign rm_q = roundmode_e'(rm_q_vote);
      // TMR: input_tag_q (voted value in input_tag_q, copies input_tag_q0..2)
      vfu_tag_t input_tag_q0, input_tag_q1, input_tag_q2;
      `FFL(input_tag_q0, input_tag, int_fpu_in_valid && int_fpu_in_ready, '{vsew: EW_8, default: '0})
      `FFL(input_tag_q1, input_tag, int_fpu_in_valid && int_fpu_in_ready, '{vsew: EW_8, default: '0})
      `FFL(input_tag_q2, input_tag, int_fpu_in_valid && int_fpu_in_ready, '{vsew: EW_8, default: '0})
      logic [$bits(input_tag_q)-1:0] input_tag_q_vote;
      bitwise_TMR_voter_fail #(.DataWidth($bits(input_tag_q)), .VoterType(1)) i_input_tag_q_voter (
        .a_i(input_tag_q0), .b_i(input_tag_q1), .c_i(input_tag_q2), .majority_o(input_tag_q_vote), .fault_detected_o(ff_tmr_err_1[((0)*(N_FPU)+fpu)*12+10]));
      assign input_tag_q = vfu_tag_t'(input_tag_q_vote);
      // TMR: fpu_in_valid_q (voted value in fpu_in_valid_q, copies fpu_in_valid_q0..2)
      logic fpu_in_valid_q0, fpu_in_valid_q1, fpu_in_valid_q2;
      `FFL(fpu_in_valid_q0, int_fpu_in_valid, int_fpu_in_ready, 1'b0)
      `FFL(fpu_in_valid_q1, int_fpu_in_valid, int_fpu_in_ready, 1'b0)
      `FFL(fpu_in_valid_q2, int_fpu_in_valid, int_fpu_in_ready, 1'b0)
      bitwise_TMR_voter_fail #(.DataWidth($bits(fpu_in_valid_q)), .VoterType(1)) i_fpu_in_valid_q_voter (
        .a_i(fpu_in_valid_q0), .b_i(fpu_in_valid_q1), .c_i(fpu_in_valid_q2), .majority_o(fpu_in_valid_q), .fault_detected_o(ff_tmr_err_1[((0)*(N_FPU)+fpu)*12+11]));
      assign int_fpu_in_ready = !fpu_in_valid_q || fpu_in_valid_q && fpu_in_ready_d;

      fpnew_top #(
        .Features                   (FPUFeatures           ),
        .Implementation             (FPUImplementation     ),
        .TagType                    (vfu_tag_t             ),
        .StochasticRndImplementation(fpnew_pkg::DEFAULT_RSR)
      ) i_fpu (
        .clk_i         (clk_i                                                  ),
        .rst_ni        (rst_ni                                                 ),
        .hart_id_i     (hart_id_i),
        .flush_i       (1'b0                                                   ),
        .busy_o        (fpu_busy_d[fpu]                                        ),
        .operands_i    ({fpu_operand3_q, fpu_operand2_q, fpu_operand1_q}       ),
        // Only the FPU0 executes scalar instructions
        .in_valid_i    (fpu_in_valid_q                                         ),
        .in_ready_o    (fpu_in_ready_d                                         ),
        .op_i          (fpu_op_q                                               ),
        .src_fmt_i     (fpu_src_fmt_q                                          ),
        .dst_fmt_i     (fpu_dst_fmt_q                                          ),
        .int_fmt_i     (fpu_int_fmt_q                                          ),
        .vectorial_op_i(fpu_vectorial_op_q                                     ),
        .op_mod_i      (fpu_op_mode_q                                          ),
        .tag_i         (input_tag_q                                            ),
        .simd_mask_i   ('1                                                     ),
        .rnd_mode_i    (rm_q                                                   ),
        .result_o      (fpu_result[fpu*ELEN +: ELEN]                           ),
        .out_valid_o   (int_fpu_result_valid                                   ),
        .out_ready_i   (result_ready                                           ),
        .status_o      (fpu_status_d[fpu]                                      ),
        .tag_o         (tag                                                    )
      );

      if (fpu == 0) begin: gen_fpu_tag
        assign fpu_result_tag = tag;
      end: gen_fpu_tag
    end : gen_fpnew
  end: gen_fpu else begin: gen_no_fpu
    assign is_fpu_busy      = 1'b0;
    assign fpu_in_ready     = '0;
    assign fpu_result       = '0;
    assign fpu_result_valid = '0;
    assign fpu_result_tag   = '0;
    assign fpu_status_o     = '0;
  end: gen_no_fpu


  assign scalar_correctable_fault_o   = vfu_scalar_response_err[0] | spatz_rsp_register_err[0] | operation_queue_err[0] | ff_tmr_fault_any;
  assign scalar_uncorrectable_fault_o = vfu_scalar_response_err[1] | spatz_rsp_register_err[1] | operation_queue_err[1];

endmodule : spatz_scalar_unit
