# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

set modules_to_retime {
    snitch_fpu*
    snitch_shared_muldiv*
    redmule_fma*
}

# Retiming
foreach module $modules_to_retime {
    if {[sizeof_collection [get_modules $module -quiet]] > 0} {
        set_optimize_registers -modules [get_modules $module] true
        log "Retiming" "Enabled retiming for module pattern: $module"
    } else {
        warn "Module pattern $module did not match any modules."
    }
}
# Avoid retiming of the whole cluster
set_app_options -name compile.retiming.group_chained_retiming_modules -value false
