BITS 32

section .text

global _start
extern kmain

_start:
    call kmain

.hang:
    cli
    hlt
    jmp .hang
