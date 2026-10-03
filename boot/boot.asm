BITS 16
ORG 0x7C00

KERNEL_SECTORS EQU 240

start:
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl
    sti

    mov si, msg
    call print

    mov dl, [boot_drive]
    mov ah, 0x08
    int 0x13
    jc geom_error

    mov al, cl
    and al, 0x3F
    xor ah, ah
    test ax, ax
    jz geom_error
    mov [sectors_per_track], ax

    xor ax, ax
    mov al, dh
    inc ax
    mov [heads], ax

    mov si, geom_ok
    call print

    xor ax, ax
    mov [sector_index], ax

.read_next:
    call print_hex8
    mov byte [retries], 3

.read_attempt:
    ; Map each 512-byte sector at a fresh 0x20-byte segment.
    ; This keeps ES:BX from wrapping after the first 64 KiB.
    mov ax, [sector_index]
    shl ax, 5
    add ax, 0x1000
    mov es, ax
    xor bx, bx

    ; sector_index is the zero-based index within kernel.bin.
    ; boot.bin occupies disk LBA 0, so the kernel begins at LBA 1.
    xor dx, dx
    mov ax, [sector_index]
    inc ax
    div word [sectors_per_track]
    mov [sector_remainder], dx

    xor dx, dx
    div word [heads]
    mov [cylinder], ax
    mov [head], dl

    mov ax, [sector_remainder]
    inc ax
    mov [sector_number], al

    mov ax, [cylinder]
    mov ch, al
    mov cl, [sector_number]
    mov al, ah
    and al, 3
    shl al, 6
    or cl, al

    mov dh, [head]
    mov dl, [boot_drive]

    mov ax, 0x0201
    int 0x13
    jnc .read_ok

    xor ah, ah
    mov dl, [boot_drive]
    int 0x13
    dec byte [retries]
    jnz .read_attempt
    jmp disk_error

.read_ok:
    inc word [sector_index]
    cmp word [sector_index], KERNEL_SECTORS
    jb .read_next

    mov si, disk_ok
    call print

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode

geom_error:
    mov si, geom_err
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
    cld
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print
.done:
    ret

print_hex8:
    push ax
    mov al, [sector_index]
    shr al, 4
    call print_nibble
    pop ax
    mov al, [sector_index]
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

    ; Jump to the linked kernel entry using an absolute register target.
    mov eax, 0x10000
    jmp eax

BITS 16
boot_drive db 0
sector_index dw 0
retries db 0
sectors_per_track dw 0
heads dw 0
sector_remainder dw 0
cylinder dw 0
head db 0
sector_number db 0

msg db 'NovaOS booting...',13,10,0
geom_ok db ' CHS OK...',0
disk_ok db ' Disk OK',13,10,0
geom_err db ' CHS unavailable.',13,10,0
err db ' Disk read failed.',13,10,0

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
