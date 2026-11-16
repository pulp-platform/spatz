// Copyright 2022 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Ihor Sachok <ihor.sachok@unibo.it>

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>
#include <math.h>

#include DATAHEADER
#include "kernel/crc.c"


// static void append_bits(uint8_t *buf, size_t bit_offset, uint64_t value, unsigned nbits)
// {
//     for (unsigned k = 0; k < nbits; k++) {
//         unsigned bit = (value >> (nbits - 1 - k)) & 1;
//         size_t   pos = bit_offset + k;
//         if (bit) buf[pos / 8] |=  (uint8_t)(0x80 >> (pos % 8));
//         else     buf[pos / 8] &= (uint8_t)~(0x80 >> (pos % 8));
//     }
// }

// static int roundtrip(const char *name, uint64_t poly, unsigned order,
//                      const uint8_t *m, size_t msg_bits)
// {
//     size_t total      = msg_bits + order;
//     size_t word_bytes = (total + 7) / 8;

//     uint8_t word[word_bytes];
//     for (size_t i = 0; i < word_bytes; i++) word[i] = 0;

//     crc_init(&crc_ctx, poly, order);
//     uint64_t c = crc_calculate(&crc_ctx, m, msg_bits);

//     for (size_t i = 0; i < (msg_bits + 7) / 8; i++) word[i] = m[i];
//     if (msg_bits % 8) word[msg_bits / 8] &= (uint8_t)(0xFF << (8 - msg_bits % 8));
//     append_bits(word, msg_bits, c, order);

//     uint64_t check = crc_calculate(&crc_ctx, word, total);
//     printf("%-7s order=%2u  CRC=0x%06llX  check=%llu  %s\n",
//            name, order, (unsigned long long)c, (unsigned long long)check,
//            check == 0 ? "OK" : "FAIL");
//     return check == 0 ? 0 : 1;
// }

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid       = snrt_cluster_core_idx();
  uint8_t *msg;
  static crc_t crc_ctx;

  // Reset timer
  unsigned int timer = (unsigned int)-1;
  uint32_t c = 0;

  // Allocate message buffer in L1
  if (cid == 0) {
    //msg = (uint8_t *)snrt_l1alloc(msg_len * sizeof(uint8_t));
    msg = (uint8_t *)snrt_l1alloc(CRC_MSG_LEN * sizeof(uint8_t));
  }

  // Load message from DRAM into L1 via DMA
  if (cid == 0) {
    //snrt_dma_start_1d(msg, crc_msg_dram, msg_len * sizeof(uint8_t));
    snrt_dma_start_1d(msg, crc_msg_dram, CRC_MSG_LEN * sizeof(uint8_t));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Start dump
  if (cid == 0)
    start_kernel();
  // Actual work: only core 0 (scalar CRC)
  // int failures = 0;
  // if (cid == 0) {
  //   failures += roundtrip("CRC24A", 0x1864cfb, 24, msg, msg_len * 8);
  //   failures += roundtrip("CRC16",  0x11021,   16, msg, msg_len * 8);
  //   failures += roundtrip("CRC11",  0xE21,     11, msg, msg_len * 8);
  //   failures += roundtrip("CRC24A", 0x1864cfb, 24, msg, msg_len * 8 - 3);
  //   failures += roundtrip("CRC11",  0xE21,     11, msg, msg_len * 8 - 5);
  // }
  if (cid == 0)
    crc_init(&crc_ctx, CRC_POLY, CRC_ORDER);
  // Start timer
  if (cid == 0)
    timer = benchmark_get_cycle();

  if (cid == 0){
    c = crc_calculate(&crc_ctx, msg, (size_t)CRC_MSG_LEN * 8);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // End dump
  if (cid == 0)
    stop_kernel();

  // End timer
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Display results
  // if (cid == 0) {
  //   printf("\n----- CRC (%d bytes) -----\n", msg_len);
  //   printf("The execution took %u cycles.\n", timer);
  //   if (failures == 0)
  //     printf("All CRC checks passed.\n");
  //   else
  //     printf("%d CRC check(s) FAILED.\n", failures);
  // }
  if (cid == 0) {
    printf("----- CRC (%u bytes, order=%u) -----\n", CRC_MSG_LEN, CRC_ORDER);
    printf("The execution took %u cycles.\n", timer);
    if (c == CRC_GOLD)
      printf("CRC 0x%llX OK\n", (unsigned long long)c);
    else
      printf("CRC got=0x%llX exp=0x%llX FAIL\n",
             (unsigned long long)c, (unsigned long long)CRC_GOLD);
  }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return 0;
}