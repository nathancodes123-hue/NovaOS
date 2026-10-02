BITS 32

section .text

global _start
extern kmain
extern __bss_start
extern __bss_end
extern kernel_stack_top

_start:
    cli
    cld

    ; Move off the bootloader stack before touching the kernel BSS.
    ; LEA makes these linker symbols explicit addresses, not memory loads.
    lea esp, [kernel_stack_top]
    lea edi, [__bss_start]
    lea ecx, [__bss_end]
    sub ecx, edi

    xor eax, eax
    test ecx, ecx
    jz .bss_done

    mov edx, ecx
    shr ecx, 2
    rep stosd

    mov ecx, edx
    and ecx, 3
    rep stosb

.bss_done:
    call kmain

.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
kernel_stack:
    resb 16384
kernel_stack_top:
