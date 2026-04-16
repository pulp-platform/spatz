# Copyright (c) 2025 ETH Zurich.
# Yichao Zhang <yiczhang@iis.ee.ethz.ch>
# Tim Fischer  <fischeti@iis.ee.ethz.ch>

#############
## Helpers ##
#############

proc get_clk_pin {cell} {
  return [get_pins -of_objects $cell -filter "name==CP"]
}

proc get_q_pin {cell} {
  return [get_pins -of_objects $cell -filter "name==Q"]
}

proc get_d_pin {cell} {
  return [get_pins -of_objects $cell -filter "name==D"]
}

proc get_fanin_to {port_or_pin} {
  return [all_transitive_fanin -to $port_or_pin -flat -startpoints_only -only_cells]
}

proc get_fanout_from {port_or_pin} {
  return [all_transitive_fanout -from $port_or_pin -flat -endpoints_only -only_cells]
}

###############
## Instances ##
###############

set CLINT_ASYNC_REG [get_fanout_from [get_ports pad_rtc_clk_i]]
set ASYNC_PINS_CLINT [get_d_pin $CLINT_ASYNC_REG]

set CHS_SPI_PINS [get_pins -of_objects [get_block_inst cheshire_tile] -filter "name=~*spih*"]
set CHS_I2C_PINS [get_pins -of_objects [get_block_inst cheshire_tile] -filter "name=~*i2c*"]

set SLINK_TX_SLOW_SRC_REG(slink) [get_cells -hierarchical gen_phy_channels_0__i_serial_link_physical_i_serial_link_physical_tx_clk_slow_reg]
set SLINK_TX_SLOW_SRC_REG(dram_slink) [get_cells -hierarchical i_dram_serial_link_gen_phy_channels_0__i_serial_link_physical_i_serial_link_physical_tx_clk_slow_reg]

set SLINK_TX_DDR_SEL_REG(slink) [get_cells -hierarchical gen_phy_channels_0__i_serial_link_physical_i_serial_link_physical_tx_ddr_sel_reg]
set SLINK_TX_DDR_SEL_REG(dram_slink) [get_cells -hierarchical i_dram_serial_link_gen_phy_channels_0__i_serial_link_physical_i_serial_link_physical_tx_ddr_sel_reg]

foreach slink {slink dram_slink} {
  set SL_IN($slink)       [get_ports pad_${slink}_i]
  set SL_OUT($slink)      [get_ports pad_${slink}_o]
  set SL_OUT_CLK($slink)  [get_ports pad_${slink}_rcv_clk_o]

  # The source register for the forwarded clock is the fanin of the `*_rcv_clk_o` port
  set SLINK_TX_DDR_SRC_REG($slink) [filter_collection [get_fanin_to [get_ports pad_${slink}_rcv_clk_o[0]]] "name=~*rcv_clk_o*"]
  set SLINK_TX_DDR_SRC_CLK($slink) [get_clk_pin $SLINK_TX_DDR_SRC_REG($slink)]
  set SLINK_TX_DDR_SRC_Q($slink) [get_q_pin $SLINK_TX_DDR_SRC_REG($slink)]

  # The source register for the slow clock is in the same hierarchy
  # as the source register for the `*_rcv_clk_o` port but is called `clk_slow_reg`
  # set SLINK_TX_SLOW_SRC_REG($slink) [get_cells [join [lrange [split [get_object_name $SLINK_TX_DDR_SRC_REG($slink)] "/"] 0 end-1] "/"]/clk_slow_reg]
  set SLINK_TX_SLOW_SRC_CLK($slink) [get_clk_pin $SLINK_TX_SLOW_SRC_REG($slink)]
  set SLINK_TX_SLOW_SRC_Q($slink) [get_q_pin $SLINK_TX_SLOW_SRC_REG($slink)]

  set SLINK_TX_DDR_SEL_Q($slink) [get_q_pin $SLINK_TX_DDR_SEL_REG($slink)]

}


###################
## Re-Constraint ##
###################

# We don't want to false path them, but we constrain them very loosely
set SPI_FREQ_HZ 20e6
set I2C_FREQ_HZ 20e6

set SL_MAX_SKEW 0.5

set current_mode [current_mode]

foreach_in_collection mode [get_modes] {

    current_mode $mode

    ##########
    ## JTAG ##
    ##########
    puts "JTAG..."

    set JTAG_TCK [get_attribute [get_clocks jtag_clk] period]
    set_input_delay  -modes $mode -min -add_delay -clock jtag_clk [expr $JTAG_TCK * 0.10]     [get_ports {pad_jtag_tdi_i pad_jtag_tms_i}]
    set_input_delay  -modes $mode -max -add_delay -clock jtag_clk [expr $JTAG_TCK * 0.50]     [get_ports {pad_jtag_tdi_i pad_jtag_tms_i}]
    set_output_delay -modes $mode -min -add_delay -clock jtag_clk [expr $JTAG_TCK * 0.10 / 2] [get_ports pad_jtag_tdo_o]
    set_output_delay -modes $mode -max -add_delay -clock jtag_clk [expr $JTAG_TCK * 0.50 / 2] [get_ports pad_jtag_tdo_o]

    set_max_delay $JTAG_TCK  -from [get_ports pad_jtag_trst_ni]
    set_false_path -hold     -from [get_ports pad_jtag_trst_ni]

    set RTC_TCK [get_attribute [get_clocks rtc_clk] period]
    set CLK_UNCERTAINTY [get_attribute [get_clocks soc_clk] hold_uncertainty]

    set_max_delay [expr $RTC_TCK * 0.2] -to $ASYNC_PINS_CLINT -ignore_clock_latency
    set_min_delay [expr -$CLK_UNCERTAINTY] -to $ASYNC_PINS_CLINT -ignore_clock_latency

    ###############
    ## I2C & SPI ##
    ###############
    puts "I2C & SPI..."

    # Justification: The previous constraints did still include the clock latency, which is not desired.
    # The clock latency is mupliple ns, which causes problems in the end with timing closure.
    set SPI_TCK [freq_to_period $SPI_FREQ_HZ]
    set_max_delay [expr $SPI_TCK * 0.35] -through $CHS_SPI_PINS -ignore_clock_latency -reset_path
    set_max_delay [expr $SPI_TCK * 0.35] -from [get_ports pad_spih*] -through $CHS_SPI_PINS -ignore_clock_latency -reset_path
    set_max_delay [expr $SPI_TCK * 0.35] -to [get_ports pad_spih*] -through $CHS_SPI_PINS -ignore_clock_latency -reset_path

    set I2C_TCK [freq_to_period $I2C_FREQ_HZ]
    set_max_delay [expr $I2C_TCK * 0.35] -through $CHS_I2C_PINS -ignore_clock_latency -reset_path
    set_max_delay [expr $I2C_TCK * 0.35] -from [get_ports pad_i2c*] -through $CHS_I2C_PINS -ignore_clock_latency -reset_path
    set_max_delay [expr $I2C_TCK * 0.35] -to [get_ports pad_i2c*] -through $CHS_I2C_PINS -ignore_clock_latency -reset_path

    foreach slink {slink dram_slink} {

        if {[sizeof_collection [get_clocks ${slink}_clk_gen_ddr -quiet]] > 0} {
            remove_clocks [get_clocks ${slink}_clk_gen_ddr]
        }

        if {[sizeof_collection [get_clocks ${slink}_clk_gen_slow_drv -quiet]] > 0} {
            remove_clocks [get_clocks ${slink}_clk_gen_slow_drv]
        }

        # Create slow clock driving TX output (worst case: divided by 4)
        puts "SLINK slow TX clk: master-clk from [get_object_name $SLINK_TX_SLOW_SRC_CLK($slink)] & drives pins [get_object_name $SLINK_TX_SLOW_SRC_Q($slink)]"
        create_generated_clock -name ${slink}_clk_gen_slow_drv \
            -edges {1 5 9} \
            -source $SLINK_TX_SLOW_SRC_CLK($slink) \
            $SLINK_TX_SLOW_SRC_Q($slink)

        # Create clock for serial link TX (worst case: divided by 4, +90 deg)
        puts "SLINK slow RX clk: master-clk from [get_object_name $SLINK_TX_DDR_SRC_CLK($slink)] & drives pins [get_object_name $SLINK_TX_DDR_SRC_Q($slink)]"
        create_generated_clock -name ${slink}_clk_gen_ddr \
            -edges {3 7 11} \
            -source $SLINK_TX_DDR_SRC_CLK($slink) \
            $SLINK_TX_DDR_SRC_Q($slink)


        set SLINK_TCK [get_attribute [get_clocks ${slink}_clk_gen_ddr] period]

        # DDR Input: Maximize assumed *transition* (unstable) interval by maximizing input delay span.
        # Transitions happen *between* sampling input clock edges, so centered around T/4 *after* sampling edges.
        # We assume that the transition takes up almost a full half period, so (T/4 - (T/4-skew), T/4 + (T/4-skew)).
        set_input_delay -modes $mode -min -add_delay             -clock ${slink}_clk [expr                 + $SL_MAX_SKEW] $SL_IN($slink)
        set_input_delay -modes $mode -min -add_delay -clock_fall -clock ${slink}_clk [expr                 + $SL_MAX_SKEW] $SL_IN($slink)
        set_input_delay -modes $mode -max -add_delay             -clock ${slink}_clk [expr  $SLINK_TCK / 2 - $SL_MAX_SKEW] $SL_IN($slink)
        set_input_delay -modes $mode -max -add_delay -clock_fall -clock ${slink}_clk [expr  $SLINK_TCK / 2 - $SL_MAX_SKEW] $SL_IN($slink)

        # DDR Output: Maximize *stable* interval we provide by maximizing output delay span (i.e. range in
        # which the target device may sample). This allows our outputs to transition only in a small margin.
        # The stable interval is centered around the centered clock sent for sampling, so (-T/4+skew, T/4-skew)
        set_output_delay -modes $mode -min -add_delay             -clock ${slink}_clk_gen_ddr -reference_pin $SL_OUT_CLK($slink) [expr -$SLINK_TCK / 4 + $SL_MAX_SKEW] $SL_OUT($slink)
        set_output_delay -modes $mode -min -add_delay -clock_fall -clock ${slink}_clk_gen_ddr -reference_pin $SL_OUT_CLK($slink) [expr -$SLINK_TCK / 4 + $SL_MAX_SKEW] $SL_OUT($slink)
        set_output_delay -modes $mode -max -add_delay             -clock ${slink}_clk_gen_ddr -reference_pin $SL_OUT_CLK($slink) [expr  $SLINK_TCK / 4 - $SL_MAX_SKEW] $SL_OUT($slink)
        set_output_delay -modes $mode -max -add_delay -clock_fall -clock ${slink}_clk_gen_ddr -reference_pin $SL_OUT_CLK($slink) [expr  $SLINK_TCK / 4 - $SL_MAX_SKEW] $SL_OUT($slink)

        # Do not consider noncritical edges between driving and sent TX clock
        set_false_path -setup -rise_from [get_clocks ${slink}_clk_gen_slow_drv] -rise_to [get_clocks ${slink}_clk_gen_ddr]
        set_false_path -setup -fall_from [get_clocks ${slink}_clk_gen_slow_drv] -fall_to [get_clocks ${slink}_clk_gen_ddr]
        set_false_path -hold  -rise_from [get_clocks ${slink}_clk_gen_slow_drv] -fall_to [get_clocks ${slink}_clk_gen_ddr]
        set_false_path -hold  -fall_from [get_clocks ${slink}_clk_gen_slow_drv] -rise_to [get_clocks ${slink}_clk_gen_ddr]

        # Do not consider noncritical edges between TX clock and RX clock
        set_false_path -setup -rise_from [get_clocks ${slink}_clk] -rise_to [get_clocks ${slink}_clk]
        set_false_path -setup -fall_from [get_clocks ${slink}_clk] -fall_to [get_clocks ${slink}_clk]
        set_false_path -hold -rise_from [get_clocks ${slink}_clk] -rise_to [get_clocks ${slink}_clk]
        set_false_path -hold -fall_from [get_clocks ${slink}_clk] -fall_to [get_clocks ${slink}_clk]

        # This path is from the DDR select to the output pads. Unfortunately, this register is clocked by the fast system clock,
        # but only changes its value on the slow clock. It is mostly a false path, but we need to ensure that the
        # output delay is not too large, so we set a maximum delay of 10ns
        # TODO(fischeti): Check the 10ns
        set_max_delay 10 -from $SLINK_TX_DDR_SEL_Q($slink) -to [get_ports pad_${slink}_o*] -ignore_clock_latency -reset_path

    }

    # Global reset is asynchronous and can last multiple cycles; sub-block resets
    # are software-controlled. Create a multicycle path for the global reset signal
    # allowing the reset up to 6 cycles to propagate
    set RST_CYCLES 6
    set_false_path -fall -from                                    [get_ports pad_rst_ni]
    set_multicycle_path -rise -setup $RST_CYCLES            -from [get_ports pad_rst_ni]
    set_multicycle_path -rise -hold  [expr $RST_CYCLES - 1] -from [get_ports pad_rst_ni]

    set sw_reset_pins [get_pins -of_objects [get_block_inst cheshire_tile] -filter "name=~*rst_no*"]
    set_false_path -fall -through                                    $sw_reset_pins
    set_multicycle_path -rise -setup $RST_CYCLES            -through $sw_reset_pins
    set_multicycle_path -rise -hold  [expr $RST_CYCLES - 1] -through $sw_reset_pins

}

current_mode $current_mode

#######################
##  Apply scenarios  ##
#######################

# Use slowest corner for setup
# set setup_scenarios {func_ssgnp_0p675v_m40c func_ssgnp_0p675v_125c}
set setup_scenarios {func_tt_0p75v_25c}

# Use fastest corner for hold
set hold_scenarios {func_ffgnp_0p825v_125c func_ssgnp_0p675v_m40c func_tt_0p75v_25c}

# Use hold corner for min capacitance
set min_capacitance_scenarios $hold_scenarios

# Use setup corners for max transition, max capacitance
set max_capacitance_scenarios $setup_scenarios
set max_transition_scenarios $setup_scenarios

# Use hottest corner for leakage
set leakage_scenarios {func_ssgnp_0p675v_125c}
# Use typical corner for dynamic power
set dynamic_power_scenarios {func_tt_0p75v_25c}

set_scenario_status -none                  [all_scenarios]
set_scenario_status -setup           true  $setup_scenarios
set_scenario_status -hold            true  $hold_scenarios
set_scenario_status -leakage_power   true  $leakage_scenarios
set_scenario_status -dynamic_power   true  $dynamic_power_scenarios
set_scenario_status -max_transition  true  $max_transition_scenarios
set_scenario_status -max_capacitance true  $max_capacitance_scenarios
set_scenario_status -min_capacitance true  $min_capacitance_scenarios
