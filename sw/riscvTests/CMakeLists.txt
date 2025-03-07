# Copyright 2020 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13)

# Allow spatzBenchmarks to be built as a standalone library.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
    set(CMAKE_TOOLCHAIN_FILE toolchain-gcc CACHE STRING "Toolchain to use")

    project(Benchmarks LANGUAGES C ASM)
    include(SnitchUtilities)

    # Build the runtime.
    add_subdirectory(../snRuntime snRuntime)
endif()

add_compile_options(-O3 -g -ffunction-sections)
add_compile_options(-DELEN=64)

include_directories(include)
include_directories(${SNRUNTIME_INCLUDE_DIRS})
include_directories(isa/macros/vector)

# Tests
enable_testing()
set(SNITCH_TEST_PREFIX riscvTests-)

add_snitch_test(vsetvli isa/rv64uv/vsetvli.c)

add_snitch_test(vadd  isa/rv64uv/vadd.c)
add_snitch_test(vsub  isa/rv64uv/vsub.c)
add_snitch_test(vrsub isa/rv64uv/vrsub.c)

add_snitch_test(vwadd  isa/rv64uv/vwadd.c)
add_snitch_test(vwaddu isa/rv64uv/vwaddu.c)
add_snitch_test(vwsub  isa/rv64uv/vwsub.c)
add_snitch_test(vwsubu isa/rv64uv/vwsubu.c)

add_snitch_test(vand isa/rv64uv/vand.c)
add_snitch_test(vor  isa/rv64uv/vor.c)
add_snitch_test(vxor isa/rv64uv/vxor.c)

add_snitch_test(vsll isa/rv64uv/vsll.c)
add_snitch_test(vsrl isa/rv64uv/vsrl.c)
add_snitch_test(vsra isa/rv64uv/vsra.c)

add_snitch_test(vmin  isa/rv64uv/vmin.c)
add_snitch_test(vminu isa/rv64uv/vminu.c)
add_snitch_test(vmax  isa/rv64uv/vmax.c)
add_snitch_test(vmaxu isa/rv64uv/vmaxu.c)

add_snitch_test(vmul    isa/rv64uv/vmul.c)
add_snitch_test(vmulh   isa/rv64uv/vmulh.c)
add_snitch_test(vmulhu  isa/rv64uv/vmulhu.c)
add_snitch_test(vmulhsu isa/rv64uv/vmulhsu.c)

add_snitch_test(vwmul   isa/rv64uv/vwmul.c)
add_snitch_test(vwmulu  isa/rv64uv/vwmulu.c)
add_snitch_test(vwmulsu isa/rv64uv/vwmulsu.c)

add_snitch_test(vmacc  isa/rv64uv/vmacc.c)
add_snitch_test(vmadd  isa/rv64uv/vmadd.c)
add_snitch_test(vnmsac isa/rv64uv/vnmsac.c)
add_snitch_test(vnmsub isa/rv64uv/vnmsub.c)

add_snitch_test(vwmacc   isa/rv64uv/vwmacc.c)
add_snitch_test(vwmaccu  isa/rv64uv/vwmaccu.c)
add_snitch_test(vwmaccus isa/rv64uv/vwmaccus.c)
add_snitch_test(vwmaccsu isa/rv64uv/vwmaccsu.c)

add_snitch_test(vredsum  isa/rv64uv/vredsum.c)
add_snitch_test(vredand  isa/rv64uv/vredand.c)
add_snitch_test(vredor   isa/rv64uv/vredor.c)
add_snitch_test(vredxor  isa/rv64uv/vredxor.c)
add_snitch_test(vredmin  isa/rv64uv/vredmin.c)
add_snitch_test(vredminu isa/rv64uv/vredminu.c)
add_snitch_test(vredmax  isa/rv64uv/vredmax.c)
add_snitch_test(vredmaxu isa/rv64uv/vredmaxu.c)

add_snitch_test(vmv isa/rv64uv/vmv.c)

add_snitch_test(vslide1up   isa/rv64uv/vslide1up.c)
add_snitch_test(vslideup    isa/rv64uv/vslideup.c)
add_snitch_test(vslide1down isa/rv64uv/vslide1down.c)
add_snitch_test(vslidedown  isa/rv64uv/vslidedown.c)

add_snitch_test(vdiv  isa/rv64uv/vdiv.c)
add_snitch_test(vdivu isa/rv64uv/vdivu.c)
add_snitch_test(vrem  isa/rv64uv/vrem.c)
add_snitch_test(vremu isa/rv64uv/vremu.c)

add_snitch_test(vfadd  isa/rv64uv/vfadd.c)
add_snitch_test(vfsub  isa/rv64uv/vfsub.c)
add_snitch_test(vfrsub isa/rv64uv/vfrsub.c)

add_snitch_test(vfmin isa/rv64uv/vfmin.c)
add_snitch_test(vfmax isa/rv64uv/vfmax.c)

add_snitch_test(vfmul   isa/rv64uv/vfmul.c)
add_snitch_test(vfmacc  isa/rv64uv/vfmacc.c)
add_snitch_test(vfnmacc isa/rv64uv/vfnmacc.c)
add_snitch_test(vfmsac  isa/rv64uv/vfmsac.c)
add_snitch_test(vfnmsac isa/rv64uv/vfnmsac.c)
add_snitch_test(vfmadd  isa/rv64uv/vfmadd.c)
add_snitch_test(vfnmadd isa/rv64uv/vfnmadd.c)
add_snitch_test(vfmsub  isa/rv64uv/vfmsub.c)
add_snitch_test(vfnmsub isa/rv64uv/vfnmsub.c)

add_snitch_test(vfredmin  isa/rv64uv/vfredmin.c)
add_snitch_test(vfredmax  isa/rv64uv/vfredmax.c)
add_snitch_test(vfredosum isa/rv64uv/vfredosum.c)
add_snitch_test(vfredusum isa/rv64uv/vfredusum.c)

add_snitch_test(vfwadd   isa/rv64uv/vfwadd.c)
add_snitch_test(vfwsub   isa/rv64uv/vfwsub.c)
add_snitch_test(vfwmul   isa/rv64uv/vfwmul.c)
add_snitch_test(vfwmacc  isa/rv64uv/vfwmacc.c)
add_snitch_test(vfwmsac  isa/rv64uv/vfwmsac.c)
add_snitch_test(vfwnmsac isa/rv64uv/vfwnmsac.c)

add_snitch_test(vfsgnj  isa/rv64uv/vfsgnj.c)
add_snitch_test(vfsgnjn isa/rv64uv/vfsgnjn.c)
add_snitch_test(vfsgnjx isa/rv64uv/vfsgnjx.c)

add_snitch_test(vfcvt  isa/rv64uv/vfcvt.c)
add_snitch_test(vfncvt isa/rv64uv/vfncvt.c)

add_snitch_test(vfmv isa/rv64uv/vfmv.c)
