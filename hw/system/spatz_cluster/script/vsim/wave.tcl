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

add wave -noupdate -group Mapper /tb_bin/i_dut/i_cluster_wrapper/i_cluster/i_tcdm_mapper/*

add wave -noupdate -group L1D /tb_bin/i_dut/i_cluster_wrapper/i_cluster/i_l1_controller/*
