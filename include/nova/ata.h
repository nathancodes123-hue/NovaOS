#pragma once
#include "types.h"

void ata_init(void);
int ata_present(void);
int ata_read28(u32 lba, u8 count, void *buffer);
int ata_write28(u32 lba, u8 count, const void *buffer);
