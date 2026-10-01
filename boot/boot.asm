BITS 16
ORG 0x7C00

KERNEL_SECTORS EQU 120
KERNEL_LOAD   EQU 0x1000

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl

    mov si, msg
    call print

    mov word [dap.count], KERNEL_SECTORS
    mov word [dap.offset], KERNEL_LOAD
    mov word [dap.segment], 0x0000
    mov dword [dap.lba_low], 1
    mov dword [dap.lba_high], 0

    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error

    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode

disk_error:
    mov si, err
    call print
    cli
.hang:
    hlt
    jmp .hang

print:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print
.done:
    ret

BITS 32
protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    call KERNEL_LOAD
.halt:
    cli
    hlt
    jmp .halt

BITS 16
boot_drive db 0
msg db 'NovaOS booting...',13,10,0
err db 'NovaOS disk read failed.',13,10,0

align 4
dap:
    db 0x10, 0
.count: dw 0
.offset: dw 0
.segment: dw 0
.lba_low: dd 0
.lba_high: dd 0

align 8
gdt:
    dq 0
    dw 0xFFFF, 0
    db 0, 0x9A, 0xCF, 0
    dw 0xFFFF, 0
    db 0, 0x92, 0xCF, 0
gdt_descriptor:
    dw gdt_descriptor - gdt - 1
    dd gdt

times 510-($-$$) db 0
dw 0xAA55
