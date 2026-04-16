// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Andrea Belano <andrea.belano2@unibo.it>
// Edited by: Riccardo Fiorani Gallotta <riccardo.fiorani3@unibo.it>

config tb_picobello_chip_config;
    design tb_picobello_chip;

    `ifdef NETS_LIB
    default liblist work nets_lib;
    `endif

    `ifdef PICOBELLO_CHIP_NET
    instance `PICOBELLO_CHIP_NET use nets_lib.picobello_chip;
    `endif

    `ifdef CLUSTER_TILE_NET_0
    instance `CLUSTER_TILE_NET_0 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_1
    instance `CLUSTER_TILE_NET_1 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_2
    instance `CLUSTER_TILE_NET_2 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_3
    instance `CLUSTER_TILE_NET_3 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_4
    instance `CLUSTER_TILE_NET_4 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_5
    instance `CLUSTER_TILE_NET_5 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_6
    instance `CLUSTER_TILE_NET_6 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_7
    instance `CLUSTER_TILE_NET_7 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_8
    instance `CLUSTER_TILE_NET_8 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_9
    instance `CLUSTER_TILE_NET_9 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_10
    instance `CLUSTER_TILE_NET_10 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_11
    instance `CLUSTER_TILE_NET_11 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_12
    instance `CLUSTER_TILE_NET_12 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_13
    instance `CLUSTER_TILE_NET_13 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_14
    instance `CLUSTER_TILE_NET_14 use nets_lib.cluster_tile;
    `endif

    `ifdef CLUSTER_TILE_NET_15
    instance `CLUSTER_TILE_NET_15 use nets_lib.cluster_tile;
    `endif

    `ifdef CHESHIRE_TILE_NET
    instance `CHESHIRE_TILE_NET use nets_lib.cheshire_tile;
    `endif

    `ifdef MEM_TILE_NET_0
    instance `MEM_TILE_NET_0 use nets_lib.mem_tile;
    `endif

    `ifdef MEM_TILE_NET_1
    instance `MEM_TILE_NET_1 use nets_lib.mem_tile;
    `endif

    `ifdef MEM_TILE_NET_2
    instance `MEM_TILE_NET_2 use nets_lib.mem_tile;
    `endif

    `ifdef MEM_TILE_NET_3
    instance `MEM_TILE_NET_3 use nets_lib.mem_tile;
    `endif

    `ifdef MEM_TILE_NET_4
    instance `MEM_TILE_NET_4 use nets_lib.mem_tile;
    `endif

    `ifdef MEM_TILE_NET_5
    instance `MEM_TILE_NET_5 use nets_lib.mem_tile;
    `endif

    `ifdef MEM_TILE_NET_6
    instance `MEM_TILE_NET_6 use nets_lib.mem_tile;
    `endif

    `ifdef MEM_TILE_NET_7
    instance `MEM_TILE_NET_8 use nets_lib.mem_tile;
    `endif

    `ifdef FHG_SPU_TILE_NET
    instance `FHG_SPU_TILE_NET use nets_lib.fhg_spu_tile;
    `endif

endconfig
