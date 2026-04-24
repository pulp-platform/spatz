#define __INT8__     0
#define __INT16__    1
#define __INT32__    2
#define __FP8__      4
#define __FP16__     5
#define __FP32__     6
#define __UINT8__    8
#define __UINT16__   9
#define __UINT32__  10
#define __FP8ALT__  12
#define __FP16ALT__ 13
#define __FP32ALT__ 14
#define CMUL_1 0
#define RMUL_1 0
#define CMUL_2 1
#define RMUL_2 1
#define CMUL_4 3
#define RMUL_4 3


//=============================================//
//           MATRIX MULTIPLICATIONS            //
//=============================================//
#define MATMUL_NAME(DTA,DTB,DTC) \
    matmul_##DTA##_##DTB##_##DTC

#define matmul(DTA,DTB,DTC,A,B,C,K,N,M,widening) \
    MATMUL_NAME(DTA,DTB,DTC)(A,B,C,K,N,M,widening)

// Signed Integer MatMuls
void __attribute__ ((noinline)) matmul_INT8_INT8_INT32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int widening);
void __attribute__ ((noinline)) matmul_INT16_INT16_INT32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int widening);
void __attribute__ ((noinline)) matmul_INT32_INT32_INT32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int widening);

// Floating-Point MatMuls
void __attribute__ ((noinline)) matmul_FP8_FP8_FP32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int widening);
void __attribute__ ((noinline)) matmul_BF16_BF16_FP32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int widening);
void __attribute__ ((noinline)) matmul_FP32_FP32_FP32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int widening);

// Unsigned Integer MatMuls
void __attribute__ ((noinline)) matmul_UINT8_UINT8_UINT32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int widening);
void __attribute__ ((noinline)) matmul_UINT16_UINT16_UINT32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int widening);
void __attribute__ ((noinline)) matmul_UINT32_UINT32_UINT32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int widening);

// Alt Floating-Point MatMuls
void __attribute__ ((noinline)) matmul_FP16_FP16_FP32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int widening);
void __attribute__ ((noinline)) matmul_FP8ALT_FP8ALT_FP32(void* addrA,void* addrB, void* addrC, int K, int N, int M, int widening);
