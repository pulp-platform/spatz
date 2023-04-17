# Copyright 2021 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

# Create group for core $1

add wave -noupdate -group core[$1] -group Params /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/BootAddr
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/clk_i
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/rst_i
add wave -noupdate -group core[$1] -radix unsigned /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/hart_id_i

add wave -noupdate -group core[$1] -divider Instructions
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/inst_addr_o
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/inst_data_i
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/inst_valid_o
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/inst_ready_i

add wave -noupdate -group core[$1] -divider Load/Store
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/data_req_o
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/data_rsp_i

add wave -noupdate -group core[$1] -divider Accelerator
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/acc_qreq_o
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/acc_qrsp_i
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/acc_qvalid_o
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/acc_qready_i
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/acc_prsp_i
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/acc_pvalid_i
add wave -noupdate -group core[$1] /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/acc_pready_o

add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/illegal_inst
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/stall
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/lsu_stall
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/acc_stall
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/zero_lsb
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/pc_d
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/pc_q
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/wfi_d
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/wfi_q
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/fcsr_d
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/fcsr_q
add wave -noupdate -group core[$1] -group Snitch -divider LSU
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/ls_size
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/ls_amo
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/ld_result
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/lsu_qready
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/lsu_qvalid
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/lsu_pvalid
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/lsu_pready
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/lsu_rd
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/retire_load
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/retire_i
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/retire_acc
add wave -noupdate -group core[$1] -group Snitch -divider ALU
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/opa
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/opb
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/iimm
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/uimm
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/jimm
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/bimm
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/simm
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/adder_result
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/alu_result
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/rd
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/rs1
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/rs2
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/gpr_raddr
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/gpr_rdata
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/gpr_waddr
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/gpr_wdata
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/gpr_we
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/consec_pc
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/sb_d
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/sb_q
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/is_load
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/is_store
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/is_signed
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/ls_misaligned
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/ld_addr_misaligned
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/st_addr_misaligned
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/valid_instr
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/exception
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/alu_op
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/opa_select
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/opb_select
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/write_rd
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/uses_rd
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/next_pc
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/rd_select
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/rd_bypass
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/is_branch
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/csr_rvalue
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/csr_en
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/acc_register_rd
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/operands_ready
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/dst_ready
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/opa_ready
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/opb_ready
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/shift_opa
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/shift_opa_reversed
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/shift_right_result
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/shift_left_result
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/shift_opa_ext
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/shift_right_result_ext
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/shift_left
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/shift_arithmetic
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/alu_opa
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/alu_opb
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/alu_writeback
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/acc_mem_cnt_d
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/acc_mem_cnt_q
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/acc_mem_str_cnt_d
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/acc_mem_str_cnt_q
add wave -noupdate -group core[$1] -group Snitch /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/core_events_o

add wave -noupdate -group core[$1] -group Snitch -group Internal -group RF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/i_snitch_regfile/*
add wave -noupdate -group core[$1] -group Snitch -group Internal /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_snitch/*

add wave -noupdate -group core[$1] -group Spatz /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/issue_valid_i
add wave -noupdate -group core[$1] -group Spatz /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/issue_ready_o
add wave -noupdate -group core[$1] -group Spatz /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/issue_req_i
add wave -noupdate -group core[$1] -group Spatz /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/issue_rsp_o
add wave -noupdate -group core[$1] -group Spatz /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/rsp_valid_o
add wave -noupdate -group core[$1] -group Spatz /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/rsp_ready_i
add wave -noupdate -group core[$1] -group Spatz /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/rsp_o
add wave -noupdate -group core[$1] -group Spatz /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/spatz_mem_req_o
add wave -noupdate -group core[$1] -group Spatz /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/spatz_mem_req_valid_o
add wave -noupdate -group core[$1] -group Spatz /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/spatz_mem_req_ready_i
add wave -noupdate -group core[$1] -group Spatz /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/spatz_mem_rsp_i
add wave -noupdate -group core[$1] -group Spatz /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/spatz_mem_rsp_valid_i

add wave -noupdate -group core[$1] -group Spatz -group "FPU Sequencer" /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/gen_fpu_sequencer/i_fpu_sequencer/*
add wave -noupdate -group core[$1] -group Spatz -group "FPU Sequencer" -group FPR /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/gen_fpu_sequencer/i_fpu_sequencer/i_fpr/*
add wave -noupdate -group core[$1] -group Spatz -group "FPU Sequencer" -group LSU /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/gen_fpu_sequencer/i_fpu_sequencer/i_fp_lsu/*

add wave -noupdate -group core[$1] -group Spatz -group Controller /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_controller/*

add wave -noupdate -group core[$1] -group Spatz -group VRF -divider RegisterWrite
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/waddr_i
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/wdata_i
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/we_i
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/wbe_i
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/wvalid_o
add wave -noupdate -group core[$1] -group Spatz -group VRF -divider RegisterRead
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/raddr_i
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/rdata_o
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/re_i
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/rvalid_o
add wave -noupdate -group core[$1] -group Spatz -group VRF -divider Internal
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/waddr
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/wdata
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/we
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/wbe
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/raddr
add wave -noupdate -group core[$1] -group Spatz -group VRF /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vrf/rdata

add wave -noupdate -group core[$1] -group Spatz -group VLSU /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vlsu/*

add wave -noupdate -group core[$1] -group Spatz -group VSLDU /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vsldu/*

add wave -noupdate -group core[$1] -group Spatz -group VFU /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/i_spatz/i_vfu/*

add wave -noupdate -group core[$1] -group Internal /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[$1]/i_spatz_cc/*
