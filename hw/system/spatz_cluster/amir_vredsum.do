onerror {resume}
quietly WaveActivateNextPane {} 0
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/clk_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/rst_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/hart_id_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/irq_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/flush_i_valid_o}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/flush_i_ready_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/inst_addr_o}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/inst_cacheable_o}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/inst_data_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/inst_valid_o}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/inst_ready_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_qreq_o}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_qrsp_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_qvalid_o}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_qready_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_prsp_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_pvalid_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_pready_o}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_mem_finished_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_mem_str_finished_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/data_req_o}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/data_rsp_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ptw_valid_o}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ptw_ready_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ptw_va_o}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ptw_ppn_o}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ptw_pte_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ptw_is_4mega_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/fpu_rnd_mode_o}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/fpu_fmt_mode_o}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/fpu_status_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/core_events_o}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/illegal_inst}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/illegal_csr}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/interrupt}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ecall}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ebreak}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/zero_lsb}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/meip}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/mtip}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/msip}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/mcip}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/seip}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/stip}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ssip}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/scip}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/interrupts_enabled}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/any_interrupt_pending}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/pc_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/pc_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/wfi_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/wfi_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/consec_pc}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/iimm}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/uimm}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/jimm}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/bimm}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/simm}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/opa}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/opb}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/adder_result}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/alu_result}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/rd}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/rs1}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/rs2}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/stall}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/lsu_stall}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/gpr_raddr}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/gpr_rdata}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/gpr_waddr}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/gpr_wdata}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/gpr_we}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/sb_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/sb_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/is_load}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/is_store}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/is_signed}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/is_fp_load}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/is_fp_store}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ls_misaligned}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ld_addr_misaligned}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/st_addr_misaligned}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/inst_addr_misaligned}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/itlb_valid}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/itlb_ready}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/itlb_va}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/itlb_page_fault}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/itlb_pa}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/dtlb_valid}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/dtlb_ready}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/dtlb_va}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/dtlb_page_fault}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/dtlb_pa}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/trans_ready}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/trans_active}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/itlb_trans_valid}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/dtlb_trans_valid}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/trans_active_exp}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/tlb_flush}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ls_size}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ls_amo}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ld_result}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/lsu_qready}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/lsu_qvalid}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/lsu_tlb_qready}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/lsu_tlb_qvalid}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/lsu_pvalid}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/lsu_pready}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/lsu_empty}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ls_paddr}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/lsu_rd}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/retire_load}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/retire_i}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/retire_acc}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_stall}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/valid_instr}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/exception}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/alu_op}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/opa_select}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/opb_select}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/write_rd}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/uses_rd}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/next_pc}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/rd_select}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/rd_bypass}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/is_branch}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/csr_rvalue}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/csr_en}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/scratch_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/scratch_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/epc_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/epc_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/tvec_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/tvec_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/cause_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/cause_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/cause_irq_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/cause_irq_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/spp_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/spp_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/mpp_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/mpp_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ie_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ie_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/pie_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/pie_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/eie_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/eie_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/tie_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/tie_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/sie_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/sie_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/cie_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/cie_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/seip_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/seip_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/stip_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/stip_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ssip_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/ssip_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/scip_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/scip_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/priv_lvl_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/priv_lvl_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/satp_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/satp_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/dcsr_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/dcsr_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/dpc_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/dpc_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/dscratch_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/dscratch_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/debug_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/debug_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/fcsr_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/fcsr_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/read_fcsr}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/cycle_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/instret_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/retired_instr_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/retired_load_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/retired_i_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/retired_acc_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/mseg_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/mseg_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_register_rd}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_mem_stall}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_mem_store}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/operands_ready}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/dst_ready}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/opa_ready}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/opb_ready}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/npc}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/shift_opa}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/shift_opa_reversed}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/shift_right_result}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/shift_left_result}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/shift_opa_ext}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/shift_right_result_ext}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/shift_left}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/shift_arithmetic}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/alu_opa}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/alu_opb}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/lsu_qdata}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_mem_cnt_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_mem_cnt_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_mem_str_cnt_q}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/acc_mem_str_cnt_d}
add wave -noupdate -group i_snitch {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/alu_writeback}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/clk_i}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/rst_i}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_qtag_i}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_qwrite_i}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_qsigned_i}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_qaddr_i}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_qdata_i}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_qsize_i}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_qamo_i}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_qvalid_i}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_qready_o}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_pdata_o}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_ptag_o}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_perror_o}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_pvalid_o}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_pready_i}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_empty_o}
add wave -noupdate -group i_snitch_lsu -expand -subitemconfig {{/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/data_req_o.q} -expand} {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/data_req_o}
add wave -noupdate -group i_snitch_lsu -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/data_rsp_i}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/ld_result}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/lsu_qdata}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/data_qdata}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/laq_in}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/laq_out}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/mem_out}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/laq_full}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/mem_full}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/laq_push}
add wave -noupdate -group i_snitch_lsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_snitch/i_snitch_lsu/shifted_data}
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
add wave -noupdate -group i_spatz -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/spatz_mem_req_o}
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
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vfu_rsp_buf_valid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vfu_rsp}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vfu_rsp_buf}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vlsu_req_ready}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vlsu_rsp_valid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vlsu_rsp_buf_valid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vlsu_rsp}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vlsu_rsp_buf}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vsldu_req_ready}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vsldu_rsp_valid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vsldu_rsp}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vfu_buf_usage}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vlsu_buf_usage}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vfu_buf_data}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vlsu_buf_data}
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
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_waddr_buf}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_wdata}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_wdata_buf}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_we}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_wbe}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_wbe_buf}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_wvalid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_raddr}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_re}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_rdata}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_rvalid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/sb_re}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/sb_we}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/sb_we_buf}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/sb_id}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/sb_buf_id}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vfu_buf_en}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vfu_buf_push}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vfu_buf_pop}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vrf_vfu_wvalid}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vfu_buf_full}
add wave -noupdate -group i_spatz {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/vfu_buf_empty}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/clk_i}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/rst_ni}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/hart_id_i}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/spatz_req_i}
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
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vrf_rvalid_i}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/fpu_status_o}
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
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/result_buf_tag_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/result_buf_tag_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/input_tag}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/result_buf_valid_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/result_buf_valid_q}
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
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_pointer_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_pointer_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/shift_amnt_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/shift_amnt_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/result_buf_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/result_buf_q}
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
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_vector_data}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_scalar_data}
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
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/pnt}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/fill_cnt}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/reduction_operand_request}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/el_type}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/FPUlatency}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/lat_count_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/lat_count_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/num_inter_lane_iterations_d}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/num_inter_lane_iterations_q}
add wave -noupdate -group i_vfu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/mask}
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
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/clk_i}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/rst_ni}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_req_i}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_req_valid_i}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_req_ready_o}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vlsu_rsp_valid_o}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vlsu_rsp_o}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_waddr_o}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_wdata_o}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_we_o}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_wbe_o}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_wvalid_i}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_id_o}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_raddr_o}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_re_o}
add wave -noupdate -group i_vlsu -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_rdata_i}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_rvalid_i}
add wave -noupdate -group i_vlsu -expand -subitemconfig {{/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_mem_req_o[3]} -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_mem_req_o[2]} -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_mem_req_o[1]} -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_mem_req_o[0]} -expand} {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_mem_req_o}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_mem_req_valid_o}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_mem_req_ready_i}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_mem_rsp_i}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_mem_rsp_valid_i}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_mem_finished_o}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_mem_str_finished_o}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_req_d}
add wave -noupdate -group i_vlsu -expand {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_spatz_req}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_spatz_req_valid}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_spatz_req_ready}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_is_strided}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_is_indexed}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/state_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/state_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/store_count_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/store_count_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/rob_wdata}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/rob_wid}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/rob_push}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/rob_rvalid}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/rob_rdata}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/rob_pop}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/rob_rid}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/rob_req_id}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/rob_id}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/rob_full}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/rob_empty}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_operation_valid}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_operation_last}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_counter_max}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_counter_en}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_counter_load}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_counter_delta}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_counter_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_counter_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_port_finished_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_idx_counter_delta}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_idx_counter_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_idx_counter_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_insn_finished_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_insn_finished_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_insn_pending_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_insn_pending_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_insn_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_insn_push}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_insn_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_insn_pop}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_insn_empty}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_insn_valid}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_counter_max}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_counter_en}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_counter_load}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_counter_delta}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_counter_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_counter_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_finished_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_finished_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_req_addr}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vd_vreg_addr}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vs2_vreg_addr}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vd_elem_id}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vs2_elem_id_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vs2_elem_id_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/fetch_next_idx}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_req_addr_offset}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/busy_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/busy_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vlsu_finished_req}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_mem_req}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_mem_req_valid}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/spatz_mem_req_ready}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_operation_valid}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_operation_last}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_is_load}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_is_vstart_zero}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_is_addr_unaligned}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_is_single_element_operation}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_single_element_size}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_idx_single_element_size}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_is_addr_unaligned}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_is_single_element_operation}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/commit_single_element_size}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vreg_addr_offset}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/offset_queue_full}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_req_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_req_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_req_valid_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_req_ready_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_req_valid_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vrf_req_ready_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/vreg_start_0}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/catchup}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_req_id}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_req_data}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_req_svalid}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_req_strb}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_req_lvalid}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_req_last}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_pending_d}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_pending_q}
add wave -noupdate -group i_vlsu {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vlsu/mem_pending}
add wave -noupdate -group tcdm00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/mem_cs}
add wave -noupdate -group tcdm00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/mem_wen}
add wave -noupdate -group tcdm00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/mem_add}
add wave -noupdate -group tcdm00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/mem_be}
add wave -noupdate -group tcdm00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/mem_rdata}
add wave -noupdate -group tcdm00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/mem_wdata}
add wave -noupdate -group tcdm00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/ecc_sram_gnt}
add wave -noupdate -group tcdm00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/amo_rdata_local}
add wave -noupdate -group tcdm01 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[1]/mem_cs}
add wave -noupdate -group tcdm01 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[1]/mem_wen}
add wave -noupdate -group tcdm01 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[1]/mem_add}
add wave -noupdate -group tcdm01 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[1]/mem_be}
add wave -noupdate -group tcdm01 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[1]/mem_rdata}
add wave -noupdate -group tcdm01 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[1]/mem_wdata}
add wave -noupdate -group tcdm01 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[1]/ecc_sram_gnt}
add wave -noupdate -group tcdm01 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[1]/amo_rdata_local}
add wave -noupdate -group tcdm02 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[2]/mem_cs}
add wave -noupdate -group tcdm02 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[2]/mem_wen}
add wave -noupdate -group tcdm02 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[2]/mem_add}
add wave -noupdate -group tcdm02 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[2]/mem_be}
add wave -noupdate -group tcdm02 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[2]/mem_rdata}
add wave -noupdate -group tcdm02 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[2]/mem_wdata}
add wave -noupdate -group tcdm02 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[2]/ecc_sram_gnt}
add wave -noupdate -group tcdm02 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[2]/amo_rdata_local}
add wave -noupdate -group tcdm03 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[3]/mem_cs}
add wave -noupdate -group tcdm03 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[3]/mem_wen}
add wave -noupdate -group tcdm03 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[3]/mem_add}
add wave -noupdate -group tcdm03 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[3]/mem_be}
add wave -noupdate -group tcdm03 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[3]/mem_rdata}
add wave -noupdate -group tcdm03 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[3]/mem_wdata}
add wave -noupdate -group tcdm03 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[3]/ecc_sram_gnt}
add wave -noupdate -group tcdm03 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[3]/amo_rdata_local}
add wave -noupdate -group tcdm04 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[4]/mem_cs}
add wave -noupdate -group tcdm04 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[4]/mem_wen}
add wave -noupdate -group tcdm04 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[4]/mem_add}
add wave -noupdate -group tcdm04 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[4]/mem_be}
add wave -noupdate -group tcdm04 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[4]/mem_rdata}
add wave -noupdate -group tcdm04 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[4]/mem_wdata}
add wave -noupdate -group tcdm04 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[4]/ecc_sram_gnt}
add wave -noupdate -group tcdm04 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[4]/amo_rdata_local}
add wave -noupdate -group tcdm05 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[5]/mem_cs}
add wave -noupdate -group tcdm05 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[5]/mem_wen}
add wave -noupdate -group tcdm05 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[5]/mem_add}
add wave -noupdate -group tcdm05 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[5]/mem_be}
add wave -noupdate -group tcdm05 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[5]/mem_rdata}
add wave -noupdate -group tcdm05 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[5]/mem_wdata}
add wave -noupdate -group tcdm05 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[5]/ecc_sram_gnt}
add wave -noupdate -group tcdm05 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[5]/amo_rdata_local}
add wave -noupdate -group tcdm06 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[6]/mem_cs}
add wave -noupdate -group tcdm06 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[6]/mem_wen}
add wave -noupdate -group tcdm06 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[6]/mem_add}
add wave -noupdate -group tcdm06 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[6]/mem_be}
add wave -noupdate -group tcdm06 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[6]/mem_rdata}
add wave -noupdate -group tcdm06 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[6]/mem_wdata}
add wave -noupdate -group tcdm06 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[6]/ecc_sram_gnt}
add wave -noupdate -group tcdm06 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[6]/amo_rdata_local}
add wave -noupdate -group tcdm07 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[7]/mem_cs}
add wave -noupdate -group tcdm07 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[7]/mem_wen}
add wave -noupdate -group tcdm07 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[7]/mem_add}
add wave -noupdate -group tcdm07 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[7]/mem_be}
add wave -noupdate -group tcdm07 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[7]/mem_rdata}
add wave -noupdate -group tcdm07 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[7]/mem_wdata}
add wave -noupdate -group tcdm07 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[7]/ecc_sram_gnt}
add wave -noupdate -group tcdm07 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[7]/amo_rdata_local}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/clk_i}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/rst_ni}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/scrub_trigger_i}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/scrubber_fix_o}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/scrub_uncorrectable_o}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/wdata_i}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/addr_i}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/req_i}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/we_i}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/be_i}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/rdata_o}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/gnt_o}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/single_error_o}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/multi_error_o}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/ecc_error}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/valid_read_d}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/valid_read_q}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/internal_we}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/bank_req}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/bank_we}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/bank_addr}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/bank_wdata}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/bank_rdata}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/bank_scrub_req}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/bank_scrub_we}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/bank_scrub_addr}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/bank_scrub_wdata}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/bank_scrub_rdata}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/store_state_d}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/store_state_q}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/rmw_count_d}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/rmw_count_q}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/input_buffer_d}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/input_buffer_q}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/addr_buffer_d}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/addr_buffer_q}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/be_buffer_d}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/be_buffer_q}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/be_selector}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/rmw_buffer_end}
add wave -noupdate -expand -group ecc_sram_00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/rmw_buffer_0}
add wave -noupdate -expand -group i_bank00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_bank/clk_i}
add wave -noupdate -expand -group i_bank00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_bank/rst_ni}
add wave -noupdate -expand -group i_bank00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_bank/req_i}
add wave -noupdate -expand -group i_bank00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_bank/we_i}
add wave -noupdate -expand -group i_bank00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_bank/addr_i}
add wave -noupdate -expand -group i_bank00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_bank/wdata_i}
add wave -noupdate -expand -group i_bank00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_bank/be_i}
add wave -noupdate -expand -group i_bank00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_bank/rdata_o}
add wave -noupdate -expand -group i_bank00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_bank/r_addr_q}
add wave -noupdate -expand -group i_bank00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_bank/rdata_q}
add wave -noupdate -expand -group i_bank00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_bank/rdata_d}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/clk_i}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/rst_ni}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/scrub_trigger_i}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/bit_corrected_o}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/uncorrectable_o}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/intc_req_i}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/intc_we_i}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/intc_add_i}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/intc_wdata_i}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/intc_rdata_o}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/bank_req_o}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/bank_we_o}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/bank_add_o}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/bank_wdata_o}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/bank_rdata_i}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/ecc_out_o}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/ecc_in_i}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/ecc_err_i}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/ecc_err}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/scrub_req}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/scrub_we}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/scrub_add}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/scrub_wdata}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/scrub_rdata}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/state_d}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/state_q}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/working_add_d}
add wave -noupdate -expand -group i_scrubber00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/i_scrubber/working_add_q}
add wave -noupdate -expand -group ecc_decode00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/gen_no_ecc_input/ecc_decode/in}
add wave -noupdate -expand -group ecc_decode00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/gen_no_ecc_input/ecc_decode/out}
add wave -noupdate -expand -group ecc_decode00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/gen_no_ecc_input/ecc_decode/syndrome_o}
add wave -noupdate -expand -group ecc_decode00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/gen_no_ecc_input/ecc_decode/err_o}
add wave -noupdate -expand -group ecc_decode00 {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_ecc_sram/gen_no_ecc_input/ecc_decode/single_error}
TreeUpdate [SetDefaultTree]
WaveRestoreCursors {{Cursor 1} {6435000 ps} 1} {{Cursor 2} {6413000 ps} 1} {{Cursor 3} {3985000 ps} 1} {{Cursor 4} {6418000 ps} 0}
quietly wave cursor active 4
configure wave -namecolwidth 285
configure wave -valuecolwidth 161
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
WaveRestoreZoom {6067200 ps} {6568768 ps}
