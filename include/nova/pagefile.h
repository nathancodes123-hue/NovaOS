#pragma once
#include "types.h"

#define NOVA_PAGEFILE_PAGE_SIZE 4096u
#define NOVA_PAGEFILE_START_LBA 256u
#define NOVA_PAGEFILE_PAGES 2048u
#define NOVA_PAGEFILE_SECTORS_PER_PAGE 8u

void pagefile_init(void);
int pagefile_available(void);

int pagefile_alloc(u32 *slot);
void pagefile_free(u32 slot);
int pagefile_write(u32 slot, const void *page);
int pagefile_read(u32 slot, void *page);
int pagefile_bind(u32 slot, u32 virt, u32 flags);
int pagefile_slot_for(u32 virt, u32 *slot, u32 *flags);

u32 pagefile_total_pages(void);
u32 pagefile_used_pages(void);
u32 pagefile_free_pages(void);
u32 pagefile_start_lba(void);
u32 pagefile_size_bytes(void);
