#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51
#
# Author: Tim Fischer <fischeti@iis.ee.ethz.ch>

import re
import sys

def parse_log(filename):
    # Regular expression for start of a section
    routine_re = re.compile(
        r"Inferred memory devices in process in routine '([^']+)' in file"
    )
    # Regular expression for the file path (on the next line)
    file_re = re.compile(r"^\s*(.*?\.sv)'?\s*$")

    results = []

    with open(filename) as f:
        lines = f.readlines()

    i = 0
    while i < len(lines):
        m = routine_re.match(lines[i])
        if m:
            module = m.group(1)
            # Try to get the path from the next line
            path = ""
            if i + 1 < len(lines):
                next_line = lines[i + 1].strip()
                path_match = re.search(r"(/.*?\.sv)", next_line)
                if path_match:
                    path = path_match.group(1)
            i += 1
            # skip to the header
            while i < len(lines) and not lines[i].strip().startswith('|'):
                i += 1
            # skip the two header lines
            i += 2

            # Now parse the table until a line of '='
            while i < len(lines):
                line = lines[i].rstrip('\n')
                if line.strip().startswith('='):
                    break
                if line.strip().startswith('|'):
                    fields = [f.strip() for f in line.strip().split('|')[1:-1]]
                    if (
                        len(fields) == 9
                        and fields[1] == 'Flip-flop'
                        and fields[6] not in ('Async', 'Both')
                        and fields[5] != 'Async'
                    ):
                        results.append((module, path, fields[0]))
                i += 1
        else:
            i += 1

    return results

if __name__ == "__main__":
    # Example usage: parse 'elaboration.log'
    if len(sys.argv) < 2:
        print("Usage: check_ffs.py <logfile>")
        sys.exit(1)
    results = parse_log(sys.argv[1])
    for module, path, reg in results:
        print(f"Module: {module}, Path: {path}, Register: {reg}")
