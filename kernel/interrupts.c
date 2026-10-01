#include "../include/nova/types.h"
#include "../include/nova/io.h"

typedef struct {
    u16 limit;
    u32 base;
} __attribute__((packed)) IDTR;

typedef struct {
    u16 off_lo;
    u16 sel;
    u8 zero;
    u8 flags;
    u16 off_hi;
} __attribute__((packed)) IDTEntry;

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);
extern void irq0_stub(void);
extern void irq1_stub(void);
extern void syscall_entry(void);

static IDTEntry idt[256];
static volatile u32 irq_ticks;

static void set_gate(u8 vector, void (*fn)(void), u8 flags) {
    u32 address = (u32)fn;
    idt[vector].off_lo = (u16)(address & 0xffffu);
    idt[vector].sel = 0x08;
    idt[vector].zero = 0;
    idt[vector].flags = flags;
    idt[vector].off_hi = (u16)(address >> 16);
}

static void lidt(void) {
    IDTR idtr = { (u16)(sizeof(idt) - 1), (u32)idt };
    __asm__ volatile ("lidt %0" :: "m"(idtr));
}

static void pic_remap(void) {
    u8 master_mask = inb(0x21);
    u8 slave_mask = inb(0xA1);

    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait();
    outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();

    outb(0x21, (u8)(master_mask & ~1u));
    outb(0xA1, slave_mask);
}

void irq_timer(void) {
    ++irq_ticks;
}

void irq_keyboard(void) {
    (void)inb(0x60);
}

void exception_handler(u32 vector, u32 error) {
    (void)error;
    /* Keep exceptions contained until a real panic console is available. */
    if (vector == 14)
        cli();
}

void interrupts_init(void) {
    for (u32 i = 0; i < 256; ++i)
        set_gate((u8)i, isr31, 0x8e);

    set_gate(0, isr0, 0x8e);
    set_gate(1, isr1, 0x8e);
    set_gate(2, isr2, 0x8e);
    set_gate(3, isr3, 0x8e);
    set_gate(4, isr4, 0x8e);
    set_gate(5, isr5, 0x8e);
    set_gate(6, isr6, 0x8e);
    set_gate(7, isr7, 0x8e);
    set_gate(8, isr8, 0x8e);
    set_gate(9, isr9, 0x8e);
    set_gate(10, isr10, 0x8e);
    set_gate(11, isr11, 0x8e);
    set_gate(12, isr12, 0x8e);
    set_gate(13, isr13, 0x8e);
    set_gate(14, isr14, 0x8e);
    set_gate(15, isr15, 0x8e);
    set_gate(16, isr16, 0x8e);
    set_gate(17, isr17, 0x8e);
    set_gate(18, isr18, 0x8e);
    set_gate(19, isr19, 0x8e);
    set_gate(20, isr20, 0x8e);
    set_gate(21, isr21, 0x8e);
    set_gate(22, isr22, 0x8e);
    set_gate(23, isr23, 0x8e);
    set_gate(24, isr24, 0x8e);
    set_gate(25, isr25, 0x8e);
    set_gate(26, isr26, 0x8e);
    set_gate(27, isr27, 0x8e);
    set_gate(28, isr28, 0x8e);
    set_gate(29, isr29, 0x8e);
    set_gate(30, isr30, 0x8e);
    set_gate(31, isr31, 0x8e);

    pic_remap();
    set_gate(32, irq0_stub, 0x8e);
    set_gate(33, irq1_stub, 0x8e);
    set_gate(128, syscall_entry, 0xee);

    lidt();
    sti();
}

u32 interrupt_ticks(void) {
    return irq_ticks;
}
