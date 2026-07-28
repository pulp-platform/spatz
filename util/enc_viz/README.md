# Encoding-space visualizer

`gen_encoding_viz.py` builds a single, self-contained HTML map of how the
RISC-V Vector (RVV) encoding space — and the adjacent custom major opcodes — is
used by this hardware. It is **read-only**: it never modifies the repo.

It answers four questions at a glance, checked against the RVV v1.0 ground
truth ([upstream `riscv-opcodes` `extensions/rv_v`](https://github.com/riscv/riscv-opcodes/blob/master/extensions/rv_v)):

1. **Which RVV v1.0 spec instructions does the hardware implement?**
   🟦 blue = decoded, ⬜ gray = not. *Implemented = the instruction is decoded
   by one of the decoders (snitch, spatz decoder, FPU sequencer, DMA
   front-end) — a decode check only; the logic behind the decoder is not
   verified.*
2. **Which custom instructions are hardware-backed?** One hue per custom
   extension group (same decode check).
3. **What is in the toolchain but not in hardware?** Instructions with a
   `riscv_instr.sv` localparam that no decoder references — shown ⬛ black in
   the grids *and* listed exhaustively in a dedicated section (reclaimable
   encoding space).
4. **Where does the implementation deviate from the spec?** A dedicated
   panel lists (a) encoding mismatches — same instruction name, different
   fixed bits vs the spec — and (b) custom instructions occupying
   spec-claimed encodings.

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
| upstream `riscv/riscv-opcodes` `extensions/rv_v` (auto-cloned, pinned commit) | the RVV v1.0 ground truth = the canvas (`--crypto` adds the `rv_zv*` exts) |
| `sw/toolchain/riscv-opcodes/opcodes-*` (the 7 custom entries in the Makefile `OPCODES` var) | our custom extensions |
| `sw/toolchain/riscv-opcodes/opcodes-rvv` (the 8th `OPCODES` entry) | diffed against the upstream canvas: entries absent upstream (e.g. `vlx*`, `vfwdotp`, legacy v0.9 leftovers) become the **`rvv (not in spec v1.0)`** pseudo-extension; same-name entries with different fixed bits are listed in the **spec-deviation panel** |
| `hw/ip/snitch/src/riscv_instr.sv` | the toolchain truth: "in toolchain" = has a localparam here (also cross-checked against the opcode files; a stale file triggers a warning) |
| the decoders under `hw/`: `snitch.sv`, `spatz_decoder.sv`, `spatz_fpu_sequencer.sv`, `axi_dma_tc_snitch_fe.sv` | what the hardware actually decodes |

"Implemented" = the instruction name is referenced in one of the decoder
files, either qualified (`riscv_instr::VADD_VV`) or unqualified (`FREP_O`,
when the file does `import riscv_instr::*`). This is a decode check only.

---

## Command-line options

```
python3 gen_encoding_viz.py [--repo DIR] [--out FILE] [--spec-commit SHA] [--crypto]
```

| Option | Default | Meaning |
|--------|---------|---------|
| `--repo DIR` | auto-detected | Spatz repo root (found by walking up from the script). |
| `--out FILE` | `encoding_map.html` next to the script | Where to write the HTML. |
| `--spec-commit SHA` | pinned commit | Which upstream `riscv-opcodes` commit to diff against. |
| `--crypto` | off | Also draw the ratified vector-crypto exts (`rv_zv*`) on the canvas (they claim OP-V slots too — e.g. Zvfbfwma's `vfwmaccbf16` sits on funct6 0x3b, which `vfwdotp` reuses). |
| `--decoder FILE.sv` | — | Treat an additional RTL file as a decoder (repeatable; extends the built-in list). |

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
| 🟦 blue | spec instruction the hardware decodes |
| ⬜ gray | spec instruction we do **not** implement |
| extension hue | custom instruction that **is** decoded |
| ⬛ black | in the toolchain (`riscv_instr.sv`) but decoded by no hardware (reclaimable) |
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
- **In toolchain but not decoded by hardware** — the exhaustive list of ⬛
  dead definitions, grouped per extension (collapsible), so reclaimable
  encoding space is visible at a glance rather than scattered across grids.
- **Active vs latent conflicts** — *active* = both sides decoded in hardware
  (real collision); *latent* = at least one side is a dead definition
  (paper collision only).
- **Deviations from the RVV v1.0 spec** — (a) *encoding mismatches*: same
  instruction name, different fixed bits between our `opcodes-rvv` and the
  spec (these deviations end up in the generated decoder masks); (b) *custom
  instructions on spec-claimed encodings*: a custom instruction's fixed bits
  overlap an RVV spec instruction.
- **Extension status table** — per extension: defined / implemented / dead.

Hover any cell or chip for the full 32-bit pattern, origin file, and the RTL
file(s) that decode it.

---

## Sharing the result

`encoding_map.html` is fully self-contained (all CSS/JS inlined, no external
requests), so you can open it locally, email it, or host it on any internal web
server / GitLab Pages — no dependencies required.

---

## Adding a new custom extension

The tool follows the build configuration automatically:

1. Add your `opcodes-xxx_CUSTOM` file and list it in the top-level Makefile
   `OPCODES` variable — the tool parses that variable each run and assigns the
   new extension a distinct hue automatically.
2. Run `make update_opcodes` — if you forget, the tool warns that
   `riscv_instr.sv` is stale.
3. If the new instructions are decoded by one of the known decoders
   (`snitch.sv`, `spatz_decoder.sv`, `spatz_fpu_sequencer.sv`,
   `axi_dma_tc_snitch_fe.sv`), nothing else to do. If your hardware adds a
   **new decoder file**, the tool detects instructions referenced outside the
   known decoders and prints/renders a warning — re-run with
   `--decoder your_decoder.sv` (repeatable) to include it in the scan.

## Notes & limitations

- "Implemented" is a decode check by design: the name is referenced in one of
  the decoder files. An instruction that is decoded but whose execution logic
  is broken/absent still reads as implemented — verifying the logic is out of
  scope for this tool.
- The standard-overloaded majors (OP / OP-IMM / OP-FP) are shown as lists, not
  grids — gridding them would require drawing base-ISA occupancy too.
- The upstream spec commit is pinned for reproducibility; bump `--spec-commit`
  (or edit `UPSTREAM_COMMIT` in the script) to track a newer spec.
