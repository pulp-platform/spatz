#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# SPDX-License-Identifier: Apache-2.0
"""Gather-MLP, GCN shape: wrapper of ../../../script/tf/gathermlp.py --mode gcn (see there)."""
import pathlib, subprocess, sys
TF = pathlib.Path(__file__).resolve().parents[3] / "script" / "tf"
sys.exit(subprocess.run([sys.executable, str(TF / "gathermlp.py"), '--mode', 'gcn', *sys.argv[1:]]).returncode)
