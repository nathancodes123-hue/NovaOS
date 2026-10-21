#include "../include/nova/types.h"
#include "../include/nova/fd.h"
#include "../include/nova/vfs.h"

typedef struct {
    u8 used;
    u8 readable;
    u8 writable;
    u32 inode;
    u32 offset;
} FileHandle;

static FileHandle handles[NOVA_MAX_FDS];

void fd_init(void) {
    for (u32 i = 0; i < NOVA_MAX_FDS; ++i)
        handles[i] = (FileHandle){0,0,0,0,0};

    /* Reserve the conventional standard streams for future console devices. */
    handles[NOVA_FD_STDIN].used = 1;
    handles[NOVA_FD_STDIN].readable = 1;
    handles[NOVA_FD_STDOUT].used = 1;
    handles[NOVA_FD_STDOUT].writable = 1;
    handles[NOVA_FD_STDERR].used = 1;
    handles[NOVA_FD_STDERR].writable = 1;
}

int fd_open(const char *name) {
    if (!name)
        return -1;

    u32 inode = vfs_find(name);
    if (!inode)
        return -1;

    u32 type = 0;
    if (!vfs_stat(inode, &type, NULL, NULL) || type != NOVA_VFS_FILE)
        return -1;

    for (u32 i = 3; i < NOVA_MAX_FDS; ++i) {
        if (handles[i].used)
            continue;
        handles[i].used = 1;
        handles[i].readable = 1;
        handles[i].writable = 1;
        handles[i].inode = inode;
        handles[i].offset = 0;
        return (int)i;
    }
    return -1;
}

int fd_close(int fd) {
    if (fd < 3 || (u32)fd >= NOVA_MAX_FDS || !handles[fd].used)
        return 0;
    handles[fd] = (FileHandle){0,0,0,0,0};
    return 1;
}

int fd_read(int fd, void *buffer, u32 count) {
    if (!fd_valid(fd) || !handles[fd].readable || !buffer)
        return -1;

    int n = vfs_read_at(handles[fd].inode, handles[fd].offset,
                        (u8 *)buffer, count);
    if (n > 0)
        handles[fd].offset += (u32)n;
    return n;
}

int fd_write(int fd, const void *buffer, u32 count) {
    if (!fd_valid(fd) || !handles[fd].writable || !buffer)
        return -1;

    int n = vfs_write_at(handles[fd].inode, handles[fd].offset,
                         (const u8 *)buffer, count);
    if (n > 0)
        handles[fd].offset += (u32)n;
    return n;
}

u32 fd_inode(int fd) {
    return fd_valid(fd) ? handles[fd].inode : 0;
}

int fd_valid(int fd) {
    return fd >= 0 && (u32)fd < NOVA_MAX_FDS && handles[fd].used;
}
