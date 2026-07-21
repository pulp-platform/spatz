---
name: encoding-viz
description: Generate or port the RISC-V encoding-space visualizer — an HTML map showing spec coverage (decoded vs not), hardware-backed custom instructions, toolchain-only dead definitions, and spec deviations. Use when asked to visualize/audit instruction encodings, check ISA-spec alignment, find reclaimable encoding space, or set up the tool in another hardware repo.
---

# Encoding-space visualizer

`util/enc_viz/gen_encoding_viz.py` (stdlib-only Python 3, read-only) renders a
self-contained `encoding_map.html` answering four questions:

1. Which **spec** instructions does the hardware decode (blue) vs not (gray)?
2. Which **custom** instructions are hardware-backed (one hue per extension)?
3. What is **in the toolchain but not decoded** (black + dedicated section)
   — i.e. reclaimable encoding space?
4. Where does the implementation **deviate from the spec** (same-name
   encoding mismatches; custom instructions on spec-claimed encodings)?

## Using it in this repo

```sh
cd util/enc_viz && python3 gen_encoding_viz.py          # -> encoding_map.html
python3 gen_encoding_viz.py --crypto                    # + rv_zv* on the canvas
python3 gen_encoding_viz.py --decoder new_decoder.sv    # extend decoder scan
```

- First run clones upstream `riscv/riscv-opcodes` into `.cache/` (needs network
  once); later runs are offline. Pinned via `UPSTREAM_COMMIT`.
- The enabled custom extensions are parsed **live from the top-level Makefile
  `OPCODES` variable** — after enabling/disabling an extension or running
  `make update_opcodes`, just re-run the script.
- If the console warns that an RTL file references instructions no scanned
  decoder handles, a new decoder file exists: re-run with `--decoder FILE.sv`.
- If it warns `riscv_instr.sv is stale`, run `make update_opcodes` first.

Read `util/enc_viz/README.md` for the full option table and map legend.

## Core semantics (preserve these when porting)

- **Ground truth** = a machine-readable spec opcode file (upstream
  `riscv-opcodes/extensions/rv_v` here), never a hand-copied list.
- **"Implemented" = decode check only**: the instruction name (as generated in
  the instruction package, e.g. `VADD_VV`) appears as a whole-word token in an
  explicit allowlist of decoder RTL files (`DECODER_FILES`). Do not scan all
  RTL — functional units referencing an op would create false positives.
  Whole-word matching against the candidate set + requiring the file to
  reference the instruction package makes unqualified matches safe.
- **"In toolchain"** = has a localparam in the generated instruction package
  (`riscv_instr.sv`), which is also cross-checked against the opcode files so
  stale generation is flagged instead of silently trusted.
- **Local-spec drift**: the project's local copy of the spec opcode file is
  diffed against upstream. Name-absent-upstream entries become a
  "not in spec" pseudo-extension (they are usually custom instructions or
  legacy leftovers hiding in the spec file); same-name-different-bits entries
  go to the mismatch panel.
- **Grid placement**: an instruction occupies exactly the funct7 rows its
  fixed bits allow (`(f7 & mask7) == match7`) — an op using funct7 bits as
  immediate/operand fills its whole column, a funct6-only op fills 2 rows.
  Never model partial-funct7 ops as "column claimed"; compute row
  compatibility.
- **Conflicts**: pairwise fixed-bit overlap across different origins, split
  into *active* (both decoded) vs *latent* (≥1 dead). Red is reserved for
  genuine bit overlaps only.

## Porting checklist (new hardware project)

Copy `util/enc_viz/` (script + README), then adapt the constants at the top of
the script — everything project-specific is configuration, not logic:

1. **Spec canvas**: `UPSTREAM_URL` / `UPSTREAM_COMMIT` / `SPEC_FILES` — which
   upstream extension files are the ground truth for this project (e.g.
   `rv_v`, or `rv_i` + `rv_m`, …).
2. **Local opcode files**: path in `main()` (`sw/toolchain/riscv-opcodes`
   here) and `RVV_LOCAL_FILE` (the local spec copy to diff). How is the
   enabled set declared? Adapt `parse_makefile_opcodes` (`_OPCODES_RE`) to the
   project's build variable, keep `EXT_FILES_FALLBACK` as the manual fallback.
3. **Generated instruction package**: `sv_path` (this repo:
   `hw/ip/snitch/src/riscv_instr.sv`) and, if the format differs, the
   `_SV_RE` regex in `parse_riscv_instr_sv`.
4. **Decoders**: `DECODER_FILES` — find them empirically: scan all RTL for
   whole-word instruction-name references and list the files that decode
   (case/casez on the instruction), not the FUs that consume decoded ops. The
   built-in "referenced outside decoders" warning will catch any you miss.
5. **Repo detection**: `find_repo()` walks up looking for a landmark path —
   change the marker directory.
6. **Grids**: `CUSTOM_MAJORS`, `FUNCT3_CAT`/`CATEGORY_ORDER` (OP-V-specific),
   and `MAJOR_NAMES` if the project overloads different major opcodes. Drop
   the OP-V grid entirely for non-vector projects; the funct7×funct3 custom
   grids and list sections are generic.
7. **Colors**: curate `EXT_COLORS` for the known extensions; unknown ones get
   `EXTRA_HUES` automatically.

## Verifying a port (or any substantial change)

- Cross-check the printed counts against ground truth: spec instr count must
  equal the upstream file's instruction count; `toolchain localparams` must
  match the generated package; `0 missing / 0 drift` on the consistency check.
- Simulate an end-to-end change in a throwaway fake repo (temp dir with a
  minimal Makefile, opcode files, instruction package, and a fake decoder
  referencing one of two new instructions): the map must show one live, one
  dead, and — without `--decoder` — warn about the unscanned decoder file.
- Sanity-check one known instruction per category in the HTML: a decoded spec
  op (blue), a decoded custom op (its hue), a known-dead definition (black),
  and any known deviation (mismatch panel).
