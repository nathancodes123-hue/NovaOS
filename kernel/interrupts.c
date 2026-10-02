#include "../include/nova/types.h"
#include "../include/nova/io.h"
#include "../include/nova/interrupts.h"

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
extern void syscall_tick(void);
extern void process_tick(void);

#define KEYBUF_SIZE 128u
static IDTEntry idt[256];
static volatile u32 irq_ticks;
static volatile u8 keybuf[KEYBUF_SIZE];
static volatile u32 key_head, key_tail;
static u8 key_shift;

static void set_gate(u8 vector, void (*fn)(void), u8 flags) {
    u32 address = (u32)fn;
    idt[vector] = (IDTEntry){(u16)(address & 0xffffu), 0x08, 0, flags, (u16)(address >> 16)};
}
static void lidt(void) { IDTR idtr={(u16)(sizeof(idt)-1),(u32)idt}; __asm__ volatile("lidt %0"::"m"(idtr)); }
static void pit_init(u32 hz) {
    if (!hz) hz=100; u32 divisor=1193182u/hz;
    if (divisor<1u) divisor=1u; if (divisor>65535u) divisor=65535u;
    outb(0x43,0x36); outb(0x40,(u8)divisor); outb(0x40,(u8)(divisor>>8));
}
static void key_push(u8 c) { u32 next=(key_head+1u)%KEYBUF_SIZE; if(next==key_tail)return; keybuf[key_head]=c; key_head=next; }
static u8 scancode_ascii(u8 sc) {
    static const char lower1[]="qwertyuiop", lower2[]="asdfghjkl", lower3[]="zxcvbnm";
    if(sc>=0x10&&sc<=0x19){char c=lower1[sc-0x10];return key_shift?(u8)(c-32):(u8)c;}
    if(sc>=0x1E&&sc<=0x26){char c=lower2[sc-0x1E];return key_shift?(u8)(c-32):(u8)c;}
    if(sc>=0x2C&&sc<=0x32){char c=lower3[sc-0x2C];return key_shift?(u8)(c-32):(u8)c;}
    switch(sc){
    case 0x02:return key_shift?'!':'1'; case 0x03:return key_shift?'@':'2'; case 0x04:return key_shift?'#':'3';
    case 0x05:return key_shift?'$':'4'; case 0x06:return key_shift?'%':'5'; case 0x07:return key_shift?'^':'6';
    case 0x08:return key_shift?'&':'7'; case 0x09:return key_shift?'*':'8'; case 0x0A:return key_shift?'(':'9'; case 0x0B:return key_shift?')':'0';
    case 0x0C:return key_shift?'_':'-'; case 0x0D:return key_shift?'+':'='; case 0x0E:return '\b'; case 0x0F:return '\t';
    case 0x1A:return key_shift?'{':'['; case 0x1B:return key_shift?'}':']'; case 0x1C:return '\n';
    case 0x27:return key_shift?':':';'; case 0x29:return key_shift?'~':'`'; case 0x2B:return key_shift?'|':'\\';
    case 0x33:return key_shift?'<':','; case 0x34:return key_shift?'>':'.'; case 0x35:return key_shift?'?':'/'; case 0x39:return ' ';
    default:return 0; }
}
static void pic_remap(void){
    outb(0x20,0x11);io_wait(); outb(0xA0,0x11);io_wait(); outb(0x21,0x20);io_wait(); outb(0xA1,0x28);io_wait();
    outb(0x21,0x04);io_wait(); outb(0xA1,0x02);io_wait(); outb(0x21,0x01);io_wait(); outb(0xA1,0x01);io_wait();
    outb(0x21,0xFF); outb(0xA1,0xFF);
}
void irq_timer(void){++irq_ticks;syscall_tick();process_tick();}
void irq_keyboard(void){u8 sc=inb(0x60);if(sc==0x2A||sc==0x36){key_shift=1;return;}if(sc==0xAA||sc==0xB6){key_shift=0;return;}if(sc&0x80u)return;u8 c=scancode_ascii(sc);if(c)key_push(c);}
void exception_handler(u32 vector,u32 error){(void)error;extern void panic(const char*);if(vector==14)panic("page fault");panic("CPU exception");}
void interrupts_init(void){
    cli(); irq_ticks=0; key_head=key_tail=0; key_shift=0;
    for(u32 i=0;i<256;++i)set_gate((u8)i,isr31,0x8e);
    void(*exceptions[32])(void)={isr0,isr1,isr2,isr3,isr4,isr5,isr6,isr7,isr8,isr9,isr10,isr11,isr12,isr13,isr14,isr15,isr16,isr17,isr18,isr19,isr20,isr21,isr22,isr23,isr24,isr25,isr26,isr27,isr28,isr29,isr30,isr31};
    for(u32 i=0;i<32;++i)set_gate((u8)i,exceptions[i],0x8e);
    pic_remap(); pit_init(100); set_gate(32,irq0_stub,0x8e); set_gate(33,irq1_stub,0x8e); set_gate(128,syscall_entry,0xee); lidt();
}
void interrupts_start(void){outb(0x21,0xFC);outb(0xA1,0xFF);sti();}
u32 interrupt_ticks(void){return irq_ticks;}
int keyboard_available(void){return key_head!=key_tail;}
int keyboard_read(void){if(key_head==key_tail)return -1;u8 c=keybuf[key_tail];key_tail=(key_tail+1u)%KEYBUF_SIZE;return c;}