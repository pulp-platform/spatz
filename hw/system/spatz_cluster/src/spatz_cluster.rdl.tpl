// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
${disclaimer}

<%!
import math

def next_power_of_2(n):
    """Returns the next power of 2 greater than or equal to n."""
    return 1 if n == 0 else 2**math.ceil(math.log2(n))
%>

`ifndef __${cfg['name'].upper()}_RDL__
`define __${cfg['name'].upper()}_RDL__

`include "spatz_cluster_peripheral_reg.rdl"

addrmap ${cfg['name']} {

    default regwidth = ${cfg['dma_data_width']};
    mem tcdm {
        mementries = ${hex(int(cfg['tcdm']['size'] * 1024 * 8 / cfg['dma_data_width']))};
        memwidth = ${cfg['dma_data_width']};
    };

    mem bootrom {
        mementries = ${hex(int(4 * 1024 * 8 / cfg['dma_data_width']))};
        memwidth = ${cfg['dma_data_width']};
    };



    external tcdm                 tcdm            ;
    external bootrom              bootrom         @ ${hex(next_power_of_2(cfg['tcdm']['size']) * 1024)};
    spatz_cluster_peripheral_reg  peripheral_reg  @ ${hex((next_power_of_2(cfg['tcdm']['size'])+4) * 1024)};

};

`endif // __${cfg['name'].upper()}_RDL__
