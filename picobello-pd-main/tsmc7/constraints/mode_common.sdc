# Copyright (c) 2025 ETH Zurich.
# Authors:
# - Tim Fischer <fischeti@iis.ee.ethz.ch>

######################
## Generated Clocks ##
######################
puts "Generated clocks..."

foreach slink {slink dram_slink} {

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
}

##################################
## Clock Groups & Uncertainties ##
##################################
puts "Clock groups..."

# Define which collections of clocks are asynchronous to each other
# 'allow_paths' re-activates checks on datapaths between clock domains
# this way we must constrain them seperately or we get unmet timings
set_clock_groups -asynchronous -name clk_groups_async \
     -group {rtc_clk} \
     -group {jtag_clk} \
     -group {slink_clk} \
     -group {dram_slink_clk} \
     -group {soc_clk *clk_gen_slow_drv *clk_gen_ddr} \
     -allow_paths

# We set reasonable uncertainties and transitions for all nonvirtual clocks
set CLK_UNCERTAINTY [expr $SYS_TCK * 0.05]
set_clock_uncertainty $CLK_UNCERTAINTY [all_clocks] -corners [all_corners]

####################
## Cdcs and Syncs ##
####################
puts "CDC/Sync..."

# For the following input syncs, no constraints are necessary as model their inputs as source-synchronous.
# Even if they are not, their clocks do not drive any on-chip logic, so are completely outside STA:
# * i_cheshire_soc/i_plic/u_prim_flop_2sync/gen_syncs[*].i_sync
# * i_cheshire_soc/gen_i2c.i_i2c/i2c_core/u_i2c_sync_scl/gen_syncs[0].i_sync
# * i_cheshire_soc/gen_i2c.i_i2c/i2c_core/u_i2c_sync_sda/gen_syncs[0].i_sync

# REASONING:
# max_delay checks should be as short as possible so the crossing paths are also short
# -> maximize state-resolution time of flops (avoid metastability)
# min_delay checks are slightly negative to compensate for clock uncertainty,
# this should ideally prevent the tool from adding hold-fixing buffers in the path
# https://gist.github.com/brabect1/7695ead3d79be47576890bbcd61fe426#encouraged-sdc-techniques

# Constrain `cdc_2phase` for DMI request
set_max_delay [expr $SYS_TCK * 0.2] -through $ASYNC_PINS_DMIREQ -ignore_clock_latency
set_min_delay [expr -$CLK_UNCERTAINTY] -through $ASYNC_PINS_DMIREQ -ignore_clock_latency

# Constrain `cdc_2phase` for DMI response
set_max_delay [expr $SYS_TCK * 0.2] -through $ASYNC_PINS_DMIRSP -ignore_clock_latency
set_min_delay [expr -$CLK_UNCERTAINTY] -through $ASYNC_PINS_DMIRSP -ignore_clock_latency

# Constrain `cdc_fifo_gray` for serial link in
set_max_delay [expr $SYS_TCK * 0.2] -through $ASYNC_PINS_SLINK -ignore_clock_latency
set_min_delay [expr -$CLK_UNCERTAINTY] -through $ASYNC_PINS_SLINK -ignore_clock_latency

# Constrain CLINT RTC sync
set_max_delay [expr $RTC_TCK * 0.2] -to $ASYNC_PINS_CLINT -ignore_clock_latency
set_min_delay [expr -$CLK_UNCERTAINTY] -to $ASYNC_PINS_CLINT -ignore_clock_latency

#############
## SoC Ins ##
#############

puts "Input/Outputs..."

# Pseudostatic inputs need not be timed
set_false_path -from [get_ports pad_boot_mode_i]

##########
## JTAG ##
##########
puts "JTAG..."

set_input_delay  -min -add_delay -clock jtag_clk [expr $JTAG_TCK * 0.10]     [get_ports {pad_jtag_tdi_i pad_jtag_tms_i}]
set_input_delay  -max -add_delay -clock jtag_clk [expr $JTAG_TCK * 0.50]     [get_ports {pad_jtag_tdi_i pad_jtag_tms_i}]
set_output_delay -min -add_delay -clock jtag_clk [expr $JTAG_TCK * 0.10 / 2] [get_ports pad_jtag_tdo_o]
set_output_delay -max -add_delay -clock jtag_clk [expr $JTAG_TCK * 0.50 / 2] [get_ports pad_jtag_tdo_o]

set_max_delay $JTAG_TCK  -from [get_ports pad_jtag_trst_ni]
set_false_path -hold     -from [get_ports pad_jtag_trst_ni]

##############
## SPI Host ##
##############
puts "SPI..."

set_max_delay [expr $SPI_TCK * 0.35] -through [get_pins -hierarchical i_cheshire_tile/spih*]

#########
## I2C ##
#########

set_max_delay [expr $I2C_TCK * 0.35] -through [get_pins -hierarchical i_cheshire_tile/i2c*]

##########
## UART ##
##########
puts "UART..."

# UART is a fully asynchronous protocol. We don't need to constrain it. We
# explicitely false path the ports to document our design intention and to avoid
# any SDC linter to raise concerns about unconstrained ports
set_false_path -from [get_ports pad_uart_rx_i]
set_false_path -to [get_ports pad_uart_tx_o]

###########
## GPIOs ##
###########
puts "GPIOs..."

# We leave the GPIOs unconstrained. There are no requirements on max skew
# between GPIOs nor is there any expectations on the maximum delay from GPIO
# register to pad.
set_false_path -through [get_pins -hierarchical i_cheshire_tile/gpio*]

#################
## Serial Link ##
#################
puts "Serial Link..."

set SL_MAX_SKEW 0.5

foreach slink {slink dram_slink} {

    # DDR Input: Maximize assumed *transition* (unstable) interval by maximizing input delay span.
    # Transitions happen *between* sampling input clock edges, so centered around T/4 *after* sampling edges.
    # We assume that the transition takes up almost a full half period, so (T/4 - (T/4-skew), T/4 + (T/4-skew)).
    set_input_delay -min -add_delay             -clock ${slink}_clk [expr                 + $SL_MAX_SKEW] $SL_IN($slink)
    set_input_delay -min -add_delay -clock_fall -clock ${slink}_clk [expr                 + $SL_MAX_SKEW] $SL_IN($slink)
    set_input_delay -max -add_delay             -clock ${slink}_clk [expr  $SLINK_TCK / 2 - $SL_MAX_SKEW] $SL_IN($slink)
    set_input_delay -max -add_delay -clock_fall -clock ${slink}_clk [expr  $SLINK_TCK / 2 - $SL_MAX_SKEW] $SL_IN($slink)

    # DDR Output: Maximize *stable* interval we provide by maximizing output delay span (i.e. range in
    # which the target device may sample). This allows our outputs to transition only in a small margin.
    # The stable interval is centered around the centered clock sent for sampling, so (-T/4+skew, T/4-skew)
    set_output_delay -min -add_delay             -clock ${slink}_clk_gen_ddr -reference_pin $SL_OUT_CLK($slink) [expr -$SLINK_TCK / 4 + $SL_MAX_SKEW] $SL_OUT($slink)
    set_output_delay -min -add_delay -clock_fall -clock ${slink}_clk_gen_ddr -reference_pin $SL_OUT_CLK($slink) [expr -$SLINK_TCK / 4 + $SL_MAX_SKEW] $SL_OUT($slink)
    set_output_delay -max -add_delay             -clock ${slink}_clk_gen_ddr -reference_pin $SL_OUT_CLK($slink) [expr  $SLINK_TCK / 4 - $SL_MAX_SKEW] $SL_OUT($slink)
    set_output_delay -max -add_delay -clock_fall -clock ${slink}_clk_gen_ddr -reference_pin $SL_OUT_CLK($slink) [expr  $SLINK_TCK / 4 - $SL_MAX_SKEW] $SL_OUT($slink)

    # Do not consider noncritical edges between driving and sent TX clock
    set_false_path -setup -rise_from [get_clocks ${slink}_clk_gen_slow_drv] -rise_to [get_clocks ${slink}_clk_gen_ddr]
    set_false_path -setup -fall_from [get_clocks ${slink}_clk_gen_slow_drv] -fall_to [get_clocks ${slink}_clk_gen_ddr]
    set_false_path -hold  -rise_from [get_clocks ${slink}_clk_gen_slow_drv] -fall_to [get_clocks ${slink}_clk_gen_ddr]
    set_false_path -hold  -fall_from [get_clocks ${slink}_clk_gen_slow_drv] -rise_to [get_clocks ${slink}_clk_gen_ddr]

    # Unfortunately, STA considers any cell with a clock and data pins checked with this clock an endpoint.
    # Here, we generate the clock `${slink}_clk_gen_slow_drv` driving the TX data register, then mux TX data with that clock
    # to convert from SDR to DDR. Even when the output remains stable, the first-level cells of the converting mux
    # may switch, producing an LSB endpoint event on each rising edge when the SDR holding register swaps its data;
    # this violates hold on the following falling edge checking the active-low LSB phase.
    # TODO @fischeti: This would not happen with a single-s glitch-free clock mux like in hyperbus; consider adapting RTL.
    # # Do not allow PHY (System) clock to leak to DDR outputs and be timed as output transitions
    # -through [get_pins $SLINK_TX/data_out_q_reg_0_/Q]

    # set SLO_CLK_CELLS     [get_cells -filter ref_name==sg13g2_mux2_1 [get_fanout -only_cells -from $SLO_PHY_TCLK_Q]]
    # TODO(fischeti): Check if this is needed
    # set SLO_CLK_CELLS     [all_transitive_fanout -flat -only_cells -from $SLINK_TX_SLOW_SRC_Q($slink)]
    # set SLO_CLK_MUX_PINS  [get_pins -of_objects $SLO_CLK_CELLS -filter "name == $MUX_CONTROL_PIN"]
    # set_sense -clock -stop_propagation $SLO_CLK_MUX_PINS
}


##############
## I/O pads ##
##############

# We set some arbitrary case analysis for the driver strength values.
set_case_analysis 0 [get_pins -hierarchical *drv_strength__value__0_/Q]
set_case_analysis 0 [get_pins -hierarchical *drv_strength__value__1_/Q]
set_case_analysis 0 [get_pins -hierarchical *drv_strength__value__2_/Q]
set_case_analysis 1 [get_pins -hierarchical *drv_strength__value__3_/Q]

############
## Resets ##
############

# Global reset is asynchronous and can last multiple cycles; sub-block resets
# are software-controlled. Create a multicycle path for the global reset signal
# allowing the reset up to 6 cycles to propagate
set RST_CYCLES 6
set_false_path -fall -from                                    [get_ports pad_rst_ni]
set_multicycle_path -rise -setup $RST_CYCLES            -from [get_ports pad_rst_ni]
set_multicycle_path -rise -hold  [expr $RST_CYCLES - 1] -from [get_ports pad_rst_ni]

set sw_reset_pins [get_pins -hierarchical i_cheshire_tile/*rst_no]
set_false_path -fall -through                                    $sw_reset_pins
set_multicycle_path -rise -setup $RST_CYCLES            -through $sw_reset_pins
set_multicycle_path -rise -hold  [expr $RST_CYCLES - 1] -through $sw_reset_pins
