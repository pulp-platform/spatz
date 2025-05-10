#!/usr/bin/env bash

# Copyright 2021 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Samuel Riedel, ETH Zurich

# Execute from git's root dir
cd $(git rev-parse --show-toplevel 2>/dev/null)

if [[ -n "$CI_MERGE_REQUEST_ID" ]]; then
  # Make sure we have the latest version of the target branch
  git fetch origin $CI_MERGE_REQUEST_TARGET_BRANCH_NAME
  base="origin/$CI_MERGE_REQUEST_TARGET_BRANCH_NAME"
elif [[ -n "$1" ]]; then
  base="$1"
else
  base="HEAD~1"
fi

# Check for clang format. Check if clang-format exists in LLVM_INSTALL_DIR or is
# locally installed. Error out if neither can be found.
echo "Check C and C++ source code"
( [ -d install/llvm ] || [ -n "$LLVM_INSTALL_DIR" ] ) || { echo "Error: neither install/llvm exists nor LLVM_INSTALL_DIR is set" >&2; exit 1; }; \
CF=install/llvm/bin/clang-format; [ ! -d install/llvm ] && CF="$LLVM_INSTALL_DIR/bin/clang-format"; \
./util/vendor/run_clang_format.py \
    --clang-format-executable="$CF" -r sw/spatzBenchmarks || EXIT_STATUS=$?

# Check python files
echo "Check Python files"
if [[ -n "$GITLAB_CI" ]]; then
  # Special case for gitlab ci, since it has an old default python
  python3=$(command -v python3.6) || EXIT_STATUS=$?
else
  python3=$(command -v python3) || EXIT_STATUS=$?
fi
${python3} -m flake8 --ignore=E501,W503,E203 sw/snRuntime || EXIT_STATUS=$?
${python3} -m flake8 --ignore=E501,W503,E203 sw/spatzBenchmarks || EXIT_STATUS=$?
${python3} -m flake8 --ignore=E501,W503,E203 util || EXIT_STATUS=$?

# Check for trailing whitespaces and tabs
echo "Checking for trailing whitespaces and tabs in unstaged files"
git --no-pager diff --check -- \
    ':(exclude)**.def' \
    ':(exclude)**.patch' \
    ':(exclude)sw/toolchain/**' \
    ':(exclude)sw/riscv-tests/**' \
    || EXIT_STATUS=$?

echo "Checking for trailing whitespaces and tabs between HEAD and $base"
git --no-pager diff --check $base HEAD -- \
    ':(exclude)**.def' \
    ':(exclude)**.patch' \
    ':(exclude)sw/toolchain/**' \
    ':(exclude)sw/riscv-tests/**' \
    || EXIT_STATUS=$?

exit $EXIT_STATUS
