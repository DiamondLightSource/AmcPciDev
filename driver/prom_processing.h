#ifndef PROM_PROCESSING_H
#define PROM_PROCESSING_H

#include <linux/pci.h>

#define PROM_END_TAG            0
#define PROM_DEVICE_TAG         1
#define PROM_DMA_TAG            2

#define PROM_DMA_PERM_WRITE     2
#define PROM_DMA_PERM_READ      4

#define PROM_MAX_LENGTH 4096

#define prom_for_each_entry(pos, context) \
    for (pos=prom_first_entry(context); \
        pos->tag != PROM_END_TAG; \
        pos=prom_next_entry(pos))

#define PROM_ENTRY_HEAD              \
    struct __attribute__((packed)) { \
        u8 tag;                      \
        u8 size;                     \
    }

struct prom_context {
    void __iomem *base;
    unsigned char buff[PROM_MAX_LENGTH + 1];
    size_t data_len;
    size_t nentries;
};

struct __attribute__((packed)) prom_entry {
    PROM_ENTRY_HEAD;
};

struct __attribute__((packed)) prom_device_entry {
    PROM_ENTRY_HEAD;
    // assumed to be null terminated
    char name[];
};

struct __attribute__((packed)) prom_dma_entry {
    PROM_ENTRY_HEAD;
    u64 base : 48;
    u32 length;
    u8 perm;
    // assumed to be null terminated
    char name[];
};

struct __attribute__((packed)) prom_end_entry {
    PROM_ENTRY_HEAD;
    u16 checksum;
};

ssize_t read_prom(struct prom_context *context, char *buff, loff_t off,
    size_t count);

struct prom_context *load_prom(void __iomem *base);

void release_prom_context(struct prom_context *context);

struct prom_entry *prom_first_entry(struct prom_context *context);

struct prom_entry *prom_next_entry(struct prom_entry *entry);

struct prom_entry *prom_find_entry(struct prom_context *context, int index);

#endif
