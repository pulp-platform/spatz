// Copyright 2022 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Nils Wistoff <nwistoff@iis.ee.ethz.ch>
// Paul Scheffler <paulsc@iis.ee.ethz.ch>

#pragma once

#include <stdint.h>

// Register offsets
#define UART_RBR_REG_OFFSET 0
#define UART_THR_REG_OFFSET 0
#define UART_INTR_ENABLE_REG_OFFSET 4
#define UART_INTR_IDENT_REG_OFFSET 8
#define UART_FIFO_CONTROL_REG_OFFSET 8
#define UART_LINE_CONTROL_REG_OFFSET 12
#define UART_MODEM_CONTROL_REG_OFFSET 16
#define UART_LINE_STATUS_REG_OFFSET 20
#define UART_MODEM_STATUS_REG_OFFSET 24
#define UART_DLAB_LSB_REG_OFFSET 0
#define UART_DLAB_MSB_REG_OFFSET 4

// Register fields
#define UART_LINE_STATUS_DATA_READY_BIT 0
#define UART_LINE_STATUS_THR_EMPTY_BIT 5
#define UART_LINE_STATUS_TMIT_EMPTY_BIT 6

// Base address of uart
#define __base_uart 0x03002000

static inline volatile uint8_t *reg8(void *base, int offs) {
    return (volatile uint8_t *)((uint8_t *)base + offs);
}

static inline volatile uint32_t *reg32(void *base, int offs) {
    return (volatile uint32_t *)((uint8_t *)base + offs);
}

void uart_init(void *uart_base, uint64_t freq, uint64_t baud) {
    uint64_t divisor = freq / (baud << 4);
    uint8_t dlo = (uint8_t)(divisor);
    uint8_t dhi = (uint8_t)(divisor >> 8);
    *reg8(uart_base, UART_INTR_ENABLE_REG_OFFSET) = 0x00;   // Disable all interrupts
    *reg8(uart_base, UART_LINE_CONTROL_REG_OFFSET) = 0x80;  // Enable DLAB (set baud rate divisor)
    *reg8(uart_base, UART_DLAB_LSB_REG_OFFSET) = dlo;       // divisor (lo byte)
    *reg8(uart_base, UART_DLAB_MSB_REG_OFFSET) = dhi;       // divisor (hi byte)
    *reg8(uart_base, UART_LINE_CONTROL_REG_OFFSET) = 0x03;  // 8 bits, no parity, one stop bit
    *reg8(uart_base, UART_FIFO_CONTROL_REG_OFFSET) = 0xC7;  // Enable & clear FIFO, 14B threshold
    *reg8(uart_base, UART_MODEM_CONTROL_REG_OFFSET) = 0x20; // Autoflow mode
}

int uart_read_ready(void *uart_base);

void uart_write(void *uart_base, uint8_t byte);

void uart_write_str(void *uart_base, void *src, uint64_t len);

void uart_write_flush(void *uart_base);

uint8_t uart_read(void *uart_base);

void uart_read_str(void *uart_base, void *dst, uint64_t len);

// Default UART provides console
void _putchar(char byte);

char _getchar();