// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Fabian Schuiki <fschuiki@iis.ee.ethz.ch>

package snitch_icache_pkg;

  typedef struct packed {
    logic l0_miss;
    logic l0_hit;
    logic l0_prefetch;
    logic l0_double_hit;
    logic l0_stall;
    logic l0_tag_parity_error;
    logic l0_data_parity_error;
  } icache_l0_events_t;

  typedef struct packed {
    logic l1_miss;
    logic l1_hit;
    logic l1_stall;
    logic l1_handler_stall;
    logic l1_tag_parity_error;
    logic l1_data_parity_error;
  } icache_l1_events_t;

  typedef struct packed {
    // Parameters passed to the root module.
    int unsigned NR_FETCH_PORTS;
    int unsigned LINE_WIDTH;
    int unsigned LINE_COUNT;
    int unsigned SET_COUNT;
    int unsigned PENDING_COUNT;
    int unsigned L0_LINE_COUNT;
    int unsigned FETCH_AW;
    int unsigned FETCH_DW;
    int unsigned FILL_AW;
    int unsigned FILL_DW;
    int unsigned L1_DATA_PARITY_BITS;
    int unsigned L0_DATA_PARITY_BITS;
    bit L1_TAG_SCM;
    bit EARLY_LATCH;
    bit BUFFER_LOOKUP;
    bit GUARANTEE_ORDERING;
    bit L0_PLRU;
    bit L1_PLRU;

    // Derived values.
    int unsigned FETCH_ALIGN;
    int unsigned FILL_ALIGN;
    int unsigned LINE_ALIGN;
    int unsigned COUNT_ALIGN;
    int unsigned SET_ALIGN;
    int unsigned TAG_WIDTH;
    int unsigned L0_TAG_WIDTH;
    int unsigned L0_EARLY_TAG_WIDTH;
    int unsigned ID_WIDTH;
    int unsigned PENDING_IW; // refill ID width
  } config_t;

endpackage
