/* This provides memory mapped access to the registers in BAR0 and a stream of
 * events provided through interrupts. */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/poll.h>

#include "error.h"
#include "amc_pci_core.h"
#include "amc_pci_device.h"
#include "interrupts.h"
#include "registers.h"


struct register_context {
    unsigned long base_page;
    size_t length;
    struct interrupt_control *interrupts;
    struct register_locking *locking;
    int reader_number;
};


int amc_pci_reg_open(
    struct file *file, struct pci_dev *dev,
    struct interrupt_control *interrupts,
    struct register_locking *locking)
{
    int reader_number;
    int rc = 0;

    TEST_OK(assign_reader_number(interrupts, &reader_number),
        rc=-EIO, no_reader_slot, "No reader slot");
    struct register_context *context =
        kmalloc(sizeof(struct register_context), GFP_KERNEL);
    TEST_PTR(context, rc, no_context, "Unable to allocate register context");

    *context = (struct register_context) {
        .base_page = pci_resource_start(dev, 0) >> PAGE_SHIFT,
        .length = pci_resource_len(dev, 0),
        .interrupts = interrupts,
        .locking = locking,
        .reader_number = reader_number
    };

    /* Check for lock state and count ourself in if we can. */
    mutex_lock(&locking->mutex);
    TEST_OK(!locking->locked_by, rc = -EBUSY, locked,
        "Device locked for exclusive access");
    locking->reference_count += 1;
    mutex_unlock(&locking->mutex);

    file->private_data = context;
    return 0;

locked:
    mutex_unlock(&locking->mutex);
    kfree(context);
no_context:
    unassign_reader_number(interrupts, reader_number);
no_reader_slot:
    return rc;
}


static int amc_pci_reg_release(struct inode *inode, struct file *file)
{
    struct register_context *context = file->private_data;
    struct register_locking *locking = context->locking;

    mutex_lock(&locking->mutex);
    if (locking->locked_by == context)
        locking->locked_by = NULL;
    locking->reference_count -= 1;
    mutex_unlock(&locking->mutex);
    unassign_reader_number(context->interrupts, context->reader_number);
    kfree(context);
    amc_pci_release(inode);
    return 0;
}


static int amc_pci_reg_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct register_context *context = file->private_data;

    size_t size = vma->vm_end - vma->vm_start;
    unsigned long end = (vma->vm_pgoff << PAGE_SHIFT) + size;
    if (end > context->length)
    {
        printk(KERN_WARNING CLASS_NAME " map area out of range\n");
        return -EINVAL;
    }

    /* Good advice and examples on using this function here:
     *  http://www.makelinux.net/ldd3/chp-15-sect-2
     * Also see drivers/char/mem.c in kernel sources for guidelines. */
    return io_remap_pfn_range(
        vma, vma->vm_start, context->base_page + vma->vm_pgoff, size,
        pgprot_noncached(vma->vm_page_prot));
}


static long lock_register(struct register_context *context)
{
    struct register_locking *locking = context->locking;
    int rc = 0;

    mutex_lock(&locking->mutex);
    if (locking->reference_count > 1)
    {
        printk(KERN_WARNING CLASS_NAME " device too busy to lock\n");
        rc = -EBUSY;
    }
    else if (locking->locked_by)
    {
        printk(KERN_WARNING CLASS_NAME " device already locked\n");
        rc = -EBUSY;
    }
    else
        locking->locked_by = context;
    mutex_unlock(&locking->mutex);

    return rc;
}


static long unlock_register(struct register_context *context)
{
    struct register_locking *locking = context->locking;
    int rc = 0;

    mutex_lock(&locking->mutex);
    if (locking->locked_by == context)
        locking->locked_by = NULL;
    else
    {
        printk(KERN_WARNING CLASS_NAME " device not locked by caller\n");
        rc = -EINVAL;
    }
    mutex_unlock(&locking->mutex);

    return rc;
}


static long amc_pci_reg_ioctl(
    struct file *file, unsigned int cmd, unsigned long arg)
{
    struct register_context *context = file->private_data;
    switch (cmd)
    {
        case AMC_MAP_SIZE:
            return context->length;
        case AMC_REG_LOCK:
            return lock_register(context);
        case AMC_REG_UNLOCK:
            return unlock_register(context);
        default:
            return -EINVAL;
    }
}


/* This will return one byte with the next available event mask. */
static ssize_t amc_pci_reg_read(
    struct file *file, char __user *buf, size_t count, loff_t *f_pos)
{
    struct register_context *context = file->private_data;

    /* In non blocking mode if we're not ready then say so. */
    bool no_wait = file->f_flags & O_NONBLOCK;
    if (no_wait  &&  !interrupt_events_ready(
            context->interrupts, context->reader_number))
        return -EAGAIN;

    /* Ensure we've asked for at least 4 bytes. */
    if (count < sizeof(uint32_t))
        return -EIO;

    uint32_t events;
    int rc = read_interrupt_events(context->interrupts, no_wait, &events,
                                   context->reader_number);
    if (rc < 0)
        /* Read was interrupted. */
        return rc;
    else if (copy_to_user(buf, &events, sizeof(uint32_t)) > 0)
        /* Invalid buffer specified by user process, couldn't copy. */
        return -EFAULT;
    else if (events == 0)
        /* This can happen if we're fighting with a concurrent reader and
         * O_NONBLOCK was set. */
        return -EAGAIN;
    else
        return sizeof(uint32_t);
}


static unsigned int amc_pci_reg_poll(
    struct file *file, struct poll_table_struct *poll)
{
    struct register_context *context = file->private_data;

    poll_wait(file, interrupts_wait_queue(context->interrupts), poll);
    if (interrupt_events_ready(context->interrupts, context->reader_number))
        return POLLIN | POLLRDNORM;
    else
        return 0;
}


struct file_operations amc_pci_reg_fops = {
    .owner = THIS_MODULE,
    .release = amc_pci_reg_release,
    .unlocked_ioctl = amc_pci_reg_ioctl,
    .mmap = amc_pci_reg_mmap,
    .read = amc_pci_reg_read,
    .poll = amc_pci_reg_poll,
};
