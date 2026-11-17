#!/usr/bin/env python3
import argparse,pathlib,sys,numpy as np
sys.path.append(str(pathlib.Path(__file__).resolve().parents[3]/"script"));import vme_data as od
p=argparse.ArgumentParser();p.add_argument("--output",required=True);a=p.parse_args();D,T=64,96;r=np.random.default_rng(24)
q=r.standard_normal((D,T)).astype(np.float32).astype(np.float16);k=r.standard_normal((D,T)).astype(np.float32).astype(np.float16);v=r.standard_normal((T,D)).astype(np.float32).astype(np.float16);qs=(q.astype(np.float64)*(1/np.sqrt(D))).astype(np.float16)
s=k.astype(np.float64).T@qs.astype(np.float64);s=np.where(np.arange(T)[:,None]>np.arange(T)[None,:],-np.inf,s);e=np.exp(s-s.max(0,keepdims=True));p16=(e/e.sum(0,keepdims=True)).astype(np.float16);o=(v.astype(np.float64).T@p16.astype(np.float64)).astype(np.float32)
# The online variant consumes one 32-key block at a time.  It stores each
# block's unnormalised FP16 weights, rescales the accumulated output by the
# running-max factor, and divides by the running denominator at the end.
# It is intentionally a separate reference from the exact-materialised P path.
m=np.full((T,),-np.inf,dtype=np.float64);l=np.zeros((T,),dtype=np.float64);oo=np.zeros((D,T),dtype=np.float64)
for b in range(0,T,32):
 sb=s[b:b+32]; mn=np.maximum(m,sb.max(0)); alpha=np.exp(m-mn); eb=np.exp(sb-mn)
 oo=oo*alpha+v[b:b+32].astype(np.float64).T@eb.astype(np.float16).astype(np.float64)
 l=l*alpha+eb.sum(0); m=mn
online_o=(oo/l).astype(np.float32)
def pk(x):return od.pack_pairs(od.halves(x).reshape(x.shape))
mask=np.where(np.arange(T)[:,None]>np.arange(T)[None,:],-1e30,0).astype(np.float32)
text="#include <stdint.h>\n#define D_SIZE 64\n#define T_SIZE 96\n"
for n,x in (("q",pk(qs)),("k",pk(k)),("v",pk(v)),("mask",od.words(mask)),("golden",od.words(o)),("online_golden",od.words(online_o))):text+=od.carray("d_"+n,x)+"\n"
z=pathlib.Path(a.output);z.parent.mkdir(parents=True,exist_ok=True);z.write_text(text)
