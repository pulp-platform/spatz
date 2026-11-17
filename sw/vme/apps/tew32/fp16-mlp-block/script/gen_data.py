#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
import pathlib,sys,argparse
import numpy as np
sys.path.append(str(pathlib.Path(__file__).resolve().parents[3] / 'script'))
import vme_data as tf
import vme_data as od
p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=pathlib.Path);a=p.parse_args()
def emit(app,tag,defines,arrays,footprint):
    total=sum(footprint.values())
    # The generic 1x4 GEMM consumes reduction pairs in groups of eight.
    # Latest O-POPE has 73 pairs (H144 + the bias pair); pad both operands to
    # 80 pairs with zeros without changing the logical workload.
    extra=(80-73)*(64+64)*4
    assert total+extra<=120*1024, (total,extra)
    defines.update(BASELINE_BYTES=total,VME_BYTES=total+extra,
                   GEMM2_K_SIZE=160)
    text='#include <stdint.h>\n'+''.join(f'#define {k} {v}\n' for k,v in defines.items())
    for k,v in arrays.items():
        text+=od.carray('d_'+k,v.flatten() if v.dtype==np.uint32 else od.words(v))+'\n'
        if k in ('qpk','kpk','vpk','w1','w2'):
            r,n=v.shape
            if k == 'w2' and r == 73:
                v=np.pad(v,((0,7),(0,0)))
                r=80
            tiled=v.reshape(r,n//16,16).transpose(1,0,2).copy()
            text+=od.carray('d_'+k+'_vme',tiled.flatten())+'\n'
    for rows in ([64,144] if MODE=='mlp' else [64]):
        physical_pairs=80 if rows == 144 else rows//2
        offsets=np.array([(q//16)*physical_pairs*16*4+(q%16)*4 for q in range(64)],dtype=np.uint16)
        assert offsets.max()+((rows//2)-1)*16*4+2 < physical_pairs*64*4
        text+=od.carray(f'd_offsets{rows}',offsets.view(np.uint32))+'\n'
        if rows == 144:
            seed=np.zeros((4,80,16),dtype=np.uint32)
            seed.reshape(-1)[:offsets.view(np.uint32).size]=offsets.view(np.uint32)
            for q in range(64):
                # pack_pairs uses low=even, high=odd; bias2 is even row 144.
                seed[q//16,72,q%16]=np.uint32(0x3c00)
            text+=od.carray('d_hpk_seed',seed.flatten())+'\n'
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(text)
    print(f'{MODE}: baseline={total} VME={total+extra} bytes')
tf.emit=emit
MODE='mlp'
tf.p_mlp(None,'matched',dict(C=64,T=64,HID=144))
