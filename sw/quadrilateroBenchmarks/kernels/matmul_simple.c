// Simple matmul kernels (normal vs transposed), INT32 x INT32 -> INT32 only.
// Linked into the `matmul` library: add this file to add_library(matmul ...) in CMakeLists.txt.
#include <quadrilatero.h>

#define DTA __INT32__
#define DTB __INT32__
#define DTC __INT32__

#define FUNC_NAME matmul_simple_INT32_INT32_INT32
#include <matmul_simple.h>
#undef FUNC_NAME

#define FUNC_NAME matmul_simple_t_INT32_INT32_INT32
#include <matmul_simple_t.h>
#undef FUNC_NAME

#undef DTA
#undef DTB
#undef DTC
