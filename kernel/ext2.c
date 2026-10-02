#include "../include/nova/types.h"
#include "../include/nova/block.h"
#include "../include/nova/ext2.h"

#define EXT2_MAGIC 0xEF53u
#define EXT2_DIRECT 12u
#define EXT2_INODE_SIZE_DEFAULT 128u
#define EXT2_ROOT_INODE 2u

typedef struct {
    u32 inodes_count;
    u32 blocks_count;
    u32 reserved_blocks;
    u32 free_blocks;
    u32 free_inodes;
    u32 first_data_block;
    u32 log_block_size;
    u32 log_frag_size;
    u32 blocks_per_group;
    u32 frags_per_group;
    u32 inodes_per_group;
    u16 magic;
    u16 state;
    u32 inode_size;
} __attribute__((packed)) Ext2Super;

typedef struct {
    u32 block_bitmap;
    u32 inode_bitmap;
    u32 inode_table;
    u16 free_blocks;
    u16 free_inodes;
    u16 used_dirs;
    u16 pad;
    u8 reserved[12];
} __attribute__((packed)) Ext2Group;

typedef struct {
    u16 mode;
    u16 uid;
    u32 size;
    u32 atime;
    u32 ctime;
    u32 mtime;
    u32 dtime;
    u16 gid;
    u16 links;
    u32 blocks;
    u32 flags;
    u32 osd1;
    u32 block[15];
    u32 generation;
    u32 file_acl;
    u32 dir_acl;
    u32 faddr;
    u8 osd2[12];
} __attribute__((packed)) Ext2Inode;

static NovaExt2Info info;
static Ext2Super super;
static u32 group_count;
static u32 group_table_sectors;
static u8 block_buffer[4096];
static u8 inode_buffer[256];

static int read_fs_block(u32 block, void *buffer) {
    if (!info.mounted || info.block_size < 512u || info.block_size > sizeof(block_buffer))
        return 0;
    u32 sectors = info.block_size / 512u;
    u64 lba = (u64)block * sectors;
    return block_read(lba, sectors, buffer);
}

static int read_group(u32 group, Ext2Group *out) {
    if (!out || group >= group_count)
        return 0;

    u32 table_block = super.first_data_block + 1u;
    u32 per_block = info.block_size / sizeof(Ext2Group);
    u32 block = table_block + group / per_block;
    u32 index = group % per_block;

    if (!read_fs_block(block, block_buffer))
        return 0;

    *out = ((Ext2Group *)block_buffer)[index];
    return 1;
}

static int inode_location(u32 inode, u32 *block, u32 *offset) {
    if (!inode || inode > super.inodes_count || !block || !offset)
        return 0;

    u32 index = inode - 1u;
    u32 group = index / super.inodes_per_group;
    u32 local = index % super.inodes_per_group;
    Ext2Group gd;

    if (!read_group(group, &gd))
        return 0;

    u32 per_block = info.block_size / info.inode_size;
    *block = gd.inode_table + local / per_block;
    *offset = (local % per_block) * info.inode_size;
    return 1;
}

void ext2_init(void) {
    info = (NovaExt2Info){0};
    super = (Ext2Super){0};
    group_count = 0;

    const NovaBlockDevice *dev = block_device();
    if (!dev || !dev->present)
        return;

    /*
     * ext2 superblock starts at byte 1024. For 512-byte sectors this is
     * LBA 2, so read two sectors into the superblock structure.
     */
    u8 raw[1024];
    if (!block_read(2, 2, raw))
        return;

    for (u32 i = 0; i < sizeof(Ext2Super); ++i)
        ((u8 *)&super)[i] = raw[i];

    if (super.magic != EXT2_MAGIC || !super.blocks_count ||
        !super.inodes_count || super.log_block_size > 2u)
        return;

    info.block_size = 1024u << super.log_block_size;
    info.inode_size = super.inode_size ? super.inode_size : EXT2_INODE_SIZE_DEFAULT;
    if (info.inode_size < EXT2_INODE_SIZE_DEFAULT ||
        info.inode_size > sizeof(inode_buffer) ||
        (info.block_size % info.inode_size))
        return;

    info.inodes = super.inodes_count;
    info.blocks = super.blocks_count;
    info.free_blocks = super.free_blocks;
    info.free_inodes = super.free_inodes;
    info.blocks_per_group = super.blocks_per_group;
    info.inodes_per_group = super.inodes_per_group;

    if (!info.blocks_per_group || !info.inodes_per_group)
        return;

    group_count = (super.blocks_count + super.blocks_per_group - 1u) /
                  super.blocks_per_group;
    group_table_sectors = group_count * sizeof(Ext2Group) / 512u;
    if (group_count * sizeof(Ext2Group) > 65536u)
        return;

    info.mounted = 1;
}

int ext2_mounted(void) {
    return info.mounted != 0;
}

const NovaExt2Info *ext2_info(void) {
    return &info;
}

int ext2_read_inode(u32 inode, void *buffer, u32 buffer_size) {
    if (!info.mounted || !buffer || !buffer_size)
        return 0;

    u32 block, offset;
    if (!inode_location(inode, &block, &offset))
        return 0;

    if (!read_fs_block(block, block_buffer))
        return 0;

    u32 available = info.inode_size;
    if (available > buffer_size)
        available = buffer_size;

    for (u32 i = 0; i < available; ++i)
        ((u8 *)buffer)[i] = block_buffer[offset + i];
    return (int)available;
}

static int inode_block(const Ext2Inode *node, u32 index, u32 *out) {
    if (!node || !out)
        return 0;

    if (index < EXT2_DIRECT) {
        *out = node->block[index];
        return 1;
    }

    u32 per_block = info.block_size / sizeof(u32);
    u32 remaining = index - EXT2_DIRECT;

    if (remaining < per_block) {
        if (!node->block[12])
            return 0;
        if (!read_fs_block(node->block[12], block_buffer))
            return 0;
        *out = ((u32 *)block_buffer)[remaining];
        return 1;
    }

    return 0;
}

int ext2_read_file(u32 inode, u32 offset, void *buffer, u32 size) {
    if (!info.mounted || !buffer || !size)
        return 0;

    if (ext2_read_inode(inode, inode_buffer, sizeof(inode_buffer)) < 0)
        return 0;

    Ext2Inode node;
    for (u32 i = 0; i < sizeof(node); ++i)
        ((u8 *)&node)[i] = inode_buffer[i];

    if (offset >= node.size)
        return 0;
    if (size > node.size - offset)
        size = node.size - offset;

    u8 *out = (u8 *)buffer;
    u32 done = 0;

    while (done < size) {
        u32 absolute = offset + done;
        u32 block_index = absolute / info.block_size;
        u32 block_offset = absolute % info.block_size;
        u32 chunk = info.block_size - block_offset;
        if (chunk > size - done)
            chunk = size - done;

        u32 disk_block;
        if (!inode_block(&node, block_index, &disk_block) || !disk_block)
            break;
        if (!read_fs_block(disk_block, block_buffer))
            break;

        for (u32 i = 0; i < chunk; ++i)
            out[done + i] = block_buffer[block_offset + i];
        done += chunk;
    }

    return (int)done;
}
