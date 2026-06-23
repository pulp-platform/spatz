#!/bin/bash
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Regenerate every committed `data/data_spmv_*.h` header. Edit the
# `variants` list below to add or remove a shape. Each entry is
# "<format> <N> <P_W>".

set -euo pipefail
cd "$(dirname "$0")"
mkdir -p ../data

variants=(
  "1_to_4 32 128"
  "2_to_4 32 128"
)

for v in "${variants[@]}"; do
  read -r fmt N P_W <<< "$v"
  ./gen_data.py --format "$fmt" --N "$N" --P-W "$P_W"
done
