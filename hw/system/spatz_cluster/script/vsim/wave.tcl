# Copyright 2021 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

onerror {resume}
quietly WaveActivateNextPane {} 0

proc getScriptDirectory {} {
    set dispScriptFile [file normalize [info script]]
    set scriptFolder [file dirname $dispScriptFile]
    return $scriptFolder
}

set scriptDir [getScriptDirectory]

# Add the cluster probe
add wave /tb_bin/i_dut/cluster_probe

# Add all cores
for {set core 0}  {$core < [examine -radix dec spatz_cluster_pkg::NumCores]} {incr core} {
    do ${scriptDir}/wave_core.tcl $core
}

# Add cluster waves
add wave -noupdate -group Cluster /tb_bin/i_dut/i_cluster_wrapper/i_cluster/*

add wave -noupdate -group CSR /tb_bin/i_dut/i_cluster_wrapper/i_cluster/i_snitch_cluster_peripheral/*
add wave -noupdate -group CSR -group flush_status /tb_bin/i_dut/i_cluster_wrapper/i_cluster/i_snitch_cluster_peripheral/i_spatz_cluster_peripheral_reg_top/u_l1d_flush_status/*
add wave -noupdate -group CSR -group spm_size /tb_bin/i_dut/i_cluster_wrapper/i_cluster/i_snitch_cluster_peripheral/i_spatz_cluster_peripheral_reg_top/u_cfg_l1d_spm/*

add wave -noupdate -group Mapper /tb_bin/i_dut/i_cluster_wrapper/i_cluster/i_tcdm_mapper/*

# add wave -noupdate -group L1D /tb_bin/i_dut/i_cluster_wrapper/i_cluster/i_l1_controller/*


# add wave -noupdate -group Cluster -group amo {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[1]/gen_tcdm_bank[7]/i_amo_shim/*}
# add wave -noupdate -group Cluster -group reqid_pipe {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[1]/gen_tcdm_bank[7]/i_reqid_pipe2/*}
# add wave -noupdate -group Cluster -group sram_pipe {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[1]/gen_tcdm_bank[7]/i_sram_pipe/*}
# add wave -noupdate -group Cluster -group sram {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[1]/gen_tcdm_bank[7]/i_data_mem/*}

add wave -noupdate -group Cluster -group core_xbar {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/i_tcdm_interconnect/*}
add wave -noupdate -group Cluster -group core_xbar -group req {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/i_tcdm_interconnect/gen_xbar/i_stream_xbar/*}
add wave -noupdate -group Cluster -group dma_xbar {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/i_dma_interconnect/*}
add wave -noupdate -group cache_Xbar0 /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[0]/i_cache_xbar/*
add wave -noupdate -group cache_Xbar0 -group req_xbar /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[0]/i_cache_xbar/i_req_xbar/*
add wave -noupdate -group cache_Xbar0 -group rsp_xbar /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[0]/i_cache_xbar/i_rsp_xbar/*
add wave -noupdate -group cache_Xbar1 /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[1]/i_cache_xbar/*
add wave -noupdate -group cache_Xbar1 -group req_xbar /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[1]/i_cache_xbar/i_req_xbar/*
add wave -noupdate -group cache_Xbar1 -group rsp_xbar /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[1]/i_cache_xbar/i_rsp_xbar/*
add wave -noupdate -group cache_Xbar2 /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[2]/i_cache_xbar/*
add wave -noupdate -group cache_Xbar2 -group req_xbar /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[2]/i_cache_xbar/i_req_xbar/*
add wave -noupdate -group cache_Xbar2 -group rsp_xbar /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[2]/i_cache_xbar/i_rsp_xbar/*
add wave -noupdate -group cache_Xbar3 /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[3]/i_cache_xbar/*
add wave -noupdate -group cache_Xbar3 -group req_xbar /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[3]/i_cache_xbar/i_req_xbar/*
add wave -noupdate -group cache_Xbar3 -group rsp_xbar /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[3]/i_cache_xbar/i_rsp_xbar/*
add wave -noupdate -group cache_Xbar4 /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[4]/i_cache_xbar/*
add wave -noupdate -group cache_Xbar4 -group req_xbar /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[4]/i_cache_xbar/i_req_xbar/*
add wave -noupdate -group cache_Xbar4 -group rsp_xbar /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_cache_xbar[4]/i_cache_xbar/i_rsp_xbar/*

# add wave -noupdate -group superbank0 /tb_bin/i_dut/i_cluster_wrapper/i_cluster/l1_cache_wp*
# add wave -noupdate -group superbank0 /tb_bin/i_dut/i_cluster_wrapper/i_cluster/l1_data_bank*
# add wave -noupdate -group superbank0 -group mux /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/i_tcdm_mux/*
# add wave -noupdate -group superbank0 -group bank0 /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_data_mem/*
# add wave -noupdate -group superbank0 -group bank0 -group mux /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_data_mem/gen_banks[0]/i_spm_cache_mux/*
# add wave -noupdate -group superbank0 -group bank0 -group bank /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[0]/i_data_mem/gen_banks[0]/i_data_bank/*

# add wave -noupdate -group superbank0 -group bank7 /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[7]/i_data_mem/*
# add wave -noupdate -group superbank0 -group bank7 -group mux /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[7]/i_data_mem/gen_banks[0]/i_spm_cache_mux/*
# add wave -noupdate -group superbank0 -group bank7 -group bank /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_tcdm_super_bank[0]/gen_tcdm_bank[7]/i_data_mem/gen_banks[0]/i_data_bank/*
