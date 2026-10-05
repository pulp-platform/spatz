#!/usr/bin/env python3

# Both store variants consume exactly the same padded and packed inputs.
import pathlib

exec(compile(open(pathlib.Path(__file__).parents[2] / "fp32-gemm-vtse" /
                  "script" / "gen_data.py").read(), __file__, "exec"))
