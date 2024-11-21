/* Support for DMA access via memory device. */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/pci.h>

#include "error.h"
#include "amc_pci_core.h"
#include "amc_pci_device.h"
#include "dma_control.h"

#include "memory.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Memory device. */

/* This provides read access to the DRAM via the DMA controller with registers
 * in BAR2. */


struct memory_context {
    struct dma_control *dma;        // DMA controller
    size_t base;
    size_t length;
};


int amc_pci_dma_open(
    struct file *file, struct dma_control *dma, size_t base, size_t length)
{
    int rc = 0;
    struct memory_context *context =
        kmalloc(sizeof(struct memory_context), GFP_KERNEL);
    TEST_PTR(context, rc, no_context, "Unable to allocate DMA context");

    *context = (struct memory_context) {
        .dma = dma,
        .base = base,
        .length = length,
    };

    file->private_data = context;
    return 0;

no_context:
    return rc;
}


static int amc_pci_dma_release(struct inode *inode, struct file *file)
{
    kfree(file->private_data);
    amc_pci_release(inode);
    return 0;
}


static ssize_t amc_pci_dma_write(
    struct file *file, const char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t rc = 0;
    struct memory_context *context = file->private_data;
    /* Constrain write to valid region. */
    loff_t offset = *f_pos;
    if (offset == context->length)
        return 0;
    else if (offset > context->length)
        /* Treat seeks off end of memory block as an error. */
        return -EFAULT;
    if (count > context->length - offset ||
            count > dma_buffer_size(context->dma))
        /* Can't write more than the remaining memory or DMA buffer. */
        return -EINVAL;


    void *data_buffer = dma_get_buffer(context->dma);
    ssize_t write_count = count;
    /* Lock, transfer from user space, write data, unlock. */
    dma_memory_lock(context->dma);
    write_count -= copy_from_user(data_buffer, buf, count);
    TEST_OK(write_count > 0, rc = -EFAULT, mem_err, "Failed to copy data");
    /* Misaligned writes will fail in the following function call */
    ssize_t dma_write_count = dma_operation_unlocked(
        context->dma, context->base + offset, count, DMA_TO_DEVICE);
    TEST_OK(dma_write_count > 0, rc = dma_write_count, mem_err, "DMA failed");
    dma_memory_unlock(context->dma);

    *f_pos += dma_write_count;
    if (*f_pos >= context->length)
        *f_pos = 0;

    return dma_write_count;
mem_err:
    dma_memory_unlock(context->dma);
    return rc;
}


static ssize_t amc_pci_dma_read(
    struct file *file, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t rc = 0;
    struct memory_context *context = file->private_data;
    /* Constrain read to valid region. */
    loff_t offset = *f_pos;
    if (offset == context->length)
        return 0;
    else if (offset > context->length)
        /* Treat seeks off end of memory block as an error. */
        return -EFAULT;

    void *data_buffer = dma_get_buffer(context->dma);
    size_t alignment = dma_get_alignment(context->dma);
    size_t in_offset = offset & (alignment - 1);
    size_t dma_addr = context->base + offset - in_offset;
    size_t dma_count = ALIGN(count + in_offset, alignment);
    if (dma_addr + dma_count > context->base + context->length)
        dma_count = ALIGN_DOWN(
            context->base + context->length - dma_addr, alignment);
    if (dma_count == 0)
        /* Can't read anything without violating alignment. */
        return -EFAULT;

    /* Lock, read the data, transfer it to user space, unlock. */
    dma_memory_lock(context->dma);
    ssize_t dma_read_count = dma_operation_unlocked(
        context->dma, dma_addr, dma_count, DMA_FROM_DEVICE);
    TEST_OK(dma_read_count > 0, rc = dma_read_count, mem_err, "DMA failed");
    ssize_t user_count = min(count, dma_read_count - in_offset);
    user_count -= copy_to_user(buf, data_buffer + in_offset, user_count);
    TEST_OK(user_count > 0, rc = -EFAULT, mem_err, "Failed to copy data");
    dma_memory_unlock(context->dma);
    *f_pos += user_count;
    if (*f_pos >= context->length)
        *f_pos = 0;

    return user_count;
mem_err:
    dma_memory_unlock(context->dma);
    return rc;
}


static loff_t amc_pci_dma_llseek(struct file *file, loff_t f_pos, int whence)
{
    struct memory_context *context = file->private_data;
    return generic_file_llseek_size(
        file, f_pos, whence, context->length, context->length);
}


static long amc_pci_mem_ioctl(
    struct file *file, unsigned int cmd, unsigned long arg)
{
    struct memory_context *context = file->private_data;
    switch (cmd)
    {
        case AMC_BUF_SIZE:
            return dma_buffer_size(context->dma);
        case AMC_DMA_AREA_SIZE:
            return context->length;
        default:
            return -EINVAL;
    }
}


struct file_operations amc_pci_dma_fops = {
    .owner = THIS_MODULE,
    .release = amc_pci_dma_release,
    .write = amc_pci_dma_write,
    .read = amc_pci_dma_read,
    .llseek = amc_pci_dma_llseek,
    .unlocked_ioctl = amc_pci_mem_ioctl,
};
