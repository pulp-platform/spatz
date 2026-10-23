# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator: P4b: GeMM -> residual add (unfused reference)

import pathlib
import sys

sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "common"))
import opope_data as od  # noqa: E402
import tf_data as tf  # noqa: E402


def main():
    tag, p = od.parse_cfg()
    tf.p_resid(pathlib.Path(__file__).resolve().parents[1], tag, p, False)


if __name__ == "__main__":
    main()
