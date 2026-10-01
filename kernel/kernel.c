#include "../include/nova/types.h"
#include "../include/nova/io.h"

/* Core subsystems */
extern void mm_init(void);
extern void paging_identity_map_first_4m(void);
extern void interrupts_init(void);
extern void process_init(void);
extern void scheduler(void);
extern void vfs_init(void);
extern void gui_init(void);
extern void gui_render(void);

/*
 * NovaOS system call ABI
 *
 * User code places:
 *   EAX = syscall number
 *   EBX = arg1
 *   ECX = arg2
 *   EDX = arg3
 * and executes INT 0x80.
 *
 * Return value is placed in EAX.
 */
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
    SYS_GETTIME
};

static volatile u32 syscall_ticks;
static volatile u32 current_pid = 1;
static volatile u32 next_fd = 3;

static void putc(char c) {
    static u16 x;
    static u16 y;

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
    while (*s)
        putc(*s++);
}

static void clear_screen(void) {
    volatile u16 *vga = (volatile u16 *)0xB8000;

    for (u32 i = 0; i < 80 * 25; ++i)
        vga[i] = 0x0720;
}

static u32 string_length(const char *s) {
    u32 n = 0;

    if (!s)
        return 0;

    while (s[n])
        ++n;

    return n;
}

/*
 * Kernel-side syscall dispatcher.
 * This stays small on purpose; filesystem, process, and memory logic
 * belong to their own subsystems rather than becoming kernel.c bloat.
 */
u32 syscall_dispatch(u32 number, u32 arg1, u32 arg2, u32 arg3) {
    switch (number) {
        case SYS_EXIT:
            /* Process teardown will be connected to process.c. */
            return 0;

        case SYS_WRITE:
            if (!arg1 || !arg2)
                return (u32)-1;

            if (arg1 == 1 || arg1 == 2) {
                const char *text = (const char *)arg2;
                u32 count = arg3;
                u32 written = 0;

                while (written < count && text[written]) {
                    putc(text[written]);
                    ++written;
                }

                return written;
            }

            return (u32)-1;

        case SYS_READ:
            /*
             * Console input will be connected to the keyboard/input
             * subsystem. Return zero until data is available.
             */
            (void)arg1;
            (void)arg2;
            (void)arg3;
            return 0;

        case SYS_OPEN:
            /*
             * VFS file descriptors start at 3. The actual pathname lookup
             * lives in vfs.c; this is the ABI boundary.
             */
            if (!arg1)
                return (u32)-1;
            return next_fd++;

        case SYS_CLOSE:
            return arg1 < 3 ? (u32)-1 : 0;

        case SYS_GETPID:
            return current_pid;

        case SYS_SLEEP:
            /*
             * Timer/interrupt code owns the actual wait queue.
             * Keep the syscall ABI here without duplicating the scheduler.
             */
            syscall_ticks += arg1;
            return 0;

        case SYS_YIELD:
            scheduler();
            return 0;

        case SYS_MKDIR:
            if (!arg1)
                return (u32)-1;
            return 0;

        case SYS_CREATE:
            if (!arg1)
                return (u32)-1;
            return 0;

        case SYS_GETTIME:
            return syscall_ticks;

        default:
            return (u32)-1;
    }
}

/*
 * C entry point for the INT 0x80 assembly stub.
 * The interrupt layer can call this directly after saving registers.
 */
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
        scheduler();
        gui_render();
        hlt();
    }
}
