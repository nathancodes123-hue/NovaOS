#include "../include/nova/types.h"
#include "../include/nova/io.h"

extern void mm_init(void);
extern void *kmalloc(usize size);
extern void kfree(void *ptr);
extern void paging_identity_map_first_4m(void);
extern void interrupts_init(void);
extern void process_init(void);
extern int process_exec_pe(const char *name);
extern void process_exit(int status);
extern u32 process_current_pid(void);
extern void scheduler(void);
extern void vfs_init(void);
extern void gui_init(void);
extern void gui_render(void);

enum {
    SYS_EXIT = 0,
    SYS_WRITE,
    SYS_READ,
    SYS_OPEN,
    SYS_CLOSE,
    SYS_GETPID,
    SYS_SLEEP,
    SYS_YIELD,
    SYS_MKDIR,
    SYS_CREATE,
    SYS_GETTIME,
    SYS_MALLOC,
    SYS_FREE,
    SYS_EXEC
};

static volatile u32 syscall_ticks;

static void putc(char c) {
    static u16 x, y;
    volatile u16 *vga = (volatile u16 *)0xB8000;

    if (c == '\n') {
        x = 0;
        if (++y == 25) y = 0;
        return;
    }

    vga[y * 80 + x] = (u16)(0x0700 | (u8)c);
    if (++x == 80) {
        x = 0;
        if (++y == 25) y = 0;
    }
}

static void print(const char *s) {
    while (*s) putc(*s++);
}

static void clear_screen(void) {
    volatile u16 *vga = (volatile u16 *)0xB8000;
    for (u32 i = 0; i < 80 * 25; ++i)
        vga[i] = 0x0720;
}

void panic(const char *message) {
    cli();
    clear_screen();
    print("NovaOS kernel panic:\n");
    print(message ? message : "unknown fault");
    for (;;)
        hlt();
}

u32 syscall_dispatch(u32 number, u32 arg1, u32 arg2, u32 arg3) {
    switch (number) {
        case SYS_EXIT:
            process_exit((int)arg1);
            return 0;

        case SYS_WRITE: {
            if (!arg1 || !arg2 || (arg1 != 1 && arg1 != 2))
                return (u32)-1;

            const char *text = (const char *)arg2;
            u32 written = 0;
            while (written < arg3 && text[written]) {
                putc(text[written]);
                ++written;
            }
            return written;
        }

        case SYS_READ:
            return 0;

        case SYS_OPEN:
            return arg1 ? 3 : (u32)-1;

        case SYS_CLOSE:
            return arg1 >= 3 ? 0 : (u32)-1;

        case SYS_GETPID:
            return process_current_pid();

        case SYS_SLEEP:
            return syscall_ticks + arg1;

        case SYS_YIELD:
            /* Real context switching is not implemented yet. */
            return 0;

        case SYS_MKDIR:
            return arg1 ? 0 : (u32)-1;

        case SYS_CREATE:
            return arg1 ? 0 : (u32)-1;

        case SYS_GETTIME:
            return syscall_ticks;

        case SYS_MALLOC:
            return arg1 ? (u32)kmalloc((usize)arg1) : 0;

        case SYS_FREE:
            if (arg1)
                kfree((void *)arg1);
            return 0;

        case SYS_EXEC:
            if (!arg1)
                return (u32)-1;
            return (u32)process_exec_pe((const char *)arg1);

        default:
            return (u32)-1;
    }
}

u32 syscall_handler(u32 number, u32 arg1, u32 arg2, u32 arg3) {
    return syscall_dispatch(number, arg1, arg2, arg3);
}

void syscall_tick(void) {
    ++syscall_ticks;
}

void kmain(void) {
    clear_screen();

    print("NovaOS\n");
    print("Initializing memory... ");
    mm_init();
    print("OK\n");

    print("Initializing paging... ");
    paging_identity_map_first_4m();
    print("OK\n");

    print("Initializing interrupts/syscalls... ");
    interrupts_init();
    print("OK\n");

    print("Initializing processes... ");
    process_init();
    print("OK\n");

    print("Initializing VFS... ");
    vfs_init();
    print("OK\n");

    print("Initializing GUI... ");
    gui_init();
    print("OK\n");

    print("\nNovaOS kernel ready.\n");

    for (;;) {
        gui_render();
        hlt();
    }
}
