# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

#############
## Helpers ##
#############

proc get_clk_pin {cell} {
  return [get_pins -of_objects $cell -filter "name==clocked_on"]
}

proc get_q_pin {cell} {
  return [get_pins -of_objects $cell -filter "name==Q"]
}

proc get_d_pin {cell} {
  return [get_pins -of_objects $cell -filter "name==next_state"]
}

proc get_fanin_to {port_or_pin} {
  return [all_transitive_fanin -to $port_or_pin -flat -startpoints_only -only_cells]
}

proc get_fanout_from {port_or_pin} {
  return [all_transitive_fanout -from $port_or_pin -flat -endpoints_only -only_cells]
}

#################
## Serial Link ##
#################

foreach slink {slink dram_slink} {
  set SL_IN($slink)       [get_ports pad_${slink}_i]
  set SL_OUT($slink)      [get_ports pad_${slink}_o]
  set SL_OUT_CLK($slink)  [get_ports pad_${slink}_rcv_clk_o]

  # The source register for the forwarded clock is the fanin of the `*_rcv_clk_o` port
  set SLINK_TX_DDR_SRC_REG($slink) [filter_collection [get_fanin_to [get_ports pad_${slink}_rcv_clk_o[0]]] "name==ddr_rcv_clk_o_reg"]
  set SLINK_TX_DDR_SRC_CLK($slink) [get_clk_pin $SLINK_TX_DDR_SRC_REG($slink)]
  set SLINK_TX_DDR_SRC_Q($slink) [get_q_pin $SLINK_TX_DDR_SRC_REG($slink)]

  # The source register for the slow clock is in the same hierarchy
  # as the source register for the `*_rcv_clk_o` port but is called `clk_slow_reg`
  set SLINK_TX_SLOW_SRC_REG($slink) [get_cells [join [lrange [split [get_object_name $SLINK_TX_DDR_SRC_REG($slink)] "/"] 0 end-1] "/"]/clk_slow_reg]
  set SLINK_TX_SLOW_SRC_CLK($slink) [get_clk_pin $SLINK_TX_SLOW_SRC_REG($slink)]
  set SLINK_TX_SLOW_SRC_Q($slink) [get_q_pin $SLINK_TX_SLOW_SRC_REG($slink)]
}

####################
## Cdcs and Syncs ##
####################

set CHS_DEBUG_CDC [get_object_name [get_cells -hierarchical i_dmi_cdc]]
set ASYNC_PINS_DMIREQ [get_nets $CHS_DEBUG_CDC/i_cdc_req/*async_*]
set ASYNC_PINS_DMIRSP [get_nets $CHS_DEBUG_CDC/i_cdc_resp/*async_*]

set ASYNC_PINS_SLINK [add_to_collection "" ""]
foreach slink_cdc [get_object_name [get_cells -hierarchical i_cdc_in]] {
  set ASYNC_PINS_SLINK [add_to_collection $ASYNC_PINS_SLINK [get_nets $slink_cdc/*async_*]]
}

set CLINT_ASYNC_REG [get_fanout_from [get_ports pad_rtc_clk_i]]
set ASYNC_PINS_CLINT [get_d_pin $CLINT_ASYNC_REG]

#################
## Peripherals ##
#################

# We define a single leaf register as a reference clock pin for master interfaces to avoid accumulating CTS delays
# this can really be the clock pin of any register in the peripheral

set SPI_SCK_REF_REG [filter_collection [get_fanin_to [get_ports pad_spih_sck_o]] "full_name=~*u_sck_flop*"]
set SPI_SCK_REF_CLK [get_clk_pin $SPI_SCK_REF_REG]
set SPI_SCK_REF_Q [get_q_pin $SPI_SCK_REF_REG]

set I2C_REF_REG [filter_collection [get_fanin_to [get_ports pad_i2c_scl_io]] "full_name=~*scl_q_reg*"]
set I2C_REF_CLK [get_clk_pin $I2C_REF_REG]
set I2C_REF_Q [get_q_pin $I2C_REF_REG]
