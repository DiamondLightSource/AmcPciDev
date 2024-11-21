#ifndef MEMORY_H
#define MEMORY_H

/* Memory device support. */

/* Initialises associated memory device.  The base and length of the controlled
 * memory area are passed. */
int amc_pci_dma_open(
    struct file *file, struct dma_control *dma, size_t base, size_t length);

/* File operations for memory devices. */
extern struct file_operations amc_pci_dma_fops;

#endif
