#!/usr/bin/env python3
# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

from PIL import Image, ImageChops
import numpy as np
import argparse
import math
import os

def parse_args():
    parser = argparse.ArgumentParser(description='Apply color overlay to an image. Outputs are saved next to the input as <name>_soft<ext> and <name>_overlay<ext>.')
    parser.add_argument('input', help='Input image file')
    parser.add_argument('-a', '--angle', type=float, default=0.0,
                        help='Gradient angle in degrees: 0 = left→right, 90 = top→bottom')
    parser.add_argument('--strength', type=float, default=0.6,
                        help='Effect amount for both outputs (0..1). 1 = full effect, 0 = original')
    return parser.parse_args()

args = parse_args()
die = Image.open(args.input).convert("RGB")
W, H = die.size  # Get dimensions from input image

# Derive output paths in the same directory as input
in_dir, in_name = os.path.split(args.input)
stem, ext = os.path.splitext(in_name)
if ext == "":
    ext = ".png"
out_soft_path = os.path.join(in_dir or ".", f"{stem}_soft{ext}")
out_overlay_path = os.path.join(in_dir or ".", f"{stem}_overlay{ext}")

# Build a 0..1 ramp along the requested angle
# Angle convention: 0° = left→right (along +x), 90° = top→bottom (along +y)
theta = math.radians(args.angle)
ux, uy = math.cos(theta), math.sin(theta)  # unit direction

# Coordinates (x to the right, y downwards)
x = np.arange(W, dtype=np.float32)
y = np.arange(H, dtype=np.float32)
xx, yy = np.meshgrid(x, y, indexing='xy')

# Project pixel coordinates onto direction (ux, uy)
t = xx * ux + yy * uy

# Normalize projection so that the gradient spans the whole image
corners = np.array([[0, 0], [W - 1, 0], [0, H - 1], [W - 1, H - 1]], dtype=np.float32)
proj = corners @ np.array([ux, uy], dtype=np.float32)
pmin, pmax = float(np.min(proj)), float(np.max(proj))
den = (pmax - pmin) if (pmax - pmin) != 0 else 1.0
ramp = np.clip((t - pmin) / den, 0.0, 1.0)

# 5-stop colors in sRGB
stops = [
    (0.00, np.array([0xFF,0x5A,0xA5], dtype=np.float32)),  # pink
    (0.25, np.array([0xFF,0xE1,0x6B], dtype=np.float32)),  # yellow
    (0.50, np.array([0x84,0xE3,0x5B], dtype=np.float32)),  # green
    (0.75, np.array([0x5A,0xA7,0xFF], dtype=np.float32)),  # blue
    (1.00, np.array([0x8A,0x2B,0xE2], dtype=np.float32)),  # purple
]

# Piecewise-linear interpolation over stops for the whole 2D ramp
gradient = np.zeros((H, W, 3), dtype=np.float32)
for (p0, c0), (p1, c1) in zip(stops[:-1], stops[1:]):
    seg = (ramp >= p0) & (ramp <= p1)
    if not np.any(seg):
        continue
    a = (ramp[seg] - p0) / (p1 - p0)
    # Interpolate colors per pixel in this segment
    gradient[seg] = (1.0 - a)[:, None] * c0 + (a)[:, None] * c1

grad_img = Image.fromarray(gradient.astype(np.uint8), mode="RGB")

def _clamp01(x: float) -> float:
    return 0.0 if x <= 0.0 else 1.0 if x >= 1.0 else x

strength = _clamp01(float(args.strength))

# Blend modes
# Softlight-ish via simple alpha blend
out = Image.blend(die, grad_img, strength)  # simple crossfade

# Or use multiply/screen combo to emulate Overlay, then attenuate
overlay_raw = ImageChops.overlay(die, grad_img)  # Pillow >= 9.1
overlay = Image.blend(die, overlay_raw, strength)
overlay.save(out_overlay_path)
out.save(out_soft_path)
