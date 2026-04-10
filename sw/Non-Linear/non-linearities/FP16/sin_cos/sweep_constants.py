#!/usr/bin/env python3
"""Joint sweep of INV_PIO2 x PIO2_HI and all 4 constants."""
import numpy as np, struct, warnings
warnings.filterwarnings('ignore')

def hex_to_f16(h):
    return np.frombuffer(struct.pack("<H", h), dtype=np.float16)[0]

# Read the shuffled data we just generated
data_list = []
with open("data/data.h") as f:
    in_array = False
    for line in f:
        if 'data1_dram' in line:
            in_array = True; continue
        if in_array:
            if '};' in line: break
            for tok in line.replace(',', ' ').split():
                try: data_list.append(np.float16(float(tok)))
                except: pass

data_all = np.array(data_list, dtype=np.float16)
# Restrict to |x| <= 100 for meaningful mid-range analysis
data = np.array([x for x in data_all if abs(float(x)) <= 100], dtype=np.float16)
sin_ref = np.array([np.sin(float(x)) for x in data])
cos_ref = np.array([np.cos(float(x)) for x in data])
print(f"Test set: {len(data)} values with |x| <= 100")

def hw_sim(data, ip_h, ph_h, c2_h, s3_h):
    ip = hex_to_f16(ip_h); ph = hex_to_f16(ph_h)
    c2 = hex_to_f16(c2_h); s3 = hex_to_f16(s3_h)
    N = len(data)
    s_out = np.full(N, np.nan); c_out = np.full(N, np.nan)
    for i in range(N):
        x = np.float16(data[i])
        y = np.float16(x * ip)
        if np.isinf(y) or np.isnan(y): continue
        k = int(np.round(float(y)))
        kf = np.float16(k)
        r = np.float16(x - np.float16(kf * ph))
        if np.isinf(r) or np.isnan(r): continue
        z = np.float16(r * r)
        if np.isinf(z) or np.isnan(z): continue
        cp = np.float16(np.float16(1.0) + np.float16(c2 * z))
        sp = np.float16(r * np.float16(np.float16(1.0) + np.float16(s3 * z)))
        if np.isinf(cp) or np.isnan(cp) or np.isinf(sp) or np.isnan(sp): continue
        q = k & 3; b0 = q & 1; b1 = (q >> 1) & 1
        sm = float(sp) if b0==0 else float(cp)
        cm = float(cp) if b0==0 else float(sp)
        s_out[i] = (1-2*b1)*sm
        c_out[i] = (1-2*(b1^b0))*cm
    return s_out, c_out

def safe_mae(hw, ref):
    m = ~np.isnan(hw) & ~np.isinf(hw)
    return np.mean(np.abs(hw[m]-ref[m])) if m.sum()>0 else float('inf')

# Current baseline
sh, ch = hw_sim(data, 0x3917, 0x3E48, 0xB7F2, 0xB150)
curr_mae = safe_mae(ch, cos_ref) + safe_mae(sh, sin_ref)
print(f"\nCurrent: INV_PIO2=0x3917 PIO2_HI=0x3E48 COS_C2=0xB7F2 SIN_S3=0xB150")
print(f"  Combined MAE = {curr_mae:.6f}")

# Joint INV_PIO2 x PIO2_HI sweep (±3 ULP)
print(f"\n--- Joint INV_PIO2 x PIO2_HI sweep (±3 ULP) ---")
best_rr = (0x3917, 0x3E48)
best_rr_mae = curr_mae
for ip_off in range(-3, 4):
    for ph_off in range(-3, 4):
        iph = 0x3917 + ip_off
        phh = 0x3E48 + ph_off
        sh, ch = hw_sim(data, iph, phh, 0xB7F2, 0xB150)
        mae = safe_mae(ch, cos_ref) + safe_mae(sh, sin_ref)
        if mae < best_rr_mae:
            best_rr_mae = mae
            best_rr = (iph, phh)
        prod = float(hex_to_f16(iph)) * float(hex_to_f16(phh))
        tag = " <-- current" if ip_off==0 and ph_off==0 else (" *** BEST" if (iph,phh)==best_rr else "")
        if abs(ip_off) <= 2 and abs(ph_off) <= 2:
            print(f"  IP=0x{iph:04X}({float(hex_to_f16(iph)):.6f}) PH=0x{phh:04X}({float(hex_to_f16(phh)):.6f}) prod={prod:.6f} MAE={mae:.6f}{tag}")

print(f"\n  BEST RR: INV_PIO2=0x{best_rr[0]:04X} PIO2_HI=0x{best_rr[1]:04X} MAE={best_rr_mae:.6f}")
rr_improvement = (curr_mae - best_rr_mae) / curr_mae * 100
print(f"  Improvement over current: {rr_improvement:.2f}%")

# Now do full 4-constant sweep: best RR pair + ±3 C2/S3
print(f"\n--- Full 4-constant sweep (best RR pair + ±3 C2/S3) ---")
best4 = (best_rr[0], best_rr[1], 0xB7F2, 0xB150)
best4_mae = 1e30
for c2o in range(-3, 4):
    for s3o in range(-3, 4):
        c2h = 0xB7F2 + c2o; s3h = 0xB150 + s3o
        sh, ch = hw_sim(data, best_rr[0], best_rr[1], c2h, s3h)
        mae = safe_mae(ch, cos_ref) + safe_mae(sh, sin_ref)
        if mae < best4_mae:
            best4_mae = mae
            best4 = (best_rr[0], best_rr[1], c2h, s3h)

print(f"  BEST: IP=0x{best4[0]:04X} PH=0x{best4[1]:04X} C2=0x{best4[2]:04X} S3=0x{best4[3]:04X}")
print(f"  Combined MAE = {best4_mae:.6f}")
total_improvement = (curr_mae - best4_mae) / curr_mae * 100
print(f"  Total improvement over current: {total_improvement:.2f}%")

# Decode all constants
print("\nFinal decoded values:")
for name, h in [("INV_PIO2", best4[0]), ("PIO2_HI", best4[1]), ("COS_C2", best4[2]), ("SIN_S3", best4[3])]:
    v = float(hex_to_f16(h))
    curr_h = {"INV_PIO2": 0x3917, "PIO2_HI": 0x3E48, "COS_C2": 0xB7F2, "SIN_S3": 0xB150}[name]
    changed = " (CHANGED)" if h != curr_h else " (same)"
    print(f"  {name:12s} = 0x{h:04X} = {v:+.10f}{changed}")
