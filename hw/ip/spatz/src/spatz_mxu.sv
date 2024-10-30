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
  input  logic             clear_mxu_state_i,
  input  vrf_data_t  [2:0] operands_i,
  input  logic             enable_mx_i,
  input  logic             enable_fpu_i,
  input  logic             result_valid_i,
  input  logic       [2:0] operands_ready_i,
  input  logic             vrf_wvalid_i,
  input  vlen_t            vl_i,
  input  vrf_data_t        result_i,
  input  vlen_t            last_word_i,
  input  tile_k_t          tile_dimK,
  input  tile_n_t          tile_dimN,
  input  tile_m_t          tile_dimM,
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

  tile_m_t num_rows;
  tile_n_t num_cols;

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
  logic [NrACCBanks-1:0] accu_result_valid, accu_result_valid_d, accu_result_valid_q;
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

  // Hack to get correct results during fmatmul
  // Delay the internal state clear by some cycles when the instruction changes
  localparam int unsigned CLEAR_MXU_STATE_DELAY = 3;
  logic [$clog2(CLEAR_MXU_STATE_DELAY)-1:0] mxu_cnt_q, mxu_cnt_d;
  logic mxu_cnt_en_q, mxu_cnt_en_d;
  // Delayed clear of the internal state
  logic clear_mxu_state_del;

  // Counters to track writes to vrf
  logic mx_to_write_vrf_d, mx_to_write_vrf_q;
  logic [3:0] write_cnt_d, write_cnt_q, write_limit;
  assign write_limit = 4 * (tile_dimN/4) * (tile_dimM/4);

  always_comb begin
    mxu_cnt_d             = mxu_cnt_q;
    mxu_cnt_en_d          = mxu_cnt_en_q;
    clear_mxu_state_del   = 1'b0;

    if (mxu_cnt_en_q) begin
      mxu_cnt_d = mxu_cnt_q + 1;
      if (mxu_cnt_q == (CLEAR_MXU_STATE_DELAY - 1)) begin
        mxu_cnt_d           = '0;
        mxu_cnt_en_d        = 1'b0;
        clear_mxu_state_del = 1'b1;
      end
    end

    if (clear_mxu_state_i)
      mxu_cnt_en_d = 1'b1;
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      mxu_cnt_en_q <= '0;
      mxu_cnt_q    <= '0;
      accu_result_valid_q <= '0;
      mx_to_write_vrf_q <= 1'b0;
      write_cnt_q <= '0;
    end else begin
      mxu_cnt_en_q <= mxu_cnt_en_d;
      mxu_cnt_q    <= mxu_cnt_d;
      accu_result_valid_q <= accu_result_valid_d;
      mx_to_write_vrf_q <= mx_to_write_vrf_d;
      write_cnt_q <= write_cnt_d;
    end
  end

  // Manage FSM
  `FF(block_q, block_d, first)
  // mx_write_en_d, _q, _qq (last write to VRF)
  // Clear the internal state upon a new instruction
  `FFLARNC(mx_write_enable_q[0], mx_write_enable_d   , enable_mx_i, clear_mxu_state_del, '0, clk_i, rst_ni)
  `FFLARNC(mx_write_enable_q[1], mx_write_enable_q[0], enable_mx_i, clear_mxu_state_del, '0, clk_i, rst_ni)
  `FFLARNC(mx_write_enable_q[2], mx_write_enable_q[1], enable_mx_i, clear_mxu_state_del, '0, clk_i, rst_ni)
  // Row input FPU operation counter
  `FF(col_counter, enable_mx_i ? ipu_en ? col_counter + 1 : col_counter : 0, '0)
  // Row output FPU latch/vrf accumulator counter
  `FF(acc_counter, enable_mx_i ? result_valid_i && result_ready_o ? acc_counter + 1 : acc_counter : 0, '0)
  // Save operands_i as current operands every time we get new operands
  `FFL(current_operands_q[1:0], current_operands_d[1:0], enable_mx_i && &operands_ready_i[1:0], '0)
  // Save operands_i as previous operands every time we get new operands and we are starting a new col
  `FFL(previous_operands_q, previous_operands_d[1:0], enable_mx_i && &operands_ready_i[1:0] && part_col == 0, '0)

  always_ff @(posedge clk_i) begin: proc_wdata_q
      // Save the FPU result in a FF before going into the latch
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
        // Todo: parametrize me
        // Load as many vd as (M / OperandsPerVRFFetch)
        load_vd  = num_rows == 4 ? vl_i <= 0 : vl_i <= 4;
        // mtx_A row counter from 0 to M
        part_col = num_cols == 4 ? num_rows == 4 ? col_counter[1:0] : col_counter[2:0] : col_counter;
        // Accumulator counter from 0 to M
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
      // operadns_ready_i is asserted only for one cycle
      current_operands_d[1:0] = &operands_ready_i[1:0] ? operands_i[1:0] : current_operands_q[1:0];
      previous_operands_d     = &operands_ready_i[1:0] && part_col == 0 ? operands_i[1:0] : previous_operands_q[1:0];
      // Sending the current A quadword for the last time to the FPU
      // Proceed to the next A quadword
      next_block              = &part_col[1:0] && ipu_en;
      // Operands FSM
      case (block_q)
        first : begin
          operand1       = previous_operands_d[1];
          operand2       = previous_operands_d[0];
          block_d        = next_block && num_rows != 4 ? second : block_q;
        end
        second : begin
          operand1 = current_operands_d[1];
          operand2 = previous_operands_d[0];
          block_d  = next_block ? num_cols == 4 ? first : third : block_q;
        end
        third : begin
          operand1 = previous_operands_d[1];
          operand2 = current_operands_d[0];
          block_d  = next_block ? forth : block_q;
        end
        forth : begin
          operand1 = current_operands_d[1];
          operand2 = current_operands_d[0];
          block_d  = next_block ? first : block_q;
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
    mx_write_enable_d = 1'b0;
    mx_to_write_vrf_d = mx_to_write_vrf_q;
    write_cnt_d = write_cnt_q;

    // Write back into the VRF if we have processed all the words
    // and we got a valid result
    
    // Start writes to VRF once we have send the last operands to the VFU
    mx_to_write_vrf_d = mx_to_write_vrf_q ? 1'b1 : (vl_i >= last_word_i) & word_commited_o;
    if (mx_to_write_vrf_d) begin
      // Start writing to VRF once we have the part_acc pointing to 0
      automatic logic write_en = (write_cnt_q == 0) ? (part_acc == 0) : 1'b1;
      mx_write_enable_d = result_valid_i & write_en;
      if (mx_write_enable_d) begin
        write_cnt_d = write_cnt_q + 1;
        if (write_cnt_q == (write_limit-1)) begin
          write_cnt_d = '0;
          mx_to_write_vrf_d = 1'b0;
        end
      end
    end

    // If the result from FPU is not to be written to the VRF, then store it in the accumulators
    for (int accreg=0; accreg < NrACCBanks; accreg++) begin
      accu_result_valid_d[accreg] = accu_result_valid_q[accreg] ? 1'b1 : ~mx_write_enable_d & waddr_onehot[accreg];
    end

    if (enable_mx_i) begin
      // Enable a read if we need an operand
      // If the accumulators are to be used i.e. load_vd=1'b0 then also check that the accumulators have a valid result
      mx_read_enable[1:0] = (load_vd | (~load_vd & accu_result_valid_q[part_col])) ? {2{part_col == 0 || part_col == 4}} : 2'b0;
      if ((~load_vd & accu_result_valid_q[part_col] & ipu_en))
        accu_result_valid_d[part_col] = 1'b0;
      mx_read_enable[2]   = load_vd;
    end 
  end

  //Enable when reaching first element of row and operands ready.
  //If in the first word also wait for vd ready.

  ////////////////
  // MXU -> VRF //
  ////////////////

  // Request an operand from the VRF
  assign read_enable_o   = enable_mx_i ? mx_read_enable : '0;
  // We have consumed one A quadword
  assign word_commited_o = enable_mx_i ? ipu_en && (part_col == 3 || (num_cols == 4 && part_col == 7) || part_col == 15) : '0;

  ////////////////
  // VRF -> MXU //
  ////////////////

  // Get an operand from the VRF
  // operands_ready_i : vrf_rdata_valid_o
  // operands_i       : vrf_rdata_o

  ////////////////
  // MXU -> FPU //
  ////////////////

  // Feed the functional units
  // ipu_en    : fpu_valid_i
  // operand_o : fpu_operands_i
  assign ipu_en          = enable_mx_i ? (|mx_read_enable ? |operands_ready_i :  accu_result_valid_q[part_col]) : '0;
  assign ipu_en_o        = enable_mx_i ? ipu_en : '0;
  assign operand_o       = enable_mx_i ? {operand3, operand_row, operand2} : 'x;

  ////////////////
  // FPU -> MXU //
  ////////////////

  // Receive a result from the FPUs
  // result_i       : fpu_result_o
  // result_valid_i : fpu_valid_o
  // result_ready_o : fpu_ready_i
  assign result_ready_o  = enable_mx_i ? (|mx_read_enable ? |operands_ready_i : '1) || vrf_wvalid_i : '0;

  ////////////////
  // MXU -> VRF //
  ////////////////

  // write_enable_o : vrf_wreq_i
  assign write_enable_o = mx_write_enable_d;

  ////////////////////
  // VRF addressing //
  ////////////////////

  // offset_o offsets the vrf_addr_i
  assign offset_o        = enable_mx_i ? mx_write_enable_q[0] ? part_col : part_col : '0;

endmodule
