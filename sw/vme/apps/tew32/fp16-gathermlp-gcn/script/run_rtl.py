#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run the VME application on the local cluster RTL."""
import pathlib
import subprocess
import sys
import tempfile

root = pathlib.Path(__file__).resolve().parents[6]
hw = root / 'hw/system/spatz_cluster'
elf = hw / 'sw/build/vme/apps' / 'test-vme-fp16-gathermlp-gcn'
if not elf.exists():
    sys.exit(f'Missing VME ELF: {elf}')
work = pathlib.Path(tempfile.mkdtemp(prefix='vme_fp16-gathermlp-gcn_'))
(work / 'bin/logs').mkdir(parents=True)
(work / 'logs').mkdir()
(work / 'modelsim.ini').write_text((hw / 'modelsim.ini').read_text())
(work / 'bin/logs/.rtlbinary').write_text(str(elf) + '\n')
cmd = ['questa-2021.3-kgf', 'vsim', '+permissive', '-t', '1ps', '-do',
       'run -a; quit -f', '-voptargs=-O5', '-work', str(hw / 'work-vsim'),
       '-c', '-ldflags', f'-Wl,-rpath,{hw}/work/lib -L{hw}/work/lib -lfesvr_vsim -lutil',
       'tb_bin', '+permissive-off', '++' + str(elf)]
with (work / 'run.log').open('w') as log:
    rc = subprocess.call(cmd, cwd=work, stdout=log, stderr=subprocess.STDOUT)
result = (work / 'run.log').read_text()
print(work)
print('\n'.join(result.splitlines()[-20:]))
if rc or '[SUCCESS]' not in result or 'Status=FAIL' in result:
    sys.exit(1)
