#pragma once
#include "types.h"

typedef struct {
    u32 inodes;
    u32 blocks;
    u32 free_blocks;
    u32 free_inodes;
    u32 block_size;
    u32 inode_size;
    u32 blocks_per_group;
    u32 inodes_per_group;
    u8 mounted;
} NovaExt2Info;

void ext2_init(void);
int ext2_mounted(void);
const NovaExt2Info *ext2_info(void);
int ext2_read_inode(u32 inode, void *buffer, u32 buffer_size);
int ext2_read_file(u32 inode, u32 offset, void *buffer, u32 size);
