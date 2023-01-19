// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  VSET(16, e8, m2);
  VLOAD_8(v4, 0x11, 0xd2, 0x6a, 0xcc, 0x14, 0xe4, 0x2c, 0x7f, 0xd2, 0x6b, 0x34,
          0x5c, 0x75, 0xdd, 0x0c, 0x42);
  VLOAD_8(v6, 0x77, 0xb2, 0xd1, 0x95, 0x6f, 0xbe, 0x0d, 0x5a, 0x93, 0x02, 0xaf,
          0xfd, 0x94, 0xe0, 0xb7, 0xe6);
  asm volatile("vremu.vv v2, v4, v6");
  VCMP_I8(1, v2, 0x11, 0x20, 0x6a, 0x37, 0x14, 0x26, 0x05, 0x25, 0x3f, 0x01,
          0x34, 0x5c, 0x75, 0xdd, 0x0c, 0x42);

  VSET(16, e16, m2);
  VLOAD_16(v4, 0xf77a, 0x54d7, 0xe527, 0xe28f, 0x53ed, 0x9301, 0xde4f, 0xcb17,
           0xae43, 0x9e4a, 0xa0c2, 0xdf31, 0xb66f, 0x286d, 0x1d15, 0x0480);
  VLOAD_16(v6, 0x5bfa, 0x0571, 0x8a43, 0x6350, 0xb962, 0x71fc, 0x0b54, 0x1e8b,
           0x6c25, 0x9c0d, 0x5950, 0x1887, 0xbc18, 0x628e, 0x6561, 0x407f);
  asm volatile("vremu.vv v2, v4, v6");
  VCMP_I16(2, v2, 0x3f86, 0x0338, 0x5ae4, 0x1bef, 0x53ed, 0x2105, 0x0713,
           0x13d5, 0x421e, 0x023d, 0x4772, 0x0272, 0xb66f, 0x286d, 0x1d15,
           0x0480);

  VSET(16, e32, m2);
  VLOAD_32(v4, 0x647d8841, 0xf9e0aabf, 0xea4aa122, 0xd6178d3e, 0x64a7afe5,
           0xe0350cba, 0xc72768ec, 0x9f977a31, 0x5e1c2ac4, 0xcd44b950,
           0x39dc32f4, 0x1dc82ea3, 0xd1cf125f, 0xc677269c, 0x6405ec5b,
           0x653a05ee);
  VLOAD_32(v6, 0x89828d99, 0x5c7c7db0, 0x2911efb6, 0x1f6982ff, 0x564e4bd4,
           0xc4576bff, 0x8e998104, 0x4a23ba44, 0x994b4630, 0x017ee935,
           0xa38c7dae, 0x893dfb15, 0x4969125f, 0x9a951d27, 0x09b6017f,
           0x5a0a7906);
  asm volatile("vremu.vv v2, v4, v6");
  VCMP_I32(3, v2, 0x647d8841, 0x40e7af5f, 0x1cf0f294, 0x199e7b44, 0x0e596411,
           0x1bdda0bb, 0x388de7e8, 0x0b5005a9, 0x5e1c2ac4, 0x0059ebf3,
           0x39dc32f4, 0x1dc82ea3, 0x3efceda1, 0x2be20975, 0x02e9dd65,
           0x0b2f8ce8);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v4, 0x09ab27501ccac4a6, 0x97eb5bf189b39a0e, 0x26f588069b0858c4,
           0x9a251c274a394df3, 0x54b3587602f8d9d2, 0xc3cc623deda95ca7,
           0x118c4335397980bf, 0xc3e2d283cb39133d, 0x71837e24114813fc,
           0x85a1f65867438a09, 0x80f01e0588afc9a0, 0x60e89a1e5a43d9f5,
           0x93a87cf6308ad888, 0xca3976f49ac6a681, 0xcfc7c8f225b47766,
           0xeaa4ce2cf507b527);
  VLOAD_64(v6, 0x9fed81c550326301, 0x445bb7ac18d0eaa1, 0x040f8ff58f5adf72,
           0xafc4ff6b8eb4d201, 0xfba36cabfc3fb4a0, 0x9c3ed271bf173d29,
           0xe8b7e325c9ff594b, 0x05169e56693600d7, 0x08e72c4bb62ad267,
           0xbd9677ee996d5fa5, 0x900295e8502a9817, 0x39e0bfa9927679a8,
           0xdd0ca7797d532524, 0x6f8f78c47ddee88a, 0x2f40f7661cca9eee,
           0x8e4a3b2358129e92);
  asm volatile("vremu.vv v2, v4, v6");
  VCMP_I64(4, v2, 0x09ab27501ccac4a6, 0x0f33ec995811c4cc, 0x0269786490d67dc2,
           0x9a251c274a394df3, 0x54b3587602f8d9d2, 0x278d8fcc2e921f7e,
           0x118c4335397980bf, 0x028751b02d34f353, 0x06ad6a9787463728,
           0x85a1f65867438a09, 0x80f01e0588afc9a0, 0x2707da74c7cd604d,
           0x93a87cf6308ad888, 0x5aa9fe301ce7bdf7, 0x12c3eb59b289fbae,
           0x5c5a93099cf51695);
#endif
};

void TEST_CASE2(void) {
  VSET(16, e8, m2);
  VLOAD_8(v4, 0x11, 0xd2, 0x6a, 0xcc, 0x14, 0xe4, 0x2c, 0x7f, 0xd2, 0x6b, 0x34,
          0x5c, 0x75, 0xdd, 0x0c, 0x42);
  VLOAD_8(v6, 0x77, 0xb2, 0xd1, 0x95, 0x6f, 0xbe, 0x0d, 0x5a, 0x93, 0x02, 0xaf,
          0xfd, 0x94, 0xe0, 0xb7, 0xe6);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vremu.vv v2, v4, v6, v0.t");
  VCMP_I8(5, v2, 0, 0x20, 0, 0x37, 0, 0x26, 0, 0x25, 0, 0x01, 0, 0x5c, 0, 0xdd,
          0, 0x42);

  VSET(16, e16, m2);
  VLOAD_16(v4, 0xf77a, 0x54d7, 0xe527, 0xe28f, 0x53ed, 0x9301, 0xde4f, 0xcb17,
           0xae43, 0x9e4a, 0xa0c2, 0xdf31, 0xb66f, 0x286d, 0x1d15, 0x0480);
  VLOAD_16(v6, 0x5bfa, 0x0571, 0x8a43, 0x6350, 0xb962, 0x71fc, 0x0b54, 0x1e8b,
           0x6c25, 0x9c0d, 0x5950, 0x1887, 0xbc18, 0x628e, 0x6561, 0x407f);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vremu.vv v2, v4, v6, v0.t");
  VCMP_I16(6, v2, 0, 0x0338, 0, 0x1bef, 0, 0x2105, 0, 0x13d5, 0, 0x023d, 0,
           0x0272, 0, 0x286d, 0, 0x0480);

  VSET(16, e32, m2);
  VLOAD_32(v4, 0x647d8841, 0xf9e0aabf, 0xea4aa122, 0xd6178d3e, 0x64a7afe5,
           0xe0350cba, 0xc72768ec, 0x9f977a31, 0x5e1c2ac4, 0xcd44b950,
           0x39dc32f4, 0x1dc82ea3, 0xd1cf125f, 0xc677269c, 0x6405ec5b,
           0x653a05ee);
  VLOAD_32(v6, 0x89828d99, 0x5c7c7db0, 0x2911efb6, 0x1f6982ff, 0x564e4bd4,
           0xc4576bff, 0x8e998104, 0x4a23ba44, 0x994b4630, 0x017ee935,
           0xa38c7dae, 0x893dfb15, 0x4969125f, 0x9a951d27, 0x09b6017f,
           0x5a0a7906);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vremu.vv v2, v4, v6, v0.t");
  VCMP_I32(7, v2, 0, 0x40e7af5f, 0, 0x199e7b44, 0, 0x1bdda0bb, 0, 0x0b5005a9, 0,
           0x0059ebf3, 0, 0x1dc82ea3, 0, 0x2be20975, 0, 0x0b2f8ce8);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v4, 0x09ab27501ccac4a6, 0x97eb5bf189b39a0e, 0x26f588069b0858c4,
           0x9a251c274a394df3, 0x54b3587602f8d9d2, 0xc3cc623deda95ca7,
           0x118c4335397980bf, 0xc3e2d283cb39133d, 0x71837e24114813fc,
           0x85a1f65867438a09, 0x80f01e0588afc9a0, 0x60e89a1e5a43d9f5,
           0x93a87cf6308ad888, 0xca3976f49ac6a681, 0xcfc7c8f225b47766,
           0xeaa4ce2cf507b527);
  VLOAD_64(v6, 0x9fed81c550326301, 0x445bb7ac18d0eaa1, 0x040f8ff58f5adf72,
           0xafc4ff6b8eb4d201, 0xfba36cabfc3fb4a0, 0x9c3ed271bf173d29,
           0xe8b7e325c9ff594b, 0x05169e56693600d7, 0x08e72c4bb62ad267,
           0xbd9677ee996d5fa5, 0x900295e8502a9817, 0x39e0bfa9927679a8,
           0xdd0ca7797d532524, 0x6f8f78c47ddee88a, 0x2f40f7661cca9eee,
           0x8e4a3b2358129e92);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vremu.vv v2, v4, v6, v0.t");
  VCMP_I64(8, v2, 0, 0x0f33ec995811c4cc, 0, 0x9a251c274a394df3, 0,
           0x278d8fcc2e921f7e, 0, 0x028751b02d34f353, 0, 0x85a1f65867438a09, 0,
           0x2707da74c7cd604d, 0, 0x5aa9fe301ce7bdf7, 0, 0x5c5a93099cf51695);
#endif
};

void TEST_CASE3(void) {
  VSET(16, e8, m2);
  VLOAD_8(v4, 0x98, 0x1a, 0xbe, 0x48, 0x7c, 0xd9, 0x5e, 0x58, 0x2e, 0x46, 0x0c,
          0x24, 0xc5, 0x2b, 0x37, 0xbe);
  uint64_t scalar = 5;
  asm volatile("vremu.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VCMP_I8(9, v2, 0x02, 0x01, 0x00, 0x02, 0x04, 0x02, 0x04, 0x03, 0x01, 0x00,
          0x02, 0x01, 0x02, 0x03, 0x00, 0x00);

  VSET(16, e16, m2);
  VLOAD_16(v4, 0xf11f, 0xb8cd, 0xb686, 0xc226, 0xc35a, 0xd724, 0x03f1, 0xcf10,
           0xbae0, 0x9f01, 0x1d0f, 0xf53c, 0x5461, 0x341e, 0x9ae7, 0x032b);
  scalar = 538;
  asm volatile("vremu.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VCMP_I16(10, v2, 0x018b, 0x01f7, 0x01ca, 0x00ce, 0x0202, 0x00c8, 0x01d7,
           0x011c, 0x01f0, 0x0163, 0x01bd, 0x0174, 0x0051, 0x01ae, 0x017d,
           0x0111);

  VSET(16, e32, m2);
  VLOAD_32(v4, 0x9c36da54, 0x1b1dea93, 0x80be8651, 0x03a23fcf, 0x26973d17,
           0x521f01df, 0x09e8f77a, 0x5b231aa2, 0xd4bea1df, 0x529b4f34,
           0x800a5d88, 0xe7b02512, 0xf7954032, 0x48652b8c, 0x8b14b883,
           0x121a9b8b);
  scalar = 649;
  asm volatile("vremu.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VCMP_I32(11, v2, 0x00000039, 0x00000141, 0x0000020b, 0x0000015f, 0x0000008a,
           0x00000199, 0x00000214, 0x0000006c, 0x0000025d, 0x000001a6,
           0x000000d2, 0x00000168, 0x000001e6, 0x00000266, 0x00000188,
           0x00000159);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v4, 0x1882c5f4b911b949, 0x6ca37133428ed155, 0xbacb9408aa8251bf,
           0x62d79deed97681f5, 0x56258335e007492c, 0x2428afa90a14fa61,
           0xd62824119c3084c6, 0xef97986ae9ea2da7, 0xfc28c84e37024f10,
           0x1f475f820dec67e1, 0x9c180cfef468c050, 0x4be017933813e27e,
           0xafd2b5edb83df693, 0xddd4766a628d4c30, 0xa1f4d0f48a6ac917,
           0x827a07db9e6a8897);
  scalar = 9223;
  asm volatile("vremu.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VCMP_I64(12, v2, 0x000000000000167d, 0x00000000000015f2, 0x00000000000019be,
           0x00000000000003fd, 0x00000000000010ce, 0x0000000000001863,
           0x0000000000000750, 0x0000000000000062, 0x0000000000002237,
           0x00000000000002bc, 0x0000000000000061, 0x0000000000001b82,
           0x0000000000001109, 0x0000000000000fb7, 0x00000000000011e8,
           0x0000000000000545);
#endif
};

void TEST_CASE4(void) {
  VSET(16, e8, m2);
  VLOAD_8(v4, 0x98, 0x1a, 0xbe, 0x48, 0x7c, 0xd9, 0x5e, 0x58, 0x2e, 0x46, 0x0c,
          0x24, 0xc5, 0x2b, 0x37, 0xbe);
  uint64_t scalar = 5;
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vremu.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_I8(13, v2, 0, 0x01, 0, 0x02, 0, 0x02, 0, 0x03, 0, 0x00, 0, 0x01, 0, 0x03,
          0, 0x00);

  VSET(16, e16, m2);
  VLOAD_16(v4, 0xf11f, 0xb8cd, 0xb686, 0xc226, 0xc35a, 0xd724, 0x03f1, 0xcf10,
           0xbae0, 0x9f01, 0x1d0f, 0xf53c, 0x5461, 0x341e, 0x9ae7, 0x032b);
  scalar = 538;
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vremu.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_I16(14, v2, 0, 0x01f7, 0, 0x00ce, 0, 0x00c8, 0, 0x011c, 0, 0x0163, 0,
           0x0174, 0, 0x01ae, 0, 0x0111);

  VSET(16, e32, m2);
  VLOAD_32(v4, 0x9c36da54, 0x1b1dea93, 0x80be8651, 0x03a23fcf, 0x26973d17,
           0x521f01df, 0x09e8f77a, 0x5b231aa2, 0xd4bea1df, 0x529b4f34,
           0x800a5d88, 0xe7b02512, 0xf7954032, 0x48652b8c, 0x8b14b883,
           0x121a9b8b);
  scalar = 649;
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vremu.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_I32(15, v2, 0, 0x00000141, 0, 0x0000015f, 0, 0x00000199, 0, 0x0000006c,
           0, 0x000001a6, 0, 0x00000168, 0, 0x00000266, 0, 0x00000159);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v4, 0x1882c5f4b911b949, 0x6ca37133428ed155, 0xbacb9408aa8251bf,
           0x62d79deed97681f5, 0x56258335e007492c, 0x2428afa90a14fa61,
           0xd62824119c3084c6, 0xef97986ae9ea2da7, 0xfc28c84e37024f10,
           0x1f475f820dec67e1, 0x9c180cfef468c050, 0x4be017933813e27e,
           0xafd2b5edb83df693, 0xddd4766a628d4c30, 0xa1f4d0f48a6ac917,
           0x827a07db9e6a8897);
  scalar = 9223;
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vremu.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_I64(16, v2, 0, 0x00000000000015f2, 0, 0x00000000000003fd, 0,
           0x0000000000001863, 0, 0x0000000000000062, 0, 0x00000000000002bc, 0,
           0x0000000000001b82, 0, 0x0000000000000fb7, 0, 0x0000000000000545);
#endif
};

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  // TEST_CASE2();
  TEST_CASE3();
  // TEST_CASE4();

  EXIT_CHECK();
}
