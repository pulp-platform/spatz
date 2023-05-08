#!/usr/bin/env python3
# Copyright 2020 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

import argparse
import hjson
import pathlib
import sys

from jsonref import JsonRef
from clustergen.cluster import SnitchClusterTB


def main():
    """Generate a Spatz cluster TB and all corresponding configuration files."""
    parser = argparse.ArgumentParser(prog="clustergen")
    parser.add_argument(
        "--clustercfg",
        "-c",
        metavar="file",
        type=argparse.FileType("r"),
        required=True,
        help="A cluster configuration file",
    )
    parser.add_argument(
        "--outdir", "-o", type=pathlib.Path, required=True, help="Target directory."
    )

    args = parser.parse_args()

    # Read HJSON description
    with args.clustercfg as file:
        try:
            srcfull = file.read()
            obj = hjson.loads(srcfull, use_decimal=True)
            obj = JsonRef.replace_refs(obj)
        except ValueError:
            raise SystemExit(sys.exc_info()[1])

    cluster_tb = SnitchClusterTB(obj)

    if not args.outdir.is_dir():
        exit("Out directory is not a valid path.")

    outdir = args.outdir / "generated"
    outdir.mkdir(parents=True, exist_ok=True)

    spatzoutdir = outdir / "../../../../ip/spatz/src/generated"
    spatzoutdir.mkdir(parents=True, exist_ok=True)
    with open(spatzoutdir / "spatz_pkg.sv", "w") as f:
        f.write(cluster_tb.render_spatzpkg())

    with open(outdir / "spatz_cluster_wrapper.sv", "w") as f:
        f.write(cluster_tb.render_wrapper())

    with open(outdir / "link.ld", "w") as f:
        f.write(cluster_tb.render_linker_script())

    with open(outdir / "bootdata.cc", "w") as f:
        f.write(cluster_tb.render_bootdata())

    with open(outdir / "bootdata_bootrom.cc", "w") as f:
        f.write(cluster_tb.render_bootdata_bootrom())

    with open(outdir / "memories.json", "w") as f:
        f.write(cluster_tb.cluster.memory_cfg())

    with open(outdir / "testharness.sv", "w") as f:
        f.write(cluster_tb.render_testbench())


if __name__ == "__main__":
    main()
