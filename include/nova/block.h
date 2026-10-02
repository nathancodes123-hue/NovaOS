#pragma once
#include "types.h"

#define NOVA_BLOCK_SIZE 512u

typedef struct {
    u32 sector_size;
    u64 sector_count;
    u8 present;
} NovaBlockDevice;

void block_init(void);
const NovaBlockDevice *block_device(void);
int block_read(u64 lba, u32 count, void *buffer);
int block_write(u64 lba, u32 count, const void *buffer);
