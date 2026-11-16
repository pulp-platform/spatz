#!/usr/bin/env python3
import re, sys
import csv

data = open(sys.argv[1]).read() if len(sys.argv) > 1 else sys.stdin.read()

pat = {
    "var_to_check":  re.compile(r"Update_variable_to_check took (\d+) cycles"),
    "check_to_var":  re.compile(r"Update_check_to_variable_messages took (\d+) cycles"),
    "soft_bits":     re.compile(r"Update_soft_bits took (\d+) cycles"),
}

cols = {k: [int(x) for x in p.findall(data)] for k, p in pat.items()}
n = min(len(v) for v in cols.values())
w = csv.writer(sys.stdout, delimiter="\t")
w.writerow(["var_to_check", "check_to_var", "soft_bits"])
for i in range(n):
    w.writerow([cols['var_to_check'][i], cols['check_to_var'][i], cols['soft_bits'][i]])