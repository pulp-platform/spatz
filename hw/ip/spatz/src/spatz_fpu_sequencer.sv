// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich
//
// The FPU sequencer gives basic FPU capabilities to Spatz. It manages the
// floating-point scalar register file, and floating point memory requests.

module spatz_fpu_sequencer
  import spatz_pkg::*;
  import rvv_pkg::*;
  import fpnew_pkg::*;
  import reqrsp_pkg::*;
  import cf_math_pkg::idx_width; #(
    // Memory request
    parameter type dreq_t            = logic,
    parameter type drsp_t            = logic,
    // Snitch interface
    parameter type spatz_issue_req_t = logic,
    parameter type spatz_issue_rsp_t = logic,
    parameter type spatz_rsp_t       = logic,

    parameter int unsigned AddrWidth           = 32,
    parameter int unsigned DataWidth           = FLEN,
    parameter int unsigned NumOutstandingLoads = 1,
    localparam int unsigned IdWidth = cf_math_pkg::idx_width(NumOutstandingLoads)
  ) (
    input  logic             clk_i,
    input  logic             rst_ni,
    // Snitch interface
    input  spatz_issue_req_t issue_req_i,
    input  logic             issue_valid_i,
    output logic             issue_ready_o,
    output spatz_issue_rsp_t issue_rsp_o,
    output spatz_rsp_t       resp_o,
    output logic             resp_valid_o,
    input  logic             resp_ready_i,
    // Spatz interface
    output spatz_issue_req_t issue_req_o,
    output logic             issue_valid_o,
    input  logic             issue_ready_i,
    input  spatz_issue_rsp_t issue_rsp_i,
    input  spatz_rsp_t       resp_i,
    input  logic             resp_valid_i,
    output logic             resp_ready_o,
    // Memory interface
`ifdef MEMPOOL_SPATZ
    output logic             fp_lsu_mem_req_valid_o,
    input  logic             fp_lsu_mem_req_ready_i,
    input  logic             fp_lsu_mem_rsp_valid_i,
    output logic             fp_lsu_mem_rsp_ready_o,
`endif
    output dreq_t            fp_lsu_mem_req_o,
    input  drsp_t            fp_lsu_mem_rsp_i,
    output logic             fp_lsu_mem_finished_o,
    output logic             fp_lsu_mem_str_finished_o,
    // Spatz VLSU side channel
    input  logic             spatz_mem_finished_i,
    input  logic             spatz_mem_str_finished_i
  );

`include "common_cells/registers.svh"
`include "common_cells/assertions.svh"
`include "reqrsp_interface/typedef.svh"

  // Capture requests that read and write to the scalar floating-point register file

  /////////////////
  //  Addresses  //
  /////////////////

  localparam int unsigned NrFPReg     = 32;
  localparam int unsigned FPRRegWidth = cf_math_pkg::idx_width(NrFPReg);

  logic [FPRRegWidth-1:0] fd, fs1, fs2, fs3;
  assign fd  = issue_req_i.data_op[7 + FPRRegWidth - 1:7];
  assign fs1 = issue_req_i.data_op[15 + FPRRegWidth - 1:15];
  assign fs2 = issue_req_i.data_op[20 + FPRRegWidth - 1:20];
  assign fs3 = issue_req_i.data_op[27 + FPRRegWidth - 1:27];

  /////////////////////
  //  Register File  //
  /////////////////////

  localparam int unsigned NrReadPorts  = 3;
  localparam int unsigned NrWritePorts = 2;

  logic [NrReadPorts-1:0][FPRRegWidth-1:0]  fpr_raddr;
  logic [NrReadPorts-1:0][FLEN-1:0]         fpr_rdata;
  logic [NrWritePorts-1:0][FPRRegWidth-1:0] fpr_waddr;
  logic [NrWritePorts-1:0][FLEN-1:0]        fpr_wdata;
  logic [NrWritePorts-1:0]                  fpr_we;

  assign fpr_raddr = {fs3, fs2, fs1};

  snitch_regfile #(
    .DATA_WIDTH    (FLEN        ),
    .ADDR_WIDTH    (FPRRegWidth ),
    .NR_READ_PORTS (NrReadPorts ),
    .NR_WRITE_PORTS(NrWritePorts),
    .ZERO_REG_ZERO (0           )
  ) i_fpr (
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

  // Does this instruction write back to the scalar regfile?
  logic use_rd;

  // Are the operands available?
  logic operands_available;
  assign operands_available = !((use_fs1 && sb_q[fs1]) || (use_fs2 && sb_q[fs2]) || (use_fs3 && sb_q[fs3]) || (use_fd && sb_q[fd]));

  // Is this a move instruction between GPR and FPR?
  logic is_move;

  // Is this a load or a store?
  logic is_load;
  logic is_store;

  // Is this an instruction that executes in this sequencer?
  logic is_local;
  assign is_local = is_move || is_load || is_store;

  // Stalling?
  logic stall;
  logic lsu_stall;
  logic vlsu_stall;
  logic move_stall;
  assign stall = lsu_stall || vlsu_stall || move_stall || !operands_available || (!is_local && !issue_ready_i);

  // Illegal instruction
  logic illegal_inst;

  // Are we retiring something?
  logic [1:0] retire;

  always_comb begin
    // Maintain state
    sb_d = sb_q;
    if (use_fd && !is_move && !stall)
      sb_d[fd] = 1'b1;
    if (retire[0])
      sb_d[fpr_waddr[0]] = 1'b0;
    if (retire[1])
      sb_d[fpr_waddr[1]] = 1'b0;
  end

  ///////////////////////////
  //  Instruction decoder  //
  ///////////////////////////

  typedef enum logic [1:0] {
    Byte     = 2'b00,
    HalfWord = 2'b01,
    Word     = 2'b10,
    Double   = 2'b11
  } ls_size_t;
  ls_size_t  ls_size;

  always_comb begin
    // We are not reading any operands
    use_fs1 = 1'b0;
    use_fs2 = 1'b0;
    use_fs3 = 1'b0;
    use_fd  = 1'b0;
    use_rd  = 1'b0;

    // Default element size
    ls_size  = Word;
    is_load  = 1'b0;
    is_store = 1'b0;
    is_move  = 1'b0;

    // Not an illegal instruction
    illegal_inst = 1'b0;

    if (issue_valid_i)
      unique casez (issue_req_i.data_op)
        // Byte Precision Floating-Point
        riscv_instr::FADD_B,
        riscv_instr::FSUB_B,
        riscv_instr::FMUL_B,
        riscv_instr::FDIV_B,
        riscv_instr::FSQRT_B,
        riscv_instr::FSGNJ_B,
        riscv_instr::FSGNJN_B,
        riscv_instr::FSGNJX_B,
        riscv_instr::FMIN_B,
        riscv_instr::FMAX_B,
        riscv_instr::FCLASS_B,
        riscv_instr::FLE_B,
        riscv_instr::FLT_B,
        riscv_instr::FEQ_B,
        riscv_instr::FCVT_B_W,
        riscv_instr::FCVT_B_WU,
        riscv_instr::FCVT_W_B,
        riscv_instr::FCVT_WU_B,
        riscv_instr::FMADD_B,
        riscv_instr::FMSUB_B,
        riscv_instr::FNMSUB_B,
        riscv_instr::FNMADD_B,
        riscv_instr::FCVT_B_H,
        riscv_instr::FCVT_H_B: begin
          if (RVF && (!(issue_req_i.data_op inside {riscv_instr::FDIV_B, riscv_instr::FSQRT_B}) || FDivSqrt)) begin
            use_fs1 = !(issue_req_i.data_op inside {riscv_instr::FCVT_B_W, riscv_instr::FCVT_B_WU});
            use_fs2 = !(issue_req_i.data_op inside {riscv_instr::FCLASS_B});
            use_fs3 = issue_req_i.data_op inside {riscv_instr::FMADD_B, riscv_instr::FMSUB_B, riscv_instr::FNMSUB_B, riscv_instr::FNMADD_B};
            use_fd  = !(issue_req_i.data_op inside {riscv_instr::FCLASS_B, riscv_instr::FLE_B, riscv_instr::FLT_B, riscv_instr::FEQ_B, riscv_instr::FCVT_W_B, riscv_instr::FCVT_WU_B});
          end else begin
            illegal_inst = 1'b1;
          end
        end
        riscv_instr::FMV_X_B: begin
          if (RVF) begin
            use_rd  = 1'b1;
            use_fs1 = 1'b1;
            is_move = 1'b1;
          end else begin
            illegal_inst = 1'b1;
          end
        end
        riscv_instr::FMV_B_X: begin
          if (RVF) begin
            use_fd  = 1'b1;
            is_move = 1'b1;
          end else begin
            illegal_inst = 1'b1;
          end
        end

        // Half Precision Floating-Point
        riscv_instr::FADD_H,
        riscv_instr::FSUB_H,
        riscv_instr::FMUL_H,
        riscv_instr::FDIV_H,
        riscv_instr::FSQRT_H,
        riscv_instr::FSGNJ_H,
        riscv_instr::FSGNJN_H,
        riscv_instr::FSGNJX_H,
        riscv_instr::FMIN_H,
        riscv_instr::FMAX_H,
        riscv_instr::FCLASS_H,
        riscv_instr::FLE_H,
        riscv_instr::FLT_H,
        riscv_instr::FEQ_H,
        riscv_instr::FCVT_H_W,
        riscv_instr::FCVT_H_WU,
        riscv_instr::FCVT_W_H,
        riscv_instr::FCVT_WU_H,
        riscv_instr::FMADD_H,
        riscv_instr::FMSUB_H,
        riscv_instr::FNMSUB_H,
        riscv_instr::FNMADD_H,
        riscv_instr::FCVT_H_S,
        riscv_instr::FCVT_S_H: begin
          if (RVF && (!(issue_req_i.data_op inside {riscv_instr::FDIV_H, riscv_instr::FSQRT_H}) || FDivSqrt)) begin
            use_fs1 = !(issue_req_i.data_op inside {riscv_instr::FCVT_H_W, riscv_instr::FCVT_H_WU});
            use_fs2 = !(issue_req_i.data_op inside {riscv_instr::FCLASS_H});
            use_fs3 = issue_req_i.data_op inside {riscv_instr::FMADD_H, riscv_instr::FMSUB_H, riscv_instr::FNMSUB_H, riscv_instr::FNMADD_H};
            use_fd  = !(issue_req_i.data_op inside {riscv_instr::FCLASS_H, riscv_instr::FLE_H, riscv_instr::FLT_H, riscv_instr::FEQ_H, riscv_instr::FCVT_W_H, riscv_instr::FCVT_WU_H});
          end else begin
            illegal_inst = 1'b1;
          end
        end
        riscv_instr::FMV_X_H: begin
          if (RVF) begin
            use_rd  = 1'b1;
            use_fs1 = 1'b1;
            is_move = 1'b1;
          end else begin
            illegal_inst = 1'b1;
          end
        end
        riscv_instr::FMV_H_X: begin
          if (RVF) begin
            use_fd  = 1'b1;
            is_move = 1'b1;
          end else begin
            illegal_inst = 1'b1;
          end
        end

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
        riscv_instr::FNMADD_S,
        riscv_instr::FCVT_S_D,
        riscv_instr::FCVT_D_S: begin
          if (RVF && (!(issue_req_i.data_op inside {riscv_instr::FDIV_S, riscv_instr::FSQRT_S}) || FDivSqrt)) begin
            use_fs1 = !(issue_req_i.data_op inside {riscv_instr::FCVT_S_W, riscv_instr::FCVT_S_WU});
            use_fs2 = !(issue_req_i.data_op inside {riscv_instr::FCLASS_S});
            use_fs3 = issue_req_i.data_op inside {riscv_instr::FMADD_S, riscv_instr::FMSUB_S, riscv_instr::FNMSUB_S, riscv_instr::FNMADD_S};
            use_fd  = !(issue_req_i.data_op inside {riscv_instr::FCLASS_S, riscv_instr::FLE_S, riscv_instr::FLT_S, riscv_instr::FEQ_S, riscv_instr::FCVT_W_S, riscv_instr::FCVT_WU_S});
          end else begin
            illegal_inst = 1'b1;
          end
        end
        riscv_instr::FMV_X_S: begin
          if (RVF) begin
            use_rd  = 1'b1;
            use_fs1 = 1'b1;
            is_move = 1'b1;
          end else begin
            illegal_inst = 1'b1;
          end
        end
        riscv_instr::FMV_S_X: begin
          if (RVF) begin
            use_fd  = 1'b1;
            is_move = 1'b1;
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
          if (RVD && (!(issue_req_i.data_op inside {riscv_instr::FDIV_D, riscv_instr::FSQRT_D}) || FDivSqrt)) begin
            use_fs1 = !(issue_req_i.data_op inside {riscv_instr::FCVT_D_W, riscv_instr::FCVT_D_WU});
            use_fs2 = 1'b1;
            use_fs3 = issue_req_i.data_op inside {riscv_instr::FMADD_D, riscv_instr::FMSUB_D, riscv_instr::FNMSUB_D, riscv_instr::FNMADD_D};
            use_fd  = !(issue_req_i.data_op inside {riscv_instr::FCLASS_D, riscv_instr::FLE_D, riscv_instr::FLT_D, riscv_instr::FEQ_D, riscv_instr::FCVT_W_D, riscv_instr::FCVT_WU_D});
          end else begin
            illegal_inst = 1'b1;
          end
        end

        // Floating-Point Load/Store
        riscv_instr::FLB,
        riscv_instr::FLH,
        riscv_instr::FLW,
        riscv_instr::FLD: begin
          use_fd = 1'b1;
          casez (issue_req_i.data_op)
            riscv_instr::FLB: ls_size          = Byte;
            riscv_instr::FLH: ls_size          = HalfWord;
            riscv_instr::FLW: ls_size          = Word;
            riscv_instr::FLD: if (RVD) ls_size = Double;
            default:;
          endcase
          is_load      = 1'b1;
          illegal_inst = !RVD && issue_req_i.data_op inside {riscv_instr::FLD};
        end
        riscv_instr::FSB,
        riscv_instr::FSH,
        riscv_instr::FSW,
        riscv_instr::FSD: begin
          use_fs2 = 1'b1;
          casez (issue_req_i.data_op)
            riscv_instr::FSB: ls_size          = Byte;
            riscv_instr::FSH: ls_size          = HalfWord;
            riscv_instr::FSW: ls_size          = Word;
            riscv_instr::FSD: if (RVD) ls_size = Double;
            default:;
          endcase
          is_store     = 1'b1;
          illegal_inst = !RVD && issue_req_i.data_op inside {riscv_instr::FSD};
        end

        // Vector instructions with FP scalar operand
        riscv_instr::VFADD_VF,
        riscv_instr::VFSUB_VF,
        riscv_instr::VFMIN_VF,
        riscv_instr::VFMAX_VF,
        riscv_instr::VFSGNJ_VF,
        riscv_instr::VFSGNJN_VF,
        riscv_instr::VFSGNJX_VF,
        riscv_instr::VFSLIDE1UP_VF,
        riscv_instr::VFSLIDE1DOWN_VF,
        riscv_instr::VFMV_V_F,
        riscv_instr::VFMV_S_F,
        riscv_instr::VFMUL_VF,
        riscv_instr::VFRSUB_VF,
        riscv_instr::VFMADD_VF,
        riscv_instr::VFNMADD_VF,
        riscv_instr::VFMSUB_VF,
        riscv_instr::VFNMSUB_VF,
        riscv_instr::VFMACC_VF,
        riscv_instr::VFNMACC_VF,
        riscv_instr::VFMSAC_VF,
        riscv_instr::VFNMSAC_VF,
        riscv_instr::VFWADD_VF,
        riscv_instr::VFWADD_WF,
        riscv_instr::VFWSUB_VF,
        riscv_instr::VFWSUB_WF,
        riscv_instr::VFWMUL_VF,
        riscv_instr::VFWMACC_VF,
        riscv_instr::VFWNMACC_VF,
        riscv_instr::VFWMSAC_VF,
        riscv_instr::VFWNMSAC_VF,
        riscv_instr::VFWDOTP_VF: begin
          if (RVF) begin
            use_fs1 = 1'b1;
          end else begin
            illegal_inst = 1'b1;
          end
        end

        // Move to the FPR
        riscv_instr::VFMV_F_S: begin
          use_fd = 1'b1;
        end

        default:; // Do nothing
      endcase
  end

  //////////////////////////////
  //  Instruction dispatcher  //
  //////////////////////////////

  always_comb begin
    // By default, forward instructions to Spatz
    issue_req_o   = issue_req_i;
    issue_valid_o = issue_valid_i && !stall && !is_local;
    issue_ready_o = (is_local && !stall) || (!is_local && issue_ready_i && !stall);

    // Replace integer operands with FP operands, if needed
    if (use_fs1)
      issue_req_o.data_arga = fpr_rdata[0];
    if (use_fs2)
      issue_req_o.data_argb = fpr_rdata[1];
    if (use_fs3)
      issue_req_o.data_argc = fpr_rdata[2];

    // Does this instruction write to the FPR?
    issue_req_o.id[5] = use_fd;

    // Forward the response back to Snitch
    if (!is_local)
      issue_rsp_o = issue_rsp_i;
    else
      issue_rsp_o = spatz_issue_rsp_t'{
        accept   : use_rd,
        writeback: use_rd,
        loadstore: is_load | is_store,
        exception: illegal_inst,
        default  : '0
      };
  end

  ///////////
  //  LSU  //
  ///////////

  logic [4:0]           fp_lsu_qtag;
  logic                 fp_lsu_qwrite;
  logic                 fp_lsu_qsigned;
  logic [AddrWidth-1:0] fp_lsu_qaddr;
  logic [DataWidth-1:0] fp_lsu_qdata;
  logic [1:0]           fp_lsu_qsize;
  reqrsp_pkg::amo_op_e  fp_lsu_qamo;
  logic fp_lsu_qvalid;
  logic fp_lsu_qready;

  logic [DataWidth-1:0] fp_lsu_pdata;
  logic [4:0]           fp_lsu_ptag;
  logic                 fp_lsu_pvalid;
  logic                 fp_lsu_pready;

  // TODO: remove hardcoding
  logic [AddrWidth-1:0] mem_qaddr;
  logic                 mem_qwrite;
  logic [DataWidth-1:0] mem_qdata;
  logic [StrbWidth-1:0] mem_qstrb;
  logic [IdWidth-1:0]   mem_qid;
  logic [DataWidth-1:0] mem_pdata;
  logic                 mem_perror;
  logic [IdWidth-1:0]   mem_pid;

  snitch_lsu #(
    .NaNBox             (1                  ),
    .dreq_t             (dreq_t             ),
    .drsp_t             (drsp_t             ),
    .DataWidth          (FLEN               ),
    .NumOutstandingMem  (NumOutstandingLoads),
    .NumOutstandingLoads(NumOutstandingLoads)
  ) i_fp_lsu (
    .clk_i        (clk_i           ),
    .rst_i        (~rst_ni         ),
    // Request interface
    .lsu_qtag_i   (fp_lsu_qtag     ),
    .lsu_qwrite_i (fp_lsu_qwrite   ),
    .lsu_qsigned_i(fp_lsu_qsigned  ),
    .lsu_qaddr_i  (fp_lsu_qaddr    ),
    .lsu_qdata_i  (fp_lsu_qdata    ),
    .lsu_qsize_i  (fp_lsu_qsize    ),
    .lsu_qamo_i   (fp_lsu_qamo     ),
    .lsu_qvalid_i (fp_lsu_qvalid   ),
    .lsu_qready_o (fp_lsu_qready   ),
    // Response interface
    .lsu_pdata_o  (fp_lsu_pdata    ),
    .lsu_ptag_o   (fp_lsu_ptag     ),
    .lsu_perror_o (/* Unused */    ),
    .lsu_pvalid_o (fp_lsu_pvalid   ),
    .lsu_pready_i (fp_lsu_pready   ),
    // Memory interface
`ifdef MEMPOOL_SPATZ
    .data_qaddr_o (mem_qaddr              ),
    .data_qwrite_o(mem_qwrite             ),
    .data_qamo_o  (/* Unused */           ),
    .data_qdata_o (mem_qdata              ),
    .data_qstrb_o (mem_qstrb              ),
    .data_qid_o   (mem_qid                ),
    .data_qvalid_o(fp_lsu_mem_req_valid_o ),
    .data_qready_i(fp_lsu_mem_req_ready_i ),
    .data_pdata_i (mem_pdata              ),
    .data_perror_i(mem_perror             ),
    .data_pid_i   (mem_pid                ),
    .data_pvalid_i(fp_lsu_mem_rsp_valid_i ),
    .data_pready_o(fp_lsu_mem_rsp_ready_o )
`else
    .data_req_o   (fp_lsu_mem_req_o),
    .data_rsp_i   (fp_lsu_mem_rsp_i)
`endif
  );

  // Number of memory operations in the accelerator
  logic [2:0] acc_mem_cnt_q, acc_mem_cnt_d;
  `FFARN(acc_mem_cnt_q, acc_mem_cnt_d, '0, clk_i, rst_ni)

  // Number of store operations in the accelerator
  logic [2:0] acc_mem_str_cnt_q, acc_mem_str_cnt_d;
  `FFARN(acc_mem_str_cnt_q, acc_mem_str_cnt_d, '0, clk_i, rst_ni)

  // Is the current instruction a vector load?
  logic is_vector_load;
  assign is_vector_load = issue_req_i.data_op inside
    {riscv_instr::VLE8_V, riscv_instr::VLE16_V, riscv_instr::VLE32_V, riscv_instr::VLE64_V,
    riscv_instr::VLSE8_V, riscv_instr::VLSE16_V, riscv_instr::VLSE32_V, riscv_instr::VLSE64_V,
    riscv_instr::VLOXEI8_V, riscv_instr::VLOXEI16_V, riscv_instr::VLOXEI32_V, riscv_instr::VLOXEI64_V,
    riscv_instr::VLUXEI8_V, riscv_instr::VLUXEI16_V, riscv_instr::VLUXEI32_V, riscv_instr::VLUXEI64_V};

  // Is the current instruction a vector store?
  logic is_vector_store;
  assign is_vector_store = issue_req_i.data_op inside
    {riscv_instr::VSE8_V, riscv_instr::VSE16_V, riscv_instr::VSE32_V, riscv_instr::VSE64_V,
    riscv_instr::VSSE8_V, riscv_instr::VSSE16_V, riscv_instr::VSSE32_V, riscv_instr::VSSE64_V,
    riscv_instr::VSOXEI8_V, riscv_instr::VSOXEI16_V, riscv_instr::VSOXEI32_V, riscv_instr::VSOXEI64_V,
    riscv_instr::VSUXEI8_V, riscv_instr::VSUXEI16_V, riscv_instr::VSUXEI32_V, riscv_instr::VSUXEI64_V};

  // Do we need to delay is load/store because of the VLSU?
  assign vlsu_stall = (is_store && acc_mem_cnt_q != '0) || (is_load && acc_mem_str_cnt_q != '0) || acc_mem_cnt_q == '1;

  always_comb begin
    // Default values
    fp_lsu_qtag    = fd;
    fp_lsu_qwrite  = is_store;
    fp_lsu_qsigned = 1'b0;
    // lsu in mempool-snitch will write to argb
`ifdef MEMPOOL_SPATZ
    fp_lsu_qaddr   = issue_req_i.data_argb;
`else
    fp_lsu_qaddr   = issue_req_i.data_argc;
`endif
    fp_lsu_qdata   = fpr_rdata[1];
    fp_lsu_qsize   = ls_size;
    fp_lsu_qamo    = AMONone;
    fp_lsu_qvalid  = (is_load || is_store) && operands_available && !vlsu_stall;

    acc_mem_cnt_d     = acc_mem_cnt_q;
    acc_mem_str_cnt_d = acc_mem_str_cnt_q;

    // Is the LSU stalling?
    lsu_stall = fp_lsu_qvalid && !fp_lsu_qready;

`ifdef MEMPOOL_SPATZ
    // Assign TCDM data interface
    fp_lsu_mem_req_o = '{
      id     : mem_qid,
      addr   : mem_qaddr,
      size   : '1,
      write  : mem_qwrite,
      strb   : mem_qstrb,
      data   : mem_qdata,
      last   : 1'b1,
      default: '0
    };

    mem_pdata  = fp_lsu_mem_rsp_i.data;
    mem_perror = '0;
    mem_pid    = fp_lsu_mem_rsp_i.id;
`endif

    if ((is_vector_load || is_vector_store) && issue_ready_i && issue_valid_o)
      acc_mem_cnt_d += 1;
    if (spatz_mem_finished_i)
      acc_mem_cnt_d -= 1;

    if (is_vector_store && issue_ready_i && issue_valid_o)
      acc_mem_str_cnt_d += 1;
    if (spatz_mem_finished_i && spatz_mem_str_finished_i)
      acc_mem_str_cnt_d -= 1;
  end

  ////////////
  //  Move  //
  ////////////

  spatz_rsp_t fp_move_result_i;
  logic       fp_move_result_valid_i;
  logic       fp_move_result_ready_o;

  spatz_rsp_t fp_move_result_o;
  logic       fp_move_result_valid_o;
  logic       fp_move_result_ready_i;

  spill_register #(
    .T(spatz_rsp_t)
  ) i_fp_move_register (
    .clk_i     (clk_i                 ),
    .rst_ni    (rst_ni                ),
    .data_i    (fp_move_result_i      ),
    .valid_i   (fp_move_result_valid_i),
    .ready_o   (fp_move_result_ready_o),
    .data_o    (fp_move_result_o      ),
    .valid_o   (fp_move_result_valid_o),
    .ready_i   (fp_move_result_ready_i)
  );

  //////////////
  //  Retire  //
  //////////////

  logic outstanding_store_q, outstanding_store_d;
  `FF(outstanding_store_q, outstanding_store_d, 1'b0)

  always_comb begin
    // We are not retiring anything, by default
    retire        = '0;
    fp_lsu_pready = 1'b0;

    fpr_wdata = '0;
    fpr_we    = '0;
    fpr_waddr = '0;

    move_stall             = 1'b0;
    fp_move_result_i       = '0;
    fp_move_result_valid_i = 1'b0;
    fp_move_result_ready_i = 1'b0;

    outstanding_store_d = 1'b0;

    // Do not forward results to Snitch
    resp_o       = '0;
    resp_valid_o = 1'b0;
    resp_ready_o = 1'b0;

    // Did we just finished writing something?
    fp_lsu_mem_finished_o     = outstanding_store_q || (fp_lsu_qwrite && fp_lsu_qvalid && fp_lsu_qready);
    fp_lsu_mem_str_finished_o = outstanding_store_q || (fp_lsu_qwrite && fp_lsu_qvalid && fp_lsu_qready);

    // Was there already a store committing in this cycle?
    outstanding_store_d = outstanding_store_q && (fp_lsu_qwrite && fp_lsu_qvalid && fp_lsu_qready);

    // Is there a move trying to commit during this cycle?
    move_stall = is_move && use_rd && !fp_move_result_ready_o;
    if (is_move && use_rd && operands_available) begin
      fp_move_result_i = spatz_rsp_t'{
        id     : issue_req_i.id,
        data   : fpr_rdata[0],
        default: '0
      };
      fp_move_result_valid_i = 1'b1;
    end

    // Commit a Spatz result to Snitch
    if (resp_valid_i && !resp_i.id[5]) begin
      resp_o       = resp_i;
      resp_valid_o = resp_valid_i;
      resp_ready_o = resp_ready_i;
    end
    // Commit a Spatz result to the FPR
    else if (resp_valid_i && resp_i.id[5]) begin
      resp_ready_o = 1'b1;
      retire[0]    = 1'b1;

      fpr_wdata[0] = resp_i.data;
      fpr_waddr[0] = resp_i.id[4:0];
      fpr_we[0]    = 1'b1;
    end
    // Commit a move result
    else if (fp_move_result_valid_o) begin
      resp_o                 = fp_move_result_o;
      resp_valid_o           = fp_move_result_valid_o;
      fp_move_result_ready_i = resp_ready_i;
    end

    // Commit a FP LSU response
    if (fp_lsu_pvalid && !outstanding_store_q) begin
      fp_lsu_pready = 1'b1;
      retire[1]     = 1'b1;

      fpr_wdata[1] = fp_lsu_pdata;
      fpr_waddr[1] = fp_lsu_ptag;
      fpr_we[1]    = 1'b1;

      // Was there a store being acknowledged in this cycle?
      outstanding_store_d = fp_lsu_mem_str_finished_o;

      // Finished a load
      fp_lsu_mem_finished_o     = 1'b1;
      fp_lsu_mem_str_finished_o = 1'b0;

      move_stall = is_move && use_fd;
    end
    // Commit moves to the RF
    else if (is_move && use_fd && !stall) begin
      fpr_wdata[1] = issue_req_i.data_arga;
      fpr_waddr[1] = fd;
      fpr_we[1]    = 1'b1;
    end
  end

  //////////////////
  //  Assertions  //
  //////////////////

  `ASSERT(MemoryOperationCounterRollover,
    acc_mem_cnt_q == '0 |=> acc_mem_cnt_q != '1, clk_i, !rst_ni)

  `ASSERT(MemoryStoreOperationCounterRollover,
    acc_mem_str_cnt_q == '0 |=> acc_mem_str_cnt_q != '1, clk_i, !rst_ni)

endmodule: spatz_fpu_sequencer
