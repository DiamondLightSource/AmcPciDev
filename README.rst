AMC PCIe Driver
===============

This Linux kernel driver is designed to provide device nodes for access to FPGA
cards over PCIe.  Currently the driver looks for cards with PCIe system ID
10EE:7038 (Signal processing controller: Xilinx Corporation FPGA Card XC7VX690T)
and subsystem id 10EE:0007, but it would be desirable to use a more focused
identification.

This driver expects the PCIe end point to provide two BARs, BAR0 and BAR2, and
it will look for an identification area at offset 0x2000 into BAR2.  This ID
area is used to determine the device name and which DMA areas are provided.

BAR0 is intended to provide access to the general purpose registers in the FPGA
implementation.  User space access to this register area is available by using
``mmap`` to map the device node `name`\ .\ ``reg``, which is always available.
This node also provides information about interrupts which can be obtained
through calls to ``select`` and ``read``.

BAR2 is entirly under the control of this driver and is expected to provide the
following resources:

..  list-table::
    :widths:    5 15 50
    :header-rows: 1

    * - Offset
      - Name
      - Description

    * - 0x0000
      - DMA Controller
      - If present this must be a Xilinx DMA controller as described in PG034.
        Only a minimal controller configuration is supported.

    * - 0x1000
      - Interrupt Controller
      - This is a Xilinx interrupt controller as described in PG099.

    * - 0x2000
      - Identity PROM
      - This identification area is used to determine which DMA areas are
        available.

At present only DMA read operations are supported.
