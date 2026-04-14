#include <quadrilatero.h>

// INT8 x INT8 -> INT32
#define DTA __INT8__
#define DTB __INT8__
#define DTC __INT32__
#define FUNC_NAME MATMUL_NAME(INT8,INT8,INT32)
#include <matmul.h>
#undef FUNC_NAME
#undef DTA
#undef DTB
#undef DTC

// INT16 x INT16 -> INT32
#define DTA __INT16__
#define DTB __INT16__
#define DTC __INT32__
#define FUNC_NAME MATMUL_NAME(INT16,INT16,INT32)
#include <matmul.h>
#undef FUNC_NAME
#undef DTA
#undef DTB
#undef DTC

// INT32 x INT32 -> INT32
#define DTA __INT32__
#define DTB __INT32__
#define DTC __INT32__
#define FUNC_NAME MATMUL_NAME(INT32,INT32,INT32)
#include <matmul.h>
#undef FUNC_NAME
#undef DTA
#undef DTB
#undef DTC

// FP8 x FP8 -> FP32
#define DTA __FP8__
#define DTB __FP8__
#define DTC __FP32__
#define FUNC_NAME MATMUL_NAME(FP8,FP8,FP32)
#include <matmul.h>
#undef FUNC_NAME
#undef DTA
#undef DTB
#undef DTC

// FP16 x FP16 -> FP32
#define DTA __FP16__
#define DTB __FP16__
#define DTC __FP32__
#define FUNC_NAME MATMUL_NAME(FP16,FP16,FP32)
#include <matmul.h>
#undef FUNC_NAME
#undef DTA
#undef DTB
#undef DTC

// FP32 x FP32 -> FP32
#define DTA __FP32__
#define DTB __FP32__
#define DTC __FP32__
#define FUNC_NAME MATMUL_NAME(FP32,FP32,FP32)
#include <matmul.h>
#undef FUNC_NAME
#undef DTA
#undef DTB
#undef DTC
