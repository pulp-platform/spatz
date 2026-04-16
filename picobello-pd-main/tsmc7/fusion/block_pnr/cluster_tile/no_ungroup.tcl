# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

if {[info exists ::NO_UNGROUP]} {

    set modules_to_not_ungroup {
        snitch_cluster_wrapper*
        snitch_hwpe_subsystem*
        snitch_tcdm_aligner*
        snitch_cc*
        idma_inst64_top*
        floo_nw_router*
        floo_nw_chimney*
        axi_*xbar*
        snitch_tcdm_interconnect*
        snitch_cluster_peripheral*
        snitch_icache*
    }

    foreach module $modules_to_not_ungroup {
        if {[sizeof_collection [get_modules $module -quiet]] > 0} {
            set_ungroup [get_modules $module] false
            log "Ungrouping" "Disabling ungrouping for module pattern: $module"
        } else {
            warn "Module pattern $module did not match any modules."
        }
    }
} else {
    log "Ungrouping" "Ungrouping is globally enabled. To disable ungrouping, set the NO_UNGROUP variable."
}
