// pf_x86.c  ? Particle Filter + integer-only decisions (Q8.8)
// Heavy Task A: long-vector PF (IMU + wheel predict, GNSS reweight) + realistic sensor sim
// Light Task B: integer-only rules. Prints metrics so you can see why it fires.

// #define _GNU_SOURCE
// #include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include "data/data.h"


#ifndef NPART
#define NPART 4096
#define invNPART 0.000244141f
#endif

float *rand1;
float *rand2;
float *rand3;
float *rand4;
float *rand5;
float *rand6;
float *sinLUT;
float *cosLUT;
float *divLUT;
float *sqrtLUT;

//1024
#define SHIFT 0x0000003FF
#define PI 3.14159265358979323846f


enum
{
    QUEUE_SIZE = 1024
};

typedef struct
{
    int16_t neff_q8, serr_q8, dop_q8;
} feat_q8_t;

typedef struct
{
    feat_q8_t buf[QUEUE_SIZE];
    _Atomic unsigned h, t;
} spsc_t;

static inline int spsc_push(spsc_t *q, feat_q8_t v)
{
    unsigned h = atomic_load_explicit(&q->h, memory_order_relaxed);
    unsigned n = (h + 1) & (QUEUE_SIZE - 1);
    unsigned t = atomic_load_explicit(&q->t, memory_order_acquire);
    if (n == t)
        return 0;
    q->buf[h] = v;
    atomic_store_explicit(&q->h, n, memory_order_release);
    return 1;
}
static inline int spsc_pop(spsc_t *q, feat_q8_t *o)
{
    unsigned t = atomic_load_explicit(&q->t, memory_order_relaxed);
    unsigned h = atomic_load_explicit(&q->h, memory_order_acquire);
    if (t == h)
        return 0;
    *o = q->buf[t];
    atomic_store_explicit(&q->t, (t + 1) & (QUEUE_SIZE - 1), memory_order_release);
    return 1;
}

static inline int16_t clamp16(int x) { return x > 32767 ? 32767 : (x < -32768 ? -32768 : (int16_t)x); }
static inline int16_t q8(float x)
{
    int v = (int)lrintf(x * 256.f);
    return clamp16(v);
}

// ---------- Particle set (SoA) ----------
typedef struct
{
    float *x, *y, *th, *v, *w;
    int n;
} pf_t;

// // RNG
// static inline unsigned lcg(uint64_t *s)
// {
//     *s = (*s) * 6364136223846793005ULL + 1ULL;
//     return (unsigned)(*s >> 32);
// }
// static inline float urand(uint64_t *s) { return (lcg(s) / 4294967296.0f); }
// // N(0,1)
// static inline float randn(uint64_t *s)
// {
//     float u1 = urand(s);
//     if (u1 < 1e-7f)
//         u1 = 1e-7f;
//     float u2 = urand(s);
//     float r = sqrtf(-2.0f * logf(u1));
//     float th = 6.28318530718f * u2;
//     return r * cosf(th);
// }

float min = 1000;
float max = -1000;
// ---------- Heavy kernels ----------
// static void pf_predict_noisy(pf_t *pf, float dt, float gz, float v_meas,
//                              float sig_gz, float sig_v, uint64_t *seed, unsigned int vl, int vlmax)
// {
static void pf_predict_noisy(pf_t *pf, float dt, float gz, float v_meas,
                             float sig_gz, float sig_v, int seed, unsigned int vl, int vlmax)
{
    float *restrict x = pf->x, *restrict y = pf->y, *restrict th = pf->th, *restrict v = pf->v;
    const int n = pf->n;

    // for (int i = 0; i < n; i++)
    // {
    //     float gz_i = gz + sig_gz * randn(seed);
    //     float vm_i = v_meas + sig_v * randn(seed);
    //     th[i] += gz_i * dt;
    //     float c = cosf(th[i]), s = sinf(th[i]);//max = 0.270404	 min = -0.006079
    //     v[i] = 0.95f * v[i] + 0.05f * vm_i;
    //     x[i] += v[i] * c * dt;
    //     y[i] += v[i] * s * dt;
    // }
    int itr = 0;
    int num_elements = n;
    int VLMAX = vlmax;//m4
    
    //rand1 -> v0  => gz_i
    //rand2 -> v4  => vm_i
    //th    -> V8
    //C     -> V12
    //S     -> V16
    //V     -> V20
    //x     -> V24
    //t     -> V28
    asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){
        int index = (seed + itr*VLMAX) & (SHIFT);
        asm volatile("vle32.v      v0 , (%0)"     ::"r"(rand3 + index));  
        asm volatile("vle32.v      v4 , (%0)"     ::"r"(rand4 + index));
        asm volatile("vfmul.vf     v0 , v0, %0"   ::"f"(sig_gz));
        asm volatile("vfadd.vf     v0 , v0, %0"   ::"f"(gz));               //gz_i
        asm volatile("vfmul.vf     v4 , v4, %0"   ::"f"(sig_v));
        asm volatile("vfadd.vf     v4 , v4, %0"   ::"f"(v_meas));           //vm_i
        asm volatile("vle32.v      v8 , (%0)"     ::"r"(th + itr*VLMAX));
        asm volatile("vle32.v      v20, (%0)"     ::"r"(v + itr*VLMAX));
        asm volatile("vfmacc.vf    v8 , %0, v0"   ::"f"(dt));               //th[i] += gz_i * dt
        asm volatile("vse32.v      v8, (%0)"      ::"r"(th+itr*VLMAX));
        asm volatile("vfmul.vf     v8 , v8, %0"   ::"f"(LUTSizef));          //LUT index
        asm volatile("vfcvt.xu.f.v v8 , v8");                               //Conv to uint
        asm volatile("vle32.v      v24, (%0)"     ::"r"(x + itr*VLMAX));
        asm volatile("vle32.v      v28, (%0)"     ::"r"(y + itr*VLMAX));        
        asm volatile("vmul.vx      v8 , v8, %[A]" ::[A] "r"(4));            //32/8 = 4 (index in byte)
        asm volatile("vfmul.vf     v4 , v0, %0"   ::"f"(0.05f));            //fill the gap with vm_i*0.05f
        asm volatile("vluxei32.v   v12, (%0), v8" ::"r"(sinLUT));           //s
        asm volatile("vluxei32.v   v16, (%0), v8" ::"r"(cosLUT));           //c
        asm volatile("vfmadd.vf    v20, %[A], v4" ::[A] "f"(0.95f));        //v[i] = 0.95f * v[i] + ...
        asm volatile("vse32.v      v20, (%0)"     ::"r"(v+itr*VLMAX));
        asm volatile("vfmul.vf     v12, v12 , %0" ::"f"(dt));               //s * dt
        asm volatile("vfmul.vf     v16, v16 , %0" ::"f"(dt));               //c * dt
        asm volatile("vfmacc.vv    v24, v12, v20");                         //x[i] += v[i] * c * dt
        asm volatile("vfmacc.vv    v28, v16, v20");                         //y[i] += v[i] * s * dt
        asm volatile("vse32.v      v24, (%0)"     ::"r"(x+itr*VLMAX));
        asm volatile("vse32.v      v28, (%0)"     ::"r"(y+itr*VLMAX));
        itr++;
    }
}

// Robust reweight: w /= (1 + r2/s2)
// w = w / (1 + r2/s2) => w / (r2/s2) => w*s2/r2
static void pf_update_gnss(pf_t *pf, float zx, float zy, float s2, unsigned int vl, int vlmax)
{
    float *restrict x = pf->x, *restrict y = pf->y, *restrict w = pf->w;
    
    // const float invs2 = 1.0f / s2;//max = 1.166400	 min = 0.360000 CAN GO TOP with approximation
    float invs2;
    const float invS2MAX = 0.857338820f;
    const float denomMAX = 0.009783675f;
    invs2 = divLUT[(int)(s2*invS2MAX*1024)]*invS2MAX;

    const int n = pf->n;
    float r2;
    // for (int i = 0; i < n; i++)
    // {
    //     float dx = x[i] - zx, dy = y[i] - zy, r2 = dx * dx + dy * dy;
    //     w[i] = w[i] / (1.0f + r2 * invs2);//     max = 102.211082	 min = 1.000001
    //                                       //inv2 max = 2.777778	     min = 0.857339
    //                                       //r2   max = 38.239498	 min = 0.000001
    //     // if(r2 < 0.0001) printf("r2 = %f\n",r2);
    //     // if(r2 > max) max = r2;
    //     // if(r2 < min) min = r2;         
    // }

    int itr = 0;
    int num_elements = n;
    int VLMAX = vlmax;//m8
    //x     -> v0 => dx
    //y     -> v8 => dy,r2
    //w     -> V16
    asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){
        asm volatile("vle32.v      v0 , (%0)"     ::"r"(x + itr*VLMAX));  
        asm volatile("vle32.v      v8 , (%0)"     ::"r"(y + itr*VLMAX));
        asm volatile("vle32.v      v16, (%0)"     ::"r"(w + itr*VLMAX));
        asm volatile("vfsub.vf     v0 , v0, %0"   ::"f"(zx));           //dx
        asm volatile("vfsub.vf     v8 , v8, %0"   ::"f"(zy));           //dy
        asm volatile("vfmul.vv     v0 , v0, v0");                       //dx * dx
        asm volatile("vfmul.vv     v8 , v8, v8");                       //dy * dy
        asm volatile("vfadd.vv     v8 , v0, v8");                       //r2
        asm volatile("vfmul.vf     v8 , v8, %0"   ::"f"(invs2));        //r2 * invs2
        asm volatile("vfadd.vf     v8 , v8, %0"   ::"f"(1.0f));         //1 + r2 * invs2
        asm volatile("vfmul.vf     v8 , v8, %0"   ::"f"(denomMAX));     //scaling
        asm volatile("vfmul.vf     v8 , v8, %0"   ::"f"(LUTSizef));      //LUT index
        asm volatile("vfcvt.xu.f.v v8 , v8");                           //Conv to uint
        asm volatile("vmul.vx      v8 , v8, %[A]" ::[A] "r"(4));        //32/8 = 4 (index in byte)
        asm volatile("vluxei32.v   v12, (%0), v8" ::"r"(divLUT));       //s
        asm volatile("vfmul.vf     v8 , v8, %0"   ::"f"(denomMAX));     //scaling back
        asm volatile("vfmul.vv     v16, v16, v8");                      //w[i] / (1.0f + r2 * invs2)
        asm volatile("vse32.v      v16, (%0)"     ::"r"(w+itr*VLMAX));
        itr++;
    }
}

static void pf_normalize_metrics(pf_t *pf, float *neff_ratio, unsigned int vl)
{
    const int n = pf->n;
    float sw = 0.f, sww = 0.f;
    float temp;
    float *restrict w = pf->w;
    // for (int i = 0; i < n; i++)
    //     sw += pf->w[i];
    // if (sw <= 1e-12f)
    // {
    //     float u = 1.0f / n;
    //     for (int i = 0; i < n; i++)
    //         pf->w[i] = u;
    //     *neff_ratio = 1.0f;
    //     return;
    // }

    int itr = 0;
    int num_elements = n;
    int VLMAX = 16;//m1 => 16 element
    asm volatile("vsetvli  %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(VLMAX));
    asm volatile("vle32.v      v0 , (%0)"     ::"r"(w + itr*VLMAX));  
    itr++;
    while(itr*VLMAX + VLMAX <= num_elements){
        asm volatile("vle32.v      v1 , (%0)"     ::"r"(w + itr*VLMAX));  
        asm volatile("vfadd.vv     v0 , v1, v0");
        itr++;
    }
    asm volatile("vfredsum.vs  v1, v0, v31");
    asm volatile("vfmv.f.s %0, v1" : "=f"(sw));

    // float inv = 1.0f / sw;//max = 1.000006	 min = 0.024417
    // for (int i = 0; i < n; i++)
    // {
    //     float wi = pf->w[i] * inv;
    //     pf->w[i] = wi;
    //     sww += wi * wi;
    // }
    // *neff_ratio = 1.0f / (sww * n);//SWW max = 0.575709	 min = 0.000122

    float inv;
    const float invMAX = 0.999994f;
    inv = divLUT[(int)(sw*invMAX*1024)]*invMAX;

    int itr = 0;
    int num_elements = n;
    int VLMAX = 16;//m1 => 16 element
    asm volatile("vsetvli  %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(VLMAX));
    asm volatile("vfsub.vv     v0 , v0, v0");
    while(itr*VLMAX + VLMAX <= num_elements){
        asm volatile("vle32.v      v1 , (%0)"     ::"r"(w + itr*VLMAX));  
        asm volatile("vfmul.vf     v1 , v1, %0"   ::"f"(inv));
        asm volatile("vse32.v      v1 , (%0)"     ::"r"(w+itr*VLMAX));
        asm volatile("vfmul.vv     v2 , v1, v1");
        asm volatile("vfadd.vv     v0 , v0, v1");
        itr++;
    }
    asm volatile("vfredsum.vs  v1, v0, v31");
    asm volatile("vfmv.f.s %0, v1" : "=f"(temp));
    const float invN = invNPART;
    *neff_ratio = divLUT[(int)(temp*1024)]*invN;
}

// ---------- Tasks ----------
typedef struct
{
    spsc_t *q;
} Aargs;
typedef struct
{
    spsc_t *q;
} Bargs;

static void *taskA(void *vp, pf_t pf)
{
    
    
    unsigned int vl;
    int vlmax = 128;
    
    
    Aargs *a = (Aargs *)vp;
    spsc_t *Q = a->q;

    // Ground-truth state (independent of sensors)
    float tx = 0.f, ty = 0.f, tth = 0.f; // truth pose
    const float dt = 0.01f;              // 100 Hz
    const int steps = 2500;              // 25 s
    float t = 0.f;

    // For GNSS speed from successive fixes
    int have_gnss_prev = 0;
    float zx_prev = 0.f, zy_prev = 0.f;
    float v_gps_inst = 0.f;

    // Noise seeds
    // uint64_t seed = 0xBADA55CAFEBEEFULL;
    // srand(1234);

    for (int k = 0; k < steps; k++, t += dt)
    {

        int k_index = (k > 2048)?(k - 2048):k;
        // // Truth dynamics
        // float gz_true = 0.02f * sinf(0.2f * t);// MAX 5
        // float v_true = 5.0f + 0.2f * sinf(0.05f * t);
        // tth += gz_true * dt;
        // tx += v_true * cosf(tth) * dt;
        // ty += v_true * sinf(tth) * dt;

        // // Sensor qualities / events
        // float dop = 1.0f + 0.1f * sinf(0.01f * t);

        // Truth dynamics
        float arg = t*0.2f;
        if(arg > PI) arg -= PI;
        float gz_true = sinLUT[(int)(arg*1024)]*0.02f;
        float v_true = 5.0f + 0.2f * sinLUT[(int)(0.05f*1024)];
        tth += gz_true * dt;
        tx += v_true * cosLUT[(int)(tth*1024)] * dt;
        ty += v_true * sinLUT[(int)(tth*1024)] * dt;

        // Sensor qualities / events
        float dop = 1.0f + 0.1f * sinLUT[(int)(t*0.01f*1024)];

        if (t > 4.0f && t < 7.0f)
            dop = 1.8f; // multipath
        bool gnss_available = true;
        if (t > 12.0f && t < 14.0f)
        {
            gnss_available = false;
            dop = 2.0f;
        }                                                   // blackout
        float slip = (t > 8.0f && t < 9.5f) ? 1.35f : 1.0f; // wheel slip window
        float gz_bias = (t > 16.0f && t < 19.0f) ? 0.02f : 0.0f;

        // Measured sensors (wheel & IMU)
        // float gz_meas = gz_true + gz_bias + 0.01f * randn(&seed);
        // float v_meas = (v_true * slip) + 0.10f * randn(&seed); // slip affects ONLY wheel

        float gz_meas = gz_true + gz_bias + 0.01f * rand1[k_index];
        float v_meas = (v_true * slip) + 0.10f * rand2[k_index]; // slip affects ONLY wheel
        
        // PF predict with noise (ensures spread)
        pf_predict_noisy(&pf, dt, gz_meas, v_meas, 0.02f, 0.25f, k_index, vl, vlmax/2);

        // GNSS update when available (positions around TRUTH, not zero!)
        if (gnss_available && (k % 10) == 0)
        {                           // 10 Hz fixes
            float sig = 0.6f * dop; // sigma proportional to DOP
            // float zx = tx + sig * randn(&seed);
            // float zy = ty + sig * randn(&seed);
            float zx = tx + sig * rand5[k%10];
            float zy = ty + sig * rand6[k%10];
            pf_update_gnss(&pf, zx, zy, sig * sig, vl, vlmax/2);

            // GNSS instantaneous speed from successive fixes
            if (have_gnss_prev)
            {
                float dx = zx - zx_prev, dy = zy - zy_prev;
                //max = 141.749176	 min = 0.004555
                // v_gps_inst = 10.0f * sqrtf(dx * dx + dy * dy); // since fixes are 10x slower
                v_gps_inst = 10.0f * sqrtLUT[(int)(dx * dx + dy * dy)*1024];
            }
            zx_prev = zx;
            zy_prev = zy;
            have_gnss_prev = 1;
        }

        float neff = 1.f;
        pf_normalize_metrics(&pf, &neff, vl);

        // Slip feature: | GNSS speed (from fixes) ? wheel speed |
        float speed_err = have_gnss_prev ? fabsf(v_gps_inst - v_meas) : 0.f;

        // Push integers (Q8.8)
        feat_q8_t f = {q8(neff), q8(speed_err), q8(dop)};
        (void)spsc_push(Q, f);

        // struct timespec ts = {0, 10000000};
        // nanosleep(&ts, NULL); // 10 ms
    }
    return NULL;
}

typedef enum
{
    OK = 0,
    GNSS_BAD = 1,
    WHEEL_SLIP = 2
} decision_t;

static void *taskB(void *vp)
{
    Bargs *b = (Bargs *)vp;
    spsc_t *Q = b->q;
    feat_q8_t f;

    // Print a header and then one line per sample (debug-friendly)
    printf("neff,dop,speed_err,decision\n");

    for (;;)
    {
        if (!spsc_pop(Q, &f))
        {
            struct timespec ts = {0, 100000};
            nanosleep(&ts, NULL);
            continue;
        }

        // integer-only decisions on Q8.8
        decision_t d = OK;
        // GNSS BAD if poor geometry OR (optionally) low Neff
        if (f.dop_q8 > 333 /* >1.30 */ || f.neff_q8 < 205 /* <0.80 */)
            d = GNSS_BAD;
        // WHEEL SLIP if |v_gps - v_wheel| > ~0.35 m/s
        if (d == OK && f.serr_q8 > 90 /* >0.352 m/s */)
            d = WHEEL_SLIP;

        // Debug print (convert back to float for visibility)
        float neff = f.neff_q8 / 256.f, dop = f.dop_q8 / 256.f, serr = f.serr_q8 / 256.f;
        const char *label = (d == OK) ? "OK" : (d == GNSS_BAD) ? "GNSS_BAD"
                                                               : "WHEEL_SLIP";
        printf("%.3f,%.3f,%.3f,%s\n", neff, dop, serr, label);
    }
    return NULL;
}

int main(void)
{
    spsc_t Q = {.h = 0, .t = 0};
    // pthread_t ta, tb;
    Aargs aa = {.q = &Q};
    Bargs bb = {.q = &Q};

    pf_t pf;
    pf.n = NPART;

    pf.x =(float *) snrt_l1alloc (NPART * sizeof(float));
    pf.y =(float *) snrt_l1alloc (NPART * sizeof(float));
    pf.th=(float *) snrt_l1alloc (NPART * sizeof(float));
    pf.v =(float *) snrt_l1alloc (NPART * sizeof(float));
    pf.w =(float *) snrt_l1alloc (NPART * sizeof(float));

    rand1 =(float *) snrt_l1alloc (LUTSize * sizeof(float));
    snrt_dma_start_1d(rand1, rand1_dram, LUTSize * sizeof(float));

    rand2 =(float *) snrt_l1alloc (LUTSize * sizeof(float));
    snrt_dma_start_1d(rand2, rand2_dram, LUTSize * sizeof(float));

    rand3 =(float *) snrt_l1alloc (LUTSize * sizeof(float));
    snrt_dma_start_1d(rand3, rand3_dram, LUTSize * sizeof(float));

    rand4 =(float *) snrt_l1alloc (LUTSize * sizeof(float));
    snrt_dma_start_1d(rand4, rand4_dram, LUTSize * sizeof(float));

    rand5 =(float *) snrt_l1alloc (LUTSize/2 * sizeof(float));
    snrt_dma_start_1d(rand5, rand5_dram, LUTSize/2* sizeof(float));

    rand6 =(float *) snrt_l1alloc (LUTSize/2 * sizeof(float));
    snrt_dma_start_1d(rand6, rand6_dram, LUTSize/2* sizeof(float));

    divLUT =(float *) snrt_l1alloc (LUTSize * sizeof(float));
    snrt_dma_start_1d(rand6, div_dram, LUTSize* sizeof(float));

    sinLUT =(float *) snrt_l1alloc (LUTSize * sizeof(float));
    snrt_dma_start_1d(rand6, sin_dram, LUTSize* sizeof(float));

    cosLUT =(float *) snrt_l1alloc (LUTSize * sizeof(float));
    snrt_dma_start_1d(rand6, cos_dram, LUTSize* sizeof(float));

    sqrtLUT =(float *) snrt_l1alloc (LUTSize * sizeof(float));
    snrt_dma_start_1d(rand6, sqrt_dram, LUTSize* sizeof(float));


    for (int i = 0; i < NPART; i++)
    {
        pf.x[i] = 0;
        pf.y[i] = 0;
        pf.th[i] = 0;
        pf.v[i] = 5.0f;
        pf.w[i] = invNPART;
    }


    taskA(&aa, pf);
    printf("max = %f\t min = %f\n",max,min);
    // pthread_create(&ta, NULL, taskA, &aa);

    // pthread_create(&tb, NULL, taskB, &bb);
    // sleep(20); // long enough to hit multipath, slip, blackout, bias
    return 0;
}
