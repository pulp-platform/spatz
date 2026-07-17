# Encoding-space visualizer

`gen_encoding_viz.py` builds a single, self-contained HTML map of how the
RISC-V Vector (RVV) encoding space — and the adjacent custom major opcodes — is
used by this hardware. It is **read-only**: it never modifies the repo.

It answers three questions at a glance:
- Which RVV spec instructions do we implement, and which don't we?
- Which encoding space do our custom extensions occupy (and how much is free)?
- Where do two extensions (or an extension and the spec) actually collide?

---

## Quick start

```sh
cd util/enc_viz
python3 gen_encoding_viz.py
# opens: util/enc_viz/encoding_map.html  (open it in any browser)
```

First run clones the upstream spec (~seconds, needs network); later runs are offline.

Re-run it any time the RTL, the opcode files, or `riscv_instr.sv` change — the
map always reflects the current sources.

---

## Requirements

- **Python 3** (standard library only — no pip installs).
- **git** and **network access on the first run** only, to fetch the upstream
  spec. It is cached under `util/enc_viz/.cache/` (git-ignored); subsequent runs
  are fully offline.

---

## What it reads

| Input | Role |
|-------|------|
| upstream `riscv/riscv-opcodes` (auto-cloned, pinned commit) | the full RVV spec = the canvas |
| `sw/toolchain/riscv-opcodes/opcodes-*` (the 7 custom entries in the Makefile `OPCODES` var; the 8th, `opcodes-rvv`, is the spec) | our custom extensions |
| `hw/ip/snitch/src/riscv_instr.sv` | the exact generated bit patterns |
| every `*.sv`/`*.svh` under `hw/` (except `riscv_instr.sv`) | what the hardware actually decodes/executes |

"Implemented / handled by HW" = the instruction name is referenced in the
decode/execute RTL, either qualified (`riscv_instr::VADD_VV`) or unqualified
(`FREP_O`, when a file does `import riscv_instr::*`).

---

## Command-line options

```
python3 gen_encoding_viz.py [--repo DIR] [--out FILE] [--spec-commit SHA] [--no-crypto]
```

| Option | Default | Meaning |
|--------|---------|---------|
| `--repo DIR` | auto-detected | Spatz repo root (found by walking up from the script). |
| `--out FILE` | `encoding_map.html` next to the script | Where to write the HTML. |
| `--spec-commit SHA` | pinned commit | Which upstream `riscv-opcodes` commit to diff against. |
| `--no-crypto` | off | Drop the ratified vector-crypto extensions from the spec canvas. |

Examples:
```sh
# write somewhere else
python3 gen_encoding_viz.py --out /tmp/enc.html

# run from anywhere, point at a specific checkout
python3 util/enc_viz/gen_encoding_viz.py --repo /path/to/spatz
```

---

## Reading the map

**Colors**
| Color | Meaning |
|-------|---------|
| 🟦 blue | spec instruction we implement |
| ⬜ gray | spec instruction we do **not** implement |
| extension hue | custom instruction that **is** decoded/executed |
| ⬛ black | custom instruction defined in the build but **dead** in hardware (reclaimable) |
| blue/gray two-tone | one slot holding both implemented and unimplemented sub-encodings |
| 🟥 red hatch | a **genuine** encoding conflict (two instructions' fixed bits overlap) |
| empty | free encoding space |

**Sections**
- **OP-V grid** — funct6 (rows) × category (columns) for major 0x57. A cell
  labeled `name +N` holds several legal sub-encodings that differ by
  `vm`/`vs1`/`vs2`/`rs1`; hover to see each one and its status.
- **Custom major grids** — CUSTOM-0/1/2/3 (0x0b/2b/5b/7b) as funct7 × funct3
  grids (collapsible; the summary line shows used/free slot counts). I-type
  immediate ops claim a whole funct3 column and are shown in a banner; free
  cells inside a claimed column are red-hatched.
- **Standard opcodes overloaded by custom ext** — rv32b (OP / OP-IMM),
  smallfloat (OP-FP / FMA), vector-crypto (0x77), listed so nothing is dropped.
- **Active vs latent conflicts** — *active* = both sides are live in hardware
  (real collision); *latent* = at least one side is a dead definition
  (paper collision only).
- **Extension status table** — per extension: defined / implemented / dead.

Hover any cell or chip for the full 32-bit pattern, origin file, and the RTL
file(s) that decode it.

---

## Sharing the result

`encoding_map.html` is fully self-contained (all CSS/JS inlined, no external
requests), so you can open it locally, email it, or host it on any internal web
server / GitLab Pages — no dependencies required.

---

## Notes & limitations

- "Implemented" is a strong proxy (name referenced in decode/execute RTL). A
  rare instruction that is decoded but silently never executed would still read
  as implemented.
- The standard-overloaded majors (OP / OP-IMM / OP-FP) are shown as lists, not
  grids — gridding them would require drawing base-ISA occupancy too.
- The upstream spec commit is pinned for reproducibility; bump `--spec-commit`
  (or edit `UPSTREAM_COMMIT` in the script) to track a newer spec.
