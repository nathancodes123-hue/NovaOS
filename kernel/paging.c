#include "../include/nova/types.h"

#define PD_ENTRIES 1024
#define PT_ENTRIES 1024
#define MAX_KERNEL_TABLES 16

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
    while (n--) *b++ = 0;
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
    u32 pdi = virt >> 22;
    u32 pti = (virt >> 12) & 1023u;
    u32 *table;

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
    }

    table[pti] = (phys & PAGE_MASK) | (flags & (WRITE | USER)) | PRESENT;
    invlpg(virt);
    return 1;
}

int paging_unmap(u32 virt) {
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

u32 paging_directory(void) {
    return kernel_pd_phys;
}
