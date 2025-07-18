/* Implements access to memory via dma. */

#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/module.h>

#include "error.h"
#include "debug.h"
#include "utils.h"

#include "dma_control.h"


#define DMA_BLOCK_SHIFT     20  // Default DMA block size as power of 2

static int dma_block_shift = DMA_BLOCK_SHIFT;
module_param(dma_block_shift, int, S_IRUGO);


/* The DMA transfer count is limited to 23 bits, so the maximum transfer size is
 * 2^23-1 = 8388607 bytes, and we align the limit. */
#define MAX_DMA_TRANSFER    ((1 << 23) - 1)


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


/* Xilinx AXI DMA Controller, as defined in Xilinx PG034 documentation. */
struct axi_dma_controller {
    uint32_t cdmacr;            // 00 CDMA control
    uint32_t cdmasr;            // 04 CDMA status
    uint32_t curdesc_pntr;      // 08 (Not used, for scatter gather)
    uint32_t curdesc_pntr_msb;  // 0C (ditto)
    uint32_t taildesc_pntr;     // 10 (ditto)
    uint32_t taildesc_pntr_msb; // 14 (ditto)
    uint32_t sa;                // 18 Source address lower 32 bits
    uint32_t sa_msb;            // 1C Source address, upper 32 bits
    uint32_t da;                // 20 Destination address, lower 32 bits
    uint32_t da_msb;            // 24 Destination address, upper 32 bits
    uint32_t btt;               // 28 Bytes to transfer, writing triggers DMA
};


/* Control bits. */
#define CDMACR_Err_IrqEn    (1 << 14)   // Enable interrupt on error
#define CDMACR_IrqEn        (1 << 12)   // Enable completion interrupt
#define CDMACR_Reset        (1 << 2)    // Force soft reset of controller

/* Status bits. */
#define CDMASR_Err_Irq      (1 << 14)   // DMA error event seen
#define CDMASR_IOC_Irq      (1 << 12)   // DMA completion event seen
#define CDMASR_DMADecErr    (1 << 6)    // Address decode error seen
#define CDMASR_DMASlvErr    (1 << 5)    // Slave response error seen
#define CDMASR_DMAIntErr    (1 << 4)    // DMA internal error seen
#define CDMASR_Idle         (1 << 1)    // Last command completed


struct dma_control {
    /* Parent device. */
    struct pci_dev *pdev;

    /* BAR2 memory region for DMA controller. */
    struct axi_dma_controller __iomem *regs;

    /* Memory region for DMA. */
    int buffer_shift;       // log2(buffer_size)
    size_t buffer_size;     // Buffer size in bytes, equal to 1<<buffer_shift
    void *buffer;           // DMA transfer buffer
    dma_addr_t buffer_dma;  // Associated DMA address

    /* Mutex for exclusive access to DMA engine. */
    struct mutex mutex;

    /* Completion for DMA transfer. */
    struct completion dma_done;

    ssize_t alignment;
    ssize_t max_transfer;
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* DMA. */


static int reset_dma_controller(struct dma_control *dma)
{
    writel(CDMACR_Reset, &dma->regs->cdmacr);

    /* In principle we should wait for the reset to complete, though it doesn't
     * actually seem to take an observable time normally.  We use a deadline
     * just in case something goes wrong so we don't deadlock. */
    unsigned long deadline = jiffies + msecs_to_jiffies(1);
    int count = 0;
    int reset_stat = 0;
    while (
        reset_stat = readl(&dma->regs->cdmacr),
        (reset_stat & CDMACR_Reset) && time_before(jiffies, deadline))
    {
        count += 1;
    }

    if (reset_stat & CDMACR_Reset)
        return -EIO;

    /* Now restore the default working state. */
    writel(CDMACR_IrqEn | CDMACR_Err_IrqEn, &dma->regs->cdmacr);
    return 0;
}


static int check_dma_status(struct dma_control *dma)
{
    uint32_t status = readl(&dma->regs->cdmasr);
    bool error =
        status & (CDMASR_DMADecErr | CDMASR_DMASlvErr | CDMASR_DMAIntErr);
    if (error)
    {
        printk(KERN_ERR "DMA error code: %08x\n", status);
        return -EIO;
    }
    else
        return 0;
}


static int maybe_reset_dma(struct dma_control *dma)
{
    uint32_t status = readl(&dma->regs->cdmasr);
    int rc = 0;
    bool error =
        status & (CDMASR_DMADecErr | CDMASR_DMASlvErr | CDMASR_DMAIntErr);
    bool idle = status & CDMASR_Idle;
    if (error || !idle)
    {
        printk(KERN_INFO "Forcing reset of DMA controller (status = %08x)\n",
            status);
        rc = reset_dma_controller(dma);
    }
    return rc;
}


void dma_interrupt(struct dma_control *dma)
{
    uint32_t cdmasr = readl(&dma->regs->cdmasr);
    writel(cdmasr, &dma->regs->cdmasr);

    complete(&dma->dma_done);
}


static int configure_dma_engine(
    struct dma_control *dma, size_t src, size_t dst, size_t count)
{
    dev_dbg(&dma->pdev->dev,
        "Requesting DMA transfer 0x%08zx -> 0x%08zx, 0x%08zx bytes\n",
        src, dst, count);
    ssize_t alignment = dma_get_alignment(dma);
    int rc = IS_ALIGNED(src, alignment) &&
             IS_ALIGNED(dst, alignment) &&
             IS_ALIGNED(count, alignment) ? 0 : -EINVAL;
    TEST_RC(rc, unaligned_error, "DMA operation not aligned");
    /* Reset the DMA engine if necessary. */
    rc = (ssize_t) maybe_reset_dma(dma);
    TEST_RC(rc, reset_error, "Failed to reset DMA");

    /* Configure the engine for transfer. */
    writel((uint32_t) (src >> 32), &dma->regs->sa_msb);
    writel((uint32_t) src, &dma->regs->sa);
    writel((uint32_t) dst, &dma->regs->da);
    writel((uint32_t) (dst >> 32), &dma->regs->da_msb);
    /* Ensure we don't request an under sized buffer. */
    writel(count, &dma->regs->btt);
reset_error:
unaligned_error:
    return rc;
}


/* Caller must have dma memory locked. */
ssize_t dma_operation_unlocked(
    struct dma_control *dma, size_t start, size_t count,
    enum dma_data_direction dir)
{
    if (count > dma->max_transfer)
        count = dma->max_transfer;

    /* Hand the buffer over to the DMA engine. */
    dma_sync_single_for_device(
        &dma->pdev->dev, dma->buffer_dma, dma->buffer_size, dir);

    reinit_completion(&dma->dma_done);
    ssize_t rc;
    if (dir == DMA_TO_DEVICE)
        rc = configure_dma_engine(dma, dma->buffer_dma, start, count);
    else if (dir == DMA_FROM_DEVICE)
        rc = configure_dma_engine(dma, start, dma->buffer_dma, count);
    else
        rc = -EINVAL;

    TEST_RC(rc, dma_error, "Failed to configure DMA");

    /* Wait for transfer to complete.  If we're killed, unlock and bail.  Note
     * that this call is only killable (kill -9) and not interruptible because
     * if the DMA engine does fail to complete then we have a bit of a problem
     * anyway, and if this completion were to be interrupted normally there
     * would be a hazard from the residual DMA in progress. */
    rc = wait_for_completion_killable(&dma->dma_done);
    TEST_RC(rc, killed, "DMA transfer killed");

    /* Restore the buffer to CPU access (really just flushes associated cache
     * entries). */
    dma_sync_single_for_cpu(
        &dma->pdev->dev, dma->buffer_dma, dma->buffer_size, dir);

    rc = check_dma_status(dma);
    if (rc)
        goto dma_error;

    return count;

dma_error:
killed:
    return rc;
}


void dma_memory_lock(struct dma_control *dma)
{
    mutex_lock(&dma->mutex);
}


void dma_memory_unlock(struct dma_control *dma)
{
    mutex_unlock(&dma->mutex);
}


void *dma_get_buffer(struct dma_control *dma)
{
    return dma->buffer;
}


size_t dma_buffer_size(struct dma_control *dma)
{
    return dma->buffer_size;
}


size_t dma_get_alignment(struct dma_control *dma)
{
    return dma->alignment;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Initialisation and shutdown. */


int initialise_dma_control(
    struct pci_dev *pdev, void __iomem *regs, struct dma_control **pdma,
    u8 dma_mask, u8 dma_alignment_shift)
{
    dev_dbg(&pdev->dev,
        "Initialising DMA control with mask %d and alignment %llu\n",
        dma_mask, 1ull << dma_alignment_shift);
    int rc = 0;
    TEST_OK(dma_block_shift >= PAGE_SHIFT, rc = -EINVAL, no_memory,
        "Invalid DMA buffer size");

    rc = dma_set_mask(&pdev->dev, DMA_BIT_MASK(dma_mask));
    TEST_RC(rc, no_dma_mask, "Unable to set DMA mask");

    /* Create and return DMA control structure. */
    struct dma_control *dma = kmalloc(sizeof(struct dma_control), GFP_KERNEL);
    TEST_PTR(dma, rc, no_memory, "Unable to allocate DMA control");
    *pdma = dma;
    dma->pdev = pdev;
    dma->regs = regs;

    /* Allocate DMA buffer area. */
    dma->buffer_shift = dma_block_shift;
    dma->buffer_size = 1 << dma_block_shift;
    dma->buffer = (void *) __get_free_pages(
        GFP_KERNEL, dma->buffer_shift - PAGE_SHIFT);
    TEST_PTR(dma->buffer, rc, no_buffer, "Unable to allocate DMA buffer");
    dma->alignment = 1ull << dma_alignment_shift;
    dma->max_transfer =
        ALIGN_DOWN(min((size_t) MAX_DMA_TRANSFER, dma->buffer_size),
            dma->alignment);

    /* Get the associated DMA address for the buffer. */
    dma->buffer_dma = dma_map_single(
        &pdev->dev, dma->buffer, dma->buffer_size, DMA_BIDIRECTIONAL);
    TEST_OK(!dma_mapping_error(&pdev->dev, dma->buffer_dma),
        rc = -EIO, no_dma_map, "Unable to map DMA buffer");

    /* Final initialisation, now ready to run. */
    mutex_init(&dma->mutex);
    init_completion(&dma->dma_done);
    rc = reset_dma_controller(dma);
    TEST_RC(rc, reset_error, "Failed to reset DMA");

    return 0;


reset_error:
    dma_unmap_single(
        &pdev->dev, dma->buffer_dma, dma->buffer_size, DMA_BIDIRECTIONAL);
no_dma_map:
    free_pages((unsigned long) dma->buffer, dma->buffer_shift - PAGE_SHIFT);
no_buffer:
    kfree(dma);
no_dma_mask:
no_memory:
    return rc;
}


void terminate_dma_control(struct dma_control *dma)
{
    dma_unmap_single(
        &dma->pdev->dev, dma->buffer_dma, dma->buffer_size, DMA_FROM_DEVICE);
    free_pages((unsigned long) dma->buffer, dma->buffer_shift - PAGE_SHIFT);
    kfree(dma);
}
