#!/usr/bin/env python3
import argparse,json,pathlib
import numpy as np

def carray(name,values):
    flat=np.asarray(values).reshape(-1)
    body=", ".join(hex(int(value)) for value in flat)
    return (f"static const uint32_t {name}[{flat.size}] "
            f"__attribute__((section(\".data\"), aligned(64)))={{{body}}};\n")

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--config",required=True,type=pathlib.Path)
    parser.add_argument("--output",required=True,type=pathlib.Path)
    args=parser.parse_args()
    cfg=json.loads(args.config.read_text())
    m,reduction,n=(cfg[key] for key in ("M","N","K"))
    cbw,cbn,ncb=(cfg[key] for key in ("CB_W","CBN","NCB"))
    assert m in (64,96)
    assert (reduction,n,cbw,cbn,ncb)==(128,128,16,64,2)
    rng=np.random.default_rng(cfg["seed"])
    cb=rng.uniform(-0.5,0.5,(ncb,cbn,2,cbw)).astype(np.float16)
    groups=n//cbw
    pairs=reduction//2
    idx=rng.integers(0,cbn,(ncb,pairs,groups))
    x=rng.uniform(-1,1,(m,reduction)).astype(np.float16)
    scale=rng.uniform(0.5,1.5,reduction).astype(np.float16)
    x=(x*scale[None,:]).astype(np.float16)
    entries=(cb[0][idx[0]]+cb[1][idx[1]]).astype(np.float16)
    weight=np.zeros((reduction,n),dtype=np.float16)
    weight[0::2]=entries[:,:,0].reshape(pairs,n)
    weight[1::2]=entries[:,:,1].reshape(pairs,n)
    golden=(x.astype(np.float64)@weight.astype(np.float64)).astype(np.float32)
    halves=x.view(np.uint16).astype(np.uint32)
    words=(halves[:,0::2]<<16)|halves[:,1::2]
    apack=words.reshape(m//16,16,pairs).transpose(0,2,1).copy()
    cbh=cb.view(np.uint16).astype(np.uint32)
    cbwords=(cbh[:,:,0]<<16)|cbh[:,:,1]
    ii=idx.astype(np.uint32)
    idxwords=(ii[0,:,0::2]|(ii[1,:,0::2]<<7)|
              (ii[0,:,1::2]<<14)|(ii[1,:,1::2]<<21))
    lines=["#pragma once","#include <stdint.h>",carray("vq_apack_dram",apack),
           carray("vq_cb_dram",cbwords),carray("vq_idx_dram",idxwords),
           carray("vq_golden_dram",golden.view(np.uint32))]
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text("\n".join(lines))
if __name__=="__main__": main()
