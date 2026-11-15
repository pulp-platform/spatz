#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
"""FP16 GeMM -> GELU (O-POPE P2: C=128, T=64, COUT=192): wrapper of ../../../script/tf/gemm_gelu.py --mode p2 (see there)."""
import pathlib, subprocess, sys
TF = pathlib.Path(__file__).resolve().parents[3] / "script" / "tf"
sys.exit(subprocess.run([sys.executable, str(TF / "gemm_gelu.py"), '--mode', 'p2', *sys.argv[1:]]).returncode)
