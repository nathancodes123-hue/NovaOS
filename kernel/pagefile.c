#include "../include/nova/types.h"
#include "../include/nova/block.h"
#include "../include/nova/pagefile.h"

typedef struct {
    u32 virt;
    u32 flags;
} SwapBinding;

static u32 slot_bits[NOVA_PAGEFILE_PAGES / 32u];
static SwapBinding bindings[NOVA_PAGEFILE_PAGES];
static u32 used_pages;
static int available;

static void zero(void *ptr, usize size) {
    u8 *p = (u8 *)ptr;
    while (size--)
        *p++ = 0;
}

static int valid_slot(u32 slot) {
    return slot < NOVA_PAGEFILE_PAGES;
}

static u32 bit_word(u32 slot) {
    return slot >> 5;
}

static u32 bit_mask(u32 slot) {
    return 1u << (slot & 31u);
}

static int slot_used(u32 slot) {
    return (slot_bits[bit_word(slot)] & bit_mask(slot)) != 0;
}

static void slot_set(u32 slot) {
    slot_bits[bit_word(slot)] |= bit_mask(slot);
}

static void slot_clear(u32 slot) {
    slot_bits[bit_word(slot)] &= ~bit_mask(slot);
}

void pagefile_init(void) {
    zero(slot_bits, sizeof(slot_bits));
    zero(bindings, sizeof(bindings));
    used_pages = 0;
    available = 0;

    const NovaBlockDevice *dev = block_device();
    if (!dev || !dev->present || dev->sector_size != 512u)
        return;

    if (dev->sector_count &&
        (u64)NOVA_PAGEFILE_START_LBA +
        (u64)NOVA_PAGEFILE_PAGES * NOVA_PAGEFILE_SECTORS_PER_PAGE >
        dev->sector_count)
        return;

    available = 1;
}

int pagefile_available(void) {
    return available;
}

int pagefile_alloc(u32 *slot) {
    if (!available || !slot)
        return 0;

    for (u32 i = 0; i < NOVA_PAGEFILE_PAGES; ++i) {
        if (slot_used(i))
            continue;

        slot_set(i);
        bindings[i].virt = 0;
        bindings[i].flags = 0;
        ++used_pages;
        *slot = i;
        return 1;
    }

    return 0;
}

void pagefile_free(u32 slot) {
    if (!available || !valid_slot(slot) || !slot_used(slot))
        return;

    slot_clear(slot);
    bindings[slot].virt = 0;
    bindings[slot].flags = 0;
    if (used_pages)
        --used_pages;
}

int pagefile_write(u32 slot, const void *page) {
    if (!available || !valid_slot(slot) || !page || !slot_used(slot))
        return 0;

    u64 lba = (u64)NOVA_PAGEFILE_START_LBA +
              (u64)slot * NOVA_PAGEFILE_SECTORS_PER_PAGE;

    return block_write(lba, NOVA_PAGEFILE_SECTORS_PER_PAGE, page);
}

int pagefile_read(u32 slot, void *page) {
    if (!available || !valid_slot(slot) || !page || !slot_used(slot))
        return 0;

    u64 lba = (u64)NOVA_PAGEFILE_START_LBA +
              (u64)slot * NOVA_PAGEFILE_SECTORS_PER_PAGE;

    return block_read(lba, NOVA_PAGEFILE_SECTORS_PER_PAGE, page);
}

int pagefile_bind(u32 slot, u32 virt, u32 flags) {
    if (!available || !valid_slot(slot) || !slot_used(slot) ||
        (virt & (NOVA_PAGEFILE_PAGE_SIZE - 1u)) != 0)
        return 0;

    bindings[slot].virt = virt;
    bindings[slot].flags = flags;
    return 1;
}

int pagefile_slot_for(u32 virt, u32 *slot, u32 *flags) {
    if (!available || !slot)
        return 0;

    u32 page = virt & ~(NOVA_PAGEFILE_PAGE_SIZE - 1u);
    for (u32 i = 0; i < NOVA_PAGEFILE_PAGES; ++i) {
        if (!slot_used(i) || bindings[i].virt != page)
            continue;

        *slot = i;
        if (flags)
            *flags = bindings[i].flags;
        return 1;
    }

    return 0;
}

u32 pagefile_total_pages(void) {
    return available ? NOVA_PAGEFILE_PAGES : 0;
}

u32 pagefile_used_pages(void) {
    return used_pages;
}

u32 pagefile_free_pages(void) {
    return available ? NOVA_PAGEFILE_PAGES - used_pages : 0;
}

u32 pagefile_start_lba(void) {
    return NOVA_PAGEFILE_START_LBA;
}

u32 pagefile_size_bytes(void) {
    return NOVA_PAGEFILE_PAGES * NOVA_PAGEFILE_PAGE_SIZE;
}
