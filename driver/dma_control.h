#ifndef DMA_CONTROL_H
#define DMA_CONTROL_H
#include <linux/dma-direction.h>

/* All DMA transfers must occur on a 32-byte alignment, I guess this is the
 * 256-bit transfer size.  Alas, if this rule is violated then the DMA engine
 * simply locks up without reporting an error. */
#define DMA_DEFAULT_ALIGNMENT_SHIFT 5
#define DMA_DEFAULT_MASK 47

/* Interface to DMA engine. */

/* This interface provides access to both areas of DRAM on the FPGA. */

struct dma_control;

/* Initialises DMA control, returns structure used for access. */
int initialise_dma_control(
    struct pci_dev *pdev, void __iomem *regs, struct dma_control **pdma,
    u8 dma_mask, u8 dma_alignment_shift);

void terminate_dma_control(struct dma_control *dma);


/* This is called to read the specified block from FPGA memory into the DMA
 * buffer which is returned.  This buffer *must* be released when finished with
 * so that other readers can proceed.  The length parameter is updated with the
 * number of bytes actually read. */
ssize_t dma_operation_unlocked(
    struct dma_control *dma, size_t start, size_t count,
    enum dma_data_direction dir);

void dma_memory_lock(struct dma_control *dma);
void dma_memory_unlock(struct dma_control *dma);

void *dma_get_buffer(struct dma_control *dma);
size_t dma_get_alignment(struct dma_control *dma);

/* To be called each time a DMA completion interrupt is seen. */
void dma_interrupt(struct dma_control *dma);

/* Returns available DMA buffer size. */
size_t dma_buffer_size(struct dma_control *dma);

#endif
