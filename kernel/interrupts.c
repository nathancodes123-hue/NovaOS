#include "../include/nova/types.h"
#include "../include/nova/io.h"

typedef struct { u16 limit; u32 base; } __attribute__((packed)) IDTR;
typedef struct { u16 off_lo; u16 sel; u8 zero; u8 flags; u16 off_hi; } __attribute__((packed)) IDTEntry;

extern void isr0(void); extern void isr1(void); extern void isr2(void); extern void isr3(void);
extern void isr4(void); extern void isr5(void); extern void isr6(void); extern void isr7(void);
extern void isr8(void); extern void isr9(void); extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);
extern void irq0_stub(void); extern void irq1_stub(void); extern void syscall_entry(void);

static IDTEntry idt[256];
static volatile u32 irq_ticks;

static void set_gate(u8 vector, void (*fn)(void), u8 flags) {
    u32 address = (u32)fn;
    idt[vector] = (IDTEntry){
        (u16)(address & 0xffffu), 0x08, 0, flags, (u16)(address >> 16)
    };
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
    extern void panic(const char *);
    if (vector == 14)
        panic("page fault");
    panic("CPU exception");
}

void interrupts_init(void) {
    for (u32 i = 0; i < 256; ++i)
        set_gate((u8)i, isr31, 0x8e);

    void (*exceptions[32])(void) = {
        isr0,isr1,isr2,isr3,isr4,isr5,isr6,isr7,
        isr8,isr9,isr10,isr11,isr12,isr13,isr14,isr15,
        isr16,isr17,isr18,isr19,isr20,isr21,isr22,isr23,
        isr24,isr25,isr26,isr27,isr28,isr29,isr30,isr31
    };

    for (u32 i = 0; i < 32; ++i)
        set_gate((u8)i, exceptions[i], 0x8e);

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
