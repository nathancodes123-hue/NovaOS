#pragma once
#include "types.h"

enum {
    PAGING_PRESENT = 0x001u,
    PAGING_WRITE   = 0x002u,
    PAGING_USER    = 0x004u
};

void paging_identity_map_first_4m(void);

int paging_map(u32 virt, u32 phys, u32 flags);
int paging_map_range(u32 virt, u32 phys, u32 pages, u32 flags);
int paging_get(u32 virt, u32 *phys, u32 *flags);
int paging_unmap(u32 virt);
int paging_unmap_range(u32 virt, u32 pages);

int paging_alloc_page(u32 virt, u32 flags);
int paging_alloc_pages(u32 virt, u32 pages, u32 flags);
int paging_free_page(u32 virt);
int paging_free_pages(u32 virt, u32 pages);

int paging_swap_out(u32 virt, u32 slot);
int paging_swap_in(u32 virt, u32 slot, u32 flags);

u32 paging_directory(void);
u32 paging_table_count(void);
