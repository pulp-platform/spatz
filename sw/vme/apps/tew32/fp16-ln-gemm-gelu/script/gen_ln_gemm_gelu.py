#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
"""FP16 LayerNorm -> GeMM -> GELU (O-POPE P3: C=96, T=64, COUT=176): wrapper of ../../../script/tf/gemm_gelu.py --mode p3 --halfrows 1 --margin 1 (see there)."""
import pathlib, subprocess, sys
TF = pathlib.Path(__file__).resolve().parents[3] / "script" / "tf"
sys.exit(subprocess.run([sys.executable, str(TF / "gemm_gelu.py"), '--mode', 'p3', '--halfrows', '1', '--margin', '1', *sys.argv[1:]]).returncode)
