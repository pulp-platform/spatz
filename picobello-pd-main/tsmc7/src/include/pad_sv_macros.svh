// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Tim Fischer <fischeti@iis.ee.ethz.ch>

// TODO(fischeti): Expose RTE signal if multiple rte signals are needed
`define DEFAULT_RTE rte

`define TSMC7_IO_PAD_TEMPLATE(orient, pad_name, pad_signal, pad2core_signal, core2pad_signal, ctrl, rte_signal=`DEFAULT_RTE) \
  PDDWUWSWEWCDGS_``orient i_``pad_name ( \
    .C(pad2core_signal),                 \
    .DS0(ctrl.ds[0]),                    \
    .DS1(ctrl.ds[1]),                    \
    .DS2(ctrl.ds[2]),                    \
    .DS3(ctrl.ds[3]),                    \
    .I(core2pad_signal),                 \
    .IE(ctrl.ie),                        \
    .OE(ctrl.oe),                        \
    .PAD(pad_signal),                    \
    .PU(ctrl.pu),                        \
    .PD(ctrl.pd),                        \
    .ST(ctrl.st),                        \
    .RTE(rte_signal)                     \
  );

`define TSMC7_INPUT_IO_PAD_H(pad, pad2core_signal, ctrl) \
  `TSMC7_IO_PAD_TEMPLATE(H, pad, pad, pad2core_signal, 1'b0, ctrl)

`define TSMC7_INPUT_IO_PAD_V(pad, pad2core_signal, ctrl) \
  `TSMC7_IO_PAD_TEMPLATE(V, pad, pad, pad2core_signal, 1'b0, ctrl)

`define TSMC7_OUTPUT_IO_PAD_H(pad, core2pad_signal, ctrl) \
  `TSMC7_IO_PAD_TEMPLATE(H, pad, pad, , core2pad_signal, ctrl)

`define TSMC7_OUTPUT_IO_PAD_V(pad, core2pad_signal, ctrl) \
  `TSMC7_IO_PAD_TEMPLATE(V, pad, pad, , core2pad_signal, ctrl)

`define TSMC7_INOUT_IO_PAD_H(pad, pad2core_signal, core2pad_signal, ctrl) \
  `TSMC7_IO_PAD_TEMPLATE(H, pad, pad, pad2core_signal, core2pad_signal, ctrl)

`define TSMC7_INOUT_IO_PAD_V(pad, pad2core_signal, core2pad_signal, ctrl) \
  `TSMC7_IO_PAD_TEMPLATE(V, pad, pad, pad2core_signal, core2pad_signal, ctrl)

`define TSMC7_INPUT_IO_PAD_ARRAY_H(pad, pad2core_signal, ctrl, num_signals) \
  for (genvar ii = 0; ii < num_signals; ii++) begin \
    `TSMC7_IO_PAD_TEMPLATE(H, pad, pad[ii], pad2core_signal[ii], 1'b0, ctrl) \
  end

`define TSMC7_INPUT_IO_PAD_ARRAY_V(pad, pad2core_signal, ctrl, num_signals) \
  for (genvar ii = 0; ii < num_signals; ii++) begin \
    `TSMC7_IO_PAD_TEMPLATE(V, pad, pad[ii], pad2core_signal[ii], 1'b0, ctrl) \
  end

`define TSMC7_OUTPUT_IO_PAD_ARRAY_H(pad, core2pad_signal, ctrl, num_signals) \
  for (genvar ii = 0; ii < num_signals; ii++) begin \
    `TSMC7_IO_PAD_TEMPLATE(H, pad, pad[ii], , core2pad_signal[ii], ctrl) \
  end

`define TSMC7_OUTPUT_IO_PAD_ARRAY_V(pad, core2pad_signal, ctrl, num_signals) \
  for (genvar ii = 0; ii < num_signals; ii++) begin \
    `TSMC7_IO_PAD_TEMPLATE(V, pad, pad[ii], , core2pad_signal[ii], ctrl) \
  end

`define TSMC7_INOUT_IO_PAD_ARRAY_H(pad, pad2core_signal, core2pad_signal, ctrl, num_signals) \
  for (genvar ii = 0; ii < num_signals; ii++) begin \
    `TSMC7_IO_PAD_TEMPLATE(H, pad, pad[ii], pad2core_signal[ii], core2pad_signal[ii], ctrl) \
  end

`define TSMC7_INOUT_IO_PAD_ARRAY_V(pad, pad2core_signal, core2pad_signal, ctrl, num_signals) \
  for (genvar ii = 0; ii < num_signals; ii++) begin \
    `TSMC7_IO_PAD_TEMPLATE(V, pad, pad[ii], pad2core_signal[ii], core2pad_signal[ii], ctrl) \
  end

`define TSMC7_INPUT_IO_PAD_2DARRAY_H(pad, pad2core_signal, ctrl, dim_one, dim_two) \
  for (genvar ii = 0; ii < dim_one; ii++) begin \
    for (genvar jj = 0; jj < dim_two; jj++) begin \
    `TSMC7_IO_PAD_TEMPLATE(H, pad, pad[ii][jj], pad2core_signal[ii][jj], 1'b0, ctrl) \
    end \
  end

`define TSMC7_INPUT_IO_PAD_2DARRAY_V(pad, pad2core_signal, ctrl, dim_one, dim_two) \
  for (genvar ii = 0; ii < dim_one; ii++) begin \
    for (genvar jj = 0; jj < dim_two; jj++) begin \
    `TSMC7_IO_PAD_TEMPLATE(V, pad, pad[ii][jj], pad2core_signal[ii][jj], 1'b0, ctrl) \
    end \
  end

`define TSMC7_OUTPUT_IO_PAD_2DARRAY_H(pad, core2pad_signal, ctrl, dim_one, dim_two) \
  for (genvar ii = 0; ii < dim_one; ii++) begin \
    for (genvar jj = 0; jj < dim_two; jj++) begin \
    `TSMC7_IO_PAD_TEMPLATE(H, pad, pad[ii][jj], , core2pad_signal[ii][jj], ctrl) \
    end \
  end

`define TSMC7_OUTPUT_IO_PAD_2DARRAY_V(pad, core2pad_signal, ctrl, dim_one, dim_two) \
  for (genvar ii = 0; ii < dim_one; ii++) begin \
    for (genvar jj = 0; jj < dim_two; jj++) begin \
    `TSMC7_IO_PAD_TEMPLATE(V, pad, pad[ii][jj], , core2pad_signal[ii][jj], ctrl) \
    end \
  end

`define TSMC7_INOUT_IO_PAD_2DARRAY_H(pad, pad2core_signal, core2pad_signal, ctrl, dim_one, dim_two) \
  for (genvar ii = 0; ii < dim_one; ii++) begin \
    for (genvar jj = 0; jj < dim_two; jj++) begin \
    `TSMC7_IO_PAD_TEMPLATE(H, pad, pad[ii][jj], pad2core_signal[ii][jj], core2pad_signal[ii][jj], ctrl) \
    end \
  end

`define TSMC7_INOUT_IO_PAD_2DARRAY_V(pad, pad2core_signal, core2pad_signal, ctrl, dim_one, dim_two) \
  for (genvar ii = 0; ii < dim_one; ii++) begin \
    for (genvar jj = 0; jj < dim_two; jj++) begin \
    `TSMC7_IO_PAD_TEMPLATE(V, pad, pad[ii][jj], pad2core_signal[ii][jj], core2pad_signal[ii][jj], ctrl) \
    end \
  end
