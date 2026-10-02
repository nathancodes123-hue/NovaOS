BITS 16
ORG 0x7C00

KERNEL_SECTORS EQU 120

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
    xor ax, ax
    mov [sector_index], ax

.read_next:
    mov si, step3
    call print_hex8

    mov ax, [sector_index]
    shl ax, 5
    add ax, 0x0100
    mov [dap.segment], ax

    mov word [dap.count], 1
    mov word [dap.offset], 0

    xor eax, eax
    mov ax, [sector_index]
    inc eax
    mov [dap.lba_low], eax
    mov dword [dap.lba_high], 0

    mov byte [retries], 3

.try_read:
    mov dl, [boot_drive]
    mov si, dap
    mov ah, 0x42
    int 0x13
    jnc .read_ok

    xor ah, ah
    int 0x13
    dec byte [retries]
    jnz .try_read
    jmp disk_error

.read_ok:
    inc word [sector_index]
    cmp word [sector_index], KERNEL_SECTORS
    jb .read_next

    mov si, read_ok_msg
    call print
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode

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

print_hex8:
    mov al, [sector_index]
    mov ah, al
    shr al, 4
    call print_nibble
    mov al, ah
    and al, 0x0F
    call print_nibble
    mov al, ' '
    mov ah, 0x0E
    int 0x10
    ret

print_nibble:
    cmp al, 10
    jb .digit
    add al, 'A' - 10
    jmp .out
.digit:
    add al, '0'
.out:
    mov ah, 0x0E
    int 0x10
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
    jmp 0x1000

BITS 16
boot_drive db 0
sector_index dw 0
retries db 0

msg db 'NovaOS booting...',13,10,0
step1 db ' BIOS...',0
step2 db ' LBA OK...',0
step3 db ' ',0
read_ok_msg db ' Disk OK',13,10,0
lba_err db ' LBA unavailable.',13,10,0
err db ' Disk read failed.',13,10,0

align 4
dap:
    db 0x10, 0
.count: dw 1
.offset: dw 0
.segment: dw 0
.lba_low: dd 1
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
