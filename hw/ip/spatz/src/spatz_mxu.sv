// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Enis Mustafa, ETH Zurich
//

module spatz_mxu
  import spatz_pkg::*; import rvv_pkg::*;
(
  input  logic             clk_i,
  input  logic             rst_ni,
  input  vrf_data_t  [2:0] operands_i,
  input  logic             enable_mx_i,
  input  logic             enable_fpu_i,
  input  logic             result_valid_i,
  input  logic       [2:0] operands_ready_i,
  input  logic             vrf_wvalid_i,
  input  vlen_t            vl_i,
  input  vrf_data_t        result_i,
  input  vlen_t            last_word_i,
  input  elen_t            tile_dimK,
  input  elen_t            tile_dimN,
  input  elen_t            tile_dimM,
  output vrf_data_t  [2:0] operand_o,
  output logic       [2:0] read_enable_o,
  output logic             write_enable_o,
  output logic             word_commited_o,
  output logic             ipu_en_o,
  output logic             result_ready_o,
  output vlen_t            offset_o
);

  `include "common_cells/registers.svh"

  typedef enum logic [1:0]{
    first, second, third, forth
  } operand_fsm_t;

  elen_t num_cols;
  elen_t num_rows;

  operand_fsm_t block_q;
  operand_fsm_t block_d;
  logic         next_block;

  logic [2:0] mx_read_enable;
  logic       mx_write_enable_d;
  logic [2:0] mx_write_enable_q;

  vrf_data_t operand1;
  vrf_data_t operand2;
  vrf_data_t operand3;
  vrf_data_t operand_row;

  //Accumulator signal
  vrf_data_t [NrACCBanks-1:0] accu_result_q;
  vrf_data_t wdata_q;
  logic [NrACCBanks-1:0] waddr_onehot;

  logic             load_vd;
  logic             ipu_en;
  logic       [3:0] col_counter, part_col;
  logic       [3:0] acc_counter, part_acc;
  vrf_data_t  [1:0] current_operands_q;
  vrf_data_t  [1:0] current_operands_d;
  vrf_data_t  [1:0] previous_operands_q;
  vrf_data_t  [1:0] previous_operands_d;

  `FF(block_q, block_d, first)
  `FFL(mx_write_enable_q[0], mx_write_enable_d, enable_mx_i, '0)
  `FFL(mx_write_enable_q[1], mx_write_enable_q[0], enable_mx_i, '0)
  `FFL(mx_write_enable_q[2], mx_write_enable_q[1], enable_mx_i, '0)
  // Row input FPU operation counter
  `FF(col_counter, enable_mx_i ? ipu_en ? col_counter + 1 : col_counter : 0, '0)
  `FF(acc_counter, enable_mx_i ? result_valid_i && result_ready_o ? acc_counter + 1 : acc_counter : 0, '0)
  `FFL(current_operands_q[1:0], current_operands_d[1:0], enable_mx_i && &operands_ready_i[1:0], '0)
  `FFL(previous_operands_q, previous_operands_d[1:0], enable_mx_i && &operands_ready_i[1:0] && part_col == 0, '0)

  always_ff @(posedge clk_i) begin: proc_wdata_q
      wdata_q <= result_i;
    end: proc_wdata_q

    // Select which destination bytes to write into
    for (genvar accreg = 0; accreg < NrACCBanks; accreg++) begin: gen_waddr_onehot
      // Create latch clock signal
      assign waddr_onehot[accreg] = enable_mx_i && result_valid_i && accreg == part_acc;
    end: gen_waddr_onehot

    // Store result into accumulator
    for (genvar accreg = 0; accreg < NrACCBanks; accreg++) begin: gen_write_mem
        logic clk_latch;

        tc_clk_gating i_accumulator_cg (
          .clk_i    (clk_i                  ),
          .en_i     (waddr_onehot[accreg]   ),
          .test_en_i(1'b0                   ),
          .clk_o    (clk_latch              )
        );

        /* verilator lint_off NOLATCH */
        always_latch begin
          if (clk_latch)
            accu_result_q[accreg] <= wdata_q;
        end
        /* verilator lint_on NOLATCH */
        
    end: gen_write_mem

  always_comb begin
    load_vd = 0;
    num_cols = 0;
    num_rows = 0;
    part_col = 0;
    part_acc = 0;

    if(enable_mx_i) begin
        num_cols = tile_dimN;
        num_rows = tile_dimM;
        load_vd = num_rows == 4 ? vl_i <= 0 : vl_i <= 4;
        part_col = num_cols == 4 ? num_rows == 4 ? col_counter[1:0] : col_counter[2:0] : col_counter;
        part_acc = num_cols == 4 ? num_rows == 4 ? acc_counter[1:0] : acc_counter[2:0] : acc_counter;
    end
  end

  always_comb begin : operand_proc
    block_d                 = block_q;
    operand_row             = '0;
    operand1                = '0;
    operand2                = '0;
    operand3                = '0;
    current_operands_d[1:0] = '0;
    previous_operands_d     = '0;
    next_block              = '0;

    if(enable_mx_i) begin
      current_operands_d[1:0] = &operands_ready_i[1:0]?operands_i[1:0] : current_operands_q[1:0];
      previous_operands_d     = &operands_ready_i[1:0] && part_col == 0 ? operands_i[1:0] : previous_operands_q[1:0];
      next_block              = &part_col[1:0] && ipu_en;
      //Operands FSM
      case (block_q)
        first : begin
          operand1       = previous_operands_d[1];
          operand2       = previous_operands_d[0];
          block_d        = next_block && num_rows != 4 ? second : block_d;
          fetch_operands = part_col == 0 || part_col == 4;
        end
        second : begin
          operand1 = current_operands_d[1];
          operand2 = previous_operands_d[0];
          block_d  = next_block ? num_cols == 4 ? first : third : block_d;
        end
        third : begin
          operand1 = previous_operands_d[1];
          operand2 = current_operands_d[0];
          block_d  = next_block ? forth : block_d;
        end
        forth : begin
          operand1 = current_operands_d[1];
          operand2 = current_operands_d[0];
          block_d  = next_block ? first : block_d;
        end
        default : begin
          operand1 = previous_operands_d[1];
          operand2 = previous_operands_d[0];
        end
      endcase // block_q

      if(load_vd) begin
        operand3 = operands_i[2];
      end else begin
        operand3 = accu_result_q[part_col];
      end

      //Replicate one element to apply on whole vreg_data
      operand_row = {1*N_FPU{operand1[part_col[1:0]*ELEN +: ELEN]}};
    end
  end

  always_comb begin : rw_enable_proc
    mx_read_enable = '0;
    mx_write_enable_d = '0;
    if (enable_mx_i) begin
      mx_read_enable[1:0] = {2{part_col == 0 || part_col == 4}};
      mx_read_enable[2]   = load_vd;
      mx_write_enable_d   = vl_i >= last_word_i && result_valid_i;
    end
  end

  //Enable when reaching first element of row and operands ready.
  //If in the first word also wait for vd ready.
  assign ipu_en          = enable_mx_i ? load_vd || |mx_read_enable ? |operands_ready_i : result_valid_i : '0;
  assign word_commited_o = enable_mx_i ? ipu_en && (part_col == 3 || (num_cols == 4 && part_col == 7) || part_col == 15) : '0;
  assign read_enable_o   = enable_mx_i ? mx_read_enable : '0;
  assign operand_o       = enable_mx_i ? {operand3, operand_row, operand2} : 'x;
  assign ipu_en_o        = enable_mx_i ? ipu_en : '0;
  assign offset_o        = enable_mx_i ? mx_write_enable_q[0] ? part_col : part_col : '0;
  assign result_ready_o  = enable_mx_i ? (|mx_read_enable ? |operands_ready_i : '1) || vrf_wvalid_i : '0;
  assign write_enable_o  = enable_mx_i ? (enable_fpu_i ? mx_write_enable_q[2] : mx_write_enable_q[0]) : '0;
endmodule
