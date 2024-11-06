#include "error.h"
#include "prom_processing.h"
#include "utils.h"

#include "default_prom.c"

#define PROM_VERSION            1
#define PROM_NAME_MAX_LENGTH    256
#define PROM_FIRST_ENTRY_OFFSET 5
#define PROM_VERSION_OFFSET     4
#define PROM_CHECKSUM_SIZE      2



static int validate_prom(struct prom_context *context)
{
    return calc_checksum16(context->buff, context->data_len) == 0;
}


struct prom_entry *prom_first_entry(struct prom_context *context)
{
    return (struct prom_entry *) &context->buff[PROM_FIRST_ENTRY_OFFSET];
}


struct prom_entry *prom_next_entry(struct prom_entry *entry)
{
    if (entry->tag == PROM_END_TAG)
        return entry;
    return (struct prom_entry *) (((char *) entry) + entry->size + 2);
}


struct prom_entry *prom_find_entry(struct prom_context *context, int index)
{
    int i = 0;
    struct prom_entry *pentry;
    prom_for_each_entry(pentry, context)
    {
        if (i == index)
            return pentry;
        i++;
    }
    return NULL;
}


static int check_magic(const u32 magic)
{
    char *buff = (char *) &magic;
    return buff[0] == 'D' && buff[1] == 'I' && buff[2] == 'A' && buff[3] == 'G';
}


void release_prom_context(struct prom_context *context)
{
    kfree(context);
}


ssize_t read_prom(struct prom_context *context, char *buff, loff_t off,
    size_t count)
{
    if (off > PROM_MAX_LENGTH)
        return -EIO;
    // offset and count should be multiple of 4 bytes
    loff_t off_al = off/4*4;
    size_t size = min(PROM_MAX_LENGTH - (size_t) off_al, count);
    for (int i = 0; i < size; i += 4)
    {
        u32 rval = ioread32(context->base + off_al + i);
        memcpy(buff + i, (char *) &rval, 4);
    }
    return size;
}


struct prom_context *load_prom(void __iomem *base)
{
    int rc = 0;
    struct prom_context *context =
        kzalloc(sizeof(struct prom_context), GFP_KERNEL);
    TEST_PTR(context, rc, no_memory, "Unable to allocate PROM buffer");

    context->base = base;

    u32 rval = ioread32(base);
    if (!check_magic(rval))
    {
        printk(KERN_INFO "PROM memory not found, falling back to default");
        memcpy(context->buff, default_prom, sizeof(default_prom));
    }
    else
    {
        TEST_OK(
            read_prom(context, context->buff, 0, PROM_MAX_LENGTH)
                == PROM_MAX_LENGTH,
            rc = -EIO, io_error, "Could not read PROM");
    }

    TEST_OK(context->buff[PROM_VERSION_OFFSET] == PROM_VERSION,
        rc = -EIO, no_version, "PROM version is not supported");

    int ent_i = PROM_FIRST_ENTRY_OFFSET;
    while (ent_i < PROM_MAX_LENGTH && context->buff[ent_i] != PROM_END_TAG)
    {
        if (context->buff[ent_i] == PROM_DMA_TAG)
            context->has_dma = true;

        ent_i += context->buff[ent_i + 1] + 2;
        context->nentries++;
    }

    context->data_len = ent_i + context->buff[ent_i + 1] + 2;

    // At this point we've either run off the end of memory, or we're sitting
    // on a putative end marker
    TEST_OK(
        context->data_len < PROM_MAX_LENGTH &&
        context->buff[ent_i] == PROM_END_TAG &&
        (context->buff[ent_i + 1] == PROM_CHECKSUM_SIZE ||
         context->buff[ent_i + 1] == PROM_CHECKSUM_SIZE + 1),
        rc = -EIO, invalid_prom, "PROM end marker not found");

    TEST_OK(validate_prom(context), rc = -EIO, invalid_prom,
        "Invalid PROM data");
    return context;

invalid_prom:
no_version:
io_error:
    kfree(context);
no_memory:
    return ERR_PTR(rc);
}
