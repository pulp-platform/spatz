# VME MatMul utilization

This document collects the available rolled/iCache and unrolled/iBuffer RTL
measurements. Cycles cover the kernel and exclude DMA. Utilization is
`M*N*K/(64*cycles)` for FP32->FP32 and `M*N*K/(128*cycles)` for FP16->FP32.

## TE=16

### FP32->FP32

Workloads: M64_N64_K64, M64_N64_K128, and M64_N64_K256.

#### VTSE

| Version | Schedule | K64 cycles | Util. | K128 cycles | Util. | K256 cycles | Util. |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| rolled (iCache) | `store_afterK` | 5,693 | 71.95% | 9,799 | 83.60% | 17,981 | 91.12% |
| rolled (iCache) | `store_interleave` | 5,328 | 76.88% | 9,744 | 84.07% | 17,616 | 93.01% |
| rolled (iCache) | `vsetvl` | 4,712 | 86.93% | 8,804 | 93.05% | 17,001 | 96.37% |
| unrolled (iBuffer) | `store_afterK` | 4,966 | 82.48% | 9,062 | 90.40% | 17,262 | 94.91% |
| unrolled (iBuffer) | `store_interleave` | 4,698 | 87.19% | 8,794 | 93.15% | 16,994 | 96.41% |
| unrolled (iBuffer) | `vsetvl` | 4,414 | 92.80% | 8,510 | 96.26% | 16,705 | 98.08% |

#### VTMV + VSE

| Version | Schedule | K64 cycles | Util. | K128 cycles | Util. | K256 cycles | Util. |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| rolled (iCache) | `store_afterK` | 5,708 | 71.76% | 9,810 | 83.51% | 17,996 | 91.04% |
| rolled (iCache) | `store_group` | — | — | 10,087 | 81.21% | — | — |
| unrolled (iBuffer) | `store_afterK` | 5,424 | 75.52% | 9,520 | 86.05% | 17,712 | 92.50% |
| unrolled (iBuffer) | `store_interleave` | 4,979 | 82.27% | 9,076 | 90.26% | 17,267 | 94.89% |
| unrolled (iBuffer) | `vsetvl` | 5,036 | 81.33% | 9,133 | 89.70% | 17,324 | 94.57% |

### FP16->FP32

Workloads: M64_N64_K128, M64_N64_K256, and M64_N64_K512.

#### VTSE

| Version | Schedule | K128 cycles | Util. | K256 cycles | Util. | K512 cycles | Util. |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| rolled (iCache) | `store_afterK` | 5,625 | 72.82% | 9,731 | 84.18% | 17,913 | 91.46% |
| rolled (iCache) | `store_interleave` | 5,214 | 78.56% | 9,308 | 88.01% | 17,503 | 93.61% |
| rolled (iCache) | `vsetvl` | 5,011 | 81.74% | 9,098 | 90.04% | 17,299 | 94.71% |
| unrolled (iBuffer) | `store_afterK` | 4,934 | 83.02% | 9,034 | 90.68% | 17,226 | 95.11% |
| unrolled (iBuffer) | `store_interleave` | 4,835 | 84.72% | 8,935 | 91.68% | 17,127 | 95.66% |

#### VTMV + VSE

| Version | Schedule | K128 cycles | Util. | K256 cycles | Util. | K512 cycles | Util. |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| rolled (iCache) | `store_afterK` | 5,691 | 71.97% | 9,799 | 83.60% | 17,979 | 91.13% |
| rolled (iCache) | `store_interleave` | 6,046 | 67.75% | 10,141 | 80.78% | 18,334 | 89.36% |
| rolled (iCache) | `store_group` | 5,171 | 79.21% | 9,271 | 88.36% | 17,463 | 93.82% |
| rolled (iCache) | `vsetvl` | 6,378 | 64.22% | 10,470 | 78.24% | 18,666 | 87.77% |
| unrolled (iBuffer) | `store_afterK` | 5,452 | 75.13% | 9,549 | 85.79% | 17,740 | 92.36% |
| unrolled (iBuffer) | `store_interleave` | 5,552 | 73.78% | 9,649 | 84.90% | 17,840 | 91.84% |
| unrolled (iBuffer) | `store_group` | 5,406 | 75.77% | 9,934 | 82.46% | 19,008 | 86.20% |

## TE=32

Only rolled/iCache measurements are currently available for TE=32.

### FP32->FP32

Workloads: M128_N128_K64, M128_N128_K128, and M128_N128_K256.

#### VTSE, rolled (iCache)

| Schedule | K64 cycles | Util. | K128 cycles | Util. | K256 cycles | Util. |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `store_afterK` | 18,890 | 86.73% | 35,284 | 92.87% | 68,052 | 96.30% |
| `store_interleave` | 17,968 | 91.18% | 34,353 | 95.39% | 67,121 | 97.64% |
| `vsetvl` | 17,501 | 93.62% | 33,886 | 96.70% | 66,654 | 98.32% |

#### VTMV + VSE, rolled (iCache)

| Schedule | K64 cycles | Util. | K128 cycles | Util. | K256 cycles | Util. |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `store_afterK` | 21,389 | 76.60% | 37,773 | 86.75% | 70,541 | 92.90% |
| `store_interleave` | 20,841 | 78.61% | 37,225 | 88.03% | 69,993 | 93.63% |
| `vsetvl` | 21,279 | 77.00% | 37,663 | 87.00% | 70,431 | 93.05% |

### FP16->FP32

Workloads: M128_N128_K128, M128_N128_K256, and M128_N128_K512. No TE=32
VTSE snapshot is recorded.

#### VTMV + VSE, rolled (iCache)

| Schedule | K128 cycles | Util. | K256 cycles | Util. | K512 cycles | Util. |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `store_afterK` | 18,952 | 86.45% | 35,346 | 92.71% | — | — |
| `store_interleave` | 18,266 | 89.70% | 34,669 | 94.52% | — | — |
| `vsetvl` | — | — | 34,678 | 94.49% | — | — |
