/* 
   Copyright (c) 2012,2013   Axel Kohlmeyer <akohlmey@gmail.com> 
   All rights reserved.
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
   * Neither the name of the <organization> nor the
     names of its contributors may be used to endorse or promote products
     derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
 
#include "defines.h" 
#include "plp_exp.h"


#define get_bits(x) (*(unsigned int*)&x)
 
typedef union
{
    float    f;
    uint32_t u;
    int32_t  i;
}  ufi_t;

#define FM_FLOAT_LOG2OFE  1.4426950408889634074f 
#define FM_FLOAT_LOG2OF10 3.32192809488736234789f 
#if BITS == 32
#define FM_FLOAT_BIAS 127
#define FM_FLOAT_INIT_EXP(var,num) var.i = (((int) num) + FM_FLOAT_BIAS) << 23
#else
#define FM_FLOAT_BIAS 15
#define FM_FLOAT_INIT_EXP(var,num) var.i = (((int) num) + FM_FLOAT_BIAS) << 10
#endif

float fm_exp2f_p[] = {
    1.535336188319500e-4f,
    1.339887440266574e-3f,
    9.618437357674640e-3f,
    5.550332471162809e-2f,
    2.402264791363012e-1f,
    6.931472028550421e-1f,
    1.000000000000000f
};

float fm_exp2f(float x)
{

    float ipart, fpart;
    ufi_t epart;

    ipart = (int)(x + 0.5f);
    fpart = x - ipart;
    FM_FLOAT_INIT_EXP(epart,ipart);

    x =           fm_exp2f_p[0];
    x = x*fpart + fm_exp2f_p[1];
    x = x*fpart + fm_exp2f_p[2];
    x = x*fpart + fm_exp2f_p[3];
    x = x*fpart + fm_exp2f_p[4];
    x = x*fpart + fm_exp2f_p[5];
    x = x*fpart + fm_exp2f_p[6];

    return (float)epart.f*x;
}

float fm_expf(float x)
{
    return fm_exp2f(FM_FLOAT_LOG2OFE*x);
}
 
#ifdef LIBM_ALIAS   
float expf(float x) __attribute__ ((alias("fm_expf"))); 
#endif


/* ants */

unsigned int mantissa_mask = 0x007fffff;
unsigned int exponent_mask = 0x7ff80000;
unsigned int m_infinite = 0xff800000;
unsigned int p_infinite = 0x7f800000;
unsigned int not_a_number = 0x7f800001;
unsigned int mantissa_mask16 = 0x007f;
unsigned int mantissa_mask16alt = 0x03ff;
float zero = 0.0f;
float one = 1.0e+00f;
float half = 5.0e-01f;
float half_pi = 1.570796326794897e+00f;
// float16 zero16 = 0.0f;
// float16 one_16 = 1.0f;
// float16 half_16 = 5.0e-01f;
// float16 half_pi_16 = 1.570796326794897e+00f;
// float16alt zero16alt = 0.0f;
// float16alt one_16alt = 1.0f;
// float16alt twom3_16alt = 0.125f;
// float16alt zero3_16alt = 0.3f;
// float16alt zero7_16alt = 0.78125f;
// float16alt half_pi_16alt = 1.570796326794897e+00f;
// float16alt pio4_16alt = 0.785398163397448e+00f;

// Exponential

float P1 =  1.66666666e-01f;
float P2 = -2.77777777e-03f;
float P3 =  6.61375632e-05f;
float P4 = -1.65339022e-06f;
float P5 =  4.13813679e-08f;
// float16 P1_16 =  1.66666666e-01f;
// float16 P2_16 = -2.77777777e-03f;
// float16 P3_16 =  6.61375632e-05f;
// float16 P4_16 = -1.65339022e-06f;
// float16 P5_16 =  4.13813679e-08f;
// float16alt ln2half = 0.346573590279973e+00f;
// float16alt ln2one_five = 1.039720770839918e+00f;
// float16 P1_16alt =  1.66666666e-01f;
// float16 P2_16alt = -2.77777777e-03f;
// float16 P3_16alt =  6.61375632e-05f;
// float16 P4_16alt = -1.65339022e-06f;
// float16 P5_16alt =  4.13813679e-08f;


// float16 exp16(float16 x)
// {
//   int k, xsb;
//   unsigned short hx;

//   // Overflow threshold
//   // 0x498c
//   float16 o_threshold = 11.089866f;
//   // Underflow threshold
//   // 0xc98c
//   float16 u_threshold = -11.089866f;
//   float16 huge = 1.0e+6f;
//   float16 twom10 = 9.765625e-04f;
//   float16 ln2hi[2] = {6.93147180e-01f, -6.93147180e-01f};
//   float16 ln2lo[2] = {1.90821492e-10f, -1.90821492e-10f};
//   float16 hi, lo;
//   float16 half[2] = {0.5f, -0.5f};
//   float16 t;
//   float16 invln2 = 1.44269504f;
//   float16 one = 1.0f;
//   float16 y, c;

//   hx = get_bits(x);
//   // Sign bit of x
//   xsb = (hx >> 15) & 1;
//   // Value of |x|
//   hx &= 0x7FFF;

//   // Filter out non-finite argument  
//   // If |x| >= 11.089...
//   if(hx >= o_threshold){
//     // NaN
//     if(hx >= 0x7c00){
//         if(((hx & 0x7c00) | (hx & 0x03ff)) != 0)
//           // NaN
//           return x + x;
//         else
//           // exp(+ inf) = + inf; exp(- inf) = 0
//           return (xsb == 0) ? x : zero16;
//     }
//     if(x > o_threshold)
//       // Overflow
//       return huge * huge;
//     if(x < u_threshold)
//       // Underflow
//       return twom10 * twom10;
//   }
 
//   // Argument reduction  
//   // If |x| > 0.5 ln2
//   if(hx > 0x3800){
//     // And |x| < 1.5 ln2
//     if(hx < 0x3e00){
//       hi = x - ln2hi[xsb];
//       lo = ln2lo[xsb];
//       k = 1 - xsb - xsb;
//     }
//     else{
//       k = (int)(invln2 * x + half[xsb]);
//       t = k;
//       // t * ln2hi is exact here
//       hi = x - t * ln2hi[0];
//       lo = t * ln2lo[0];
//     }
//     x = hi - lo;
//   }
//   else
//     // When |x| < 2^-28
//     if(hx < 0x0150){
//       // Trigger inexact
//       if((huge + x) > one_16)
//         return one_16 + x;
//     }
//     else
//       k = 0;

//   // x is now in primary range  
//   // Load ants to avoid load stalls
//   register float16 x_reg asm("t1") = x;
//   register float16 P1_reg asm("a1") = P1_16;
//   register float16 P2_reg asm("a2") = P2_16;
//   register float16 P3_reg asm("a3") = P3_16;
//   register float16 P4_reg asm("a4") = P4_16;
//   register float16 P5_reg asm("a5") = P5_16;
//   asm volatile ("":::"memory");
//   t = x_reg * x_reg;
//   c = x_reg - t * (P1_reg + t * (P2_reg + t * (P3_reg + t * (P4_reg + t * P5_reg))));
//   if(k == 0)
//     return one_16 - ((x_reg * c) / (c - (2 * one_16)) - x_reg);
//   else
//     y = one_16 - ((lo - (x_reg * c) / ((2 * one_16) - c)) - hi);
//   if(k >= -12){
//     // Add k to y's exponent
//     get_bits(y) += (k << 10);
//     return y;
//   }
//   else{
//     // Add k to y's exponent
//     get_bits(y) += ((k + 10) << 10);
//     return y * twom10;
//   }
   
// }

float exp32(float x)
{
  int k, xsb;
  unsigned hx;
  // Overflow threshold
  // 0x42b17218
  float o_threshold = 88.72283905f;
  // Underflow threshold
  // 0xc2b17218
  float u_threshold = -88.72283905f;
  float huge = 1.0e+30f;
  float twom100 = 7.88860905e-31f;
  float ln2hi[2] = {6.93147180e-01f, -6.93147180e-01f};
  float ln2lo[2] = {1.90821492e-10f, -1.90821492e-10f};
  float hi, lo;
  float half[2] = {0.5f, -0.5f};
  float t;
  float invln2 = 1.44269504f;
  float one = 1.0f;
  float y, c;

  hx = get_bits(x);
  // Sign bit of x
  xsb = (hx >> 31) & 1;
  // Value of |x|
  hx &= 0x7FFFFFFF;

  /* Filter out non-finite argument */
  // If |x| >= 88.722...
  if(hx >= 0x42b17218){
    // NaN
    if(hx >= 0x7fc00000){
        if(((hx & 0x7fff0000) | (hx & 0x0000ffff)) != 0)
          // NaN
          return x + x;
        else
          // exp(+ inf) = + inf; exp(- inf) = 0
          return (xsb == 0) ? x : 0.0f;
    }
    if(x > o_threshold)
      // Overflow
      return huge * huge;
    if(x < u_threshold)
      // Underflow
      return twom100 * twom100;
  }

  /* Argument reduction */
  // If |x| > 0.5 ln2
  if(hx > 0x3eb17218){
    // And |x| < 1.5 ln2
    if(hx < 0x3f851592){
      hi = x - ln2hi[xsb];
      lo = ln2lo[xsb];
      k = 1 - xsb - xsb;
    }
    else{
      k = (int)(invln2 * x + half[xsb]);
      t = k;
      // t * ln2hi is exact here
      hi = x - t * ln2hi[0];
      lo = t * ln2lo[0];
    }
    x = hi - lo;
  }
  else
    // When |x| < 2^-28
    if(hx < 0x31800000){
      // Trigger inexact
      if((huge + x) > one)
        return one + x;
    }
    else
      k = 0;

  /* x is now in primary range */
  // Load ants to avoid load stalls
  register float P1_reg asm("a1") = P1;
  register float P2_reg asm("a2") = P2;
  register float P3_reg asm("a3") = P3;
  register float P4_reg asm("a4") = P4;
  register float P5_reg asm("a5") = P5;
  asm volatile ("":::"memory");
  t = x * x;
  c = x - t * (P1_reg + t * (P2_reg + t * (P3_reg + t * (P4_reg + t * P5_reg))));
  if(k == 0)
    return one - ((x * c) / (c - 2.0f) - x);
  else
    y = one - ((lo - (x * c) / (2.0f - c)) - hi);
  if(k >= -125){
    // Add k to y's exponent
     
    get_bits(y) += (k << 23);
    return y;
  }
  else{
    // Add k to y's exponent
    
    get_bits(y) += ((k + 100) << 23);
    return y * twom100;
  }
}