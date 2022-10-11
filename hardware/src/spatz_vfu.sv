// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// The Vector Functional Unit (VFU) executes all arithmetic and logical
// vector instructions. It can be configured with a parameterizable amount
// of IPUs that work in parallel.

module spatz_vfu import spatz_pkg::*; import rvv_pkg::*; import cf_math_pkg::idx_width;
  import fpnew_pkg::roundmode_e; import fpnew_pkg::status_t; (
    input  logic             clk_i,
    input  logic             rst_ni,
    // Spatz req
    input  spatz_req_t       spatz_req_i,
    input  logic             spatz_req_valid_i,
    output logic             spatz_req_ready_o,
    // VFU response
    output logic             vfu_rsp_valid_o,
    input  logic             vfu_rsp_ready_i,
    output vfu_rsp_t         vfu_rsp_o,
    // VRF
    output vreg_addr_t       vrf_waddr_o,
    output vreg_data_t       vrf_wdata_o,
    output logic             vrf_we_o,
    output vreg_be_t         vrf_wbe_o,
    input  logic             vrf_wvalid_i,
    output spatz_id_t  [3:0] vrf_id_o,
    output vreg_addr_t [2:0] vrf_raddr_o,
    output logic       [2:0] vrf_re_o,
    input  vreg_data_t [2:0] vrf_rdata_i,
    input  logic       [2:0] vrf_rvalid_i,
    // FPU side channel
    input  roundmode_e       fpu_rnd_mode_i,
    output status_t          fpu_status_o
  );

// Include FF
`include "common_cells/registers.svh"

  // Instruction tag (propagated together with the operands through the pipelines)
  typedef struct packed {
    spatz_id_t id;

    vew_e vsew;
    vlen_t vstart;
    logic [idx_width(N_IPU*4):0] vl;

    logic [GPRWidth-1:0] rd;
    logic wb;
    logic last;
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
  logic [$clog2(N_IPU*4):0] nr_elem_word;
  assign nr_elem_word = N_IPU * (1 << (EW_32 - spatz_req.vtype.vsew));

  // Did we reach the last elements of the instruction?
  logic last_word;
  assign last_word = spatz_req.vl <= vlen_t'(nr_elem_word);

  // Are we running integer or floating-point instructions?
  enum logic {
    VFU_RunningIPU, VFU_RunningFPU
  } state_d, state_q;
  `FF(state_q, state_d, VFU_RunningIPU)

  // Propagate the tags through the functional units
  vfu_tag_t ipu_tag, fpu_tag, tag;
  vfu_tag_t op_tag;

  assign tag    = state_q == VFU_RunningIPU ? ipu_tag : fpu_tag;
  assign op_tag = '{
    id    : spatz_req.id,
    rd    : spatz_req.rd,
    wb    : spatz_req.op_arith.is_scalar,
    last  : last_word,
    vsew  : spatz_req.vtype.vsew,
    vstart: spatz_req.vstart,
    vl    : vl_q[idx_width(N_IPU*4):0]
  };

  // Number of words advanced by vstart
  vlen_t vstart;
  assign vstart = ((tag.vstart / N_IPU) >> (EW_32 - tag.vsew)) << (EW_32 - tag.vsew);

  // Should we stall?
  logic stall;

  // Are the VFU operands ready?
  logic op1_is_ready, op2_is_ready, op3_is_ready, operands_ready;
  assign op1_is_ready   = spatz_req_valid && (!spatz_req.use_vs1 || vrf_rvalid_i[1]);
  assign op2_is_ready   = spatz_req_valid && (!spatz_req.use_vs2 || vrf_rvalid_i[0]);
  assign op3_is_ready   = spatz_req_valid && (!spatz_req.vd_is_src || vrf_rvalid_i[2]);
  assign operands_ready = op1_is_ready && op2_is_ready && op3_is_ready && (!spatz_req.op_arith.is_scalar || vfu_rsp_ready_i) && !stall;

  // Valid operations
  logic [N_IPU*ELENB-1:0] valid_operations;
  assign valid_operations = spatz_req.op_arith.is_scalar ? 4'hf : '1;

  // Pending results
  logic [N_IPU*ELENB-1:0] pending_results;
  assign pending_results = tag.wb ? 4'hf : '1;

  // Did we commit a word to the VRF?
  logic word_issued;

  // Currently running instructions
  logic [NrParallelInstructions-1:0] running_d, running_q;
  `FF(running_q, running_d, '0)

  // Is this a FPU instruction
  logic is_fpu_insn;
  assign is_fpu_insn = FPU_EN && spatz_req.op inside {[VFADD:VFNMADD]};

  // Is the FPU busy?
  logic is_fpu_busy;

  // Scalar results (sent back to Snitch)
  elen_t scalar_result;

  // Are any results valid?
  logic [N_IPU*ELENB-1:0] result_valid;

  always_comb begin: control_proc
    // Maintain state
    vl_d      = vl_q;
    busy_d    = busy_q;
    running_d = running_q;
    state_d   = state_q;

    // We are not stalling
    stall = 1'b0;

    // We are handling an instruction
    spatz_req_ready = 1'b0;

    // Do not ack anything
    vfu_rsp_valid_o = 1'b0;
    vfu_rsp_o       = '0;

    // Change number of remaining elements
    if (word_issued)
      vl_d = vl_q + nr_elem_word;

    // Current state of the VFU
    if (spatz_req_valid)
      unique case (state_q)
        VFU_RunningIPU: begin
          // Go to the FPU state directly
          if (is_fpu_insn) begin
            state_d = VFU_RunningFPU;
            stall   = 1'b1;
          end
        end
        VFU_RunningFPU: begin
          // Only go back to the FPU state once the FPUs are no longer busy
          if (!is_fpu_insn)
            if (is_fpu_busy)
              stall = 1'b1;
            else
              state_d = VFU_RunningIPU;
        end
      endcase

    // Finished the execution!
    if (spatz_req_valid && vl_d >= spatz_req.vl) begin
      spatz_req_ready         = spatz_req_valid;
      busy_d                  = 1'b0;
      vl_d                    = '0;
      running_d[spatz_req.id] = 1'b0;

      // Is this an IPU instruction? If so, acknowledge directly
      if (!FPU_EN || state_q == VFU_RunningIPU) begin
        vfu_rsp_o.id     = spatz_req.id;
        vfu_rsp_o.rd     = spatz_req.rd;
        vfu_rsp_o.wb     = spatz_req.op_arith.is_scalar;
        vfu_rsp_o.result = scalar_result;
        vfu_rsp_valid_o  = 1'b1;
      end
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

    // An FPU instruction finished execution
    if (FPU_EN && state_q == VFU_RunningFPU && tag.last) begin
      vfu_rsp_o.id     = tag.id;
      vfu_rsp_o.rd     = tag.rd;
      vfu_rsp_o.wb     = tag.wb;
      vfu_rsp_o.result = scalar_result;
      vfu_rsp_valid_o  = 1'b1;
    end
  end: control_proc

  //////////////
  // Operands //
  //////////////

  // IPU results
  logic [N_IPU*ELEN-1:0]  ipu_result;
  logic [N_IPU*ELENB-1:0] ipu_result_valid;
  logic [N_IPU*ELENB-1:0] ipu_in_ready;

  // FPU results
  logic [N_IPU*ELEN-1:0]  fpu_result;
  logic [N_IPU*ELENB-1:0] fpu_result_valid;
  logic [N_IPU*ELENB-1:0] fpu_in_ready;

  // Operands and result signals
  logic [N_IPU*ELEN-1:0]  operand1, operand2, operand3;
  logic [N_IPU*ELENB-1:0] in_ready;
  logic [N_IPU*ELEN-1:0]  result;
  logic                   result_ready;
  always_comb begin: operand_proc
    if (spatz_req.op_arith.is_scalar)
      operand1 = {1*N_IPU{spatz_req.rs1}};
    else if (spatz_req.use_vs1)
      operand1 = vrf_rdata_i[1];
    else begin
      // Replicate scalar operands
      unique case (spatz_req.vtype.vsew)
        EW_8 : operand1   = {4*N_IPU{spatz_req.rs1[7:0]}};
        EW_16: operand1   = {2*N_IPU{spatz_req.rs1[15:0]}};
        default: operand1 = {1*N_IPU{spatz_req.rs1}};
      endcase
    end
    operand2 = spatz_req.op_arith.is_scalar ? {1*N_IPU{spatz_req.rs2}} : vrf_rdata_i[0];
    operand3 = spatz_req.op_arith.is_scalar ? {1*N_IPU{spatz_req.rsd}} : vrf_rdata_i[2];
  end: operand_proc

  assign in_ready     = state_q == VFU_RunningIPU ? ipu_in_ready     : fpu_in_ready;
  assign result       = state_q == VFU_RunningIPU ? ipu_result       : fpu_result;
  assign result_valid = state_q == VFU_RunningIPU ? ipu_result_valid : fpu_result_valid;

  assign scalar_result = spatz_req.op_arith.is_scalar ? result[ELEN-1:0] : '0;
  assign result_ready  = &(result_valid | ~pending_results);

  // Did we issue a word to the IPUs?
  assign word_issued = spatz_req_valid && &(in_ready | ~valid_operations) && !stall;

  ///////////////////////
  // Operand Requester //
  ///////////////////////

  vreg_be_t       vreg_wbe;
  logic           vreg_we;
  logic     [2:0] vreg_r_req;

  // Address register
  vreg_addr_t [2:0] vreg_addr_q, vreg_addr_d;
  `FF(vreg_addr_q, vreg_addr_d, '0)

  // Calculate new vector register address
  always_comb begin : vreg_addr_proc
    vreg_addr_d = vreg_addr_q;

    vrf_raddr_o = vreg_addr_d;
    vrf_waddr_o = vreg_addr_d[2];

    if (spatz_req_valid && vl_q == '0) begin
      vreg_addr_d[0] = (spatz_req.vs2 + vstart) << $clog2(NrWordsPerVector);
      vreg_addr_d[1] = (spatz_req.vs1 + vstart) << $clog2(NrWordsPerVector);
      vreg_addr_d[2] = (spatz_req.vd + vstart) << $clog2(NrWordsPerVector);

      // Direct feedthrough
      vrf_raddr_o = vreg_addr_d;
      vrf_waddr_o = vreg_addr_d[2];

      // Did we commit a word already?
      if (word_issued) begin
        vreg_addr_d[0] = vreg_addr_d[0] + 1;
        vreg_addr_d[1] = vreg_addr_d[1] + 1;
        vreg_addr_d[2] = vreg_addr_d[2] + 1;
      end
    end else if (spatz_req_valid && vl_q < spatz_req.vl && word_issued) begin
      vreg_addr_d[0] = vreg_addr_q[0] + 1;
      vreg_addr_d[1] = vreg_addr_q[1] + 1;
      vreg_addr_d[2] = vreg_addr_q[2] + 1;
    end
  end: vreg_addr_proc

  always_comb begin : operand_req_proc
    vreg_r_req = '0;
    vreg_we    = '0;
    vreg_wbe   = '0;

    if (spatz_req_valid && vl_q < spatz_req.vl)
      // Request operands
      vreg_r_req = {spatz_req.vd_is_src, spatz_req.use_vs1, spatz_req.use_vs2};

    // Got a new result
    if (&(result_valid | ~pending_results)) begin
      vreg_we  = !tag.wb;
      vreg_wbe = '1;

      // If we are in the last group or at the start and vstart is nonzero,
      // create the byte enable (be) mask for write back to register file.
      if (spatz_req.vstart != '0 && busy_q == 1'b0)
        unique case (spatz_req.vtype.vsew)
          EW_8 : vreg_wbe   = ~(vreg_be_t'('1) >> ((N_IPU * (1 << (EW_32 - tag.vsew))) - tag.vstart[idx_width(N_IPU * 4)-1:0]));
          EW_16: vreg_wbe   = ~(vreg_be_t'('1) >> ((N_IPU * (1 << (EW_32 - tag.vsew))) - tag.vstart[idx_width(N_IPU * 2)-1:0]));
          default: vreg_wbe = ~(vreg_be_t'('1) >> ((N_IPU * (1 << (EW_32 - tag.vsew))) - tag.vstart[idx_width(N_IPU * 1)-1:0]));
        endcase
      else
        if (last_word)
          unique case (tag.vsew)
            EW_8 : vreg_wbe   = vreg_be_t'('1) >> tag.vl[idx_width(N_IPU * 4):0];
            EW_16: vreg_wbe   = vreg_be_t'('1) >> tag.vl[idx_width(N_IPU * 2):0];
            default: vreg_wbe = vreg_be_t'('1) >> tag.vl[idx_width(N_IPU * 1):0];
          endcase
    end
  end : operand_req_proc

  // Register file signals
  assign vrf_re_o    = vreg_r_req;
  assign vrf_we_o    = vreg_we;
  assign vrf_wbe_o   = vreg_wbe;
  assign vrf_wdata_o = result;
  assign vrf_id_o    = {4{spatz_req.id}};

  //////////
  // IPUs //
  //////////

  for (genvar ipu = 0; unsigned'(ipu) < N_IPU; ipu++) begin : gen_ipus
    vfu_tag_t int_ipu_tag;

    spatz_ipu #(
      .tag_t(vfu_tag_t)
    ) i_ipu (
      .clk_i            (clk_i                                                                                           ),
      .rst_ni           (rst_ni                                                                                          ),
      .operation_i      (spatz_req.op                                                                                    ),
      // Only the IPU0 executes scalar instructions
      .operation_valid_i(spatz_req_valid && operands_ready && (!spatz_req.op_arith.is_scalar || ipu == 0) && !is_fpu_insn),
      .op_s1_i          (operand1[ipu*ELEN +: ELEN]                                                                      ),
      .op_s2_i          (operand2[ipu*ELEN +: ELEN]                                                                      ),
      .op_d_i           (operand3[ipu*ELEN +: ELEN]                                                                      ),
      .tag_i            (op_tag                                                                                          ),
      .carry_i          ('0                                                                                              ),
      .sew_i            (spatz_req.vtype.vsew                                                                            ),
      .be_o             (/* Unused */                                                                                    ),
      .result_o         (ipu_result[ipu*ELEN +: ELEN]                                                                    ),
      .result_valid_o   (ipu_result_valid[ipu*ELENB +: ELENB]                                                            ),
      .result_ready_i   (result_ready                                                                                    ),
      .tag_o            (int_ipu_tag                                                                                     )
    );

    assign ipu_in_ready[ipu*ELENB +: ELENB] = '1;
    if (ipu == 0) begin: gen_ipu_tag
      assign ipu_tag = int_ipu_tag;
    end: gen_ipu_tag
  end : gen_ipus

  ////////////
  //  FPUs  //
  ////////////

  fpnew_pkg::operation_e fpu_op;
  fpnew_pkg::fp_format_e fpu_src_fmt, fpu_dst_fmt;
  fpnew_pkg::int_format_e fpu_int_fmt;
  fpnew_pkg::roundmode_e fpu_rnd_mode;
  logic fpu_op_mode;
  logic fpu_vectorial_op;

  logic [N_IPU-1:0] fpu_busy_d, fpu_busy_q;
  `FF(fpu_busy_q, fpu_busy_d, '0)

  status_t [N_IPU-1:0] fpu_status;

  always_comb begin: gen_fpu_decoder
    fpu_op           = fpnew_pkg::FMADD;
    fpu_src_fmt      = fpnew_pkg::FP32;
    fpu_dst_fmt      = fpnew_pkg::FP32;
    fpu_int_fmt      = fpnew_pkg::INT32;
    fpu_rnd_mode     = fpu_rnd_mode_i;
    fpu_op_mode      = 1'b0;
    fpu_vectorial_op = 1'b0;
    is_fpu_busy      = |fpu_busy_q;

    fpu_status_o = '0;
    for (int fpu = 0; fpu < N_IPU; fpu++)
      fpu_status_o |= fpu_status;

    if (FPU_EN) begin
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

        VFDIV : fpu_op = fpnew_pkg::DIV;
        VFSQRT: fpu_op = fpnew_pkg::SQRT;

        VFMIN: begin
          fpu_op       = fpnew_pkg::MINMAX;
          fpu_rnd_mode = fpnew_pkg::RNE;
        end
        VFMAX: begin
          fpu_op       = fpnew_pkg::MINMAX;
          fpu_rnd_mode = fpnew_pkg::RTZ;
        end

        VFSGNJ: begin
          fpu_op       = fpnew_pkg::SGNJ;
          fpu_rnd_mode = fpnew_pkg::RNE;
        end
        VFSGNJN: begin
          fpu_op       = fpnew_pkg::SGNJ;
          fpu_rnd_mode = fpnew_pkg::RTZ;
        end
        VFSGNJX: begin
          fpu_op       = fpnew_pkg::SGNJ;
          fpu_rnd_mode = fpnew_pkg::RDN;
        end

        VFCLASS: fpu_op = fpnew_pkg::CLASSIFY;
        VFLE   : begin
          fpu_op       = fpnew_pkg::CMP;
          fpu_rnd_mode = fpnew_pkg::RNE;
        end
        VFLT : begin
          fpu_op       = fpnew_pkg::CMP;
          fpu_rnd_mode = fpnew_pkg::RTZ;
        end
        VFEQ : begin
          fpu_op       = fpnew_pkg::CMP;
          fpu_rnd_mode = fpnew_pkg::RDN;
        end

        VF2I: begin
          fpu_op       = fpnew_pkg::F2I;
          fpu_rnd_mode = spatz_req.rm;
        end
        VF2U: begin
          fpu_op       = fpnew_pkg::F2I;
          fpu_op_mode  = 1'b1;
          fpu_rnd_mode = spatz_req.rm;
        end
        VI2F: begin
          fpu_op       = fpnew_pkg::I2F;
          fpu_rnd_mode = spatz_req.rm;
        end
        VU2F: begin
          fpu_op       = fpnew_pkg::I2F;
          fpu_op_mode  = 1'b1;
          fpu_rnd_mode = spatz_req.rm;
        end

        default:;
      endcase
    end
  end: gen_fpu_decoder

  for (genvar fpu = 0; unsigned'(fpu) < N_IPU; fpu++) begin : gen_fpus
    if (FPU_EN) begin: gen_fpu
      logic int_fpu_result_valid;
      logic int_fpu_in_ready;
      vfu_tag_t int_fpu_tag;

      assign fpu_in_ready[fpu*ELENB +: ELENB]     = {ELENB{int_fpu_in_ready}};
      assign fpu_result_valid[fpu*ELENB +: ELENB] = {ELENB{int_fpu_result_valid}};

      fpnew_top #(
        .Features      (FPUFeatures      ),
        .Implementation(FPUImplementation),
        .TagType       (vfu_tag_t        )
      ) i_fpu (
        .clk_i         (clk_i                                                                                          ),
        .rst_ni        (rst_ni                                                                                         ),
        .flush_i       (1'b0                                                                                           ),
        .busy_o        (fpu_busy_d[fpu]                                                                                ),
        .operands_i    ({operand3[fpu*ELEN +: ELEN], operand2[fpu*ELEN +: ELEN], operand1[fpu*ELEN +: ELEN]}           ),
        // Only the FPU0 executes scalar instructions
        .in_valid_i    (spatz_req_valid && operands_ready && (!spatz_req.op_arith.is_scalar || fpu == 0) && is_fpu_insn),
        .in_ready_o    (int_fpu_in_ready                                                                               ),
        .op_i          (fpu_op                                                                                         ),
        .src_fmt_i     (fpu_src_fmt                                                                                    ),
        .dst_fmt_i     (fpu_dst_fmt                                                                                    ),
        .int_fmt_i     (fpu_int_fmt                                                                                    ),
        .vectorial_op_i(fpu_vectorial_op                                                                               ),
        .op_mod_i      (fpu_op_mode                                                                                    ),
        .tag_i         (op_tag                                                                                         ),
        .rnd_mode_i    (fpu_rnd_mode                                                                                   ),
        .result_o      (fpu_result[fpu*ELEN +: ELEN]                                                                   ),
        .out_valid_o   (int_fpu_result_valid                                                                           ),
        .out_ready_i   (result_ready                                                                                   ),
        .status_o      (fpu_status[fpu]                                                                                ),
        .tag_o         (int_fpu_tag                                                                                    )
      );

      if (fpu == 0) begin: gen_fpu_tag
        assign fpu_tag = int_fpu_tag;
      end: gen_fpu_tag
    end: gen_fpu else begin: gen_no_fpu
      assign fpu_status[fpu]                      = '0;
      assign fpu_busy_d[fpu]                      = 1'b0;
      assign fpu_in_ready[fpu*ELENB +: ELENB]     = '0;
      assign fpu_result[fpu*ELEN +: ELEN]         = '0;
      assign fpu_result_valid[fpu*ELENB +: ELENB] = '0;
      if (fpu == 0) begin: gen_fpu_tag
        assign fpu_tag = '0;
      end: gen_fpu_tag
    end: gen_no_fpu
  end : gen_fpus

endmodule : spatz_vfu
