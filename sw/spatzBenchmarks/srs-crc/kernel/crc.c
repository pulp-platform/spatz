#include "crc.h"
#include <stddef.h>
#include <snrt.h>
#include <stdbool.h>

void crc_init(crc_t *crc, uint32_t polynom, unsigned order)
{
    crc->order    = order;
    crc->mask     = (((uint32_t)1 << (order - 1)) - 1) << 1 | 1;
    crc->poly_low = polynom & crc->mask;
    crc->polynom  = polynom;

    unsigned pad     = (order < 8) ? (8 - order) : 0;
    unsigned ord     = order + pad - 8;
    uint32_t crchighbit = (uint32_t)1 << (order - 1);

    for (unsigned i = 0; i < 256; i++) {
        uint32_t remainder = (uint32_t)i << ord;
        for (unsigned j = 0; j < 8; j++) {
            bool bit = (remainder & crchighbit) != 0;
            remainder <<= 1;
            if (bit) remainder ^= polynom;
        }
        crc->table[i] = (remainder >> pad) & crc->mask;
    }
}

static uint32_t crc_put_byte(const crc_t *crc, uint32_t reg, uint8_t byte)
{
    unsigned idx;
    if (crc->order == 24) {
        idx = ((reg >> 16) & 0xFF) ^ byte;
    } else if (crc->order > 8) {
      unsigned ord = crc->order - 8;
      idx          = ((reg >> ord) & 0xFF) ^ byte;
    } else {
      unsigned ord = 8 - crc->order;
      idx          = ((reg << ord) & 0xFF) ^ byte;
    }
    return ((reg << 8) ^ crc->table[idx]);
}

static uint32_t crc_reverse_bit(const crc_t *crc, uint32_t reg, unsigned nbits)
{
    uint32_t m, rmask = 0x1;
    for (m = 0; m < nbits; m++) {
        if ((rmask & reg) == 0x01)
            reg = (reg ^ crc->polynom) >> 1;
        else
            reg = reg >> 1;
    }
    return reg & crc->mask;
}

uint32_t crc_calculate(const crc_t *crc, const uint8_t *data, size_t num_bits)
{
    if (crc->order < 8) {
        uint32_t highbit = (uint32_t)1 << crc->order;
        uint32_t rem = 0;
        for (size_t i = 0; i < num_bits; i++) {
            uint32_t bit = (data[i >> 3] >> (7 - (i & 7))) & 1;
            rem = (rem << 1) | bit;
            if (rem & highbit) rem ^= crc->polynom;
        }
        for (unsigned k = 0; k < crc->order; k++) {
            rem <<= 1;
            if (rem & highbit) rem ^= crc->polynom;
        }
        return rem & crc->mask;
    }

    uint32_t reg    = 0;
    size_t   nbytes = num_bits / 8;
    unsigned res8   = num_bits % 8;

    for (size_t i = 0; i < nbytes; i++)
        reg = crc_put_byte(crc, reg, data[i]);

    if (res8 > 0) {
        uint8_t last = data[nbytes] & (uint8_t)(0xFF << (8 - res8));
        reg = crc_put_byte(crc, reg, last);
    }
    reg = reg & crc->mask;
    if (res8 > 0)
        reg = crc_reverse_bit(crc, reg, 8 - res8);

    return reg & crc->mask;
}