#ifndef PROM_PROCESSING_H
#define PROM_PROCESSING_H

#include <linux/pci.h>

#define PROM_END_TAG            0
#define PROM_DEVICE_TAG         1
#define PROM_DMA_TAG            2
#define PROM_DMA_EXT_TAG        3
#define PROM_DMA_MASK_TAG       4
#define PROM_DMA_ALIGN_TAG  5

#define PROM_DMA_PERM_WRITE     2
#define PROM_DMA_PERM_READ      4

#define PROM_MAX_LENGTH 4096
#define PROM_PERM_CAN_WRITE(perm) ((perm) & PROM_DMA_PERM_WRITE)
#define PROM_PERM_CAN_READ(perm) ((perm) & PROM_DMA_PERM_READ)

struct prom_context;

#define prom_for_each_entry(pos, context) \
    for (pos = prom_first_entry(context); \
        pos->tag != PROM_END_TAG; \
        pos = prom_next_entry(pos))

#define PROM_ENTRY_HEAD              \
    struct __attribute__((packed)) { \
        u8 tag;                      \
        u8 size;                     \
    }

struct __attribute__((packed)) prom_device_entry {
    PROM_ENTRY_HEAD;
    // assumed to be null terminated
    char name[];
};

struct __attribute__((packed)) prom_dma_entry {
    PROM_ENTRY_HEAD;
    u16 base[3];
    u32 length;
    u8 perm;
    // assumed to be null terminated
    char name[];
};

struct __attribute__((packed)) prom_dma_ext_entry {
    PROM_ENTRY_HEAD;
    u64 base;
    u64 length;
    u8 perm;
    // assumed to be null terminated
    char name[];
};

struct __attribute__((packed)) prom_dma_mask {
    PROM_ENTRY_HEAD;
    u8 mask;
};

struct __attribute__((packed)) prom_dma_align {
    PROM_ENTRY_HEAD;
    u8 shift;
};

struct __attribute__((packed)) prom_end_entry {
    PROM_ENTRY_HEAD;
    char checksum[];
};

union prom_entry {
    PROM_ENTRY_HEAD;
    struct prom_device_entry device;
    struct prom_dma_entry dma;
    struct prom_dma_ext_entry dma_ext;
    struct prom_dma_mask dma_mask;
    struct prom_dma_align dma_align;
    struct prom_end_entry end;
};

u8 *prom_get_buffer(struct prom_context *context);

size_t prom_get_length(struct prom_context *context);

size_t prom_get_nentries(struct prom_context *context);

size_t prom_get_dma_nentries(struct prom_context *context);

size_t prom_get_nentries_with_minor(struct prom_context *context);

bool prom_entry_needs_minor(union prom_entry *entry);

ssize_t read_prom(
    struct prom_context *context, char *buff, loff_t off, size_t count);

struct prom_context *load_prom(void __iomem *base);

void release_prom_context(struct prom_context *context);

union prom_entry *prom_first_entry(struct prom_context *context);
union prom_entry *prom_next_entry(union prom_entry *entry);

union prom_entry *prom_find_entry(struct prom_context *context, int index);
union prom_entry *prom_find_entry_with_minor(
    struct prom_context *context, int minor);
union prom_entry *prom_find_entry_by_tag(struct prom_context *context, u8 tag);

#endif
