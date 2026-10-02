BITS 32

section .text

global _start
extern kmain
extern __bss_start
extern __bss_end

_start:
    cli
    cld

    ; Use a fixed early stack that is outside the bootloader's 0x7C00 stack
    ; and below the kernel's loaded image. This keeps startup independent of
    ; relocatable BSS symbols until the CPU is executing normally.
    mov esp, 0x90000

    ; Clear the kernel BSS so C globals/statics start at zero.
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
    call kmain

.hang:
    cli
    hlt
    jmp .hang
