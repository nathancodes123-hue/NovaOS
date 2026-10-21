#include "../include/nova/types.h"
#include "../include/nova/ata.h"
#include "../include/nova/block.h"

static NovaBlockDevice device;

void block_init(void) {
    device.sector_size = NOVA_BLOCK_SIZE;
    device.sector_count = 0;
    device.present = (u8)ata_present();
}

const NovaBlockDevice *block_device(void) {
    return &device;
}

int block_read(u64 lba, u32 count, void *buffer) {
    if (!device.present || lba > 0xFFFFFFFFull || !count)
        return 0;

    while (count) {
        u8 chunk = count > 255u ? 255u : (u8)count;
        if (!ata_read28((u32)lba, chunk, buffer))
            return 0;
        lba += chunk;
        count -= chunk;
        buffer = (u8 *)buffer + (u32)chunk * NOVA_BLOCK_SIZE;
    }
    return 1;
}

int block_write(u64 lba, u32 count, const void *buffer) {
    if (!device.present || lba > 0xFFFFFFFFull || !count)
        return 0;

    while (count) {
        u8 chunk = count > 255u ? 255u : (u8)count;
        if (!ata_write28((u32)lba, chunk, buffer))
            return 0;
        lba += chunk;
        count -= chunk;
        buffer = (const u8 *)buffer + (u32)chunk * NOVA_BLOCK_SIZE;
    }
    return 1;
}
