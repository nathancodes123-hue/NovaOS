#pragma once
#include "types.h"

typedef struct {
    u32 base;
    u32 length;
    u32 type;
} NovaMemoryRegion;

void mm_init(void);
void mm_add_region(u32 base, u32 length, u32 type);
void mm_reserve(u32 start, u32 end);

u32 mm_alloc_frame(void);
u32 mm_alloc_zeroed_frame(void);
int mm_alloc_frames(u32 count, u32 *base);
void mm_free_frame(u32 address);
void mm_free_frames(u32 base, u32 count);

int mm_region(u32 index, NovaMemoryRegion *out);
u32 mm_region_count(void);
int mm_is_reserved(u32 address);
int mm_validate(void);

u32 mm_total_count(void);
u32 mm_used_count(void);
u32 mm_free_count(void);
u32 mm_reserved_count(void);

void *kmalloc(usize size);
void *kcalloc(usize count, usize size);
void kfree(void *ptr);
u32 mm_heap_capacity(void);
u32 mm_heap_used(void);
u32 mm_heap_free(void);
u32 mm_heap_allocations(void);
