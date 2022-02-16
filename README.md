# Spatz

Spatz is a vector coprocessor based on the [RISC-V vector extension spec v1.0](https://github.com/riscv/riscv-v-spec/releases/tag/v1.0).

## Get Started

Make sure you clone this repository recursively to get all the necessary submodules:

```bash
git submodule update --init --recursive
```

If the repository path of any submodule changes, run the following command to change your submodule's pointer to the remote repository:

```bash
git submodule sync --recursive
```

## Dependencies

When simulatig RTL code, an installation of Bender is required to generate simulation scripts:

```bash
make bender
```

## Simulation

Simulation of Spatz can be started from the `hardware` directory by using the following command:

```bash
make sim
```

This compile the hardware, open QuestaSim, and simulate Spatz. If there is no need to look at the signal waves, a headless simulation can be startet by typing:

```bash
make simc
```

## Supported Instructions

The goal of Spatz is to implement all instructions belonging to the Zve32x vector extension for embedded processors. The following lists contains all of the instructions belonging to Zve32x and an indication if they are already implemented or not. Spatz does not yet understand vector masking or vector widening/narrowing. Most of the unsupported instructions fall into these two categories.

### Vector Configuration-Setting

| Instruction | Status |
|-------------|:------:|
| vsetvli     |    ✅   |
| vsetivli    |    ✅   |
| vsetvl      |    ✅   |

### Vector Loads and Stores

| Instruction       | Status |
|-------------------|:------:|
| vle{8, 16, 32}    |    ✅   |
| vluxei{8, 16, 32} |    ❌   |
| vlse{8, 16, 32}   |    ✅   |
| vloxei{8, 16, 32} |    ❌   |
| vse{8, 16, 32}    |    ✅   |
| vsuxei{8, 16, 32} |    ❌   |
| vsse{8, 16, 32}   |    ✅   |
| vsoxei{8, 16, 32} |    ❌   |

### Vector Integer Arithmetic

| Instruction                       | Status |
|-----------------------------------|:------:|
| vadd.{vv, vx, vi}                 |    ✅   |
| vsub.{vv, vx}                     |    ✅   |
| vrsub.{vx, vi}                    |    ✅   |
| vwaddu.{vv, vx}                   |    ❌   |
| vwsubu.{vv, vx}                   |    ❌   |
| vwadd.{vv, vx}                    |    ❌   |
| vwsub.{vv, vx}                    |    ❌   |
| vwaddu.{wv, wx}                   |    ❌   |
| vwsubu.{wv, wx}                   |    ❌   |
| vwadd.{wv, wx}                    |    ❌   |
| vwsub.{wv, wx}                    |    ❌   |
| vzext.{vf2, vf4, vf8}             |    ❌   |
| vsext.{vf2, vf4, vf8}             |    ❌   |
| vadc.{vvm, vxm, vim}              |    ❌   |
| vmadc.{vvm, vxm, vim, vv, vx, vi} |    ❌   |
| vsbc.{vvm, vxm}                   |    ❌   |
| vmsbc.{vvm, vxm, vv, vx}          |    ❌   |
| vand.{vv, vx, vi}                 |    ✅   |
| vor.{vv, vx, vi}                  |    ✅   |
| vxor.{vv, vx, vi}                 |    ✅   |
| vssl.{vv, vx, vi}                 |    ❌   |
| vsrl.{vv, vx, vi}                 |    ❌   |
| vsra.{vv, vx, vi}                 |    ❌   |
| vnsrl.{wv, wx, wi}                |    ❌   |
| vnsra.{wv, wx, wi}                |    ❌   |
| vmseq.{vv, vx, vi}                |    ❌   |
| vmsne.{vv, vx, vi}                |    ❌   |
| vmsltu.{vv, vx}                   |    ❌   |
| vmslt.{vv, vx}                    |    ❌   |
| vmsleu.{vv, vx, vi}               |    ❌   |
| vmsle.{vv, vx, vi}                |    ❌   |
| vmsgtu.{vx, vi}                   |    ❌   |
| vmsgt.{vx, vi}                    |    ❌   |
| vminu.{vv. vx}                    |    ✅   |
| vmin.{vv, vx}                     |    ✅   |
| vmaxu.{vv, vx}                    |    ✅   |
| vmax.{vv, vx}                     |    ✅   |
| vmul.{vv, vx}                     |    ✅   |
| vmulh.{vv, vx}                    |    ❌   |
| vmulhu.{vv, vx}                   |    ❌   |
| vmulsu.{vv, vx}                   |    ❌   |
| vdivu.{vv, vx}                    |    ❌   |
| vdiv.{vv, vx}                     |    ❌   |
| vremu.{vv, vx}                    |    ❌   |
| vrem.{vv, vx}                     |    ❌   |
| vwmul.{vv, vx}                    |    ❌   |
| vwmulu.{vv, vx}                   |    ❌   |
| vwmulsu.{vv, vx}                  |    ❌   |
| vmacc.{vv, vx}                    |    ✅   |
| vnmsac.{vv, vx}                   |    ✅   |
| vmadd.{vv, vx}                    |    ✅   |
| vnmsub.{vv, vx}                   |    ✅   |
| vwmaccu.{vv, vx}                  |    ❌   |
| vwmacc.{vv, vx}                   |    ❌   |
| vwmaccsu.{vv, vx}                 |    ❌   |
| vwmaccus.vx                       |    ❌   |
| vmerge.{vvm, vxm, vim}            |    ❌   |
| vmv.v.{v, x, i}                   |    ✅   |

### Vector Fixed-Point Arithmetic

| Instruction          | Status |
|----------------------|:------:|
| vsaddu.{vv, vx, vi}  |    ❌   |
| vsadd.{vv, vx, vi}   |    ❌   |
| vssubu.{vv, vx}      |    ❌   |
| vssub.{vv.vx}        |    ❌   |
| vaaddu.{vv, vx}      |    ❌   |
| vaadd.{vv, vx}       |    ❌   |
| vasubu.{vv, vx}      |    ❌   |
| vasub.{vv, vx}       |    ❌   |
| vsmul.{vv, vx}       |    ❌   |
| vssrl.{vv, vx, vi}   |    ❌   |
| vssra.{vv, vx, vi}   |    ❌   |
| vnclipu.{wv, wx, wi} |    ❌   |
| vnclip.{wv, wx, wi}  |    ❌   |

### Vector Reduction

| Instruction  | Status |
|--------------|:------:|
| vredsum.vs   |    ❌   |
| vredmaxu.vs  |    ❌   |
| vredmax.vs   |    ❌   |
| vredminu.vs  |    ❌   |
| vredmin.vs   |    ❌   |
| vredand.vs   |    ❌   |
| vredor.vs    |    ❌   |
| vredxor.vs   |    ❌   |
| vwredsumu.vs |    ❌   |
| vwredsum.vs  |    ❌   |

### Vector Mask

| Instruction | Status |
|-------------|:------:|
| vmand.mm    |    ❌   |
| vmnand.mm   |    ❌   |
| vmandn.mm   |    ❌   |
| vmxor.mm    |    ❌   |
| vmor.mm     |    ❌   |
| vmorn.mm    |    ❌   |
| vmxnor.mm   |    ❌   |
| vmmv.m      |    ❌   |
| vmclr.m     |    ❌   |
| vmset.m     |    ❌   |
| vmnot.m     |    ❌   |
| vcpop.m     |    ❌   |
| vfirst.m    |    ❌   |
| vmsbf.m     |    ❌   |
| vmsif.m     |    ❌   |
| vmsof.m     |    ❌   |
| viota.m     |    ❌   |
| vid.v       |    ❌   |

### Vector Permutation

| Instruction           | Status |
|-----------------------|:------:|
| vmv.{x.s, s.x}        |    ❌   |
| vslideup.{vx, vi}     |    ✅   |
| vslidedown.{vx, vi}   |    ✅   |
| vslide1up.vx          |    ✅   |
| vslide1down.vx        |    ✅   |
| vrgather.{vv, vx, vi} |    ❌   |
| vrgatherei16.vv       |    ❌   |
| vcompress.vm          |    ❌   |
| vmv{1, 2, 4, 8}r.v    |    ❌   |
