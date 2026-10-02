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
int mm_alloc_frames(u32 count, u32 *base);
void mm_free_frame(u32 address);
void mm_free_frames(u32 base, u32 count);
u32 mm_total_count(void);
u32 mm_free_count(void);
u32 mm_reserved_count(void);
u32 mm_heap_used(void);
u32 mm_heap_free(void);
void *kmalloc(usize size);
void kfree(void *ptr);
