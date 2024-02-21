# Copyright 2020 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

set NR_QUADRANTS 1
set NR_CLUSTERS 1
set NR_CORES 9

set DMA_CORE_ID 8

add wave -noupdate -divider "Snitch Cores"

for {set CORE_ID 0} {$CORE_ID < $NR_CORES} {incr CORE_ID} {
    eval "add wave -noupdate -expand -group {Core $CORE_ID} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/i_snitch/wfi_q}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/i_snitch/pc_q}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {INT LSU/Regfile} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/i_snitch/i_snitch_regfile/mem}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {INT LSU/Regfile} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/i_snitch/i_snitch_lsu/lsu_qready_o}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {INT LSU/Regfile} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/i_snitch/i_snitch_lsu/lsu_qvalid_i}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {INT LSU/Regfile} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/i_snitch/i_snitch_lsu/lsu_pready_i}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {INT LSU/Regfile} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/i_snitch/i_snitch_lsu/lsu_pvalid_o}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {FPU} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/gen_fpu/i_snitch_fp_ss/trace_port_o}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {FPU} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/gen_fpu/i_snitch_fp_ss/i_fpu/op_i}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {FPU} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/gen_fpu/i_snitch_fp_ss/i_fpu/operands_i}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {FPU} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/gen_fpu/i_snitch_fp_ss/i_fpu/in_valid_i}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {FPU} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/gen_fpu/i_snitch_fp_ss/i_fpu/in_ready_o}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {FPU} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/gen_fpu/i_snitch_fp_ss/i_fpu/result_o}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {FPU} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/gen_fpu/i_snitch_fp_ss/i_fpu/out_valid_o}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {FPU} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/gen_fpu/i_snitch_fp_ss/i_fpu/out_ready_i}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {FPU} -group {FP LSU/Regfile} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/gen_fpu/i_snitch_fp_ss/i_ff_regfile/mem}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {FPU} -group {FP LSU/Regfile} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/gen_fpu/i_snitch_fp_ss/i_snitch_lsu/lsu_qready_o}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {FPU} -group {FP LSU/Regfile} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/gen_fpu/i_snitch_fp_ss/i_snitch_lsu/lsu_qvalid_i}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {FPU} -group {FP LSU/Regfile} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/gen_fpu/i_snitch_fp_ss/i_snitch_lsu/lsu_pready_i}"
    eval "add wave -noupdate -expand -group {Core $CORE_ID} -group {FPU} -group {FP LSU/Regfile} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$CORE_ID]/i_snitch_cc/gen_fpu/i_snitch_fp_ss/i_snitch_lsu/lsu_pvalid_o}"
}

add wave -noupdate -divider "DMA"
eval "add wave -noupdate  -childformat {{{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.aw_stall_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.ar_stall_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.r_stall_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.w_stall_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.buf_w_stall_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.buf_r_stall_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.aw_valid_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.aw_ready_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.aw_done_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.ar_valid_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.ar_ready_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.ar_done_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.r_valid_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.r_ready_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.r_done_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.w_valid_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.w_ready_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.w_done_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.b_valid_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.b_ready_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.b_done_cnt} -radix decimal} {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.dma_busy_cnt} -radix decimal}} -subitemconfig {{/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.aw_stall_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.ar_stall_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.r_stall_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.w_stall_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.buf_w_stall_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.buf_r_stall_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.aw_valid_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.aw_ready_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.aw_done_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.ar_valid_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.ar_ready_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.ar_done_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.r_valid_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.r_ready_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.r_done_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.w_valid_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.w_ready_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.w_done_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.b_valid_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.b_ready_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.b_done_cnt} {-height 16 -radix decimal} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o.dma_busy_cnt} {-height 16 -radix decimal}} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/dma_perf_o}"
eval "add wave -noupdate  {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/i_axi_dma_backend/backend_idle_o}"
eval "add wave -noupdate  {/tb_bin/i_dut/i_snitch_cluster/i_cluster/gen_core[$DMA_CORE_ID]/i_snitch_cc/gen_dma/i_axi_dma_tc_snitch_fe/i_axi_dma_backend/trans_complete_o}"

add wave -noupdate -divider "AXI"
eval "add wave -noupdate -group {Wide Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/wide_out_resp_i.r"
eval "add wave -noupdate -group {Wide Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/wide_out_resp_i.r_valid"
eval "add wave -noupdate -group {Wide Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/wide_out_req_o.r_ready"
eval "add wave -noupdate -group {Wide Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/wide_out_req_o.ar"
eval "add wave -noupdate -group {Wide Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/wide_out_req_o.ar_valid"
eval "add wave -noupdate -group {Wide Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/wide_out_resp_i.ar_ready"
eval "add wave -noupdate -group {Wide Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/wide_out_req_o.w"
eval "add wave -noupdate -group {Wide Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/wide_out_req_o.w_valid"
eval "add wave -noupdate -group {Wide Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/wide_out_resp_i.w_ready"
eval "add wave -noupdate -group {Wide Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/wide_out_req_o.aw"
eval "add wave -noupdate -group {Wide Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/wide_out_req_o.aw_valid"
eval "add wave -noupdate -group {Wide Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/wide_out_resp_i.aw_ready"

eval "add wave -noupdate -group {DMA Xbar DMA port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/slv_ports_req_i[0].aw_valid}"
eval "add wave -noupdate -group {DMA Xbar DMA port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/slv_ports_resp_o[0].aw_ready}"
eval "add wave -noupdate -group {DMA Xbar DMA port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/slv_ports_req_i[0].aw.addr}"
eval "add wave -noupdate -group {DMA Xbar DMA port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/slv_ports_req_i[0].ar_valid}"
eval "add wave -noupdate -group {DMA Xbar DMA port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/slv_ports_resp_o[0].ar_ready}"
eval "add wave -noupdate -group {DMA Xbar DMA port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/slv_ports_req_i[0].ar.addr}"
eval "add wave -noupdate -group {DMA Xbar DMA port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/slv_ports_req_i[0].w_valid}"
eval "add wave -noupdate -group {DMA Xbar DMA port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/slv_ports_resp_o[0].w_ready}"
eval "add wave -noupdate -group {DMA Xbar DMA port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/slv_ports_resp_o[0].r_valid}"
eval "add wave -noupdate -group {DMA Xbar DMA port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/slv_ports_req_i[0].r_ready}"

eval "add wave -noupdate -group {DMA Xbar TCDM port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/mst_ports_req_o[0].aw_valid}"
eval "add wave -noupdate -group {DMA Xbar TCDM port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/mst_ports_resp_i[0].aw_ready}"
eval "add wave -noupdate -group {DMA Xbar TCDM port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/mst_ports_req_o[0].aw.addr}"
eval "add wave -noupdate -group {DMA Xbar TCDM port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/mst_ports_req_o[0].ar_valid}"
eval "add wave -noupdate -group {DMA Xbar TCDM port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/mst_ports_resp_i[0].ar_ready}"
eval "add wave -noupdate -group {DMA Xbar TCDM port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/mst_ports_req_o[0].ar.addr}"
eval "add wave -noupdate -group {DMA Xbar TCDM port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/mst_ports_req_o[0].w_valid}"
eval "add wave -noupdate -group {DMA Xbar TCDM port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/mst_ports_resp_i[0].w_ready}"
eval "add wave -noupdate -group {DMA Xbar TCDM port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/mst_ports_resp_i[0].r_valid}"
eval "add wave -noupdate -group {DMA Xbar TCDM port} {/tb_bin/i_dut/i_snitch_cluster/i_cluster/i_axi_dma_xbar/mst_ports_req_o[0].r_ready}"

eval "add wave -noupdate -group {Narrow Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/narrow_out_resp_i.r"
eval "add wave -noupdate -group {Narrow Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/narrow_out_resp_i.r_valid"
eval "add wave -noupdate -group {Narrow Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/narrow_out_req_o.r_ready"
eval "add wave -noupdate -group {Narrow Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/narrow_out_req_o.ar"
eval "add wave -noupdate -group {Narrow Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/narrow_out_req_o.ar_valid"
eval "add wave -noupdate -group {Narrow Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/narrow_out_resp_i.ar_ready"
eval "add wave -noupdate -group {Narrow Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/narrow_out_req_o.w"
eval "add wave -noupdate -group {Narrow Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/narrow_out_req_o.w_valid"
eval "add wave -noupdate -group {Narrow Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/narrow_out_resp_i.w_ready"
eval "add wave -noupdate -group {Narrow Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/narrow_out_req_o.aw"
eval "add wave -noupdate -group {Narrow Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/narrow_out_req_o.aw_valid"
eval "add wave -noupdate -group {Narrow Out} /tb_bin/i_dut/i_snitch_cluster/i_cluster/narrow_out_resp_i.aw_ready"
