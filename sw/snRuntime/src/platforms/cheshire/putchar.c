// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
#include "snrt.h"

// UART base
#define CHS_UART_BASE 0x03002000

// Register offsets
#define CHS_UART_RBR_REG_OFFSET 0
#define CHS_UART_THR_REG_OFFSET 0
#define CHS_UART_INTR_ENABLE_REG_OFFSET 4
#define CHS_UART_INTR_IDENT_REG_OFFSET 8
#define CHS_UART_FIFO_CONTROL_REG_OFFSET 8
#define CHS_UART_LINE_CONTROL_REG_OFFSET 12
#define CHS_UART_MODEM_CONTROL_REG_OFFSET 16
#define CHS_UART_LINE_STATUS_REG_OFFSET 20
#define CHS_UART_MODEM_STATUS_REG_OFFSET 24
#define CHS_UART_DLAB_LSB_REG_OFFSET 0
#define CHS_UART_DLAB_MSB_REG_OFFSET 4

// Register fields
#define CHS_UART_LINE_STATUS_DATA_READY_BIT 0
#define CHS_UART_LINE_STATUS_THR_EMPTY_BIT 5
#define CHS_UART_LINE_STATUS_TMIT_EMPTY_BIT 6

static inline volatile uint8_t *reg8(void *base, int offs) {
    return (volatile uint8_t *)(base + offs);
}

static inline int __chs_uart_write_ready(void *uart_base) {
    return *reg8(CHS_UART_BASE, CHS_UART_LINE_STATUS_REG_OFFSET) & (1 << CHS_UART_LINE_STATUS_THR_EMPTY_BIT);
}

void chs_uart_write(void *uart_base, uint8_t byte) {
    while (!__chs_uart_write_ready(uart_base))
        ;
    *reg8(CHS_UART_BASE, CHS_UART_THR_REG_OFFSET) = byte;
}

// Provide an implementation for putchar.
void snrt_putchar(char character) {
    chs_uart_write(CHS_UART_BASE, character);
}
