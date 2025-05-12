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

#ifndef __ARCHI_HWPE_H__
#define __ARCHI_HWPE_H__

#define ARCHI_CL_EVT_ACC0 0
#define ARCHI_CL_EVT_ACC1 1
#define ARCHI_HWPE_ADDR_BASE 0x00000000

#define HWPE_TRIGGER          0x00
#define HWPE_ACQUIRE          0x04
#define HWPE_FINISHED         0x08
#define HWPE_STATUS           0x0c
#define HWPE_RUNNING_JOB      0x10
#define HWPE_SOFT_CLEAR       0x14

#define HWPE_BASE_ADDR       0x40
#define HWPE_N_DATA          0x44
#define HWPE_DATA_SIZE       0x48
#define HWPE_REORDER_ENABLE  0x4c

#endif
