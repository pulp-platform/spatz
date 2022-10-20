// Copyright 2022 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich

module spatz_fpu_sequencer import spatz_pkg::*; import rvv_pkg::*; import fpnew_pkg::*; #(
    parameter type x_issue_req_t  = logic,
    parameter type x_issue_resp_t = logic,
    parameter type x_result_t     = logic,

    parameter int unsigned AddrWidth           = 32,
    parameter int unsigned DataWidth           = FLEN,
    parameter int unsigned NumOutstandingLoads = 1,

    // Dependent parameters. DO NOT CHANGE.
    localparam int unsigned IdWidth   = cf_math_pkg::idx_width(NumOutstandingLoads),
    localparam int unsigned StrbWidth = DataWidth/8,
    localparam type addr_t            = logic [AddrWidth-1:0],
    localparam type data_t            = logic [DataWidth-1:0],
    localparam type strb_t            = logic [StrbWidth-1:0]
  ) (
    input  logic          clk_i,
    input  logic          rst_ni,
    // Snitch interface
    // X-Interface request
    input  logic          x_issue_valid_i,
    output logic          x_issue_ready_o,
    input  x_issue_req_t  x_issue_req_i,
    output x_issue_resp_t x_issue_resp_o,
    // X-Interface result
    output logic          x_result_valid_o,
    input  logic          x_result_ready_i,
    output x_result_t     x_result_o,
    // Spatz interface
    // X-Interface request
    output logic          x_issue_valid_o,
    input  logic          x_issue_ready_i,
    output x_issue_req_t  x_issue_req_o,
    input  x_issue_resp_t x_issue_resp_i,
    // X-Interface result
    input  logic          x_result_valid_i,
    output logic          x_result_ready_o,
    input  x_result_t     x_result_i,
    // Memory interface
    output addr_t              data_qaddr_o,
    output logic               data_qwrite_o,
    output logic [3:0]         data_qamo_o,
    output data_t              data_qdata_o,
    output strb_t              data_qstrb_o,
    output logic [IdWidth-1:0] data_qid_o,
    output logic               data_qvalid_o,
    input  logic               data_qready_i,
    input  data_t              data_pdata_i,
    input  logic               data_perror_i,
    input  logic [IdWidth-1:0] data_pid_i,
    input  logic               data_pvalid_i,
    output logic               data_pready_o
  );

  `include "common_cells/registers.svh"

  if (!FPU) begin: gen_no_fpu
    // Spatz configured without an FPU. Just forward the requests to Spatz.
    assign x_issue_req_o   = x_issue_req_i;
    assign x_issue_valid_o = x_issue_valid_i;
    assign x_issue_ready_o = x_issue_ready_i;
    assign x_issue_resp_o  = x_issue_resp_i;

    assign x_result_o       = x_result_i;
    assign x_result_valid_o = x_result_valid_i;
    assign x_result_ready_o = x_result_ready_i;

    // Tie the memory interface to zero
    assign data_qaddr_o  = '0;
    assign data_qwrite_o = 1'b0;
    assign data_qamo_o   = '0;
    assign data_qdata_o  = '0;
    assign data_qstrb_o  = '0;
    assign data_qid_o    = 1'b0;
    assign data_qvalid_o = 1'b0;
    assign data_pready_o = 1'b0;
  end: gen_no_fpu else begin: gen_fpr
    // Capture requests that read and write to the scalar floating-point register file

    /////////////////
    //  Addresses  //
    /////////////////

    localparam int unsigned NrFPReg     = 32;
    localparam int unsigned FPRRegWidth = cf_math_pkg::idx_width(NrFPReg);

    logic [FPRRegWidth-1:0] fd, fs1, fs2, fs3;
    assign fd  = x_issue_req_i.instr[7 + FPRRegWidth - 1:7];
    assign fs1 = x_issue_req_i.instr[15 + FPRRegWidth - 1:15];
    assign fs2 = x_issue_req_i.instr[20 + FPRRegWidth - 1:20];
    assign fs3 = x_issue_req_i.instr[27 + FPRRegWidth - 1:27];

    /////////////////////
    //  Register File  //
    /////////////////////

    localparam int unsigned NrReadPorts  = 3;
    localparam int unsigned NrWritePorts = 1;

    logic [NrReadPorts-1:0][FPRRegWidth-1:0] fpr_raddr;
    logic [NrReadPorts-1:0][FLEN-1:0] fpr_rdata;
    logic [NrWritePorts-1:0][FPRRegWidth-1:0] fpr_waddr;
    logic [NrWritePorts-1:0][FLEN-1:0] fpr_wdata;
    logic [NrWritePorts-1:0] fpr_we;

    assign fpr_raddr = {fs3, fs2, fs1};

    snitch_regfile #(
      .DATA_WIDTH    (FLEN        ),
      .ADDR_WIDTH    (FPRRegWidth ),
      .NR_READ_PORTS (NrReadPorts ),
      .NR_WRITE_PORTS(NrWritePorts),
      .ZERO_REG_ZERO (0           )
    ) i_fp_regfile (
      .clk_i  (clk_i    ),
      .raddr_i(fpr_raddr),
      .rdata_o(fpr_rdata),
      .waddr_i(fpr_waddr),
      .wdata_i(fpr_wdata),
      .we_i   (fpr_we   )
    );

    //////////////////
    //  Scoreboard  //
    //////////////////

    // Scoreboard
    logic [NrFPReg-1:0] sb_d, sb_q;
    `FF(sb_q, sb_d, '0)

    // Are we using the registers?
    logic use_fs1, use_fs2, use_fs3, use_fd;

    // Stalling?
    logic stall;
    logic lsu_stall;
    assign stall = lsu_stall || (use_fs1 && sb_q[fs1]) || (use_fs2 && sb_q[fs2]) || (use_fs3 && sb_q[fs3]);

    // Illegal instruction
    logic illegal_inst;

    // Are we retiring something?
    logic retire;

    always_comb begin
      // Maintain state
      sb_q = sb_d;
      if (use_fd && !stall)
        sb_d[fd] = 1'b1;
      if (retire)
        sb_d[fpr_waddr[0]] = 1'b0;
    end

    ///////////////////////////
    //  Instruction decoder  //
    ///////////////////////////

    enum logic [1:0] {
      Byte     = 2'b00,
      HalfWord = 2'b01,
      Word     = 2'b10,
      Double   = 2'b11
    } ls_size;

    logic is_load;
    logic is_store;

    always_comb begin: decoder
      // We are not reading any operands
      use_fs1 = 1'b0;
      use_fs2 = 1'b0;
      use_fs3 = 1'b0;
      use_fd  = 1'b0;

      // Default element size
      ls_size  = Word;
      is_load  = 1'b0;
      is_store = 1'b0;

      // Not an illegal instruction
      illegal_inst = 1'b0;

      if (x_issue_valid_i)
        unique casez (x_issue_req_i.instr)
          // Single Precision Floating-Point
          riscv_instr::FADD_S,
          riscv_instr::FSUB_S,
          riscv_instr::FMUL_S,
          riscv_instr::FDIV_S,
          riscv_instr::FSQRT_S,
          riscv_instr::FSGNJ_S,
          riscv_instr::FSGNJN_S,
          riscv_instr::FSGNJX_S,
          riscv_instr::FMIN_S,
          riscv_instr::FMAX_S,
          riscv_instr::FCLASS_S,
          riscv_instr::FLE_S,
          riscv_instr::FLT_S,
          riscv_instr::FEQ_S,
          riscv_instr::FCVT_S_W,
          riscv_instr::FCVT_S_WU,
          riscv_instr::FCVT_W_S,
          riscv_instr::FCVT_WU_S,
          riscv_instr::FMADD_S,
          riscv_instr::FMSUB_S,
          riscv_instr::FNMSUB_S,
          riscv_instr::FNMADD_S: begin
            if (RVF && (!(x_issue_req_i.instr inside {riscv_instr::FDIV_S, riscv_instr::FSQRT_S}) || FDivSqrt)) begin
              use_fs1 = !(x_issue_req_i.instr inside {riscv_instr::FCVT_S_W, riscv_instr::FCVT_S_WU});
              use_fs2 = 1'b1;
              use_fs3 = x_issue_req_i.instr inside {riscv_instr::FMADD_S, riscv_instr::FMSUB_S, riscv_instr::FNMSUB_S, riscv_instr::FNMADD_S};
              use_fd  = !(x_issue_req_i.instr inside {riscv_instr::FCLASS_S, riscv_instr::FLE_S, riscv_instr::FLT_S, riscv_instr::FEQ_S, riscv_instr::FCVT_W_S, riscv_instr::FCVT_WU_S});
            end else begin
              illegal_inst = 1'b1;
            end
          end

          // Double Precision Floating-Point
          riscv_instr::FADD_D,
          riscv_instr::FSUB_D,
          riscv_instr::FMUL_D,
          riscv_instr::FDIV_D,
          riscv_instr::FSQRT_D,
          riscv_instr::FSGNJ_D,
          riscv_instr::FSGNJN_D,
          riscv_instr::FSGNJX_D,
          riscv_instr::FMIN_D,
          riscv_instr::FMAX_D,
          riscv_instr::FCLASS_D,
          riscv_instr::FLE_D,
          riscv_instr::FLT_D,
          riscv_instr::FEQ_D,
          riscv_instr::FCVT_D_W,
          riscv_instr::FCVT_D_WU,
          riscv_instr::FCVT_W_D,
          riscv_instr::FCVT_WU_D,
          riscv_instr::FMADD_D,
          riscv_instr::FMSUB_D,
          riscv_instr::FNMSUB_D,
          riscv_instr::FNMADD_D: begin
            if (RVD && (!(x_issue_req_i.instr inside {riscv_instr::FDIV_D, riscv_instr::FSQRT_D}) || FDivSqrt)) begin
              use_fs1 = !(x_issue_req_i.instr inside {riscv_instr::FCVT_D_W, riscv_instr::FCVT_D_WU});
              use_fs2 = 1'b1;
              use_fs3 = x_issue_req_i.instr inside {riscv_instr::FMADD_D, riscv_instr::FMSUB_D, riscv_instr::FNMSUB_D, riscv_instr::FNMADD_D};
              use_fd  = !(x_issue_req_i.instr inside {riscv_instr::FCLASS_D, riscv_instr::FLE_D, riscv_instr::FLT_D, riscv_instr::FEQ_D, riscv_instr::FCVT_W_D, riscv_instr::FCVT_WU_D});
            end else begin
              illegal_inst = 1'b1;
            end
          end

          // Floating-Point Load/Store
          // Single Precision Floating-Point
          riscv_instr::FLW: begin
            if (RVF) begin
              use_fd  = 1'b1;
              ls_size = Word;
              is_load = 1'b1;
            end else begin
              illegal_inst = 1'b1;
            end
          end
          riscv_instr::FSW: begin
            if (RVF) begin
              use_fs1  = 1'b1;
              ls_size  = Word;
              is_store = 1'b1;
            end else begin
              illegal_inst = 1'b1;
            end
          end
          // Double Precision Floating-Point
          riscv_instr::FLD: begin
            if (RVD) begin
              use_fd  = 1'b1;
              ls_size = Double;
              is_load = 1'b1;
            end else begin
              illegal_inst = 1'b1;
            end
          end
          riscv_instr::FSD: begin
            if (RVD) begin
              use_fs1  = 1'b1;
              ls_size  = Double;
              is_store = 1'b1;
            end else begin
              illegal_inst = 1'b1;
            end
          end

          // Vector instructions with FP scalar operand
          riscv_instr::VFADD_VF,
          riscv_instr::VFSUB_VF,
          riscv_instr::VFMIN_VF,
          riscv_instr::VFMAX_VF,
          riscv_instr::VFSGNJ_VF,
          riscv_instr::VFSGNJN_VF,
          riscv_instr::VFSGNJX_VF,
          riscv_instr::VFMV_V_F,
          riscv_instr::VFMUL_VF,
          riscv_instr::VFRSUB_VF,
          riscv_instr::VFMADD_VF,
          riscv_instr::VFNMADD_VF,
          riscv_instr::VFMSUB_VF,
          riscv_instr::VFNMSUB_VF,
          riscv_instr::VFMACC_VF,
          riscv_instr::VFNMACC_VF,
          riscv_instr::VFMSAC_VF,
          riscv_instr::VFNMSAC_VF: begin
            if (RVF) begin
              use_fs1 = 1'b1;
            end else begin
              illegal_inst = 1'b1;
            end
          end

          default:; // Do nothing
        endcase
    end: decoder

    //////////////////////////////
    //  Instruction dispatcher  //
    //////////////////////////////

    always_comb begin: dispatcher
      // By default, forward instructions to Spatz
      x_issue_req_o   = x_issue_req_i;
      x_issue_valid_o = x_issue_valid_i && !stall;
      x_issue_ready_o = x_issue_ready_i && !stall;
      x_issue_resp_o  = x_issue_resp_i;

      // Replace integer operands with FP operands, if needed
      if (use_fs1)
        x_issue_req_o.rs[0] = fpr_rdata[0];
      if (use_fs2)
        x_issue_req_o.rs[1] = fpr_rdata[1];
      if (use_fs3)
        x_issue_req_o.rs[2] = fpr_rdata[2];
    end: dispatcher

    ///////////
    //  LSU  //
    ///////////

    logic [4:0]           fp_lsu_qtag;
    logic                 fp_lsu_qwrite;
    logic                 fp_lsu_qsigned;
    logic [AddrWidth-1:0] fp_lsu_qaddr;
    logic [DataWidth-1:0] fp_lsu_qdata;
    logic [1:0]           fp_lsu_qsize;
    logic [3:0]           fp_lsu_qamo;
    logic                 fp_lsu_qvalid;
    logic                 fp_lsu_qready;

    logic [DataWidth-1:0] fp_lsu_pdata;
    logic [4:0]           fp_lsu_ptag;
    logic                 fp_lsu_pvalid;
    logic                 fp_lsu_pready;

    snitch_lsu #(
      .DataWidth          (FLEN               ),
      .NaNBox             (1                  ),
      .NumOutstandingLoads(NumOutstandingLoads)
    ) i_fp_lsu (
      .clk_i        (clk_i         ),
      .rst_i        (~rst_ni       ),
      // Request interface
      .lsu_qtag_i   (fp_lsu_qtag   ),
      .lsu_qwrite   (fp_lsu_qwrite ),
      .lsu_qsigned  (fp_lsu_qsigned),
      .lsu_qaddr_i  (fp_lsu_qaddr  ),
      .lsu_qdata_i  (fp_lsu_qdata  ),
      .lsu_qsize_i  (fp_lsu_qsize  ),
      .lsu_qamo_i   (fp_lsu_qamo   ),
      .lsu_qvalid_i (fp_lsu_qvalid ),
      .lsu_qready_o (fp_lsu_qready ),
      // Response interface
      .lsu_pdata_o  (fp_lsu_pdata  ),
      .lsu_ptag_o   (fp_lsu_ptag   ),
      .lsu_perror_o (/* Unused */  ),
      .lsu_pvalid_o (fp_lsu_pvalid ),
      .lsu_pready_i (fp_lsu_pready ),
      // Memory interface
      .data_qaddr_o (data_qaddr_o  ),
      .data_qwrite_o(data_qwrite_o ),
      .data_qamo_o  (data_qamo_o   ),
      .data_qdata_o (data_qdata_o  ),
      .data_qstrb_o (data_qstrb_o  ),
      .data_qid_o   (data_qid_o    ),
      .data_qvalid_o(data_qvalid_o ),
      .data_qready_i(data_qready_i ),
      .data_pdata_i (data_pdata_i  ),
      .data_perror_i(data_perror_i ),
      .data_pid_i   (data_pid_i    ),
      .data_pvalid_i(data_pvalid_i ),
      .data_pready_o(data_pready_o )
    );

    always_comb begin: lsu_dispatch
      // Default values
      fp_lsu_qtag    = fd;
      fp_lsu_qwrite  = is_store;
      fp_lsu_qsigned = 1'b0;
      fp_lsu_qaddr   = x_issue_req_i.rs[0];
      fp_lsu_qdata   = x_issue_req_i.rs[1];
      fp_lsu_qsize   = ls_size;
      fp_lsu_qamo    = '0;
      fp_lsu_qvalid  = (is_load || is_store);

      // Is the LSU stalling?
      lsu_stall = fp_lsu_qvalid && !fp_lsu_qready;
    end: lsu_dispatch

    //////////////
    //  Retire  //
    //////////////

    always_comb begin: retire_proc
      // We are not retiring anything, by default
      retire        = 1'b0;
      fp_lsu_pready = 1'b0;

      fpr_wdata = '0;
      fpr_we    = '0;
      fpr_waddr = '0;

      // Do not forward results to Snitch
      x_result_o       = '0;
      x_result_valid_o = 1'b0;
      x_result_ready_o = 1'b0;

      // Commit a Spatz result to Snitch
      if (x_result_valid_i && !x_result_i.id[5]) begin
        x_result_o       = x_result_i;
        x_result_valid_o = x_result_valid_i;
        x_result_ready_o = x_result_ready_i;
      end
      // Commit a Spatz result to the FPR
      else if (x_result_valid_i && x_result_i.id[5]) begin
        x_result_ready_o = 1'b1;
        retire           = 1'b1;

        fpr_wdata[0] = x_result_i.data;
        fpr_waddr[0] = x_result_i.id[4:0];
        fpr_we[0]    = 1'b1;
      end else
      // Commit a FP LSU response
      if (fp_lsu_pvalid) begin
        fp_lsu_pready = 1'b1;
        retire        = 1'b1;

        fpr_wdata[0] = fp_lsu_pdata;
        fpr_waddr[0] = fp_lsu_ptag;
        fpr_we[0]    = 1'b1;
      end
    end: retire_proc

  end: gen_fpr

endmodule: spatz_fpu_sequencer
