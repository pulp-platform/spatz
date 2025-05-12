/*
 * Copyright (C) 2021 ETH Zurich and University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Authors: Luca Bertaccini <lbertaccini@iis.ee.ethz.ch>
 */

#ifndef __HAL_HWPE_H__
#define __HAL_HWPE_H__

//#include "hal/pulp.h"

extern unsigned int hwpe_sel_addr;
extern unsigned int hwpe_base_addr;
extern unsigned int hwpe_cg_addr;

/* LOW-LEVEL HAL */
#define HWPE_ADDR_BASE ARCHI_HWPE_ADDR_BASE
#define HWPE_ADDR_SPACE 0x00000100

// For all the following functions we use __builtin_pulp_OffsetedWrite and __builtin_pulp_OffsetedRead
// instead of classic load/store because otherwise the compiler is not able to correctly factorize
// the HWPE base in case several accesses are done, ending up with twice more code

#define HWPE_WRITE_CL_PERIPH(address, value) *(int *)(address) = value
#define HWPE_WRITE(value, offset) *(int *)(HWPE_ADDR_BASE + offset) = value
#define HWPE_READ(offset) *(int *)(HWPE_ADDR_BASE + offset)


static inline void hwpe_base_addr_set(unsigned int value) {
  HWPE_WRITE(value, hwpe_base_addr+HWPE_BASE_ADDR);
}

static inline void hwpe_n_data_set(unsigned int value) {
  HWPE_WRITE(value, hwpe_base_addr+HWPE_N_DATA);
}

static inline void hwpe_data_size_set(unsigned int value) {
  HWPE_WRITE(value, hwpe_base_addr+HWPE_DATA_SIZE);
}

static inline void hwpe_reorder_enable_set(unsigned int value) {
  HWPE_WRITE(value, hwpe_base_addr+HWPE_REORDER_ENABLE);
}

static inline void hwpe_trigger_job() {
  HWPE_WRITE(0, hwpe_base_addr+HWPE_TRIGGER);
}

static inline int hwpe_acquire_job() {
  return HWPE_READ(hwpe_base_addr+HWPE_ACQUIRE);
}

static inline unsigned int hwpe_get_status() {
  return HWPE_READ(hwpe_base_addr+HWPE_STATUS);
}

static inline void hwpe_soft_clear() {
  volatile int i;
  HWPE_WRITE(0, hwpe_base_addr+HWPE_SOFT_CLEAR);
}

static inline void hwpe_mux_enable() {
  unsigned int i = 0;
  HWPE_WRITE_CL_PERIPH(hwpe_sel_addr, i);
}

static inline void hwpe_cg_enable() {
  unsigned int i = 1;
  HWPE_WRITE_CL_PERIPH(hwpe_cg_addr, i);
}

static inline void hwpe_cg_disable() {
  unsigned int i = 0;
  HWPE_WRITE_CL_PERIPH(hwpe_cg_addr, i);
}
/*
static inline void plp_hwpe_enable() {
  *(volatile int*) (ARCHI_SOC_EU_ADDR + (3 << 3)) |=  0xc00;
}

static inline void plp_hwpe_disable() {
  *(volatile int*) (ARCHI_SOC_EU_ADDR + (3 << 3)) &= ~0xc00;
}
*/
#endif /* __HAL_HWPE_H__ */
