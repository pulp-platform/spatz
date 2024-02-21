#!/usr/bin/env python3
# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Luca Colagrande <colluca@iis.ee.ethz.ch>

import sys
from pathlib import Path

sys.path.append(str(Path(__file__).parent / '../../util/sim'))
from sim_utils import parser, get_simulations, run_simulations  # noqa: E402
from Simulator import QuestaSimulator, VCSSimulator, VerilatorSimulator, \
                      BansheeSimulator  # noqa: E402


SIMULATORS = {
    'vsim': QuestaSimulator(Path(__file__).parent.resolve() / 'bin/snitch_cluster.vsim'),
    'vcs': VCSSimulator(Path(__file__).parent.resolve() / 'bin/snitch_cluster.vcs'),
    'verilator': VerilatorSimulator(Path(__file__).parent.resolve() / 'bin/snitch_cluster.vlt'),
    'banshee': BansheeSimulator(Path(__file__).parent.resolve() / 'src/banshee.yaml')
}


def main():
    args = parser('vsim', SIMULATORS.keys()).parse_args()
    simulations = get_simulations(args.testlist, SIMULATORS[args.simulator], args.run_dir)
    return run_simulations(simulations,
                           n_procs=args.n_procs,
                           dry_run=args.dry_run,
                           early_exit=args.early_exit,
                           verbose=args.verbose,
                           report_path=Path(args.run_dir) / 'report.csv')


if __name__ == '__main__':
    sys.exit(main())
