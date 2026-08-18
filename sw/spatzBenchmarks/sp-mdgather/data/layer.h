// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

/**
 * @struct md_layer_struct
 * @brief Parameters of the molecular-dynamics cluster-pair gather benchmark
 *        (GROMACS-style cluster-pair algorithm, simplified FMA-only force).
 * @var md_layer_struct::NC
 * Number of atom clusters (4 atoms per cluster). Coordinates are stored
 * cluster-blocked in the padded xyzq X4 layout: 64 bytes per cluster,
 * x[4] fp32 | y[4] fp32 | z[4] fp32 | q[4] fp32.
 * @var md_layer_struct::LIST
 * Number of j-cluster entries in the pair list of each i-cluster.
 * @var md_layer_struct::CUT2
 * Squared cutoff radius of the (simplified) pair interaction.
 */
typedef struct md_layer_struct {
  uint32_t NC;
  uint32_t LIST;
  float CUT2;
} md_layer;
