.section .text

.macro ISR_NOERR n
.globl isr\n
.type isr\n, @function
isr\n:
    pushl $0
    pushl $\n
    jmp isr_common
.endm

.macro ISR_ERR n
.globl isr\n
.type isr\n, @function
isr\n:
    pushl $\n
    jmp isr_common
.endm

isr_common:
    pusha
    pushl 36(%esp)
    pushl 32(%esp)
    call exception_handler
    addl $8, %esp
    popa
    addl $8, %esp
    iretd

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
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

.globl irq0_stub
.type irq0_stub, @function
irq0_stub:
    pusha
    call irq_timer
    popa
    movb $0x20, %al
    outb %al, $0x20
    iretd

.globl irq1_stub
.type irq1_stub, @function
irq1_stub:
    pusha
    call irq_keyboard
    popa
    movb $0x20, %al
    outb %al, $0x20
    iretd

.globl syscall_entry
.type syscall_entry, @function
syscall_entry:
    pusha
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax
    call syscall_handler
    addl $16, %esp
    movl %eax, 28(%esp)
    popa
    iretd
