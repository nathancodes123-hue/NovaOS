BITS 16
ORG 0x7C00

KERNEL_SECTORS EQU 120
KERNEL_LOAD   EQU 0x1000
DISK_RETRIES  EQU 3

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

    ; Get BIOS CHS geometry.
    mov dl, [boot_drive]
    mov ah, 0x08
    int 0x13
    jc disk_error

    and cl, 0x3F
    mov [sectors_per_track], cl
    mov [max_head], dh

    ; Start at CHS 0/0/2 (sector 1 is the boot sector).
    mov byte [current_sector], 2
    mov byte [current_head], 0
    mov word [current_cylinder], 0
    mov word [remaining_sectors], KERNEL_SECTORS
    mov bx, KERNEL_LOAD

.read_sector:
    mov byte [retry_count], DISK_RETRIES

.retry:
    mov ah, 0x02
    mov al, 1
    mov ch, byte [current_cylinder]
    mov cl, [current_sector]
    mov dh, [current_head]
    mov dl, [boot_drive]
    int 0x13
    jnc .sector_ok

    xor ah, ah
    mov dl, [boot_drive]
    int 0x13
    dec byte [retry_count]
    jnz .retry
    jmp disk_error

.sector_ok:
    add bx, 512
    dec word [remaining_sectors]
    jz .all_read

    inc byte [current_sector]
    mov al, [current_sector]
    cmp al, [sectors_per_track]
    jbe .read_sector

    mov byte [current_sector], 1
    inc byte [current_head]
    mov al, [current_head]
    cmp al, [max_head]
    jbe .read_sector

    mov byte [current_head], 0
    inc word [current_cylinder]
    jmp .read_sector

.all_read:
    mov si, read_ok_msg
    call print

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
retry_count db 0
sectors_per_track db 0
max_head db 0
current_sector db 0
current_head db 0
current_cylinder dw 0
remaining_sectors dw 0
msg db 'NovaOS booting...',13,10,0
read_ok_msg db 'Disk OK',13,10,0
err db 'NovaOS disk read failed.',13,10,0

align 4
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
