// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

#include <stdint.h>

// The boot data generated along with the system RTL.
struct BootData {
    uint64_t boot_addr;
    uint64_t core_count;
    uint64_t hartid_base;
    uint64_t tcdm_start;
    uint64_t tcdm_size;
    uint64_t tcdm_offset;
    uint64_t global_mem_start;
    uint64_t global_mem_end;
};

extern "C" const BootData BOOTDATA = {.boot_addr = ${hex(cfg['cluster']['boot_addr'])},
                           .core_count = ${cfg['cluster']['nr_cores']},
                           .hartid_base = ${cfg['cluster']['cluster_base_hartid']},
                           .tcdm_start = ${hex(cfg['cluster']['cluster_base_addr'])},
                           .tcdm_size = ${hex(cfg['cluster']['tcdm']['size'] * 1024)},
                           .tcdm_offset = ${hex(cfg['cluster']['cluster_base_offset'])},
                           .global_mem_start = ${hex(cfg['dram']['address'])},
                           .global_mem_end = ${hex(cfg['dram']['address'] + cfg['dram']['length'])}};