BITS 32

section .text

global _start
extern kmain
extern __bss_start
extern __bss_end

_start:
    cli
    cld

    ; Keep the early kernel stack below the kernel image/BSS.
    ; The kernel heap is a 1 MiB BSS object, so the old 0x90000
    ; stack address could be overwritten while BSS/heap is cleared.
    mov esp, 0x9000

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
