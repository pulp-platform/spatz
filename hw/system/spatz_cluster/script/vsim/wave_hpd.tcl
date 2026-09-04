# Copyright 2026
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

# HPDcache counterpart to wave.tcl, for the spatz_cluster_hpd variant
# (l1_cache: hpdcache). InSitu-Cache's L1D/Mapper/superbank* groups don't
# apply here: spatz_addr_mapper (i_tcdm_mapper) is bypassed out of the
# datapath entirely for this variant, and the SPM banks (superbank0/etc.)
# are dead hardware -- HPDcache owns its own internal storage instead of
# sharing the SPM's physical SRAM. The L1D group below is pointed at
# HPDcache's own submodule hierarchy instead:
#   i_l1_controller (hpd_spatz_cache_ctrl)
#     i_par_coalescer_for_spatz, i_id_buffer, i_hpd_resp_buf
#     i_hpdcache (hpdcache.sv)
#       hpdcache_ctrl_i
#         hpdcache_ctrl_pe_i   -- per-request pipeline stage/FSM: this is
#                                  where a new core request can be held off
#                                  (st0_req_mshr_check_o and friends), the
#                                  first place to look for "non-blocking in
#                                  name only" stalls
#         hpdcache_rtab_i      -- replay table (retried/rolled-back reqs)
#         hpdcache_memctrl_i   -- refill/writeback memory-side arbitration
#       hpdcache_miss_handler_i -- refill_fsm_q: the miss/refill FSM that
#                                  drains the MSHR; REFILL_IDLE vs busy
#                                  states gate how fast outstanding misses
#                                  clear, worth watching against
#                                  hpdcache_ctrl_pe_i's stall reasons
#         hpdcache_mshr_i      -- MSHR occupancy/alloc/ack
#         gen_wb_cbuf/hpdcache_cbuf_i -- coalescing buffer for in-flight
#                                  refill data; nested one level under a
#                                  named generate-if (only instantiated when
#                                  wbEn is set, true for our write-back-only
#                                  config)
#       (no write-buffer submodule: hpdcache_wbuf_i only exists when wtEn is
#        set; our config is write-back-only (wtEn=0), so hpdcache.sv takes
#        the gen_no_wbuf branch and no such submodule exists to probe)
#       hpdcache_uc_i          -- uncached path
#       hpdcache_cmo_i         -- CMO path (cache-init trigger lands here)

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

set cache_path /tb_bin/i_dut/i_cluster_wrapper/i_cluster/i_l1_controller
add wave -noupdate -group L1D ${cache_path}/*
add wave -noupdate -group L1D -group coalescer ${cache_path}/i_par_coalescer_for_spatz/*
add wave -noupdate -group L1D -group id_buffer ${cache_path}/i_id_buffer/*
add wave -noupdate -group L1D -group resp_buf ${cache_path}/i_hpd_resp_buf/*

set hpd_path ${cache_path}/i_hpdcache
add wave -noupdate -group L1D -group hpdcache ${hpd_path}/*
add wave -noupdate -group L1D -group hpdcache -group ctrl ${hpd_path}/hpdcache_ctrl_i/*
add wave -noupdate -group L1D -group hpdcache -group ctrl -group pipeline ${hpd_path}/hpdcache_ctrl_i/hpdcache_ctrl_pe_i/*
add wave -noupdate -group L1D -group hpdcache -group ctrl -group rtab ${hpd_path}/hpdcache_ctrl_i/hpdcache_rtab_i/*
add wave -noupdate -group L1D -group hpdcache -group ctrl -group memctrl ${hpd_path}/hpdcache_ctrl_i/hpdcache_memctrl_i/*
add wave -noupdate -group L1D -group hpdcache -group miss_handler ${hpd_path}/hpdcache_miss_handler_i/*
add wave -noupdate -group L1D -group hpdcache -group miss_handler -group mshr ${hpd_path}/hpdcache_miss_handler_i/hpdcache_mshr_i/*
# hpdcache_cbuf_i lives inside a named generate-if scope (gen_wb_cbuf),
# instantiated only when HPDcacheCfg.u.wbEn is set -- true for our
# write-back-only config (wtEn=0/wbEn=1 in hpd_spatz_cache_ctrl.sv's
# HpdUserCfg), so the extra path level is always present here.
add wave -noupdate -group L1D -group hpdcache -group miss_handler -group cbuf ${hpd_path}/hpdcache_miss_handler_i/gen_wb_cbuf/hpdcache_cbuf_i/*
# hpdcache_wbuf_i only exists when HPDcacheCfg.u.wtEn is set (write-through
# buffering); our config is write-back-only (wtEn=0), so hpdcache.sv takes
# the gen_no_wbuf branch instead and no write-buffer submodule is
# instantiated at all -- nothing to add here for this config.
add wave -noupdate -group L1D -group hpdcache -group uncached ${hpd_path}/hpdcache_uc_i/*
add wave -noupdate -group L1D -group hpdcache -group cmo ${hpd_path}/hpdcache_cmo_i/*

add wave -noupdate -group Cluster -group core_xbar {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/i_tcdm_interconnect/*}
add wave -noupdate -group Cluster -group core_xbar -group req {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/i_tcdm_interconnect/gen_xbar/i_stream_xbar/*}
add wave -noupdate -group Cluster -group dma_xbar {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/i_dma_interconnect/*}
