#define __INT8__     0
#define __INT16__    1
#define __INT32__    2
#define __FP8__      4
#define __FP16__     5
#define __FP32__     6
#define __FP8ALT__  12
#define __FP16ALT__ 13
#define __FP32ALT__ 14
#define CMUL_1 0
#define RMUL_1 0
#define CMUL_2 1
#define RMUL_2 1
#define CMUL_4 3
#define RMUL_4 3

#define MATMUL_NAME(DTA,DTB,DTC) \
    matmul_##DTA##_##DTB##_##DTC

#define matmul(DTA,DTB,DTC,A,B,C,K,N,M,shift) \
    MATMUL_NAME(DTA,DTB,DTC)(A,B,C,K,N,M,shift)


void __attribute__ ((noinline)) matmul_INT8_INT8_INT32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int shift);
void __attribute__ ((noinline)) matmul_INT16_INT16_INT32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int shift);
void __attribute__ ((noinline)) matmul_INT32_INT32_INT32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int shift);
void __attribute__ ((noinline)) matmul_FP8_FP8_FP32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int shift);
void __attribute__ ((noinline)) matmul_FP16_FP16_FP32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int shift);
void __attribute__ ((noinline)) matmul_FP32_FP32_FP32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int shift);
