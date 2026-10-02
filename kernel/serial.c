#include "../include/nova/types.h"
#include "../include/nova/serial.h"

#define COM1 0x3F8

static int ready;

static void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0,%1" :: "a"(value), "Nd"(port));
}

static u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1,%0" : "=a"(value) : "Nd"(port));
    return value;
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
    ready = 1;
}

int serial_ready(void) {
    return ready;
}

void serial_putc(char c) {
    if (!ready)
        return;
    if (c == '\n')
        serial_putc('\r');
    for (u32 i = 0; i < 100000u; ++i)
        if (inb(COM1 + 5) & 0x20)
            break;
    outb(COM1, (u8)c);
}

void serial_write(const char *text) {
    if (!text)
        return;
    while (*text)
        serial_putc(*text++);
}
