#pragma once
#include "types.h"

#define NOVA_MAX_FDS 64u
#define NOVA_FD_STDIN 0u
#define NOVA_FD_STDOUT 1u
#define NOVA_FD_STDERR 2u

void fd_init(void);
int fd_open(const char *name);
int fd_close(int fd);
int fd_read(int fd, void *buffer, u32 count);
int fd_write(int fd, const void *buffer, u32 count);
u32 fd_inode(int fd);
int fd_valid(int fd);
