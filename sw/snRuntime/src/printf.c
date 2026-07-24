// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
#include "snrt.h"

// Write characters to the testbench fake UART at 0xA000_0000 (see the
// `fake_uart` symbol in the linker script). Unlike the HTIF syscall path in
// snrt_putchar, this also works when main memory is modeled by DRAMSys and
// the host cannot observe the putchar buffer.
extern volatile char fake_uart;

void _putchar(char character) { fake_uart = character; }

/// vendor printf settings

#if defined(__TOOLCHAIN_GCC__)
// the gcc toolchain doesn't support this
#define PRINTF_DISABLE_SUPPORT_FLOAT
#endif

// Include the vendorized tiny printf implementation.
#include "../vendor/printf.c"
