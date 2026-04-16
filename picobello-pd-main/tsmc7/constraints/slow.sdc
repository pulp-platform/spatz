# Copyright (c) 2025 ETH Zurich.
# Authors:
# - Tim Fischer <fischeti@iis.ee.ethz.ch>

###################
## Case Analysis ##
###################

set_case_analysis 0 [get_ports pad_test_mode_i]

# We bypass the SW reset and clock enable registers in this mode,
# (No need to set register values, as they are bypassed)
set_case_analysis 1 [get_ports pad_clk_rst_bypass_i]

##################
## Input Clocks ##
##################
puts "Clocks..."

remove_clock -all

# No FLL clock in this mode.

set REF_FREQ_HZ 100e6
set REF_TCK [freq_to_period $REF_FREQ_HZ]
create_clock -name ref_clk -period $REF_TCK [get_pins i_pad_ref_clk_i/C]

set JTAG_FREQ_HZ 30e6
set JTAG_TCK [freq_to_period $JTAG_FREQ_HZ]
create_clock -name jtag_clk -period $JTAG_TCK [get_ports pad_jtag_tck_i]

set RTC_FREQ_HZ 1e6
set RTC_TCK [freq_to_period $RTC_FREQ_HZ]
create_clock -name rtc_clk -period $RTC_TCK [get_ports pad_rtc_clk_i]

set SLINK_TCK [expr 4 * $REF_TCK]
create_clock -name slink_clk -period $SLINK_TCK [get_ports pad_slink_rcv_clk_i]
create_clock -name dram_slink_clk -period $SLINK_TCK [get_ports pad_dram_slink_rcv_clk_i]

set SPI_FREQ_HZ 100e6
set SPI_TCK [freq_to_period $SPI_FREQ_HZ]

set I2C_FREQ_HZ 100e6
set I2C_TCK [freq_to_period $I2C_FREQ_HZ]

# In the `slow` mode, we bypass the FLL, so we do a case analysis
# to ensure that the FLL is used as the clock source.
set_case_analysis 1 [get_ports pad_bypass_fll_i]
create_generated_clock -name soc_clk -divide_by 1 \
    -source [get_pins i_pad_ref_clk_i/C] \
    [get_pins i_clk_mux_glitch_free/i_test_clk_mux/i_clk_mux2/Z]

# The system clock frequency is defined by the reference clock.
# This is used for the common constraints.
set SYS_TCK $REF_TCK

# Source the rest of the constraints, which are common to all modes
# and depend on the clock definitions above.
source $PDDIR/tsmc7/constraints/mode_common.sdc
