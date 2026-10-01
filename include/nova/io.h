#pragma once
#include "types.h"
static inline void outb(u16 port,u8 value){__asm__ volatile("outb %0,%1"::"a"(value),"Nd"(port));}
static inline u8 inb(u16 port){u8 v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port));return v;}
static inline void outw(u16 port,u16 value){__asm__ volatile("outw %0,%1"::"a"(value),"Nd"(port));}
static inline void io_wait(void){outb(0x80,0);}
static inline void cli(void){__asm__ volatile("cli");}
static inline void sti(void){__asm__ volatile("sti");}
static inline void hlt(void){__asm__ volatile("hlt");}
