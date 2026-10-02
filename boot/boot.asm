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
    sti

    mov si, msg
    call print

    mov si, step1
    call print

    mov dl, [boot_drive]
    mov ah, 0x41
    mov bx, 0x55AA
    int 0x13
    jc lba_error
    cmp bx, 0xAA55
    jne lba_error
    test cx, 1
    jz lba_error

    mov si, step2
    call print

    mov word [dap.count], 32
    mov word [dap.offset], KERNEL_LOAD
    mov word [dap.segment], 0
    mov dword [dap.lba_low], 1
    mov dword [dap.lba_high], 0

    ; First 32 sectors: 0x1000 -> 0x5000
    call read_chunk
    jc disk_error

    ; Second 32 sectors: 0x5000 -> 0x9000
    add word [dap.offset], 0x4000
    add dword [dap.lba_low], 32
    call read_chunk
    jc disk_error

    ; Third 32 sectors: 0x9000 -> 0xD000
    add word [dap.offset], 0x4000
    add dword [dap.lba_low], 32
    call read_chunk
    jc disk_error

    ; Final 24 sectors: 0xD000 -> exactly 0x10000.
    ; Do not use 32 here: that would cross the 64 KiB boundary.
    mov word [dap.count], 24
    add word [dap.offset], 0x4000
    add dword [dap.lba_low], 32
    call read_chunk
    jc disk_error

    mov si, read_ok_msg
    call print

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode

read_chunk:
    mov si, step3
    call print

    mov dl, [boot_drive]
    mov si, dap
    mov ah, 0x42
    int 0x13
    ret

lba_error:
    mov si, lba_err
    call print
    jmp halt

disk_error:
    mov si, err
    call print
halt:
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
    jmp KERNEL_LOAD
.halt32:
    cli
    hlt
    jmp .halt32

BITS 16
boot_drive db 0
msg db 'NovaOS booting...',13,10,0
step1 db ' BIOS...',0
step2 db ' LBA OK...',0
step3 db ' R',0
read_ok_msg db ' Disk OK',13,10,0
lba_err db ' LBA unavailable.',13,10,0
err db ' Disk read failed.',13,10,0

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
