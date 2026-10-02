#pragma once
#include "types.h"

enum {
    NOVA_VFS_DIR = 0,
    NOVA_VFS_FILE = 1
};

void vfs_init(void);
int vfs_mkdir(u32 parent, const char *name);
int vfs_create(u32 parent, const char *name);
int vfs_write(u32 inode, const u8 *data, u32 n);
int vfs_read(u32 inode, u8 *data, u32 n);
int vfs_write_at(u32 inode, u32 offset, const u8 *data, u32 n);
int vfs_read_at(u32 inode, u32 offset, u8 *data, u32 n);
int vfs_truncate(u32 inode);
int vfs_stat(u32 inode, u32 *type, u32 *size, u32 *parent);
int vfs_list(u32 parent, u32 index, char *name, u32 name_size, u32 *inode, u32 *type);
u32 vfs_find(const char *name);
u32 vfs_size(u32 inode);
u32 vfs_root(void);
