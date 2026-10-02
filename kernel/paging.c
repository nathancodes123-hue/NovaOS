#include "../include/nova/types.h"
#include "../include/nova/mm.h"
#include "../include/nova/paging.h"

#define PD_ENTRIES 1024u
#define PT_ENTRIES 1024u
#define MAX_KERNEL_TABLES 16u

#define PRESENT 0x001u
#define WRITE   0x002u
#define USER    0x004u

static u32 page_directory[PD_ENTRIES] __attribute__((aligned(4096)));
static u32 first_table[PT_ENTRIES] __attribute__((aligned(4096)));
static u32 kernel_tables[MAX_KERNEL_TABLES][PT_ENTRIES] __attribute__((aligned(4096)));
static u32 kernel_table_used;
static u32 kernel_pd_phys;

static void zero(void *p, usize n) {
    u8 *b = (u8 *)p;
    while (n--)
        *b++ = 0;
}

static void invlpg(u32 address) {
    __asm__ volatile ("invlpg (%0)" :: "r"(address) : "memory");
}

static void load_cr3(u32 physical) {
    __asm__ volatile ("mov %0, %%cr3" :: "r"(physical) : "memory");
}

static u32 read_cr0(void) {
    u32 value;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(value));
    return value;
}

static void write_cr0(u32 value) {
    __asm__ volatile ("mov %0, %%cr0" :: "r"(value) : "memory");
}

static u32 page_align_down(u32 address) {
    return address & PAGE_MASK;
}

static int page_aligned(u32 address) {
    return (address & (PAGE_SIZE - 1u)) == 0;
}

void paging_identity_map_first_4m(void) {
    zero(page_directory, sizeof(page_directory));
    zero(first_table, sizeof(first_table));
    zero(kernel_tables, sizeof(kernel_tables));
    kernel_table_used = 0;

    for (u32 i = 0; i < PT_ENTRIES; ++i)
        first_table[i] = (i * PAGE_SIZE) | PRESENT | WRITE;

    page_directory[0] = ((u32)first_table) | PRESENT | WRITE;
    kernel_pd_phys = (u32)page_directory;

    load_cr3(kernel_pd_phys);
    write_cr0(read_cr0() | 0x80000000u);
}

int paging_map(u32 virt, u32 phys, u32 flags) {
    u32 pdi;
    u32 pti;
    u32 *table;

    if (!page_aligned(virt) || !page_aligned(phys))
        return 0;

    pdi = virt >> 22;
    pti = (virt >> 12) & 1023u;

    if (!(page_directory[pdi] & PRESENT)) {
        if (kernel_table_used >= MAX_KERNEL_TABLES)
            return 0;

        table = kernel_tables[kernel_table_used++];
        zero(table, PAGE_SIZE);
        page_directory[pdi] = ((u32)table) | PRESENT | WRITE;
        if (flags & USER)
            page_directory[pdi] |= USER;
    } else {
        table = (u32 *)(page_directory[pdi] & PAGE_MASK);
        if ((flags & USER) && !(page_directory[pdi] & USER))
            page_directory[pdi] |= USER;
    }

    table[pti] = phys | (flags & (WRITE | USER)) | PRESENT;
    invlpg(virt);
    return 1;
}

int paging_map_range(u32 virt, u32 phys, u32 pages, u32 flags) {
    if (!pages)
        return 0;

    for (u32 i = 0; i < pages; ++i) {
        if (virt > 0xFFFFFFFFu - i * PAGE_SIZE ||
            phys > 0xFFFFFFFFu - i * PAGE_SIZE)
            return 0;

        if (!paging_map(virt + i * PAGE_SIZE, phys + i * PAGE_SIZE, flags))
            return 0;
    }
    return 1;
}

int paging_get(u32 virt, u32 *phys, u32 *flags) {
    if (!page_aligned(virt) || !phys)
        return 0;

    u32 pdi = virt >> 22;
    u32 pti = (virt >> 12) & 1023u;

    if (!(page_directory[pdi] & PRESENT))
        return 0;

    u32 *table = (u32 *)(page_directory[pdi] & PAGE_MASK);
    u32 entry = table[pti];
    if (!(entry & PRESENT))
        return 0;

    *phys = entry & PAGE_MASK;
    if (flags)
        *flags = entry & (PRESENT | WRITE | USER);
    return 1;
}

int paging_unmap(u32 virt) {
    if (!page_aligned(virt))
        return 0;

    u32 pdi = virt >> 22;
    u32 pti = (virt >> 12) & 1023u;

    if (!(page_directory[pdi] & PRESENT))
        return 0;

    u32 *table = (u32 *)(page_directory[pdi] & PAGE_MASK);
    if (!(table[pti] & PRESENT))
        return 0;

    table[pti] = 0;
    invlpg(virt);
    return 1;
}

int paging_unmap_range(u32 virt, u32 pages) {
    if (!pages)
        return 0;

    int changed = 0;
    for (u32 i = 0; i < pages; ++i) {
        if (virt > 0xFFFFFFFFu - i * PAGE_SIZE)
            break;
        if (paging_unmap(virt + i * PAGE_SIZE))
            changed = 1;
    }
    return changed;
}

int paging_alloc_page(u32 virt, u32 flags) {
    u32 phys = mm_alloc_frame();
    if (!phys)
        return 0;

    if (!paging_map(virt, phys, flags)) {
        mm_free_frame(phys);
        return 0;
    }

    /* Zero through the mapped virtual address, not the raw physical address. */
    u8 *p = (u8 *)virt;
    for (u32 i = 0; i < PAGE_SIZE; ++i)
        p[i] = 0;

    return 1;
}

int paging_alloc_pages(u32 virt, u32 pages, u32 flags) {
    if (!pages || !page_aligned(virt))
        return 0;

    for (u32 i = 0; i < pages; ++i) {
        if (virt > 0xFFFFFFFFu - i * PAGE_SIZE ||
            !paging_alloc_page(virt + i * PAGE_SIZE, flags)) {
            if (i)
                paging_free_pages(virt, i);
            return 0;
        }
    }
    return 1;
}

int paging_free_page(u32 virt) {
    u32 phys;
    if (!paging_get(virt, &phys, NULL))
        return 0;

    if (!paging_unmap(virt))
        return 0;

    mm_free_frame(phys);
    return 1;
}

int paging_free_pages(u32 virt, u32 pages) {
    if (!pages)
        return 0;

    u32 freed = 0;
    for (u32 i = 0; i < pages; ++i) {
        if (virt > 0xFFFFFFFFu - i * PAGE_SIZE)
            break;
        if (paging_free_page(virt + i * PAGE_SIZE))
            ++freed;
    }
    return freed == pages;
}

u32 paging_directory(void) {
    return kernel_pd_phys;
}

u32 paging_table_count(void) {
    return kernel_table_used + 1u;
}
