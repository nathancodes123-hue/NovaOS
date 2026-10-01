BITS 32

section .text

extern exception_handler
extern irq_timer
extern irq_keyboard
extern syscall_handler

%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1
    jmp isr_common
%endmacro

isr_common:
    cld
    pusha
    push dword [esp + 36]
    push dword [esp + 32]
    call exception_handler
    add esp, 8
    popa
    add esp, 8
    iretd

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR 8
ISR_NOERR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

global irq0_stub
irq0_stub:
    cld
    pusha
    call irq_timer
    popa
    mov al, 0x20
    out 0x20, al
    iretd

global irq1_stub
irq1_stub:
    cld
    pusha
    call irq_keyboard
    popa
    mov al, 0x20
    out 0x20, al
    iretd

global syscall_entry
syscall_entry:
    cld
    pusha
    push edx
    push ecx
    push ebx
    push eax
    call syscall_handler
    add esp, 16
    mov [esp + 28], eax
    popa
    iretd
