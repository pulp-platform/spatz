#!/usr/bin/env python3
# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

"""
KLayout-only helper: fill a JSON template with GDS geometry fields.

Usage (from shell):
  oseda klayout -b -r scripts/fill_config.py -rd infile=path/to/design.gds[.gz] -rd template=path/to/template.json -rd out=path/to/output.json

The script will open the GDS with pya, compute bounding box and dbu,
and populate common fields in the template under `gds` and `image` keys:
  - gds.width_um, gds.height_um, gds.x_offset_um, gds.y_offset_um
  - gds.width_dbu, gds.height_dbu, gds.dbu (µm per DBU)
  - image.width_px, image.height_px (if image size present, recomputed from dbu_p_px)
It writes the filled JSON to `out`.
"""
import json
import sys
import os
import pya

app = pya.Application.instance()
args = app.arguments()

def kv_from_args(tokens):
    kv = {}
    for t in tokens:
        if '=' in t and not t.startswith('-'):
            k, v = t.split('=', 1)
            kv[k] = v
    return kv

kv = kv_from_args(args)
infile = kv.get('infile')
# Default template to examples/picobello/cfg.json.in if not provided
template = kv.get('template')
out = kv.get('out')
top_override = kv.get('top')  # optional top cell override

if not template or not out:
    print('Usage: oseda klayout -b -r scripts/fill_config.py -rd infile=design.gds -rd template=tmpl.json -rd out=filled.json')
    raise SystemExit(1)

# Read template JSON first
with open(template, 'r') as f:
    data = json.load(f)

# If the template includes a GDS filename and a chip name, prefer the template's
# GDS file to avoid mixing templates for one chip with another GDS file.
templ_gds_file = None
if 'gds' in data and 'file' in data['gds']:
    templ_gds_file = data['gds']['file']
if 'general' in data and 'chip' in data['general'] and templ_gds_file:
    if infile and infile != templ_gds_file:
        print(f"Warning: template specifies GDS '{templ_gds_file}' for chip '{data['general']['chip']}' - ignoring provided infile '{infile}' to avoid mixing")
    infile = templ_gds_file

# infile still required now
if not infile:
    print('No GDS infile provided or found in template.gds.file')
    raise SystemExit(1)

# Load GDS and compute bbox
ly = pya.Layout()
ly.read(infile)
dbu = ly.dbu  # micron per database unit
tops = ly.top_cells()
if not tops:
    print(f'No top cells in {infile}')
    raise SystemExit(2)

# If the template indicates a top cell name, try to use that; otherwise use first
top_name = None
if top_override:
    top_name = top_override
elif 'general' in data and 'chip' in data['general']:
    top_name = data['general']['chip']
else:
    top_name = None

if top_name:
    for t in tops:
        try:
            name = getattr(t, 'name', None) or (t.name() if hasattr(t, 'name') else None)
        except Exception:
            name = None
        if name == top_name:
            selected = t
            break

selected = None

# If a top override or template-specified chip name exists, try to select it
if top_name:
    for t in tops:
        try:
            name = getattr(t, 'name', None) or (t.name() if hasattr(t, 'name') else None)
        except Exception:
            name = None
        if name == top_name:
            selected = t
            break

if selected is None:
    # No explicit top requested/found: use the first top cell
    selected = tops[0]

b = selected.bbox()
width_dbu = b.width()
height_dbu = b.height()
width_um = width_dbu * dbu
height_um = height_dbu * dbu
left_um = b.left * dbu
bottom_um = b.bottom * dbu

# Ensure general.chip is the selected top cell name
sel_name = None
try:
    sel_name = getattr(selected, 'name', None) or (selected.name() if hasattr(selected, 'name') else None)
except Exception:
    sel_name = None
if not sel_name:
    sel_name = str(selected.cell_index())
data.setdefault('general', {})
data['general']['chip'] = sel_name

# Fill gds fields in template, but only add keys that are missing to keep template shape
data.setdefault('gds', {})
g = data['gds']
g.setdefault('file', infile)
g.setdefault('width_um', round(width_um, 6))
g.setdefault('height_um', round(height_um, 6))
g.setdefault('x_offset_um', round(left_um, 6))
g.setdefault('y_offset_um', round(bottom_um, 6))

# Normalize the gds file path to an absolute path so downstream tools that
# prepend the work directory won't accidentally create duplicate path segments
# (e.g. /scratch/.../renderics/renderics/...). If the user intentionally
# provided an absolute path in the template, keep it.
if 'file' in g and g['file']:
    # If the path is already absolute, leave it. Otherwise convert to an
    # absolute path relative to the current working directory.
    if not os.path.isabs(g['file']):
        g['file'] = os.path.abspath(g['file'])

# Provide px dimensions handling per user request:
# - Accept px_width/px_height from -rd args
# - If only one provided, compute the other from GDS aspect ratio
# - If neither provided, default px_width=4000 and compute height
args_kv = kv
# accept either px_width or width_px etc.
def get_arg_px(name_variants):
    for n in name_variants:
        if n in args_kv:
            try:
                return int(args_kv[n])
            except Exception:
                return None
    return None

pxw_arg = get_arg_px(['px_width', 'width_px', 'pxwidth', 'width'])
pxh_arg = get_arg_px(['px_height', 'height_px', 'pxheight', 'height'])

data.setdefault('image', {})
img = data['image']

# Prefer existing values in template
pxw = img.get('px_width') or img.get('width_px') or img.get('pxwidth')
pxh = img.get('px_height') or img.get('height_px') or img.get('pxheight')

# Override with args if provided
if pxw_arg is not None:
    pxw = pxw_arg
if pxh_arg is not None:
    pxh = pxh_arg

# If neither present, default px_width=4000
if not pxw and not pxh:
    pxw = 4000

# compute missing dimension from aspect ratio (use um sizes)
if pxw and not pxh:
    # pxh = pxw * (height_um / width_um)
    if width_um > 0:
        pxh = int(round(pxw * (height_um / width_um)))
    else:
        pxh = pxw
elif pxh and not pxw:
    if height_um > 0:
        pxw = int(round(pxh * (width_um / height_um)))
    else:
        pxw = pxh

# Set back using template naming convention (prefer px_width / px_height)
img.setdefault('px_width', int(pxw))
img.setdefault('px_height', int(pxh))

# Work directory handling: allow overriding via -rd work.dir=... or variants.
# If provided on the command line, use that. Otherwise preserve any template
# value. If the template doesn't contain a work.dir, default to the current
# working directory where the script was invoked.
data.setdefault('work', {})
work = data['work']
# Do not allow overriding work.dir from the command line. Preserve any
# template value if present; otherwise default to the directory containing
# the output JSON (`out`). If `out` has no directory, fall back to cwd.
if 'dir' in work and work['dir']:
    # keep template-provided value
    pass
else:
    out_dir = os.path.dirname(out) or os.getcwd()
    if not out_dir:
        out_dir = os.getcwd()
    work['dir'] = out_dir

# Write filled JSON
with open(out, 'w') as f:
    json.dump(data, f, indent=2)

print(f'Wrote filled config to {out}')
sys.exit(0)
