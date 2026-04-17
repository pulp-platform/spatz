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

// UINT8 x UINT8 -> UINT32
#define DTA __UINT8__
#define DTB __UINT8__
#define DTC __UINT32__
#define FUNC_NAME MATMUL_NAME(UINT8,UINT8,UINT32)
#include <matmul.h>
#undef FUNC_NAME
#undef DTA
#undef DTB
#undef DTC

// UINT16 x UINT16 -> UINT32
#define DTA __UINT16__
#define DTB __UINT16__
#define DTC __UINT32__
#define FUNC_NAME MATMUL_NAME(UINT16,UINT16,UINT32)
#include <matmul.h>
#undef FUNC_NAME
#undef DTA
#undef DTB
#undef DTC

// UINT32 x UINT32 -> UINT32
#define DTA __UINT32__
#define DTB __UINT32__
#define DTC __UINT32__
#define FUNC_NAME MATMUL_NAME(UINT32,UINT32,UINT32)
#include <matmul.h>
#undef FUNC_NAME
#undef DTA
#undef DTB
#undef DTC

// FP8ALT x FP8ALT -> FP32
#define DTA __FP8ALT__
#define DTB __FP8ALT__
#define DTC __FP32__
#define FUNC_NAME MATMUL_NAME(FP8ALT,FP8ALT,FP32)
#include <matmul.h>
#undef FUNC_NAME
#undef DTA
#undef DTB
#undef DTC

// BF16 x BF16 -> FP32
#define DTA __FP16ALT__
#define DTB __FP16ALT__
#define DTC __FP32__
#define FUNC_NAME MATMUL_NAME(BF16,BF16,FP32)
#include <matmul.h>
#undef FUNC_NAME
#undef DTA
#undef DTB
#undef DTC