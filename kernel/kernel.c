#include "../include/nova/types.h"
#include "../include/nova/io.h"
#include "../include/nova/mm.h"
#include "../include/nova/guest.h"
#include "../include/nova/interrupts.h"
#include "../include/nova/process.h"
#include "../include/nova/vfs.h"
#include "../include/nova/paging.h"
#include "../include/nova/fd.h"
#include "../include/nova/serial.h"
#include "../include/nova/rtc.h"
#include "../include/nova/pci.h"
#include "../include/nova/nsh.h"

extern void paging_identity_map_first_4m(void);
extern int process_exec_pe(const char *name);
extern u32 process_current_pid(void);
extern u32 interrupt_ticks(void);

enum { SYS_EXIT=0, SYS_WRITE, SYS_READ, SYS_OPEN, SYS_CLOSE, SYS_GETPID,
       SYS_SLEEP, SYS_YIELD, SYS_MKDIR, SYS_CREATE, SYS_GETTIME,
       SYS_MALLOC, SYS_FREE, SYS_EXEC };

static volatile u32 syscall_ticks;
static u16 cursor_x, cursor_y;

static void vga_update_cursor(void) {
    u16 position = (u16)(cursor_y * 80u + cursor_x);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(position & 0xFFu));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)(position >> 8));
}

void putc(char c) {
    volatile u16 *vga = (volatile u16 *)0xB8000;
    if (c == '\r') { vga_update_cursor(); return; }
    if (c == '\n') { cursor_x = 0; if (++cursor_y >= 25) cursor_y = 0; vga_update_cursor(); return; }
    if (c == '\b') {
        if (cursor_x) --cursor_x;
        vga[cursor_y * 80 + cursor_x] = 0x0720;
        vga_update_cursor();
        return;
    }
    vga[cursor_y * 80 + cursor_x] = (u16)(0x0700u | (u8)c);
    if (++cursor_x >= 80) { cursor_x = 0; if (++cursor_y >= 25) cursor_y = 0; }
    vga_update_cursor();
}

static void print(const char *s) { if (s) while (*s) putc(*s++); }

static void print_u32(u32 value) {
    char b[11]; u32 i=0;
    if (!value) { putc('0'); return; }
    while (value && i < sizeof(b)-1u) { b[i++]=(char)('0'+value%10u); value/=10u; }
    while (i) putc(b[--i]);
}

static void print_hex(u32 value) {
    static const char d[]="0123456789ABCDEF";
    for (int shift=28; shift>=0; shift-=4) putc(d[(value>>shift)&15u]);
}

static void clear_screen(void) {
    volatile u16 *vga=(volatile u16*)0xB8000;
    for (u32 i=0;i<80u*25u;++i) vga[i]=0x0720;
    cursor_x=cursor_y=0;
    vga_update_cursor();
}

void panic(const char *message) {
    cli();
    clear_screen();
    print("========================================\n");
    print("           NOVAOS KERNEL PANIC          \n");
    print("========================================\n\n");
    print("Reason: "); print(message ? message : "unknown fault"); print("\n");
    print("PID: "); print_u32(process_current_pid()); print("\n");
    print("Ticks: "); print_u32(interrupt_ticks()); print("\n");
    u32 cr3; __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    print("CR3: 0x"); print_hex(cr3); print("\n");
    print("Free frames: "); print_u32(mm_free_count()); print("\n");
    print("Heap used: "); print_u32(mm_heap_used()); print(" bytes\n\n");
    print("System halted. A reboot is required.\n");
    guest_debug_puts("NovaOS: KERNEL PANIC: ");
    guest_debug_puts(message ? message : "unknown fault");
    guest_debug_puts("\n");
    for (;;) hlt();
}

u32 syscall_dispatch(u32 n,u32 a1,u32 a2,u32 a3) {
    switch(n) {
    case SYS_EXIT: process_exit((int)a1); return 0;
    case SYS_WRITE:
        if(!a1||!a2||(a1!=1&&a1!=2)) return (u32)-1;
        { const char *s=(const char*)a2; u32 w=0; while(w<a3&&s[w]){putc(s[w]);++w;} return w; }
    case SYS_READ: {
        int n = fd_read((int)a1, (void *)a2, a3);
        return n < 0 ? (u32)-1 : (u32)n;
    }
    case SYS_OPEN:
        return a1 ? (u32)fd_open((const char *)a1) : (u32)-1;
    case SYS_CLOSE:
        return fd_close((int)a1) ? 0 : (u32)-1;
    case SYS_GETPID: return process_current_pid();
    case SYS_SLEEP: return process_sleep(a1) ? 0 : (u32)-1;
    case SYS_YIELD: scheduler(); return 0;
    case SYS_MKDIR:
        return a1 ? (u32)(vfs_mkdir(vfs_root(), (const char *)a1) ? 0 : -1) : (u32)-1;
    case SYS_CREATE:
        return a1 ? (u32)(vfs_create(vfs_root(), (const char *)a1) ? 0 : -1) : (u32)-1;
    case SYS_GETTIME: { u32 now = rtc_unix_seconds(); return now ? now : syscall_ticks; }
    case SYS_MALLOC: return a1?(u32)kmalloc((usize)a1):0;
    case SYS_FREE: if(a1) kfree((void*)a1); return 0;
    case SYS_EXEC: return a1?(u32)process_exec_pe((const char*)a1):(u32)-1;
    default: return (u32)-1;
    }
}

u32 syscall_handler(u32 n,u32 a1,u32 a2,u32 a3) { return syscall_dispatch(n,a1,a2,a3); }
void syscall_tick(void) { ++syscall_ticks; }

void kmain(void) {
    cli();
    clear_screen();
    print("NovaOS kernel boot\n-----------------\n");
    serial_init();
    serial_write("NovaOS kernel boot\\n");
    rtc_init();

    NovaRtcTime rtc_time;
    print("Initializing RTC... "); print(rtc_read(&rtc_time) ? "OK\n" : "unavailable\n");
    print("Initializing guest tools... ");
    guest_tools_init();
    print("OK"); if (guest_is_virtualized()) print(" (virtualized)"); print("\n");

    print("Initializing physical memory... ");
    mm_init(); print("OK\n  frames: "); print_u32(mm_total_count());
    print(" total, "); print_u32(mm_free_count()); print(" free\n  heap: ");
    print_u32(mm_heap_free()); print(" bytes available\n");

    print("Initializing paging... "); paging_identity_map_first_4m(); print("OK\n");
    print("Initializing PCI... "); pci_init(); print("OK\n");
    print("Initializing interrupts/syscalls... "); interrupts_init(); print("OK\n");
    print("Initializing processes... "); process_init(); print("OK\n");
    print("Initializing VFS... "); vfs_init(); print("OK\n");
    fd_init();

    print("Initializing Nova Shell... ");
    nsh_init();
    print("OK\n\n");
    print("nsh$ ");

    print("\nNovaOS kernel ready.\n");
    print("Starting hardware interrupts... ");
    interrupts_start();
    print("OK\n");

    for (;;) {
        nsh_run();
        hlt();
    }
}
