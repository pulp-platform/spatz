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

// Exponential

float P1 =  1.66666666e-01f;
float P2 = -2.77777777e-03f;
float P3 =  6.61375632e-05f;
float P4 = -1.65339022e-06f;
float P5 =  4.13813679e-08f;

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
  // Load DATA_LOCATIONants to avoid load stalls
  register float P1_reg asm("a1") = P1;
  register float P2_reg asm("a2") = P2;
  register float P3_reg asm("a3") = P3;
  register float P4_reg asm("a4") = P4;
  register float P5_reg asm("a5") = P5;
  asm volatile ("":::"memory");
  // printf("x = %f\t,k = %d\n",x,k);
  t = x * x;
  c = x - t * (P1_reg + t * (P2_reg + t * (P3_reg + t * (P4_reg + t * P5_reg))));
  // printf("t = %f\t,c = %f\n",t,c);
  if(k == 0){
    // printf("div = %f\n",(x * c) / (c - 2.0f));
    return one - ((x * c) / (c - 2.0f) - x);}
    // return (2*c+c*x-2-3*x)/(c-2);}
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