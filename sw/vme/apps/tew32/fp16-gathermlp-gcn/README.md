# FP16 GCN layer (O-POPE gathermlp gcn: N=127, F=64, L=8, Dout=128)

Gather + FP16 pooling (O-POPE order; index words through FP registers), then the 1x2c staggered GeMM with the ReLU fused into the tile drain (tile -> VRF -> vfmax -> vse). Generator: `../../script/tf/gathermlp.py` (wrapper `script/gen_gathermlp_gcn.py`).

Build and run (from `hw/system/spatz_cluster`; the checks after the measured region can need more than 60 s):

```
(cd sw/build && cmake . && make test-vme-fp16-gathermlp-gcn_N127_F64_O128)
bin/spatz_cluster.vsim sw/build/vme/apps/test-vme-fp16-gathermlp-gcn_N127_F64_O128
```

Results against O-POPE: `runtime/opope_summary.md` (repository root).
