// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich
//
// The Vector Functional Unit (VFU) executes all arithmetic and logical
// vector instructions. It can be configured with a parameterizable amount
// of IPUs that work in parallel.

module spatz_vfu
  import spatz_pkg::*;
  import rvv_pkg::*;
  import cf_math_pkg::idx_width;
  import fpnew_pkg::*; #(
    /// FPU configuration.
    parameter fpu_implementation_t FPUImplementation = fpu_implementation_t'(0)
  ) (
    input  logic             clk_i,
    input  logic             rst_ni,
    input  logic [31:0]      hart_id_i,
    // Spatz req
    input  spatz_req_t       spatz_req_i,
    input  logic             spatz_req_valid_i,
    output logic             spatz_req_ready_o,
    // VFU response
    output logic             vfu_rsp_valid_o,
    input  logic             vfu_rsp_ready_i,
    output vfu_rsp_t         vfu_rsp_o,
    // VRF
    output vrf_addr_t        vrf_waddr_o,
    output vrf_data_t        vrf_wdata_o,
    output logic             vrf_we_o,
    output vrf_be_t          vrf_wbe_o,
    input  logic             vrf_wvalid_i,
    output spatz_id_t  [3:0] vrf_id_o,
    output vrf_addr_t  [2:0] vrf_raddr_o,
    output logic       [2:0] vrf_re_o,
    input  vrf_data_t  [2:0] vrf_rdata_i,
    input  logic       [2:0] vrf_rvalid_i,
    // FPU side channel
    output status_t          fpu_status_o
  );

// Include FF
`include "common_cells/registers.svh"

  // Instruction tag (propagated together with the operands through the pipelines)
  typedef struct packed {
    spatz_id_t id;

    vew_e vsew;
    vlen_t vstart;

    // Encodes both the scalar RD and the VD address in the VRF
    vrf_addr_t vd_addr;
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

  spill_register #(
    .T(spatz_req_t)
  ) i_operation_queue (
    .clk_i  (clk_i                                          ),
    .rst_ni (rst_ni                                         ),
    .data_i (spatz_req_i                                    ),
    .valid_i(spatz_req_valid_i && spatz_req_i.ex_unit == VFU),
    .ready_o(spatz_req_ready_o                              ),
    .data_o (spatz_req                                      ),
    .valid_o(spatz_req_valid                                ),
    .ready_i(spatz_req_ready                                )
  );

  ///////////////
  //  Control  //
  ///////////////

  // Vector length counter
  vlen_t vl_q, vl_d;
  `FF(vl_q, vl_d, '0)

  // Are we busy?
  logic busy_q, busy_d;
  `FF(busy_q, busy_d, 1'b0)

  // Number of elements in one VRF word
  logic [$clog2(N_FU*(ELEN/8)):0] nr_elem_word;
  assign nr_elem_word = (N_FU * (1 << (MAXEW - spatz_req.vtype.vsew))) >> spatz_req.op_arith.is_narrowing;

  // Are we running integer or floating-point instructions?
  typedef enum logic {
    VFU_RunningIPU, VFU_RunningFPU
   } state_t;
   state_t state_d, state_q;
  `FF(state_q, state_d, VFU_RunningFPU)

  // Propagate the tags through the functional units
  vfu_tag_t ipu_result_tag, fpu_result_tag, result_tag;
  vfu_tag_t input_tag;

  assign result_tag = state_q == VFU_RunningIPU ? ipu_result_tag : fpu_result_tag;

  // Number of words advanced by vstart
  vlen_t vstart;
  assign vstart = ((spatz_req.vstart / N_FU) >> (MAXEW - spatz_req.vtype.vsew)) << (MAXEW - spatz_req.vtype.vsew);

  // Should we stall?
  logic stall;

  // Do we have the reduction operand?
  logic reduction_operand_ready_d, reduction_operand_ready_q;

  // Are the VFU operands ready?
  logic op1_is_ready, op2_is_ready, op3_is_ready, operands_ready;
  assign op1_is_ready   = spatz_req_valid && ((!spatz_req.op_arith.is_reduction && (!spatz_req.use_vs1 || vrf_rvalid_i[1])) || (spatz_req.op_arith.is_reduction && reduction_operand_ready_q));
  assign op2_is_ready   = spatz_req_valid && ((!spatz_req.use_vs2 || vrf_rvalid_i[0]) || spatz_req.op_arith.is_reduction);
  assign op3_is_ready   = spatz_req_valid && (!spatz_req.vd_is_src || vrf_rvalid_i[2]);
  assign operands_ready = op1_is_ready && op2_is_ready && op3_is_ready && (!spatz_req.op_arith.is_scalar || vfu_rsp_ready_i) && !stall;

  // Valid operations
  logic [N_FU*ELENB-1:0] valid_operations;
  assign valid_operations = (spatz_req.op_arith.is_scalar || spatz_req.op_arith.is_reduction) ? (spatz_req.vtype.vsew == EW_32 ? 4'hf : 8'hff) : '1;

  // Pending results
  logic [N_FU*ELENB-1:0] pending_results;
  assign pending_results = result_tag.wb ? (spatz_req.vtype.vsew == EW_32 ? 4'hf : 8'hff) : '1;

  // Did we issue a microoperation?
  logic word_issued;

  // Currently running instructions
  logic [NrParallelInstructions-1:0] running_d, running_q;
  `FF(running_q, running_d, '0)

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

  // Reduction state
  typedef enum logic [2:0] {
    Reduction_NormalExecution,
    Reduction_Wait,
    Reduction_Init,
    Reduction_Reduce,
    Reduction_WriteBack
   } reduction_state_t;
   reduction_state_t reduction_state_d, reduction_state_q;
  `FF(reduction_state_q, reduction_state_d, Reduction_NormalExecution)

  // Is the reduction done?
  logic reduction_done;

  // Are we producing the upper or lower part of the results of a narrowing instruction?
  logic narrowing_upper_d, narrowing_upper_q;
  `FF(narrowing_upper_q, narrowing_upper_d, 1'b0)

  // Are we reading the upper or lower part of the operands of a widening instruction?
  logic widening_upper_d, widening_upper_q;
  `FF(widening_upper_q, widening_upper_d, 1'b0)

  // Are any results valid?
  logic [N_FU*ELEN-1:0]  result;
  logic [N_FU*ELENB-1:0] result_valid;
  logic                  result_ready;

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
    vfu_rsp_valid_o = 1'b0;
    vfu_rsp_o       = '0;

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
    if (spatz_req_valid && ((vl_d >= spatz_req.vl && !spatz_req.op_arith.is_reduction) || reduction_done)) begin
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
    if ((result_tag.last && &(result_valid | ~pending_results) && reduction_state_q inside {Reduction_NormalExecution, Reduction_Wait}) || reduction_done) begin
      vfu_rsp_o.id      = result_tag.id;
      vfu_rsp_o.rd      = result_tag.vd_addr[GPRWidth-1:0];
      vfu_rsp_o.wb      = result_tag.wb;
      vfu_rsp_o.result  = result_tag.wb ? scalar_result : '0;
      vfu_rsp_valid_o   = 1'b1;
    end
  end: control_proc

  //////////////
  // Operands //
  //////////////

  // Reduction registers
  elen_t [1:0] reduction_q, reduction_d;
  `FFL(reduction_q, reduction_d, reduction_operand_ready_d, '0)

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
  always_comb begin: operand_proc
    if (spatz_req.op_arith.is_scalar)
      operand1 = {1*N_FU{spatz_req.rs1}};
    else if (spatz_req.use_vs1)
      operand1 = spatz_req.op_arith.is_reduction ? $unsigned(reduction_q[1]) : vrf_rdata_i[1];
    else begin
      // Replicate scalar operands
      unique case (spatz_req.op == VSDOTP ? vew_e'(spatz_req.vtype.vsew + 1) : spatz_req.vtype.vsew)
        EW_8 : operand1   = MAXEW == EW_32 ? {4*N_FU{spatz_req.rs1[7:0]}}  : {8*N_FU{spatz_req.rs1[7:0]}};
        EW_16: operand1   = MAXEW == EW_32 ? {2*N_FU{spatz_req.rs1[15:0]}} : {4*N_FU{spatz_req.rs1[15:0]}};
        EW_32: operand1   = MAXEW == EW_32 ? {1*N_FU{spatz_req.rs1[31:0]}} : {2*N_FU{spatz_req.rs1[31:0]}};
        default: operand1 = {1*N_FU{spatz_req.rs1}};
      endcase
    end

    if ((!spatz_req.op_arith.is_scalar || spatz_req.op == VADD) && spatz_req.use_vs2)
      operand2 = spatz_req.op_arith.is_reduction ? $unsigned(reduction_q[0]) : vrf_rdata_i[0];
    else
      // Replicate scalar operands
      unique case (spatz_req.op == VSDOTP ? vew_e'(spatz_req.vtype.vsew + 1) : spatz_req.vtype.vsew)
        EW_8 : operand2   = MAXEW == EW_32 ? {4*N_FU{spatz_req.rs2[7:0]}}  : {8*N_FU{spatz_req.rs2[7:0]}};
        EW_16: operand2   = MAXEW == EW_32 ? {2*N_FU{spatz_req.rs2[15:0]}} : {4*N_FU{spatz_req.rs2[15:0]}};
        EW_32: operand2   = MAXEW == EW_32 ? {1*N_FU{spatz_req.rs2[31:0]}} : {2*N_FU{spatz_req.rs2[31:0]}};
        default: operand2 = {1*N_FU{spatz_req.rs2}};
      endcase

    operand3 = spatz_req.op_arith.is_scalar ? {1*N_FU{spatz_req.rsd}} : vrf_rdata_i[2];
  end: operand_proc

  assign in_ready     = state_q == VFU_RunningIPU ? ipu_in_ready     : fpu_in_ready;
  assign result       = state_q == VFU_RunningIPU ? ipu_result       : fpu_result;
  assign result_valid = state_q == VFU_RunningIPU ? ipu_result_valid : fpu_result_valid;

  assign scalar_result = result[ELEN-1:0];

  ///////////////////////
  //  Reduction logic  //
  ///////////////////////

  // Reduction pointer
  vlen_t reduction_pointer_d, reduction_pointer_q;
  `FF(reduction_pointer_q, reduction_pointer_d, '0)

  // Are the reduction operands ready?
  `FF(reduction_operand_ready_q, reduction_operand_ready_d, 1'b0)

  // Do we need to request reduction operands?
  logic [1:0] reduction_operand_request;

  always_comb begin: proc_reduction
    // Maintain state
    reduction_state_d   = reduction_state_q;
    reduction_pointer_d = reduction_pointer_q;

    // No operands
    reduction_d               = reduction_q;
    reduction_operand_ready_d = 1'b0;

    // Did we issue a word to the FUs?
    word_issued = 1'b0;

    // Are we ready to accept a result?
    result_ready = 1'b0;

    // Reduction did not finish
    reduction_done = 1'b0;

    // Only request when initializing the reduction register
    reduction_operand_request[0] = (reduction_state_q == Reduction_Init) || !spatz_req.op_arith.is_reduction;
    reduction_operand_request[1] = (reduction_state_q inside {Reduction_Init, Reduction_Reduce}) || !spatz_req.op_arith.is_reduction;

    unique case (reduction_state_q)
      Reduction_NormalExecution: begin
        // Did we issue a word to the FUs?
        word_issued = spatz_req_valid && &(in_ready | ~valid_operations) && operands_ready && !stall;

        // Are we ready to accept a result?
        result_ready = &(result_valid | ~pending_results) && ((result_tag.wb && vfu_rsp_ready_i) || vrf_wvalid_i);

        // Initialize the pointers
        reduction_pointer_d = '0;

        // Do we have a new reduction instruction?
        if (spatz_req_valid && !running_q[spatz_req.id] && spatz_req.op_arith.is_reduction)
          reduction_state_d = is_fpu_busy ? Reduction_Wait : Reduction_Init;
      end

      Reduction_Wait: begin
        // Are we ready to accept a result?
        result_ready = &(result_valid | ~pending_results) && ((result_tag.wb && vfu_rsp_ready_i) || vrf_wvalid_i);

        if (!is_fpu_busy)
          reduction_state_d = Reduction_Init;
      end

      Reduction_Init: begin
        // Initialize the reduction
        // verilator lint_off SELRANGE
        unique case (spatz_req.vtype.vsew)
          EW_8 : begin
            reduction_d[0] = $unsigned(vrf_rdata_i[0][7:0]);
            reduction_d[1] = $unsigned(vrf_rdata_i[1][8*reduction_pointer_q[idx_width(N_FU*ELENB)-1:0] +: 8]);
          end
          EW_16: begin
            reduction_d[0] = $unsigned(vrf_rdata_i[0][15:0]);
            reduction_d[1] = $unsigned(vrf_rdata_i[1][16*reduction_pointer_q[idx_width(N_FU*ELENB)-2:0] +: 16]);
          end
          EW_32: begin
            reduction_d[0] = $unsigned(vrf_rdata_i[0][31:0]);
            reduction_d[1] = $unsigned(vrf_rdata_i[1][32*reduction_pointer_q[idx_width(N_FU*ELENB)-3:0] +: 32]);
          end
          default: begin
          `ifdef MEMPOOL_SPATZ
            reduction_d = '0;
          `else
            if (MAXEW == EW_64) begin
              reduction_d[0] = $unsigned(vrf_rdata_i[0][63:0]);
              reduction_d[1] = $unsigned(vrf_rdata_i[1][64*reduction_pointer_q[idx_width(N_FU*ELENB)-4:0] +: 64]);
            end
          `endif
          end
        endcase
        // verilator lint_on SELRANGE

        if (vrf_rvalid_i[0] && vrf_rvalid_i[1]) begin
          automatic logic [idx_width(N_FU*ELENB)-1:0] pnt;

          reduction_operand_ready_d = 1'b1;
          reduction_pointer_d       = reduction_pointer_q + 1;
          reduction_state_d         = Reduction_Reduce;

          // Request next word
          pnt = reduction_pointer_d << int'(spatz_req.vtype.vsew);
          if (!(|pnt))
            word_issued = 1'b1;
        end
      end

      Reduction_Reduce: begin
        // Forward result
        // verilator lint_off SELRANGE
        unique case (spatz_req.vtype.vsew)
          EW_8 : begin
            reduction_d[0] = $unsigned(result[7:0]);
            reduction_d[1] = $unsigned(vrf_rdata_i[1][8*reduction_pointer_q[idx_width(N_FU*ELENB)-1:0] +: 8]);
          end
          EW_16: begin
            reduction_d[0] = $unsigned(result[15:0]);
            reduction_d[1] = $unsigned(vrf_rdata_i[1][16*reduction_pointer_q[idx_width(N_FU*ELENB)-2:0] +: 16]);
          end
          EW_32: begin
            reduction_d[0] = $unsigned(result[31:0]);
            reduction_d[1] = $unsigned(vrf_rdata_i[1][32*reduction_pointer_q[idx_width(N_FU*ELENB)-3:0] +: 32]);
          end
          default: begin
          `ifdef MEMPOOL_SPATZ
            reduction_d = '0;
          `else
            if (MAXEW == EW_64) begin
              reduction_d[0] = $unsigned(result[63:0]);
              reduction_d[1] = $unsigned(vrf_rdata_i[1][64*reduction_pointer_q[idx_width(N_FU*ELENB)-4:0] +: 64]);
            end
          `endif
          end
        endcase
        // verilator lint_on SELRANGE

        // Got a result!
        if (result_valid[0]) begin
          // Did we get an operand?
          if (vrf_rvalid_i[1]) begin
            automatic logic [idx_width(N_FU*ELENB)-1:0] pnt;

            // Bump pointer
            reduction_pointer_d = reduction_pointer_q + 1;

            // Acknowledge result
            result_ready = 1'b1;

            // Trigger a request
            reduction_operand_ready_d = 1'b1;

            // Request next word
            pnt = reduction_pointer_d << int'(spatz_req.vtype.vsew);
            if (!(|pnt))
              word_issued = 1'b1;
          end
        end

        // Are we done?
        if (reduction_pointer_q == spatz_req.vl) begin
          reduction_state_d         = Reduction_WriteBack;
          result_ready              = 1'b0;
          reduction_operand_ready_d = 1'b0;
        end
      end

      Reduction_WriteBack: begin
        // Acknowledge result
        if (vrf_wvalid_i) begin
          result_ready = 1'b1;

          // We are done with the reduction
          reduction_state_d = Reduction_NormalExecution;

          // Finish the reduction
          reduction_done = 1'b1;
        end
      end

      default;
    endcase
  end: proc_reduction

  ///////////////////////
  // Operand Requester //
  ///////////////////////

  vrf_be_t       vreg_wbe;
  logic          vreg_we;
  logic    [2:0] vreg_r_req;

  // Address register
  vrf_addr_t [2:0] vreg_addr_q, vreg_addr_d;
  `FF(vreg_addr_q, vreg_addr_d, '0)

  // Calculate new vector register address
  always_comb begin : vreg_addr_proc
    vreg_addr_d = vreg_addr_q;

    vrf_raddr_o = vreg_addr_d;
    vrf_waddr_o = result_tag.vd_addr;

    // Tag (propagated with the operations)
    input_tag = '{
      id             : spatz_req.id,
      vsew           : spatz_req.vtype.vsew,
      vstart         : spatz_req.vstart,
      vd_addr        : spatz_req.op_arith.is_scalar ? vrf_addr_t'(spatz_req.rd) : vreg_addr_q[2],
      wb             : spatz_req.op_arith.is_scalar,
      last           : last_request,
      narrowing      : spatz_req.op_arith.is_narrowing,
      narrowing_upper: narrowing_upper_q,
      reduction      : spatz_req.op_arith.is_reduction
    };

    if (spatz_req_valid && vl_q == '0) begin
      vreg_addr_d[0] = (spatz_req.vs2 + vstart) << $clog2(NrWordsPerVector);
      vreg_addr_d[1] = (spatz_req.vs1 + vstart) << $clog2(NrWordsPerVector);
      vreg_addr_d[2] = (spatz_req.vd + vstart) << $clog2(NrWordsPerVector);

      // Direct feedthrough
      vrf_raddr_o = vreg_addr_d;
      if (!spatz_req.op_arith.is_scalar)
        input_tag.vd_addr = vreg_addr_d[2];

      // Did we commit a word already?
      if (word_issued) begin
        vreg_addr_d[0] = vreg_addr_d[0] + (!spatz_req.op_arith.widen_vs2 || widening_upper_q);
        vreg_addr_d[1] = vreg_addr_d[1] + (!spatz_req.op_arith.widen_vs1 || widening_upper_q);
        vreg_addr_d[2] = vreg_addr_d[2] + (!spatz_req.op_arith.is_reduction && (!spatz_req.op_arith.is_narrowing || narrowing_upper_q));
      end
    end else if (spatz_req_valid && vl_q < spatz_req.vl && word_issued) begin
      vreg_addr_d[0] = vreg_addr_q[0] + (!spatz_req.op_arith.widen_vs2 || widening_upper_q);
      vreg_addr_d[1] = vreg_addr_q[1] + (!spatz_req.op_arith.widen_vs1 || widening_upper_q);
      vreg_addr_d[2] = vreg_addr_q[2] + (!spatz_req.op_arith.is_reduction && (!spatz_req.op_arith.is_narrowing || narrowing_upper_q));
    end
  end: vreg_addr_proc

  always_comb begin : operand_req_proc
    vreg_r_req = '0;
    vreg_we    = '0;
    vreg_wbe   = '0;

    if (spatz_req_valid && vl_q < spatz_req.vl)
      // Request operands
      vreg_r_req = {spatz_req.vd_is_src, spatz_req.use_vs1 && reduction_operand_request[1], spatz_req.use_vs2 && reduction_operand_request[0]};

    // Got a new result
    if (&(result_valid | ~pending_results) && !result_tag.reduction) begin
      vreg_we  = !result_tag.wb;
      vreg_wbe = '1;

      if (result_tag.narrowing) begin
        // Only write half of the elements
        vreg_wbe = result_tag.narrowing_upper ? {{(N_FU*ELENB/2){1'b1}}, {(N_FU*ELENB/2){1'b0}}} : {{(N_FU*ELENB/2){1'b0}}, {(N_FU*ELENB/2){1'b1}}};
      end
    end

    // Reduction finished execution
    if (reduction_state_q == Reduction_WriteBack && result_valid[0]) begin
      vreg_we = 1'b1;
      unique case (spatz_req.vtype.vsew)
        EW_8 : vreg_wbe = 1'h1;
        EW_16: vreg_wbe = 2'h3;
        EW_32: vreg_wbe = 4'hf;
        default: if (MAXEW == EW_64) vreg_wbe = 8'hff;
      endcase
    end
  end : operand_req_proc

  logic [N_FU*ELEN-1:0] vreg_wdata;
  always_comb begin: align_result
    vreg_wdata = result;

    // Realign results
    if (result_tag.narrowing) begin
      unique case (MAXEW)
        EW_64: begin
          if (RVD)
            for (int element = 0; element < N_FU; element++)
              vreg_wdata[32*element + (N_FU * ELEN * result_tag.narrowing_upper / 2) +: 32] = result[64*element +: 32];
        end
        EW_32: begin
          for (int element = 0; element < (MAXEW == EW_64 ? N_FU*2 : N_FU); element++)
            vreg_wdata[16*element + (N_FU * ELEN * result_tag.narrowing_upper / 2) +: 16] = result[32*element +: 16];
        end
        default:;
      endcase
    end
  end

  // Register file signals
  assign vrf_re_o    = vreg_r_req;
  assign vrf_we_o    = vreg_we;
  assign vrf_wbe_o   = vreg_wbe;
  assign vrf_wdata_o = vreg_wdata;
  assign vrf_id_o    = {result_tag.id, {3{spatz_req.id}}};

  //////////
  // IPUs //
  //////////

  // If there are fewer IPUs than FPUs, pipeline the execution of the integer instructions
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

  if (N_IPU < N_FU) begin: gen_pipeline_ipu
    logic [N_FU*ELEN-1:0] ipu_result_d, ipu_result_q;
    logic [N_FU*ELENB-1:0] ipu_result_valid_q, ipu_result_valid_d;
    logic [idx_width(N_FU/N_IPU)-1:0] ipu_result_pnt_d, ipu_result_pnt_q;
    vfu_tag_t ipu_result_tag_d, ipu_result_tag_q;
    logic [idx_width(N_FU/N_IPU)-1:0] ipu_operand_pnt_d, ipu_operand_pnt_q;

    `FF(ipu_result_q, ipu_result_d, '0)
    `FF(ipu_result_valid_q, ipu_result_valid_d, '0)
    `FF(ipu_result_pnt_q, ipu_result_pnt_d, '0)
    `FF(ipu_result_tag_q, ipu_result_tag_d, '0)
    `FF(ipu_operand_pnt_q, ipu_operand_pnt_d, '0)

    always_comb begin
      // Maintain state
      ipu_result_d       = ipu_result_q;
      ipu_result_valid_d = ipu_result_valid_q;
      ipu_result_pnt_d   = ipu_result_pnt_q;
      ipu_operand_pnt_d  = ipu_operand_pnt_q;
      ipu_result_tag_d   = ipu_result_tag_q;

      // Send operands
      ipu_in_ready     = 1'b0;
      int_ipu_operand1 = ipu_wide_operand1[ipu_operand_pnt_q*ELEN*N_IPU +: ELEN*N_IPU];
      int_ipu_operand2 = ipu_wide_operand2[ipu_operand_pnt_q*ELEN*N_IPU +: ELEN*N_IPU];
      int_ipu_operand3 = ipu_wide_operand3[ipu_operand_pnt_q*ELEN*N_IPU +: ELEN*N_IPU];
      if (spatz_req_valid && operands_ready && &int_ipu_in_ready && !is_fpu_insn) begin
        ipu_operand_pnt_d = ipu_operand_pnt_q + 1;
        if (ipu_operand_pnt_d == '0 || !(&valid_operations[ipu_operand_pnt_d*ELENB*N_IPU +: ELENB*N_IPU]))
          ipu_operand_pnt_d = '0;

        // Issued all elements
        if (ipu_operand_pnt_d == 0)
          ipu_in_ready = '1;
      end

      // Clean-up results
      if (result_ready) begin
        ipu_result_d       = '0;
        ipu_result_valid_d = '0;
        ipu_result_tag_d   = '0;
      end

      // Store results
      int_ipu_result_ready = '0;
      if (&int_ipu_result_valid) begin
        ipu_result_d[ipu_result_pnt_q*ELEN*N_IPU +: ELEN*N_IPU]         = int_ipu_result;
        ipu_result_valid_d[ipu_result_pnt_q*ELENB*N_IPU +: ELENB*N_IPU] = int_ipu_result_valid;
        ipu_result_tag_d                                                = int_ipu_result_tag[0];
        ipu_result_pnt_d                                                = ipu_result_pnt_q + 1;
        int_ipu_result_ready                                            = 1'b1;

        // Scalar operation
        if (ipu_result_tag_d.wb || spatz_req.op_arith.is_reduction)
          ipu_result_pnt_d = '0;
      end
    end

    // Forward results
    assign ipu_result       = ipu_result_q;
    assign ipu_result_valid = ipu_result_valid_q;
    assign ipu_result_tag   = ipu_result_tag_q;
  end: gen_pipeline_ipu else begin: gen_no_pipeline_ipu
    assign ipu_in_ready         = int_ipu_in_ready;
    assign int_ipu_operand1     = ipu_wide_operand1;
    assign int_ipu_operand2     = ipu_wide_operand2;
    assign int_ipu_operand3     = ipu_wide_operand3;
    assign ipu_result           = int_ipu_result;
    assign ipu_result_valid     = int_ipu_result_valid;
    assign int_ipu_result_ready = result_ready;
    assign ipu_result_tag       = int_ipu_result_tag[0];
  end

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
    `FF(fpu_busy_q, fpu_busy_d, '0)

    status_t [N_FPU-1:0] fpu_status_d, fpu_status_q;
    `FF(fpu_status_q, fpu_status_d, '0)

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

      `FFL(fpu_operand1_q, fpu_operand1, int_fpu_in_valid && int_fpu_in_ready, '0)
      `FFL(fpu_operand2_q, fpu_operand2, int_fpu_in_valid && int_fpu_in_ready, '0)
      `FFL(fpu_operand3_q, fpu_operand3, int_fpu_in_valid && int_fpu_in_ready, '0)
      `FFL(fpu_op_q, fpu_op, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::FMADD)
      `FFL(fpu_src_fmt_q, fpu_src_fmt, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::FP32)
      `FFL(fpu_dst_fmt_q, fpu_dst_fmt, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::FP32)
      `FFL(fpu_int_fmt_q, fpu_int_fmt, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::INT8)
      `FFL(fpu_op_mode_q, fpu_op_mode, int_fpu_in_valid && int_fpu_in_ready, 1'b0)
      `FFL(fpu_vectorial_op_q, fpu_vectorial_op, int_fpu_in_valid && int_fpu_in_ready, 1'b0)
      `FFL(rm_q, spatz_req.rm, int_fpu_in_valid && int_fpu_in_ready, fpnew_pkg::RNE)
      `FFL(input_tag_q, input_tag, int_fpu_in_valid && int_fpu_in_ready, '{vsew: EW_8, default: '0})
      `FFL(fpu_in_valid_q, int_fpu_in_valid, int_fpu_in_ready, 1'b0)
      assign int_fpu_in_ready = !fpu_in_valid_q || fpu_in_valid_q && fpu_in_ready_d;

      fpnew_top #(
        .Features                   (FPUFeatures           ),
        .Implementation             (FPUImplementation     ),
        .TagType                    (vfu_tag_t             ),
        .StochasticRndImplementation(fpnew_pkg::DEFAULT_RSR)
      ) i_fpu (
        .clk_i         (clk_i                                                  ),
        .rst_ni        (rst_ni                                                 ),
        .hart_id_i     ({hart_id_i[31-$clog2(N_FPU):0], fpu[$clog2(N_FPU)-1:0]}),
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

endmodule : spatz_vfu
