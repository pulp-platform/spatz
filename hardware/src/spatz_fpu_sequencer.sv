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
    localparam int  unsigned IdWidth   = cf_math_pkg::idx_width(NumOutstandingLoads),
    localparam int  unsigned StrbWidth = DataWidth/8,
    localparam type          addr_t    = logic [AddrWidth-1:0],
    localparam type          data_t    = logic [DataWidth-1:0],
    localparam type          strb_t    = logic [StrbWidth-1:0]
  ) (
    input  logic            clk_i,
    input  logic            rst_ni,
    // Snitch interface
    // X-Interface request
    input  x_issue_req_t    x_issue_req_i,
    input  logic            x_issue_valid_i,
    output logic            x_issue_ready_o,
    output x_issue_resp_t   x_issue_resp_o,
    // X-Interface result
    output x_result_t       x_result_o,
    output logic            x_result_valid_o,
    input  logic            x_result_ready_i,
    // Spatz interface
    // X-Interface request
    output x_issue_req_t    x_issue_req_o,
    output logic            x_issue_valid_o,
    input  logic            x_issue_ready_i,
    input  x_issue_resp_t   x_issue_resp_i,
    // X-Interface result
    input  x_result_t       x_result_i,
    input  logic            x_result_valid_i,
    output logic            x_result_ready_o,
    // Memory interface
    output spatz_mem_req_t  fp_lsu_mem_req_o,
    output logic            fp_lsu_mem_req_valid_o,
    input  logic            fp_lsu_mem_req_ready_i,
    input  spatz_mem_resp_t fp_lsu_mem_resp_i,
    input  logic            fp_lsu_mem_resp_valid_i,
    output logic            fp_lsu_mem_resp_ready_o,
    output logic            fp_lsu_mem_finished_o,
    output logic            fp_lsu_mem_str_finished_o,
    // Spatz VLSU side channel
    input  logic            spatz_mem_finished_i,
    input  logic            spatz_mem_str_finished_i
  );

`include "common_cells/registers.svh"
`include "common_cells/assertions.svh"

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

  logic [31:0] iimmediate;
  assign iimmediate = $signed({x_issue_req_i.instr[31:20]});

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

  // Stalling?
  logic stall;
  logic lsu_stall;
  logic vlsu_stall;
  logic move_stall;
  assign stall = lsu_stall || vlsu_stall || move_stall || !operands_available;

  // Illegal instruction
  logic illegal_inst;

  // Are we retiring something?
  logic [1:0] retire;

  // Is this a move instruction between GPR and FPR?
  logic is_move;

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

  enum logic [1:0] {
    Byte     = 2'b00,
    HalfWord = 2'b01,
    Word     = 2'b10,
    Double   = 2'b11
  } ls_size;

  logic is_load;
  logic is_store;

  // Is this an instruction that executes in this sequencer?
  logic is_local;
  assign is_local = is_move || is_load || is_store;

  always_comb begin: decoder
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
            use_fs2 = !(x_issue_req_i.instr inside {riscv_instr::FCLASS_S});
            use_fs3 = x_issue_req_i.instr inside {riscv_instr::FMADD_S, riscv_instr::FMSUB_S, riscv_instr::FNMSUB_S, riscv_instr::FNMADD_S};
            use_fd  = !(x_issue_req_i.instr inside {riscv_instr::FCLASS_S, riscv_instr::FLE_S, riscv_instr::FLT_S, riscv_instr::FEQ_S, riscv_instr::FCVT_W_S, riscv_instr::FCVT_WU_S});
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
            use_fs2  = 1'b1;
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
            use_fs2  = 1'b1;
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
    x_issue_valid_o = x_issue_valid_i && !stall && !is_local;
    x_issue_ready_o = (is_local && !stall) || (!is_local && x_issue_ready_i && !stall);

    // Replace integer operands with FP operands, if needed
    if (use_fs1)
      x_issue_req_o.rs[0] = fpr_rdata[0];
    if (use_fs2)
      x_issue_req_o.rs[1] = fpr_rdata[1];
    if (use_fs3)
      x_issue_req_o.rs[2] = fpr_rdata[2];

    // Does this instruction write to the FPR?
    x_issue_req_o.id[5] = use_fd;

    // Forward the response back to Snitch
    if (!is_local)
      x_issue_resp_o = x_issue_resp_i;
    else
      x_issue_resp_o = '{
        accept   : use_rd,
        writeback: use_rd,
        loadstore: is_load | is_store,
        exc      : illegal_inst,
        default  : '0
      };
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

  addr_t               mem_qaddr;
  logic                mem_qwrite;
  data_t               mem_qdata;
  strb_t               mem_qstrb;
  logic  [IdWidth-1:0] mem_qid;
  data_t               mem_pdata;
  logic                mem_perror;
  logic  [IdWidth-1:0] mem_pid;

  snitch_lsu #(
    .DataWidth          (FLEN               ),
    .NaNBox             (1                  ),
    .NumOutstandingLoads(NumOutstandingLoads)
  ) i_fp_lsu (
    .clk_i        (clk_i                  ),
    .rst_i        (~rst_ni                ),
    // Request interface
    .lsu_qtag_i   (fp_lsu_qtag            ),
    .lsu_qwrite   (fp_lsu_qwrite          ),
    .lsu_qsigned  (fp_lsu_qsigned         ),
    .lsu_qaddr_i  (fp_lsu_qaddr           ),
    .lsu_qdata_i  (fp_lsu_qdata           ),
    .lsu_qsize_i  (fp_lsu_qsize           ),
    .lsu_qamo_i   (fp_lsu_qamo            ),
    .lsu_qvalid_i (fp_lsu_qvalid          ),
    .lsu_qready_o (fp_lsu_qready          ),
    // Response interface
    .lsu_pdata_o  (fp_lsu_pdata           ),
    .lsu_ptag_o   (fp_lsu_ptag            ),
    .lsu_perror_o (/* Unused */           ),
    .lsu_pvalid_o (fp_lsu_pvalid          ),
    .lsu_pready_i (fp_lsu_pready          ),
    // Memory interface
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
    .data_pvalid_i(fp_lsu_mem_resp_valid_i),
    .data_pready_o(fp_lsu_mem_resp_ready_o)
  );

  // Number of memory operations in the accelerator
  logic [2:0] acc_mem_cnt_q, acc_mem_cnt_d;
  `FFARN(acc_mem_cnt_q, acc_mem_cnt_d, '0, clk_i, rst_ni)

  // Number of store operations in the accelerator
  logic [2:0] acc_mem_str_cnt_q, acc_mem_str_cnt_d;
  `FFARN(acc_mem_str_cnt_q, acc_mem_str_cnt_d, '0, clk_i, rst_ni)

  // Is the current instruction a vector store?
  logic is_vector_store;
  assign is_vector_store = x_issue_req_i.instr inside
    {riscv_instr::VSE8_V, riscv_instr::VSE16_V, riscv_instr::VSE32_V,
    riscv_instr::VSSE8_V, riscv_instr::VSSE16_V, riscv_instr::VSSE32_V};

  // Do we need to delay is load/store because of the VLSU?
  assign vlsu_stall = (is_store && acc_mem_cnt_q != '0) || (is_load && acc_mem_str_cnt_q != 0) || acc_mem_cnt_q == '1;

  always_comb begin: lsu_dispatch
    // Default values
    fp_lsu_qtag    = fd;
    fp_lsu_qwrite  = is_store;
    fp_lsu_qsigned = 1'b0;
    fp_lsu_qaddr   = x_issue_req_i.rs[0] + iimmediate;
    fp_lsu_qdata   = fpr_rdata[1];
    fp_lsu_qsize   = ls_size;
    fp_lsu_qamo    = '0;
    fp_lsu_qvalid  = (is_load || is_store) && operands_available && !vlsu_stall;

    acc_mem_cnt_d     = acc_mem_cnt_q;
    acc_mem_str_cnt_d = acc_mem_str_cnt_q;

    // Is the LSU stalling?
    lsu_stall = fp_lsu_qvalid && !fp_lsu_qready;

    // Assign TCDM data interface
    fp_lsu_mem_req_o = '{
      id     : mem_qid,
      addr   : mem_qaddr,
      size   : '1,
      we     : mem_qwrite,
      strb   : mem_qstrb,
      wdata  : mem_qdata,
      last   : 1'b1,
      default: '0
    };

    mem_pdata  = fp_lsu_mem_resp_i.rdata;
    mem_perror = '0;
    mem_pid    = fp_lsu_mem_resp_i.id;

    if (x_issue_resp_i.loadstore && x_issue_ready_i && x_issue_valid_o)
      acc_mem_cnt_d += 1;
    if (spatz_mem_finished_i)
      acc_mem_cnt_d -= 1;

    if (x_issue_resp_i.loadstore && x_issue_ready_i && x_issue_valid_o && is_vector_store)
      acc_mem_str_cnt_d += 1;
    if (spatz_mem_finished_i && spatz_mem_str_finished_i)
      acc_mem_str_cnt_d -= 1;
  end: lsu_dispatch

  ////////////
  //  Move  //
  ////////////

  x_result_t fp_move_result_i;
  logic      fp_move_result_valid_i;
  logic      fp_move_result_ready_o;

  x_result_t fp_move_result_o;
  logic      fp_move_result_valid_o;
  logic      fp_move_result_ready_i;

  stream_register #(
    .T(x_result_t)
  ) i_fp_move_register (
    .clk_i     (clk_i                 ),
    .rst_ni    (rst_ni                ),
    .clr_i     (1'b0                  ),
    .testmode_i(1'b0                  ),
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

  always_comb begin: retire_proc
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

    // Do not forward results to Snitch
    x_result_o       = '0;
    x_result_valid_o = 1'b0;
    x_result_ready_o = 1'b0;

    // Did we just finished writing something?
    fp_lsu_mem_finished_o     = fp_lsu_qwrite && fp_lsu_qvalid && fp_lsu_qready;
    fp_lsu_mem_str_finished_o = fp_lsu_qwrite && fp_lsu_qvalid && fp_lsu_qready;

    // Is there a move tring to commit during this cycle?
    move_stall = is_move && use_rd && !fp_move_result_ready_o;
    if (is_move && use_rd && operands_available) begin
      fp_move_result_i = '{
        id     : x_issue_req_i.id,
        data   : fpr_rdata[0],
        we     : '1,
        default: '0
      };
      fp_move_result_valid_i = 1'b1;
    end

    // Commit a Spatz result to Snitch
    if (x_result_valid_i && !x_result_i.id[5]) begin
      x_result_o       = x_result_i;
      x_result_valid_o = x_result_valid_i;
      x_result_ready_o = x_result_ready_i;
    end
    // Commit a Spatz result to the FPR
    else if (x_result_valid_i && x_result_i.id[5]) begin
      x_result_ready_o = 1'b1;
      retire[0]        = 1'b1;

      fpr_wdata[0] = x_result_i.data;
      fpr_waddr[0] = x_result_i.id[4:0];
      fpr_we[0]    = 1'b1;
    end
    // Commit a move result
    else if (fp_move_result_valid_o) begin
      x_result_o             = fp_move_result_o;
      x_result_valid_o       = 1'b1;
      fp_move_result_ready_i = 1'b1;
    end

    // Commit a FP LSU response
    if (fp_lsu_pvalid) begin
      fp_lsu_pready = 1'b1;
      retire[1]     = 1'b1;

      fpr_wdata[1] = fp_lsu_pdata;
      fpr_waddr[1] = fp_lsu_ptag;
      fpr_we[1]    = 1'b1;

      // Finished a load
      fp_lsu_mem_finished_o     = 1'b1;
      fp_lsu_mem_str_finished_o = 1'b0;

      move_stall = is_move && use_fd;
    end
    // Commit moves to the RF
    else if (is_move && use_fd && !stall) begin
      fpr_wdata[1] = x_issue_req_i.rs[0];
      fpr_waddr[1] = fd;
      fpr_we[1]    = 1'b1;
    end
  end: retire_proc

  //////////////////
  //  Assertions  //
  //////////////////

  `ASSERT(MemoryOperationCounterRollover,
    acc_mem_cnt_q == '0 |=> acc_mem_cnt_q != '1, clk_i, !rst_ni)

  `ASSERT(MemoryStoreOperationCounterRollover,
    acc_mem_str_cnt_q == '0 |=> acc_mem_str_cnt_q != '1, clk_i, !rst_ni)

endmodule: spatz_fpu_sequencer
