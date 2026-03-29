// Copyright 2024 EPFL
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// Author: Danilo Cammarata
package quadrilatero_pkg;

  localparam int unsigned N_REGS     = 16;
  localparam int unsigned DATA_WIDTH = 32;
  localparam int unsigned REG_PER_CE =  4;

  localparam int unsigned RLEN = ${cfg['rlen']};; // change register dimension
  localparam int unsigned ALEN = RLEN;            // change systolic array dimension
  localparam int unsigned LLEN = ${cfg['llen']};; // change for different bus width

  localparam int unsigned MESH_WIDTH    = RLEN/DATA_WIDTH;  
  localparam int unsigned SA_MESH_WIDTH = ALEN/DATA_WIDTH;
  localparam int unsigned N_ROWS        = RLEN/DATA_WIDTH;

  localparam int unsigned NUM_EXEC_UNITS =   4;  // change me to add units

  localparam int unsigned LSU_PORTS     = LLEN/RLEN    ;
  localparam int unsigned READ_PORTS    = 2 + LSU_PORTS;
  localparam int unsigned WRITE_PORTS   = 1 + LSU_PORTS;
  localparam int unsigned N_PORTS       = READ_PORTS + WRITE_PORTS;
  localparam int unsigned LOG_N_PORTS   = $ceil($clog2(N_PORTS));
  localparam int unsigned LOG_LSU_PORTS = LSU_PORTS > 1 ? $clog2(LSU_PORTS) : 1;
  
  localparam fpnew_pkg::fpu_features_t RV32_QUAD = '{
    Width:         32,
    EnableVectors: 1'b0,
    EnableNanBox:  1'b1,
    FpFmtMask:     6'b101100,
    IntFmtMask:    4'b0010
  };

  localparam fpnew_pkg::fpu_implementation_t FPUImplementation [1] = '{
    '{
        PipeRegs: // FMA Block
                  '{// FP32 FP64 FP16 FP8 FP16alt FP8alt
                    '{   4,   0,   0,  0,   0,      0   },   // FMA Block
                    '{   0,   0,   0,  0,   0,      0   },   // DIVSQRT
                    '{   0,   0,   0,  0,   0,      0   },   // NONCOMP
                    '{   0,   0,   0,  0,   0,      0   },   // CONV
                    '{   0,   0,   0,  0,   0,      0   }    // DOTP
                    },
        UnitTypes: '{'{fpnew_pkg::PARALLEL,
                       fpnew_pkg::DISABLED,
                       fpnew_pkg::DISABLED,
                       fpnew_pkg::DISABLED,
                       fpnew_pkg::DISABLED,
                       fpnew_pkg::DISABLED},  // FMA
                    '{fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED}, // DIVSQRT
                    '{fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED}, // NONCOMP
                    '{fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED},   // CONV
                    '{fpnew_pkg::MERGED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED}},  // DOTP
        PipeConfig: fpnew_pkg::BEFORE
    }
  };

  typedef logic [xif_pkg::X_ID_WIDTH-1:0] id_t     ;
  typedef logic [$clog2(REG_PER_CE) -1:0] acc_reg_t;
  typedef logic [$clog2(N_REGS    ) -1:0] src_reg_t;
  typedef logic [$clog2(N_ROWS    ) -1:0] row_t    ;

  typedef enum logic      {SEL1_ACT, SEL1_ACC } sel_op1_e;
  typedef enum logic[1:0] {SEL2_WGT, SEL2_ACC } sel_op2_e;
  typedef enum logic      {SEL3_ACC, SEL3_ZERO} sel_op3_e;

  typedef enum logic [$clog2(NUM_EXEC_UNITS)-1:0] {
    FU_SYSTOLIC_ARRAY = 0,
    FU_LSU,
    FU_RF,
    FU_CFG
  } execution_units_t;

  typedef struct packed {
    logic                  write  ;
    logic[LOG_N_PORTS-1:0] port_id;
  } sb_req_t;
  typedef struct packed {
    logic [31:0] data;
    logic [ 4:0] rd  ;
    id_t         id  ;
  } xif_result_t;
  typedef struct packed {
    sel_op1_e                     sel_op1        ;
    sel_op2_e                     sel_op2        ;
    sel_op3_e                     sel_op3        ;
    acc_reg_t                     md             ;
    fpnew_pkg::roundmode_e        rnd_mode       ;
    fpnew_pkg::operation_e        op             ;
    logic                         op_mod         ;
    fpnew_pkg::fp_format_e        src_fmt        ;
    fpnew_pkg::fp_format_e        dst_fmt        ;
    fpnew_pkg::int_format_e       int_fmt        ;
    logic                         vectorial_op   ;
    logic                         simd_mask      ;
  } cfg_fpu_t;
  typedef struct packed {
    logic ext_ld         ;
    logic finished       ;
    logic first_iteration;
    logic stall          ;
  } ctrl_fpu_t;
  typedef struct packed {
    // logic     wen      ;
    logic     rd       ;
    // acc_reg_t acc_waddr;
    acc_reg_t acc_raddr;
    row_t     row      ;
  } ctrl_acc_t;
  typedef struct packed {
    acc_reg_t cmul   ;
    acc_reg_t rmul   ;
    src_reg_t act_reg;
    id_t      id     ;
    logic     zero   ;
  } cfg_act_t;
  typedef struct packed {
    acc_reg_t cmul   ;
    acc_reg_t rmul   ;
    src_reg_t wgt_reg;
    id_t      id     ;
    logic     zero   ;
  } cfg_wgt_t;
  typedef struct packed {
    acc_reg_t cmul    ;
    acc_reg_t rmul    ;
    src_reg_t md      ;
    acc_reg_t acc_rreg;
    id_t mac_last_id  ;
    id_t id           ;
  } cfg_move_t;
  typedef struct packed {
    acc_reg_t cmul    ;
    acc_reg_t rmul    ;
    logic[$clog2(N_ROWS)+2:0] mtypeK;
    logic[$clog2(RLEN):0]     mtypeM;
    logic[$clog2(RLEN):0]     mtypeN;
    logic[1:0] datawidthA      ;
    logic      is_floatA       ;
    logic      is_altfmtA      ;
    logic      is_mxA          ;
    logic[1:0] datawidthB      ;
    logic      is_floatB       ;
    logic      is_altfmtB      ;
    logic      is_mxB          ;
    logic[1:0] datawidthC      ;
    logic      is_floatC       ;
    logic      is_altfmtC      ;
    logic      is_mxC          ;
  } mcfg_t;
  typedef struct packed {
    logic      [      N_REGS -1:0] mrf_read  ;
    logic      [      N_REGS -1:0] mrf_write ;
    logic[2:0][$clog2(N_REGS)-1:0] mreg      ;
    logic[REG_PER_CE -1:0] acc_write;
    logic      acc_read       ;
    logic      is_store       ;
    logic      is_rhs         ;
    logic      is_zero        ;
    logic      is_mac         ;
    logic      is_move        ;
    mcfg_t     mcfg           ;
    execution_units_t eu      ;
    xif_result_t     result   ;
    logic            result_we;
    logic[$clog2(RLEN)-1:0]             n_col_bytes   ;
    logic[$clog2(RLEN):0]               n_rows        ;
    logic[N_REGS -1:0][LOG_N_PORTS-1:0] port_id       ;
    logic[1:0]                          sa_sel_demux  ;
    logic[LOG_LSU_PORTS-1:0]            lsu_sel_demux ;

    // XIF signals
    logic [xif_pkg::X_NUM_RS-1:0][xif_pkg::X_RFR_WIDTH-1:0] rs;
    logic [xif_pkg::X_NUM_RS-1:0]                           rs_valid;
    logic [xif_pkg::X_ID_WIDTH-1:0]                         id;
    logic [1:0]                                             mode;
  } dec_instr_t;

  typedef struct packed {
    src_reg_t ms1      ;
    src_reg_t ms2      ;
    src_reg_t md       ;
    acc_reg_t acc_rreg ;
    acc_reg_t acc_wreg ;
    logic     acc_read ;
    logic     acc_write;
    acc_reg_t cmul     ;
    acc_reg_t rmul     ;
    id_t      id       ;
    sel_op1_e sel_op1  ;
    sel_op2_e sel_op2  ;
    logic     is_float ;
    logic     is_zero  ;
    logic     is_move  ;
    fpnew_pkg::roundmode_e  rnd_mode       ;
    fpnew_pkg::operation_e  op             ;
    logic                   op_mod         ;
    fpnew_pkg::fp_format_e  src_fmt        ;
    fpnew_pkg::fp_format_e  dst_fmt        ;
    fpnew_pkg::int_format_e int_fmt        ;
    logic                   vectorial_op   ;
    logic                   simd_mask      ;
  } sa_instr_t;

  typedef struct packed {
    src_reg_t ms1      ;
    src_reg_t ms2      ;
    acc_reg_t acc_wreg ;
    logic     acc_write;
    acc_reg_t cmul     ;
    acc_reg_t rmul     ;
    id_t      id       ;
    sel_op1_e sel_op1  ;
    sel_op2_e sel_op2  ;
    sel_op3_e sel_op3  ;
    logic     is_float ;
    logic     first    ;
    logic     done     ;
    logic     ms1_zero ;
    logic     ms2_zero ;
    fpnew_pkg::roundmode_e  rnd_mode     ;
    fpnew_pkg::operation_e  op           ;
    logic                   op_mod       ;
    fpnew_pkg::fp_format_e  src_fmt      ;
    fpnew_pkg::fp_format_e  dst_fmt      ;
    fpnew_pkg::int_format_e int_fmt      ;
    logic                   vectorial_op ;
    logic                   simd_mask    ;
  } mac_instr_t;
  typedef struct packed {
    src_reg_t md         ;
    acc_reg_t acc_rreg   ;
    logic     acc_read   ;
    acc_reg_t cmul       ;
    acc_reg_t rmul       ;
    id_t      id         ;
    id_t      mac_last_id;
    logic     is_move    ;
  } move_instr_t;
  typedef struct packed {
    acc_reg_t acc_wreg ;
    acc_reg_t cmul     ;
    acc_reg_t rmul     ;
    id_t      id       ;
    logic     is_zero  ;
  } zero_instr_t;
  typedef struct packed {
    logic [31:0] stride     ;
    logic [31:0] addr       ;
    src_reg_t    operand_reg;
    id_t         id         ;
    logic        is_store   ;
    logic        zero       ;
    logic        is_rhs     ;
    acc_reg_t    cmul       ;
    acc_reg_t    rmul       ;
    logic[$clog2(RLEN)-1:0]   n_col_bytes;
    logic[$clog2(RLEN):0]     n_rows     ;
  } lsu_instr_t;

  typedef enum logic [LOG_N_PORTS-1:0] {
    SA_W_R = 0,   // Read  Port : Systolic Array Weight
    SA_D_R = 1,   // Read  Port : Systolic Array Data
    SA_A_W = 2,   // Write Port : Systolic Array Accumulator
    LSU_R  = 3,   // Read  Port : Load-Store Unit
    LSU_W  = 4    // Write Port : Load-Store Unit
                  // ...
  } ports_e;
endpackage
