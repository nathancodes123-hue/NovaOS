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

    ; Clear .bss before entering C so static kernel state starts at zero.
    mov edi, __bss_start
    mov ecx, __bss_end
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
    ; The bootloader stack at 0x90000 overlaps the kernel's large .bss.
    ; Switch to the dedicated stack before entering C.
    mov esp, kernel_stack_top

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
