# Copyright 2021 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

onerror {resume}
quietly WaveActivateNextPane {} 0

# Add the kernel probe
add wave /tb_bin/i_dut/cluster_probe

# Add the EOC
add wave /tb_bin/i_dut/eoc

# Add all cores
for {set core 0}  {$core < [examine -radix dec spatz_cluster_pkg::NumCores]} {incr core} {
    do ../script/vsim/wave_core.tcl $core
}
