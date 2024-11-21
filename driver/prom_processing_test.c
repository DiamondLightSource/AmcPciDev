#include <kunit/test.h>
#include "prom_processing.h"
#include "test_assets/test_prom1.c"
#include "test_assets/test_prom2.c"
#include "test_assets/test_prom3.c"
#include "test_assets/test_prom4.c"


static u64 base_to_u64(u16 *base)
{
    return (u64) base[0] | (u64) base[1] << 16 | (u64) base[2] << 32;
}


static void test_load_prom_validation_ok(struct kunit *test)
{
    struct prom_context *context = load_prom((void *) test_prom1);
    KUNIT_EXPECT_NOT_ERR_OR_NULL(test, context);
    if (!IS_ERR(context))
        release_prom_context(context);
}


static void test_load_prom_validation_fail(struct kunit *test)
{
    struct prom_context *context = load_prom((void *) test_prom1_corrupted);
    KUNIT_EXPECT_TRUE(test, IS_ERR(context));
}


static void test_load_prom_data(struct kunit *test)
{
    struct prom_context *context = load_prom((void *) test_prom1);
    KUNIT_EXPECT_NOT_ERR_OR_NULL(test, context);
    KUNIT_EXPECT_EQ(test, 0,
        memcmp(prom_get_buffer(context), test_prom1, test_prom1_size));
    KUNIT_EXPECT_EQ(test, test_prom1_size, prom_get_length(context));
    KUNIT_EXPECT_EQ(test, test_prom1_nentries, prom_get_nentries(context));
    KUNIT_EXPECT_EQ(
        test, test_prom1_nentries, prom_get_nentries_with_minor(context));
    KUNIT_EXPECT_EQ(
        test, test_prom1_nentries - 1, prom_get_dma_nentries(context));
    release_prom_context(context);
}


static void test_prom_first_entry(struct kunit *test)
{
    struct prom_context *context = load_prom((void *) test_prom1);
    union prom_entry *dev_entry = prom_first_entry(context);
    KUNIT_EXPECT_EQ(test, (u8) PROM_DEVICE_TAG, dev_entry->tag);
    KUNIT_EXPECT_EQ(test, 0, memcmp(dev_entry->device.name, "test_dev", 8));
    KUNIT_EXPECT_EQ(test, (u8) 9, dev_entry->size);
    release_prom_context(context);
}


static void test_prom_next_entry(struct kunit *test)
{
    struct prom_context *context = load_prom((void *) test_prom1);
    union prom_entry *entry = prom_first_entry(context);
    entry = prom_next_entry(entry);
    KUNIT_EXPECT_EQ(test, (u8) PROM_DMA_TAG, entry->tag);
    KUNIT_EXPECT_EQ(test, 0, memcmp(entry->dma.name, "memA", 4));
    KUNIT_EXPECT_TRUE(test, base_to_u64(entry->dma.base) == 0);
    KUNIT_EXPECT_EQ(test, (u32) 0x12131415, entry->dma.length);
    KUNIT_EXPECT_EQ(test, (u8) 4, entry->dma.perm);
    entry = prom_next_entry(entry);
    KUNIT_EXPECT_EQ(test, (u8) PROM_DMA_TAG, entry->tag);
    KUNIT_EXPECT_EQ(test, 0, memcmp(entry->dma.name, "memB", 4));
    KUNIT_EXPECT_TRUE(test, base_to_u64(entry->dma.base) == 0xabcd11223344);
    KUNIT_EXPECT_EQ(test, (u32) 0x8912345, entry->dma.length);
    KUNIT_EXPECT_EQ(test, (u8) 2, entry->dma.perm);
    entry = prom_next_entry(entry);
    KUNIT_EXPECT_EQ(test, (u8) PROM_DMA_TAG, entry->tag);
    KUNIT_EXPECT_EQ(test, 0, memcmp(entry->dma.name, "memC", 4));
    KUNIT_EXPECT_TRUE(test,  base_to_u64(entry->dma.base) == 0x42616263);
    KUNIT_EXPECT_EQ(test, (u32) 0x100, entry->dma.length);
    KUNIT_EXPECT_EQ(test, (u8) 6, entry->dma.perm);
    entry = prom_next_entry(entry);
    KUNIT_EXPECT_TRUE(test, entry->tag == PROM_END_TAG);
    release_prom_context(context);
}


static void test_prom_find_entry_available(struct kunit *test)
{
    struct prom_context *context = load_prom((void *) test_prom1);
    union prom_entry *entry = prom_find_entry(context, 2);
    KUNIT_EXPECT_EQ(test, (u8) PROM_DMA_TAG, entry->tag);
    KUNIT_EXPECT_EQ(test, 0, memcmp(entry->dma.name, "memB", 4));
    KUNIT_EXPECT_TRUE(test, base_to_u64(entry->dma.base) == 0xabcd11223344);
    KUNIT_EXPECT_EQ(test, (u32) 0x8912345, entry->dma.length);
    KUNIT_EXPECT_EQ(test, (u8) 2, entry->dma.perm);
    entry = prom_find_entry(context, 3);
    KUNIT_EXPECT_EQ(test, (u8) PROM_DMA_TAG, entry->tag);
    KUNIT_EXPECT_EQ(test, 0, memcmp(entry->dma.name, "memC", 4));
    KUNIT_EXPECT_TRUE(test,  base_to_u64(entry->dma.base) == 0x42616263);
    KUNIT_EXPECT_EQ(test, (u32) 0x100, entry->dma.length);
    KUNIT_EXPECT_EQ(test, (u8) 6, entry->dma.perm);
    release_prom_context(context);
}


static void test_prom_find_entry_missing(struct kunit *test)
{
    struct prom_context *context = load_prom((void *) test_prom1);
    union prom_entry *entry = prom_find_entry(context, 4);
    KUNIT_EXPECT_PTR_EQ(test, (union prom_entry *) NULL, entry);
    release_prom_context(context);
}


static void test_prom_with_dma_ext_entry(struct kunit *test)
{
    struct prom_context *context = load_prom((void *) test_prom2);
    KUNIT_EXPECT_NOT_ERR_OR_NULL(test, context);
    union prom_entry *entry = prom_find_entry(context, 0);
    KUNIT_EXPECT_EQ(test, (u8) PROM_DMA_EXT_TAG, entry->tag);
    KUNIT_EXPECT_EQ(test, 0, memcmp(entry->dma_ext.name, "ddr0", 4));
    KUNIT_EXPECT_TRUE(test, entry->dma_ext.base == 0x0807060504030201);
    KUNIT_EXPECT_EQ(test, (u64) 0x090a0b0c, entry->dma_ext.length);
    KUNIT_EXPECT_EQ(test, (u8) PROM_DMA_PERM_READ, entry->dma_ext.perm);
    release_prom_context(context);
}


static void test_prom_with_dma_ext_entry_and_bigger_length(struct kunit *test)
{
    struct prom_context *context = load_prom((void *) test_prom3);
    KUNIT_EXPECT_NOT_ERR_OR_NULL(test, context);
    union prom_entry *entry = prom_find_entry(context, 0);
    KUNIT_EXPECT_EQ(test, (u8) PROM_DMA_EXT_TAG, entry->tag);
    KUNIT_EXPECT_EQ(test, 0, memcmp(entry->dma_ext.name, "ddr0", 4));
    KUNIT_EXPECT_TRUE(test, entry->dma_ext.base == 0);
    KUNIT_EXPECT_EQ(test, (u64) 0x08090a0b0c0d0e0f, entry->dma_ext.length);
    KUNIT_EXPECT_EQ(test, (u8) PROM_DMA_PERM_READ, entry->dma_ext.perm);
    release_prom_context(context);
}


static void test_prom_with_mask_and_alignment(struct kunit *test)
{
    struct prom_context *context = load_prom((void *) test_prom4);
    KUNIT_EXPECT_NOT_ERR_OR_NULL(test, context);
    union prom_entry *entry = prom_find_entry_by_tag(context, PROM_DMA_MASK_TAG);
    KUNIT_EXPECT_NOT_ERR_OR_NULL(test, entry);
    KUNIT_EXPECT_EQ(test, (u8) PROM_DMA_MASK_TAG, entry->tag);
    KUNIT_EXPECT_EQ(test, (u8) 64, entry->dma_mask.mask);
    entry = prom_find_entry_by_tag(context, PROM_DMA_ALIGN_TAG);
    KUNIT_EXPECT_NOT_ERR_OR_NULL(test, entry);
    KUNIT_EXPECT_EQ(test, (u8) PROM_DMA_ALIGN_TAG, entry->tag);
    KUNIT_EXPECT_EQ(test, (u8) 6, entry->dma_align.shift);
    entry = prom_find_entry_with_minor(context, 0);
    KUNIT_EXPECT_NOT_ERR_OR_NULL(test, entry);
    KUNIT_EXPECT_EQ(test, (u8) PROM_DEVICE_TAG, entry->tag);
    entry = prom_find_entry_with_minor(context, 1);
    KUNIT_EXPECT_NOT_ERR_OR_NULL(test, entry);
    KUNIT_EXPECT_EQ(test, (u8) PROM_DMA_TAG, entry->tag);
    release_prom_context(context);
}


static struct kunit_case prom_processing_test_cases[] = {
    KUNIT_CASE(test_load_prom_validation_ok),
    KUNIT_CASE(test_load_prom_validation_fail),
    KUNIT_CASE(test_load_prom_data),
    KUNIT_CASE(test_prom_first_entry),
    KUNIT_CASE(test_prom_next_entry),
    KUNIT_CASE(test_prom_find_entry_available),
    KUNIT_CASE(test_prom_find_entry_missing),
    KUNIT_CASE(test_prom_with_dma_ext_entry),
    KUNIT_CASE(test_prom_with_dma_ext_entry_and_bigger_length),
    KUNIT_CASE(test_prom_with_mask_and_alignment),
    {}
};


static struct kunit_suite prom_processing_test_suite = {
    .name = "prom_processing",
    .test_cases = prom_processing_test_cases,
};


kunit_test_suite(prom_processing_test_suite);

MODULE_LICENSE("GPL");
