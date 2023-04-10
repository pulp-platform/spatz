// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

#include <iostream>

#include "sim.hh"
#include "tb_lib.hh"

namespace sim {

// Bootloader
extern "C" {
extern const uint8_t tb_bootrom_start;
extern const uint8_t tb_bootrom_end;
}

asm(".global tb_bootrom_start \n"
    ".global tb_bootrom_end \n"
    "tb_bootrom_start: .incbin \"test/bootrom.bin\" \n"
    "tb_bootrom_end: \n");

// The global memory all memory ports write into.
GlobalMemory MEM;

// Override HTIF to populate bootloader with system specification and entry
// symbol.
void Sim::start() {
    htif_t::start();

    // Write the bootloader into memory.
    size_t bllen = (&tb_bootrom_end - &tb_bootrom_start);
    MEM.write(BOOTDATA.boot_addr, bllen, &tb_bootrom_start, nullptr);
    std::cerr << "Wrote " << bllen << " bytes of bootrom to 0x" << std::hex
              << BOOTDATA.boot_addr << "\n";
}

void Sim::read_chunk(addr_t taddr, size_t len, void *dst) {
    MEM.read(taddr, len, reinterpret_cast<uint8_t *>(dst));
}

void Sim::write_chunk(addr_t taddr, size_t len, const void *src) {
    uint8_t strb[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    MEM.write(taddr, len, reinterpret_cast<const uint8_t *>(src), strb);
}

}  // namespace sim
