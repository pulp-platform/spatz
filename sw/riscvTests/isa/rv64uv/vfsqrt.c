// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>
//         Matteo Perotti <mperotti@iis.ee.ethz.ch>

#include "float_macros.h"
#include "vector_macros.h"

// Simple random test with similar values
void TEST_CASE1(void) {
  VSET(16, e16, m8);
  //              -4628.000,   5116.000, -9928.000,   9392.000, -140.875,
  //              6112.000,   2598.000,   3210.000,   528.000, -3298.000,
  //              -3674.000,   368.250,   1712.000, -8584.000, -2080.000,
  //              4336.000
  VLOAD_16(v0, 0xec85, 0x6cff, 0xf0d9, 0x7096, 0xd867, 0x6df8, 0x6913, 0x6a45,
           0x6020, 0xea71, 0xeb2d, 0x5dc1, 0x66b0, 0xf031, 0xe810, 0x6c3c);
  asm volatile("vfsqrt.v v8, v0");
  //                nan,   71.500,   nan,   96.938,
  //                nan,   78.188,   50.969,   56.656,   22.984,   nan,
  //                nan,   19.188,   41.375,   nan,   nan,   65.875
  VCMP_U16(1, v8, 0x7e00, 0x5478, 0x7e00, 0x560e, 0x7e00, 0x54e2, 0x525f,
           0x5315, 0x4dbe, 0x7e00, 0x7e00, 0x4ccc, 0x512c, 0x7e00, 0x7e00,
           0x541d);



  VSET(16, e16, m8);
  //              -4628.000,   5116.000, -9928.000,   9392.000, -140.875,
  //              6112.000,   2598.000,   3210.000,   528.000, -3298.000,
  //              -3674.000,   368.250,   1712.000, -8584.000, -2080.000,
  //              4336.000
  VLOAD_16(v0, 0xf0d9, 0xf0d9, 0xf0d9, 0xf0d9, 0xd867, 0x6df8, 0x6913, 0x6a45,
           0x6020, 0xea71, 0xeb2d, 0x5dc1, 0x66b0, 0xf031, 0xe810, 0x6c3c);
  asm volatile("vfsqrt.v v8, v0");
  //                nan,   71.500,   nan,   96.938,
  //                nan,   78.188,   50.969,   56.656,   22.984,   nan,
  //                nan,   19.188,   41.375,   nan,   nan,   65.875
  VCMP_U16(2, v8, 0x7e00, 0x7e00, 0x7e00, 0x7e00, 0x7e00, 0x54e2, 0x525f,
           0x5315, 0x4dbe, 0x7e00, 0x7e00, 0x4ccc, 0x512c, 0x7e00, 0x7e00,
           0x541d);

  VSET(16, e32, m8);
  //                53688.590, -5719.180, -59560.355, -34640.023, -22323.398,
  //                -52381.586,   19136.160,   13055.238, -68576.781,
  //                -35066.488,   62475.219, -25604.578,   54705.039,
  //                -19827.459,   17792.961, -28415.572
  VLOAD_32(v0, 0x4751b897, 0xc5b2b971, 0xc768a85b, 0xc7075006, 0xc6ae66cc,
           0xc74c9d96, 0x46958052, 0x464bfcf4, 0xc785f064, 0xc708fa7d,
           0x47740b38, 0xc6c80928, 0x4755b10a, 0xc69ae6eb, 0x468b01ec,
           0xc6ddff25);
  asm volatile("vfsqrt.v v8, v0");
  //                231.708,   nan,   nan,   nan,   nan,   nan,   138.334,
  //                114.260,   nan,   nan,   249.950,   nan,   233.891,   nan,
  //                133.390,   nan
  VCMP_U32(3, v8, 0x4367b53e, 0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000,
           0x7fc00000, 0x430a5560, 0x42e484e0, 0x7fc00000, 0x7fc00000,
           0x4379f34f, 0x7fc00000, 0x4369e41e, 0x7fc00000, 0x430563e7,
           0x7fc00000);

  VSET(16, e32, m8);
  //                53688.590, -5719.180, -59560.355, -34640.023, -22323.398,
  //                -52381.586,   19136.160,   13055.238, -68576.781,
  //                -35066.488,   62475.219, -25604.578,   54705.039,
  //                -19827.459,   17792.961, -28415.572
  VLOAD_32(v0, 0xc5b2b971, 0xc5b2b971, 0xc5b2b971, 0xc5b2b971, 0xc5b2b971,
           0xc5b2b971, 0xc5b2b971, 0xc5b2b971, 0xc5b2b971, 0xc5b2b971,
           0xc5b2b971, 0xc5b2b971, 0xc5b2b971, 0xc5b2b971, 0xc5b2b971,
           0xc5b2b971);
  asm volatile("vfsqrt.v v8, v0");
  //                231.708,   nan,   nan,   nan,   nan,   nan,   138.334,
  //                114.260,   nan,   nan,   249.950,   nan,   233.891,   nan,
  //                133.390,   nan
  VCMP_U32(4, v8, 0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000,
           0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000,
           0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000,
           0x7fc00000);

#if ELEN == 64

  VSET(16, e64, m8);
  //              -2532126.867, -601715.939, -7176821.248,   9617114.284,
  //              -4651296.040, -9962642.835,   4027953.647,   7849763.850,
  //              -9544132.585, -8682313.823,   7018932.012,   639358.130,
  //              -7598169.215, -9585529.793, -4604984.668,   314584.590
  VLOAD_64(v0, 0xc143518f6efce4ae, 0xc1225ce7e096cbf0, 0xc15b609d4fd8b968,
           0x416257db4912ef24, 0xc151be4802974a67, 0xc16300925abc1630,
           0x414ebb18d2c34030, 0x415df1c8f662a87c, 0xc162343892b8d28c,
           0xc1608f693a52837e, 0x415ac66d00c810d8, 0x412382fc427c96a0,
           0xc15cfc164dc9e320, 0xc162486f39607ee9, 0xc151910e2ac0e818,
           0x411333625c861bc0);
  asm volatile("vfsqrt.v v8, v0");
  //                nan,   nan,   nan,   3101.147,   nan,   nan,   2006.976,
  //                2801.743,   nan,   nan,   2649.327,   799.599,   nan,   nan,
  //                nan,   560.878
  VCMP_U64(5, v8, 0x7ff8000000000000, 0x7ff8000000000000, 0x7ff8000000000000,
           0x40a83a4b64b82189, 0x7ff8000000000000, 0x7ff8000000000000,
           0x409f5be7acad5998, 0x40a5e37c6ac52c2f, 0x7ff8000000000000,
           0x7ff8000000000000, 0x40a4b2a7466e763d, 0x4088fcca333ab72d,
           0x7ff8000000000000, 0x7ff8000000000000, 0x7ff8000000000000,
           0x40818706fb9cc11b);

  VSET(16, e64, m8);
  //              -2532126.867, -601715.939, -7176821.248,   9617114.284,
  //              -4651296.040, -9962642.835,   4027953.647,   7849763.850,
  //              -9544132.585, -8682313.823,   7018932.012,   639358.130,
  //              -7598169.215, -9585529.793, -4604984.668,   314584.590
  VLOAD_64(v0, 0x416257db4912ef24, 0x416257db4912ef24, 0x416257db4912ef24,
           0x416257db4912ef24, 0x416257db4912ef24, 0x416257db4912ef24,
           0x416257db4912ef24, 0x416257db4912ef24, 0x416257db4912ef24,
           0x416257db4912ef24, 0x416257db4912ef24, 0x416257db4912ef24,
           0x416257db4912ef24, 0x416257db4912ef24, 0x416257db4912ef24,
           0x416257db4912ef24);
  asm volatile("vfsqrt.v v8, v0");
  //                nan,   nan,   nan,   3101.147,   nan,   nan,   2006.976,
  //                2801.743,   nan,   nan,   2649.327,   799.599,   nan,   nan,
  //                nan,   560.878
  VCMP_U64(6, v8, 0x40a83a4b64b82189, 0x40a83a4b64b82189, 0x40a83a4b64b82189,
           0x40a83a4b64b82189, 0x40a83a4b64b82189, 0x40a83a4b64b82189,
           0x40a83a4b64b82189, 0x40a83a4b64b82189, 0x40a83a4b64b82189,
           0x40a83a4b64b82189, 0x40a83a4b64b82189, 0x40a83a4b64b82189,
           0x40a83a4b64b82189, 0x40a83a4b64b82189, 0x40a83a4b64b82189,
           0x40a83a4b64b82189);
#endif

};

// Simple random test with similar values (masked)
// The numbers are the same of TEST_CASE1
void TEST_CASE2(void) {
  VSET(16, e16, m8);
  //              -4628.000,   5116.000, -9928.000,   9392.000, -140.875,
  //              6112.000,   2598.000,   3210.000,   528.000, -3298.000,
  //              -3674.000,   368.250,   1712.000, -8584.000, -2080.000,
  //              4336.000
  VLOAD_16(v8, 0xec85, 0x6cff, 0xf0d9, 0x7096, 0xd867, 0x6df8, 0x6913, 0x6a45,
           0x6020, 0xea71, 0xeb2d, 0x5dc1, 0x66b0, 0xf031, 0xe810, 0x6c3c);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v16);
  asm volatile("vfsqrt.v v16, v8, v0.t");
  //                0.000,   71.500,   0.000,   96.938,   0.000,   78.188,
  //                0.000,   56.656,   0.000,   nan,   0.000,   19.188,   0.000,
  //                nan,   0.000,   65.875
  VCMP_U16(7, v16, 0x0, 0x5478, 0x0, 0x560e, 0x0, 0x54e2, 0x0, 0x5315, 0x0,
           0x7e00, 0x0, 0x4ccc, 0x0, 0x7e00, 0x0, 0x541d);

  VSET(16, e32, m8);
  //                53688.590, -5719.180, -59560.355, -34640.023, -22323.398,
  //                -52381.586,   19136.160,   13055.238, -68576.781,
  //                -35066.488,   62475.219, -25604.578,   54705.039,
  //                -19827.459,   17792.961, -28415.572
  VLOAD_32(v8, 0x4751b897, 0xc5b2b971, 0xc768a85b, 0xc7075006, 0xc6ae66cc,
           0xc74c9d96, 0x46958052, 0x464bfcf4, 0xc785f064, 0xc708fa7d,
           0x47740b38, 0xc6c80928, 0x4755b10a, 0xc69ae6eb, 0x468b01ec,
           0xc6ddff25);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v16);
  asm volatile("vfsqrt.v v16, v8, v0.t");
  //                0.000,   nan,   0.000,   nan,   0.000,   nan,   0.000,
  //                114.260,   0.000,   nan,   0.000,   nan,   0.000,   nan,
  //                0.000,   nan
  VCMP_U32(8, v16, 0x0, 0x7fc00000, 0x0, 0x7fc00000, 0x0, 0x7fc00000, 0x0,
           0x42e484e0, 0x0, 0x7fc00000, 0x0, 0x7fc00000, 0x0, 0x7fc00000, 0x0,
           0x7fc00000);

#if ELEN == 64

  VSET(16, e64, m8);
  //              -2532126.867, -601715.939, -7176821.248,   9617114.284,
  //              -4651296.040, -9962642.835,   4027953.647,   7849763.850,
  //              -9544132.585, -8682313.823,   7018932.012,   639358.130,
  //              -7598169.215, -9585529.793, -4604984.668,   314584.590
  VLOAD_64(v8, 0xc143518f6efce4ae, 0xc1225ce7e096cbf0, 0xc15b609d4fd8b968,
           0x416257db4912ef24, 0xc151be4802974a67, 0xc16300925abc1630,
           0x414ebb18d2c34030, 0x415df1c8f662a87c, 0xc162343892b8d28c,
           0xc1608f693a52837e, 0x415ac66d00c810d8, 0x412382fc427c96a0,
           0xc15cfc164dc9e320, 0xc162486f39607ee9, 0xc151910e2ac0e818,
           0x411333625c861bc0);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v16);
  asm volatile("vfsqrt.v v16, v8, v0.t");
  //                0.000,   nan,   0.000,   3101.147,   0.000,   nan,   0.000,
  //                2801.743,   0.000,   nan,   0.000,   799.599,   0.000, nan,
  //                0.000,   560.878
  VCMP_U64(9, v16, 0x0, 0x7ff8000000000000, 0x0, 0x40a83a4b64b82189, 0x0,
           0x7ff8000000000000, 0x0, 0x40a5e37c6ac52c2f, 0x0, 0x7ff8000000000000,
           0x0, 0x4088fcca333ab72d, 0x0, 0x7ff8000000000000, 0x0,
           0x40818706fb9cc11b);

#endif
};

void TEST_CASE3(void) {
  VSET(16, e16, m8);
  VLOAD_16(v16, 0x6c85, 0x6cff, 0x70d9, 0x7096, 0x5867, 0x6df8, 0x6913, 0x6a45,
           0x6020, 0x6a71, 0x6b2d, 0x5dc1, 0x66b0, 0x7031, 0x6810, 0x6c3c);
  asm volatile("vfsqrt.v v8, v16");
  VCMP_U16(10, v8, 0x5440, 0x5478, 0x563a, 0x560e, 0x49ef, 0x54e2, 0x525f,
           0x5315, 0x4dbe, 0x532d, 0x5393, 0x4ccc, 0x512c, 0x55ca, 0x51b3,
           0x541d);

  VSET(16, e32, m8);
  VLOAD_32(v16, 0x4751b897, 0x45b2b971, 0x4768a85b, 0x47075006, 0x46ae66cc,
           0x474c9d96, 0x46958052, 0x464bfcf4, 0x4785f064, 0x4708fa7d,
           0x47740b38, 0x46c80928, 0x4755b10a, 0x469ae6eb, 0x468b01ec,
           0x46ddff25);
  asm volatile("vfsqrt.v v8, v16");
  VCMP_U32(11, v8, 0x4367b53e, 0x42974022, 0x43740cc6, 0x433a1e49, 0x43156900,
           0x4364dec7, 0x430a5560, 0x42e484e0, 0x4382ef93, 0x433b42ae,
           0x4379f34f, 0x432003a9, 0x4369e41e, 0x430ccf5c, 0x430563e7,
           0x432891b6);
#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v16, 0x4143518f6efce4ae, 0x41225ce7e096cbf0, 0x415b609d4fd8b968,
           0x416257db4912ef24, 0x4151be4802974a67, 0x416300925abc1630,
           0x414ebb18d2c34030, 0x415df1c8f662a87c, 0x4162343892b8d28c,
           0x41608f693a52837e, 0x415ac66d00c810d8, 0x412382fc427c96a0,
           0x415cfc164dc9e320, 0x4162486f39607ee9, 0x4151910e2ac0e818,
           0x411333625c861bc0);
  asm volatile("vfsqrt.v v8, v16");
  VCMP_U64(12, v8, 0x4098dd102f978f95, 0x40883da0caf0c268, 0x40a4edeb002fd771,
           0x40a83a4b64b82189, 0x40a0d95f6a1fd5f0, 0x40a8a8bb1be0df66,
           0x409f5be7acad5998, 0x40a5e37c6ac52c2f, 0x40a822b7461c1d8c,
           0x40a705273bbd09ff, 0x40a4b2a7466e763d, 0x4088fcca333ab72d,
           0x40a588f492d509d6, 0x40a83019f24db2b3, 0x40a0c3d87960c07b,
           0x40818706fb9cc11b);
#endif
};

void TEST_CASE4(void) {
  VSET(16, e32, m8);
  VLOAD_32(v16, 0x4751b897, 0x45b2b971, 0x4768a85b, 0x47075006, 0x46ae66cc,
           0x474c9d96, 0x46958052, 0x464bfcf4, 0x4785f064, 0x4708fa7d,
           0x47740b38, 0x46c80928, 0x4755b10a, 0x469ae6eb, 0x468b01ec,
           0x46ddff25);
  VCLEAR(v8);

  VSET(13, e32, m8);
  asm volatile("vfsqrt.v v8, v16");
  VCMP_U32(13, v8, 0x4367b53e, 0x42974022, 0x43740cc6, 0x433a1e49, 0x43156900,
           0x4364dec7, 0x430a5560, 0x42e484e0, 0x4382ef93, 0x433b42ae,
           0x4379f34f, 0x432003a9, 0x4369e41e);

  VSET(16, e32, m8);
  VCMP_U32(14, v8, 0x4367b53e, 0x42974022, 0x43740cc6, 0x433a1e49, 0x43156900,
           0x4364dec7, 0x430a5560, 0x42e484e0, 0x4382ef93, 0x433b42ae,
           0x4379f34f, 0x432003a9, 0x4369e41e, 0x0, 0x0, 0x0);
};

void TEST_CASE5(void) {
  VSET(16, e32, m8);
  VLOAD_32(v16, 0x4751b897, 0x45b2b971, 0x4768a85b, 0x47075006, 0x46ae66cc,
           0x474c9d96, 0x46958052, 0x464bfcf4, 0x4785f064, 0x4708fa7d,
           0x47740b38, 0x46c80928, 0x4755b10a, 0x469ae6eb, 0x468b01ec,
           0x46ddff25);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);

  VSET(13, e32, m8);
  asm volatile("vfsqrt.v v8, v16, v0.t");
  VCMP_U32(15, v8, 0x0, 0x42974022, 0x0, 0x433a1e49, 0x0, 0x4364dec7, 0x0,
           0x42e484e0, 0x0, 0x433b42ae, 0x0, 0x432003a9, 0x0);

  VSET(16, e32, m8);
  VCMP_U32(16, v8, 0x0, 0x42974022, 0x0, 0x433a1e49, 0x0, 0x4364dec7, 0x0,
           0x42e484e0, 0x0, 0x433b42ae, 0x0, 0x432003a9, 0x0, 0x0, 0x0, 0x0);
};

int main(void) {
  INIT_CHECK();
  enable_vec();
  enable_fp();
  CHANGE_RM(RM_RTZ);

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();
  TEST_CASE5();

  EXIT_CHECK();
}
