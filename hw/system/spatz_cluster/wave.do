onerror {resume}
quietly WaveActivateNextPane {} 0
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/clk_i}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/rst_ni}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/testmode_i}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/hart_id_i}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/issue_valid_i}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/issue_ready_o}
add wave -noupdate -group i_spatz -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/issue_req_i}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/issue_rsp_o}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/rsp_valid_o}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/rsp_ready_i}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/rsp_o}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/spatz_mem_req_o}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/spatz_mem_req_valid_o}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/spatz_mem_req_ready_i}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/spatz_mem_rsp_i}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/spatz_mem_rsp_valid_i}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/spatz_mem_finished_o}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/spatz_mem_str_finished_o}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/fp_lsu_mem_req_o}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/fp_lsu_mem_rsp_i}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/fpu_rnd_mode_i}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/fpu_fmt_mode_i}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/fpu_status_o}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/spatz_req}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/spatz_req_valid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vfu_req_ready}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vfu_rsp_ready}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vfu_rsp_valid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vfu_rsp}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vlsu_req_ready}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vlsu_rsp_valid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vlsu_rsp}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vsldu_req_ready}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vsldu_rsp_valid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vsldu_rsp}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/issue_req}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/issue_valid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/issue_ready}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/issue_rsp}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/resp}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/resp_valid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/resp_ready}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/fp_lsu_mem_finished}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/fp_lsu_mem_str_finished}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/spatz_mem_finished}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/spatz_mem_str_finished}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_waddr}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_wdata}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_we}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_wbe}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_wvalid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_raddr}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_re}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_rdata}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_rvalid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/sb_re}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/sb_we}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/sb_id}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/clk_i}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/rst_ni}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/hart_id_i}
add wave -noupdate -group i_vfu -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/spatz_req_i}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/spatz_req_valid_i}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/spatz_req_ready_o}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vfu_rsp_valid_o}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vfu_rsp_ready_i}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vfu_rsp_o}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vrf_waddr_o}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vrf_wdata_o}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vrf_we_o}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vrf_wbe_o}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vrf_wvalid_i}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vrf_id_o}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vrf_raddr_o}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vrf_re_o}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vrf_rdata_i}
add wave -noupdate -group i_vfu -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vrf_rvalid_i}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/fpu_status_o}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/fpu_load_ready}
add wave -noupdate -group i_vfu -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/spatz_req}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/spatz_req_valid}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/spatz_req_ready}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vl_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vl_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/busy_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/busy_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/nr_elem_word}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/state_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/state_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/ipu_result_tag}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/fpu_result_tag}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/result_tag}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/input_tag}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vstart}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/stall}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_operand_ready_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_operand_ready_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/op1_is_ready}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/op2_is_ready}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/op3_is_ready}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/operands_ready}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/valid_operations}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/pending_results}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/word_issued}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/running_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/running_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/is_fpu_insn}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/is_fpu_busy}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/is_ipu_busy}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/scalar_result}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/last_request}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_state_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_state_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_done}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/narrowing_upper_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/narrowing_upper_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/widening_upper_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/widening_upper_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/result}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/result_valid}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/result_ready}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/ipu_result}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/ipu_result_valid}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/ipu_in_ready}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/fpu_result}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/fpu_result_valid}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/fpu_in_ready}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/operand1}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/operand2}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/operand3}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/in_ready}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_pointer_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_pointer_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_operand_request}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vreg_wbe}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vreg_we}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vreg_r_req}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vreg_addr_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vreg_addr_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vreg_wdata}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/int_ipu_in_ready}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/int_ipu_operand1}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/int_ipu_operand2}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/int_ipu_operand3}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/int_ipu_result}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/int_ipu_result_tag}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/int_ipu_result_valid}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/int_ipu_result_ready}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/int_ipu_busy}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/ipu_wide_operand1}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/ipu_wide_operand2}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/ipu_wide_operand3}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/clk_i}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/rst_ni}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/issue_valid_i}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/issue_ready_o}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/issue_req_i}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/issue_rsp_o}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/rsp_valid_o}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/rsp_ready_i}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/rsp_o}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/fpu_rnd_mode_i}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/fpu_fmt_mode_i}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/spatz_req_valid_o}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/spatz_req_o}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vfu_req_ready_i}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vfu_rsp_valid_i}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vfu_rsp_ready_o}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vfu_rsp_i}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vlsu_req_ready_i}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vlsu_rsp_valid_i}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vlsu_rsp_i}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vsldu_req_ready_i}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vsldu_rsp_valid_i}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vsldu_rsp_i}
add wave -noupdate -expand -group i_controller -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/sb_enable_i}
add wave -noupdate -expand -group i_controller -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/sb_wrote_result_i}
add wave -noupdate -expand -group i_controller -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/sb_enable_o}
add wave -noupdate -expand -group i_controller -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/sb_id_i}
add wave -noupdate -expand -group i_controller -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/spatz_req}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/spatz_req_valid}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/spatz_req_illegal}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vstart_d}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vstart_q}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vl_d}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vl_q}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vtype_d}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vtype_q}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/decoder_req}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/decoder_req_valid}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/decoder_rsp}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/decoder_rsp_valid}
add wave -noupdate -expand -group i_controller -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/buffer_spatz_req}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/req_buffer_ready}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/req_buffer_valid}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/req_buffer_pop}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/read_table_d}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/read_table_q}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/write_table_d}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/write_table_q}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/noChainStop}
add wave -noupdate -expand -group i_controller -expand -subitemconfig {{/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/scoreboard_q[1]} -expand} {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/scoreboard_q}
add wave -noupdate -expand -group i_controller -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/scoreboard_d}
add wave -noupdate -expand -group i_controller -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/wrote_result_q}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/wrote_result_d}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/narrow_wide_q}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/narrow_wide_d}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/wrote_result_narrowing_q}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/wrote_result_narrowing_d}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/retire_csr}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/stall}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vfu_stall}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vlsu_stall}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vsldu_stall}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/running_insn_d}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/running_insn_q}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/next_insn_id}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/running_insn_full}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vfu_rsp}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vfu_rsp_valid}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/vfu_rsp_ready}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/rsp_valid_d}
add wave -noupdate -expand -group i_controller {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/rsp_d}
add wave -noupdate -group i_decoder {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/i_decoder/clk_i}
add wave -noupdate -group i_decoder {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/i_decoder/rst_ni}
add wave -noupdate -group i_decoder -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/i_decoder/decoder_req_i}
add wave -noupdate -group i_decoder {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/i_decoder/decoder_req_valid_i}
add wave -noupdate -group i_decoder -expand -subitemconfig {{/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/i_decoder/decoder_rsp_o.spatz_req} -expand} {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/i_decoder/decoder_rsp_o}
add wave -noupdate -group i_decoder {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/i_decoder/decoder_rsp_valid_o}
add wave -noupdate -group i_decoder {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/i_decoder/fpu_rnd_mode_i}
add wave -noupdate -group i_decoder {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/i_decoder/fpu_fmt_mode_i}
add wave -noupdate -group i_decoder {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/i_decoder/illegal_instr}
add wave -noupdate -group i_decoder {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/i_decoder/reset_vstart}
add wave -noupdate -group i_decoder {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_controller/i_decoder/spatz_req}
add wave -noupdate -expand -group i_vrf {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/clk_i}
add wave -noupdate -expand -group i_vrf {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/rst_ni}
add wave -noupdate -expand -group i_vrf {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/testmode_i}
add wave -noupdate -expand -group i_vrf -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/waddr_i}
add wave -noupdate -expand -group i_vrf -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/wdata_i}
add wave -noupdate -expand -group i_vrf -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/we_i}
add wave -noupdate -expand -group i_vrf {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/wbe_i}
add wave -noupdate -expand -group i_vrf -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/wvalid_o}
add wave -noupdate -expand -group i_vrf {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/raddr_i}
add wave -noupdate -expand -group i_vrf -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/re_i}
add wave -noupdate -expand -group i_vrf -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/rdata_o}
add wave -noupdate -expand -group i_vrf {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/rvalid_o}
add wave -noupdate -expand -group i_vrf {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/waddr}
add wave -noupdate -expand -group i_vrf {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/wdata}
add wave -noupdate -expand -group i_vrf {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/we}
add wave -noupdate -expand -group i_vrf {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/wbe}
add wave -noupdate -expand -group i_vrf {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/raddr}
add wave -noupdate -expand -group i_vrf {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/rdata}
add wave -noupdate -expand -group i_vrf {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/write_request}
add wave -noupdate -expand -group i_vrf {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/read_request}
add wave -noupdate -label mem00 -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[0]/gen_vrf_slice[0]/i_vregfile/mem}
add wave -noupdate -label mem01 -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[0]/gen_vrf_slice[1]/i_vregfile/mem}
add wave -noupdate -label mem02 -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[0]/gen_vrf_slice[2]/i_vregfile/mem}
add wave -noupdate -label mem03 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[0]/gen_vrf_slice[3]/i_vregfile/mem}
add wave -noupdate -label mem10 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[1]/gen_vrf_slice[0]/i_vregfile/mem}
add wave -noupdate -label mem11 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[1]/gen_vrf_slice[1]/i_vregfile/mem}
add wave -noupdate -label mem12 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[1]/gen_vrf_slice[2]/i_vregfile/mem}
add wave -noupdate -label mem13 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[1]/gen_vrf_slice[3]/i_vregfile/mem}
add wave -noupdate -label mem20 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[2]/gen_vrf_slice[0]/i_vregfile/mem}
add wave -noupdate -label mem21 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[2]/gen_vrf_slice[1]/i_vregfile/mem}
add wave -noupdate -label mem22 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[2]/gen_vrf_slice[2]/i_vregfile/mem}
add wave -noupdate -label mem23 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[2]/gen_vrf_slice[3]/i_vregfile/mem}
add wave -noupdate -label mem30 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[3]/gen_vrf_slice[0]/i_vregfile/mem}
add wave -noupdate -label mem31 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[3]/gen_vrf_slice[1]/i_vregfile/mem}
add wave -noupdate -label mem32 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[3]/gen_vrf_slice[2]/i_vregfile/mem}
add wave -noupdate -label mem33 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vrf/gen_reg_banks[3]/gen_vrf_slice[3]/i_vregfile/mem}
TreeUpdate [SetDefaultTree]
WaveRestoreCursors {{Cursor 1} {9461000 ps} 0} {{Cursor 2} {9466000 ps} 0} {{Cursor 3} {9468000 ps} 0} {{Cursor 4} {9441000 ps} 0}
quietly wave cursor active 1
configure wave -namecolwidth 239
configure wave -valuecolwidth 100
configure wave -justifyvalue left
configure wave -signalnamewidth 1
configure wave -snapdistance 10
configure wave -datasetprefix 0
configure wave -rowmargin 4
configure wave -childrowmargin 2
configure wave -gridoffset 0
configure wave -gridperiod 1
configure wave -griddelta 40
configure wave -timeline 0
configure wave -timelineunits ns
update
WaveRestoreZoom {9444616 ps} {9477384 ps}
