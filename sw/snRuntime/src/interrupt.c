// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
#include "encoding.h"
#include "printf.h"
#include "snrt.h"
#include "team.h"

//================================================================================
// Macros
//================================================================================

/// The cluster-local clint is in the cluster peripheral system which has 64 bit
/// register width
#define cl_clint_set(x) *(clint_p + 0) = (x)
#define cl_clint_clr(x) *(clint_p + 2) = (x)

/// The uncorrectable-fault recovery IRQ registers (ENABLE/STATUS/CLEAR) sit at
/// consecutive 64-bit-wide slots in the cluster peripheral, same as the clint above.
#define uncorrectable_irq_status_get() (*(uncorrectable_irq_p + 2))
#define uncorrectable_irq_clear_set(x) *(uncorrectable_irq_p + 4) = (x)

/// Recovery is a plain restart of the whole program from the top of main(), so a fault
/// that keeps recurring (e.g. a permanently broken cell or a false positive) can't retry forever.
#define SNRT_RECOVERY_MAX_RETRIES 3
#define SNRT_EXIT_UNRECOVERABLE_FAULT 0xFA17u

//================================================================================
// Data
//================================================================================
static volatile uint32_t clint_mutex = 0;
static __thread volatile uint32_t *clint_p;
static __thread volatile uint32_t *uncorrectable_irq_p;
static __thread uint32_t snrt_recovery_top_of_stack;
static __thread uint32_t snrt_recovery_retry_count;

//================================================================================
// ISR definitions
//================================================================================

void irq_m_soft(uint32_t core_idx);
void irq_m_timer(uint32_t core_idx);
void irq_m_ext(uint32_t core_idx);
void irq_m_cluster(uint32_t core_idx);

//================================================================================
// Runtime assembly definitions
//================================================================================

extern void _snrt_exit(uint32_t code);
// Restart entry point in start.S.in: resets sp to the saved pristine stack top
// for this hart, then re-enters main() via the normal crt0 tail.
extern void snrt_recovery_restart_point(void);

//================================================================================
// Public functions
//================================================================================

void snrt_int_init(struct snrt_team_root *team) {
    // Put the clint address in tls for faster access
    clint_p = team->peripherals.cl_clint;
    uncorrectable_irq_p = team->peripherals.uncorrectable_irq;
    
    // Arm the uncorrectable-fault recovery interrupt once, from one hart.
    // Layout (64-bit slots): [0] ENABLE, [2] STATUS, [4] CLEAR
    if (snrt_cluster_core_idx() == 0) {
        uncorrectable_irq_clear_set(0xFFFFFFFFu);   // drop anything latched during boot
        uncorrectable_irq_p[0] = 0xFFFFFFFFu;       // enable for all harts
    }

    // Uncorrectable-fault recovery relies on the cluster-local interrupt (mcip);
    // this project treats reliability features as unconditional defaults, so
    // enable it here rather than requiring every program to opt in.
    snrt_interrupt_enable(IRQ_M_CLUSTER);
    snrt_interrupt_global_enable();
}

/**
 * @brief Called from start.S.in once per hart, right after this hart's stack
 * pointer is finalized, to remember the pristine stack top for later
 * fault-recovery restarts.
 *
 * @param sp_value this hart's stack pointer immediately after crt0 stack setup
 */
void snrt_recovery_save_sp(uint32_t sp_value) {
    snrt_recovery_top_of_stack = sp_value;
}

/**
 * @brief Called from snrt_recovery_restart_point in start.S.in to recover the
 * stack pointer saved by snrt_recovery_save_sp.
 */
uint32_t snrt_recovery_get_saved_sp(void) { return snrt_recovery_top_of_stack; }

/**
 * @brief Recover from a Class-A uncorrectable fault (VRF/TCDM ECC, FPU
 * shadow-checker mismatch, icache parity, control-FSM illegal-state) by
 * clearing the fault and restarting the whole program from the top of
 * main(), or giving up after too many retries.
 * @details The corrupted data itself can't be un-corrupted, but every
 * benchmark in this suite re-fetches its inputs from DRAM and overwrites
 * (rather than accumulates into) its outputs, so restarting from the top is
 * equivalent to a fresh, correct run.
 *
 * @param cluster_core_idx index of this hart within the cluster
 */
static void snrt_recover_from_uncorrectable_fault(uint32_t cluster_core_idx) {
    uncorrectable_irq_clear_set(1u << cluster_core_idx);
    if (++snrt_recovery_retry_count > SNRT_RECOVERY_MAX_RETRIES) {
        _snrt_exit(SNRT_EXIT_UNRECOVERABLE_FAULT);
    }
    write_csr(mepc, (uintptr_t)&snrt_recovery_restart_point);
}

/**
 * @brief Interrupt service routine called by start.S on interrupts and
 * exceptions
 * @details
 *
 * @param hartid hart ID of the interrupted core
 */
void __snrt_isr(void) {
    uint32_t core_idx = snrt_global_core_idx();
    uint32_t cause = read_csr(mcause);
    // dispatch interrupt
    if (cause & MCAUSE_INTERRUPT) {
        switch (cause & ~MCAUSE_INTERRUPT) {
            case IRQ_M_SOFT:
                irq_m_soft(core_idx);
                break;
            case IRQ_M_TIMER:
                irq_m_timer(core_idx);
                break;
            case IRQ_M_EXT:
                irq_m_ext(core_idx);
                break;
            case IRQ_M_CLUSTER:
                core_idx = snrt_cluster_core_idx();
                if (uncorrectable_irq_status_get() & (1u << core_idx)) {
                    snrt_recover_from_uncorrectable_fault(core_idx);
                } else {
                    irq_m_cluster(core_idx);
                }
                break;
        }
    } else {
      _snrt_exit(0xBAD);
    }
}

/**
 * @brief Clear SW interrupt in CLINT
 * @details
 *
 * @param hartid Target interrupt to clear
 */
void snrt_int_sw_clear(uint32_t hartid) {
    snrt_mutex_lock(&clint_mutex);
    *(clint_p + ((hartid & ~0x1f) >> 5)) &= ~(1 << (hartid & 0x1f));
    snrt_mutex_release(&clint_mutex);
}

/**
 * @brief Set SW interrupt in CLINT
 * @details
 *
 * @param hartid Target interrupt to set
 */
void snrt_int_sw_set(uint32_t hartid) {
    snrt_mutex_lock(&clint_mutex);
    *(clint_p + ((hartid & ~0x1f) >> 5)) |= (1 << (hartid & 0x1f));
    snrt_mutex_release(&clint_mutex);
}

/**
 * @brief Read SW interrupt for hartid in CLINT
 *
 * @param hartid hartid to poll for interrupt flag
 * @return uint32_t 0 if no SW interrupt is pending, 1 otherwise
 */
uint32_t snrt_int_sw_get(uint32_t hartid) {
    snrt_mutex_lock(&clint_mutex);
    uint32_t ret = *(clint_p + ((hartid & ~0x1f) >> 5)) >> (hartid & 0x1f);
    snrt_mutex_release(&clint_mutex);
    return ret;
}

/**
 * @brief Set a `mask` of bits in the CLINT register `reg_off` (word offset).
 * This can be used to send interrupts to a set of cores
 *
 * @param reg_off CLINT register offset in word (=byte_off/4)
 * @param mask bit mask to set
 */
void snrt_int_clint_set(uint32_t reg_off, uint32_t mask) {
    snrt_mutex_lock(&clint_mutex);
    *(clint_p + reg_off) |= mask;
    snrt_mutex_release(&clint_mutex);
}

/**
 * @brief Go to wfi and exit with cleared SW interrupt if a SW interrupt
 * is presen
 *
 */
void snrt_int_sw_poll(void) {
    uint32_t exit = 0, hartid = snrt_hartid();
    while (!exit) {
        snrt_wfi();
        snrt_mutex_lock(&clint_mutex);
        if (*(clint_p + ((hartid & ~0x1f) >> 5)) >> (hartid & 0x1f)) {
            *(clint_p + ((hartid & ~0x1f) >> 5)) &= ~(1 << (hartid & 0x1f));
            exit = 1;
        }
        snrt_mutex_release(&clint_mutex);
    }
}

/**
 * @brief Write mask to the cluster-local interrupt clear register
 * @param mask set bit at X clears the interrupt of hart X
 */
void snrt_int_cluster_clr(uint32_t mask) { cl_clint_clr(mask); }

/**
 * @brief Write mask to the cluster-local interrupt set register
 * @param mask set bit at X sets the interrupt of hart X
 */
void snrt_int_cluster_set(uint32_t mask) { cl_clint_set(mask); }

//================================================================================
// Weak definition of IRQ handler
//================================================================================

void __attribute__((weak)) irq_m_soft(uint32_t core_idx) {
    snrt_int_sw_clear(core_idx);
}

void __attribute__((weak)) irq_m_timer(uint32_t core_idx) { (void)core_idx; }

void __attribute__((weak)) irq_m_ext(uint32_t core_idx) { (void)core_idx; }

void __attribute__((weak)) irq_m_cluster(uint32_t cluster_core_idx) {
    snrt_int_cluster_clr(1 << cluster_core_idx);
}
