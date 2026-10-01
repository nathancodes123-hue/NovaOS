#include "../include/nova/types.h"
#include "../include/nova/io.h"
typedef struct{u16 limit;u32 base;} __attribute__((packed)) IDTR;
typedef struct{u16 off_lo;u16 sel;u8 zero;u8 flags;u16 off_hi;} __attribute__((packed)) IDTEntry;
static IDTEntry idt[256];
static u32 irq_ticks;
static void set_gate(u8 n,void(*fn)(void)){u32 a=(u32)fn;idt[n]=(IDTEntry){a&0xffff,0x08,0,0x8e,(a>>16)&0xffff};}
static void lidt(void){IDTR r={sizeof(idt)-1,(u32)idt};__asm__ volatile("lidt %0"::"m"(r));}
static void pic_remap(void){
 u8 a=inb(0x21),b=inb(0xA1);
 outb(0x20,0x11);io_wait();outb(0xA0,0x11);io_wait();
 outb(0x21,0x20);io_wait();outb(0xA1,0x28);io_wait();
 outb(0x21,0x04);io_wait();outb(0xA1,0x02);io_wait();
 outb(0x21,0x01);io_wait();outb(0xA1,0x01);io_wait();
 outb(0x21,a&~1);outb(0xA1,b);
}
__attribute__((naked)) static void irq0(void){__asm__ volatile("pusha; incb irq_ticks; movb $0x20,%al; outb %al,$0x20; popa; iretd");}
__attribute__((naked)) static void irq1(void){__asm__ volatile("pusha; movl $0x20,%eax; outb %al,$0x20; popa; iretd");}
__attribute__((naked)) static void isr_default(void){__asm__ volatile("pusha; popa; iretd");}
void interrupts_init(void){
 for(u32 i=0;i<256;i++)set_gate(i,isr_default);
 set_gate(32,irq0);set_gate(33,irq1);pic_remap();lidt();sti();
}
u32 interrupt_ticks(void){return irq_ticks;}
