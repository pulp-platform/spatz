# Physical Design (PD) repository of the *picobello* project

This repository contains all the technology related files and scripts for the physical implementation of the *picobello* chip. The files in this repository are proprietary and are not available for public use.

## ⭐ Getting started

This repository can only be used in conjunction with the *picobello* repository. The `picobello-pd` repository is intended to be cloned into the `picobello` repository as a submodule, which can be done by running the following command in the `picobello` root directory:

```bash
make init-pd
```

Then, you have to initialize the cockpit structure with:

```bash
make cockpit
```
## 🧪 Platform Simulation
The Picobello simulation flow currently only supports Questasim.
To build the RTL code, do:

```bash
make vsim-compile-chip
```

Tests can be executed by setting all the required command-line variable for Cheshire, see the [Cheshire Docs](https://pulp-platform.github.io/cheshire/gs/) for more details.
To run a simple Chehire helloworld in Picobello, do the following:

```bash
make vsim-run-chip CHS_BINARY=sw/cheshire/tests/helloworld.spm.elf
```
To run an offloading example test for Snitch, do:

```bash
make vsim-run-chip CHS_BINARY=sw/cheshire/tests/simple_offload.spm.elf SN_BINARY=sw/snitch/tests/build/simple.elf
```

Use the `vsim-run-batch-chip` command to run tests in batch mode with RTL optimizations to reduce the Questasim runtime.

## 🔬 Elaboration

The elaboration can be done for each of the tiles with the following command:

```bash
make elab-<my_tile>
```

This will elaborate the entire `picobello_chip` design, but set the specified `<my_tile>` as the top module.

## 🏗️ Place and Route

There is a unique PnR flow for all the blocks in `block_pnr/pnr.tcl`. Block-specific scripts are located in the `block_pnr/<my_tile>` directories. The PnR flow can be run for a specific block by running the following command:

```bash
make pnr-<my_tile>
```

This will set the `DESIGN` tcl variable to the specified `<my_tile>`, and source the scripts from the appropriate block directory.

Every run will create new directories for the NDM, reports, and output files, in `fusion/save`, `fusion/reports` and `fusion/out` respectively. All the directories will be appended with the timestamp of the start of the run. A symlink called `<my_tile>_latest` will be created for all the directories, pointing to the latest run. This way, you can always access the latest run without having to remember the timestamp. Furthermore, you can also pass a description to the run to create additional symlinks:

```bash
make pnr-<my_tile> RUN_DESC="some_new_trial"
```

### Modes

We have two differet _modes_ to run the PnR flow, which can be selected with the `MODE` variable:

- `func`: The default mode, which uses a 1GHz clock.
- `fast`: A faster mode, which uses a 1.2GHz clock.

If you want to run the PnR flow in the `fast` mode, you can pass the `MODE` variable to the make command:

```bash
make pnr-<my_tile> MODE=fast
```

This needs to be done in the beginning of a flow, and it will activate the proper scenarios in the sourced `active_scenarios.tcl` script.

### 📍 Checkpointing

The place and route scripts make use of the `checkpoint` feature of Fusion compiler, which allows to wrap stages with actions before and after each stage, or even replace them entirely. Fusion Compiler automatically checks during start up for a `checkpoint.config.tcl` file in the current working directory. There is a default script in `fusion/scripts/checkpoint.config.pnr.tcl`, which can be symlinked or copied to each tile directory. The feature is explained in more detail in the default script. Currently, it supports the following actions:
- `save_block` - a list of stages after which the block is saved.
- `pause` - Pause after a list of stages.
- `start` - Start from a specific stage.
- `exit` - Exit after a specific stage.
- `skip` - Skip a list of stages.
- `notify` - Notify per email after a stage has completed.
- `load_from_label` - Load the library from a specified label before starting the run.

The checkpointing can be configured over the make command by passing variables. For instance, you could start a run like this:

```bash
make pnr-<my_tile> RUN_NAME=my_previous_run FROM_LABEL=powergrid FROM_STAGE=pin_placement TO_STAGE=placement
```
or a combination of the above flags. `RUN_NAME` is the name of the previous run, which you need to provide if you want to run the flow from a previous run. For instance, you could specify:

```bash
make pnr-<my_tile> RUN_NAME=my_tile_latest FROM_STAGE=placement
```
which will load the block from the latest run with the label of the stage _before_ `placement`. If the stage before the specified stage did not save a block, you can also specify the label explicitely with `FROM_LABEL`.

### 🤝🏻 Handoff

The hierarchical top-down flow that we are using suports two different flavors of the handoff (i.e. the way information is passed from the design planning to the block-level PnR flow):

- **NDM**:  In the *NDM* handoff, the block-level PnR flow will use the actual NDM database that was created during the top-down flow, which already includes mapped netlist, floorplan, constraints etc., Some things need to be redone at the moment, such as the placement of macros/std cells as well as the powergrid. The rest of the PnR flow then continues with the placement i.e. `compile_fusion -from initial_place` stages since the design is already mapped/synthesized.

- **RTL**: In the *RTL* handoff, the block-level PnR flow will start from the RTL itself, meaning it will create a new NDM database from scratch, elaborate the RTL, apply constraints, and do the initial synthesis (i.e. `compile_fusion -to logic_opto` stages). This flow is useful for the initial trials of the design, where the RTL still changes a lot, and the NDM database is not yet available.

- **Bottom-up**: In the *bottom-up* handoff, the block-level PnR flow, there is no information passed from the top-down design planning constraints, floorplan etc., need to be defined manually. This is mainly useful for backend trials to skip the design planning stage (it was not used for the tapeout).

Those handoff flavors skip specific stages in the PnR flow, which is also handled with the checkpointing feature. The handoff flavor can be selected from the make command by passing the `HANDOFF` variable with the default being `ndm`:

```bash
make pnr-<my_tile> HANDOFF=rtl
```

### 📦 Export (netlists, GDS etc.,)

You can export the design after the PnR flow is finished with the following command:

```bash
make export-<my_tile> RUN_NAME=<my_tile_run>
```

This will generate the netlist, LVS netlist, GDSII file, and SDF file for the specified `<my_tile_run>`. The results will be saved in the `fusion/out/<my_tile_run>` directory. Hint use the `FROM_LABEL` variable to export from a specific saved block, e.g. `FROM_LABEL=manual_fixes`.

### 🚚 Delivery

To deliver the NDM database for the top-level PnR flow, you can use the following command:

```bash
make delivery-<my_tile> RUN_NAME=<my_tile_run>
```

Again, use the `FROM_LABEL` variable to specify a specific saved block, e.g. `FROM_LABEL=manual_fixes`.

## 🏁 Signoff DRC

### Calibre

To run the signoff DRC with Calibre, you can use the following target:

```bash
make drc-<my_tile> RUN_NAME=<my_tile_run>
```

This will run the DRC with the GDSII file in the `fusion/out/<my_tile_run>/<my_tile>.gds.gz` directory, and the results will be saved in the `calibre/drc/out/<my_tile_run>` directory. The `RUN_NAME` variable is optional, and if not provided, it will use the latest run i.e. `<my_tile>_latest`.

#### Import Calibre DRCs back into Fusion
To import the Calibre results into Fusion, you can use the following command:

```bash
fc_shell> import_calibre_drcs
```
which will check the report from the corresponding `calibre/drc/out/<my_tile_run>` directory. You can then open the results in Fusion with the error browser.

#### Use Realtime Calibre in Fusion

Fusion supports the realtime DRC with Calibre, which allows you to run the signoff DRC in Fusion itself. This is very handy to quickly check smaller areas for signoff DRC violations. To start Fusion with the realtime DRC enabled, you can use the following command:

```bash
make start-fusion RT_CALIBRE=ON
```

which will initialize Fusion Compiler properly.

### IC Validator

> [!caution]
> This is not yet working properly, since it still reports a lot of false positives, especially within SRAM macros.

Synopsys has their own signoff DRC tool called IC Validator, which is also natively supported in Fusion Compiler. The environment is automatically setup when running `make start-fusion`. However, some additional options need to be set for which there is a helper target:

```bash
# A block with technology information needs to be opened first
fc_shell> open_block save/my_tile_latest:my_tile/some_label
fc_shell> init_icv
```
Be aware that `init_icv` command will save the block! Because signoff DRC checking is only possible from the disk space and not from memory. Then you can run signoff DRC checking with:

```bash
fc_shell> signoff_check_drc
```

and subsequently, fix the DRC violations with:

```bash
fc_shell> signoff_fix_drc
```

#### Live IC Validator
Fusion Compiler has the live drc checking features akin to realtime Calibre. You can start it from the GUI `ICV` or with the following command:

```bash
fc_shell> signoff_check_drc_live
```

## 🌝 Signoff LVS

To run the signoff LVS with Calibre, you can use the following target:

```bash
make lvs-<my_tile> RUN_NAME=<my_tile_run>
```

This requires to have an exported GDS and LVS netlist, which is done automatically during the `export` stage of the PnR flow. The target will convert the LVS netlist `fusion/out/<my_tile_run>/<my_tile>.lvs.v.gz`to a spice netlist with the `verilog2spice` script, and the run Calibre LVS together with the GDSII file `fusion/out/<my_tile_run>/<my_tile>.gds.gz`. The results will be saved to `calibre/lvs/out/<my_tile_run>/<my_tile>.lvs.report`. The `RUN_NAME` variable is optional, and if not provided, it will use the latest run i.e. `<my_tile>_latest`.

## 🧙🏻 Additional Helper targets

To open Fusion compiler, you can run the following command:

```bash
make start-fusion
```

If you want Fusion to set design specific variables for manual work on the design, you can additionally pass the `DESIGN` variable:

```bash
make start-fusion DESIGN=<my_tile>
```

Furthermore, with an open library, you can redefine the global design variables:

```tcl
fc_shell> open_block save/my_tile_latest:my_tile/some_label
fc_shell> redefine_global_design_vars
```

This will redefine `DESIGN`, `LIBDIR`, `OUTDIR`, `REPDIR`

## Running post-synthesis and post-layout simulations

To compile a simlation using the post-layout netlists from the delivery directory, run:

```bash
make vsim-compile-chip NETLISTS=<netlist list>
```

The possible values for `NETLISTS` are the block tiles (`cluster_tile`, `mem_tile`, ...) and/or `picobello_chip`.

You can also choose how many `cluster_tile` and `mem_tile` starting from the number 0 are replaced by the netlists using `N_NET_CLUSTER` and `N_NET_MEM` (by default both at 1). If you set `N_NET_MEM=3` the `mem_tile` 0 to 2 will be replaced (you need also to set `NETLISTS=mem_tile`).

To compile a simulation using the post-synthesis netlists from the handoff directory, run:

```bash
make vsim-compile-chip NETLISTS=<netlist list> FUSION_RUN=HANDOFF
```

The other possible values for `FUSION_RUN` are `DELIVERY` (default) or `<run name>` (like `latest` or a timestamp) using the netlists produced with the checkpoint `export` of the PnR flow locally.

By default the simulaton will be optimised with `+acc` option. To disable it use `ACC=OFF`.

To insted setup a sdf-annotated simulation use `SDF=ON`. It will force the `+acc` optimization option. The backannotation it's not avaible with `FUSION_RUN=HANDOFF`.

You can also force the compiler to use a path to a netlist and/or a sdf file using `NETLISTS_FILES` and/or `SDF_FILES`. To associate the file to a design you must follow the same order in `NETLISTS`. If you don't want to set a file for a design use a dot. For example:

```bash
make vsim-compile-chip NETLISTS="mem_tile fhg_spu_tile" SDF=ON NETLISTS_FILES=path/to/mem_tile/netlist.v SDF_FILES=". path/to/fhg_spu_tile/sdf_file.sdf"
```

After the design has been compiled, you can start a simulation by running, for example:

```bash
make vsim-run-chip CHS_BINARY=<path to binary> SN_BINARY=<path to binary>
```

## Dump VCD

To dump a VCD file you need to compile using the option `VCD=ON`. It will force the `+acc` optimization option. For example:

```bash
make vsim-compile-chip NETLISTS=<netlist list> VCD=ON
```

After the design has been compiled, you can start the simulation again with the option `VCD=ON`. By default it will dump only the VCD for the designs defined in `NETLISTS` during compilation.
If you don't set `VCD_START` and/or `VCD_DURATION` the dump will start a `0ns` and ends when the simulation stops itself.

For example to dump from `10ns` to `110ns` use:

```bash
make vsim-run-chip CHS_BINARY=<path to binary> SN_BINARY=<path to binary> VCD=ON VCD_START=10ns VCD_DURATION=100ns
```

The VCD files are dumped into `/pd/tsmc7/vcd/<fusion run>/dump.vcd`.

## 🖼️ Generating high-quality GDS rendering with ArtistIC

First you have to fetch the artistic submodule:

```bash
git submodule update --init --recursive
```

Then you can generate high-quality images of the GDS files with the following command:

```bash
make render-<my_tile>
```

Which will generate a PNG/PDF files under `util/out`. The coloring scheme can be adjusted in the `util/artistic_cfg.json.in` file (for additional information check the [ArtistIC documentation](https://github.com/pulp-platform/artistic/blob/main/README.md)). The config file will be used as a template for an additional script `util/artistic/gen_artistic_cfg.py` to insert additional information such as GDS path, dimensions etc., By default, it will use `4000px` of width for the image, but you can also override this by passing the `PX_WIDTH` variable:

```bash
make render-<my_tile> PX_WIDTH=3000
```
