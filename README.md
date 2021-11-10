# Spatz

Spatz is a vector coprocessor based on the [RISC-V vector extension spec v1.0](https://github.com/riscv/riscv-v-spec/releases/tag/v1.0).

## Get Started

Make sure you clone this repository recursively to get all the necessary submodules:

```bash
git submodule update --init --recursive
```

If the repository path of any submodule changes, run the following command to change your submodule's pointer to the remote repository:

```bash
git submodule sync --recursive
```

## Dependencies

To install all dependencies at once run:

```bash
make
```

If there is only a need to compile code, you can install llvm and gcc using:

```bash
make toolchain
```

When simulatig RTL code, an installation of Bender is required to generate simulation scripts:

```bash
make bender
```

## Simulation

Simulation of Spatz can be started from the `hardware` directory by using the following command:

```bash
make sim
```

This compile the hardware, open QuestaSim, and simulate Spatz. If there is no need to look at the signal waves, a headless simulation can be startet by typing:

```bash
make simc
```
