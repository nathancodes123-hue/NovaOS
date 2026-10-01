BITS 16
ORG 0x7C00
start:
 cli
 xor ax,ax
 mov ds,ax
 mov es,ax
 mov ss,ax
 mov sp,0x7C00
 mov [boot_drive],dl
 mov si,msg
 call print
 mov bx,0x1000
 mov dh,64
 call load
 jmp 0x0000:0x1000
load:
 pusha
.next:
 mov ah,0x02
 mov al,1
 mov ch,0
 mov cl,[sector]
 mov dh,0
 mov dl,[boot_drive]
 mov bx,0x1000
 int 0x13
 jc disk_error
 add bx,512
 inc byte [sector]
 dec dh
 jnz .next
 popa
 ret
disk_error:
 mov si,err
 call print
 cli
 hlt
print:
 lodsb
 or al,al
 jz .done
 mov ah,0x0E
 int 0x10
 jmp print
.done: ret
boot_drive db 0
sector db 2
msg db 'NovaOS loading...',13,10,0
err db 'Disk error.',13,10,0
times 510-($-$$) db 0
dw 0xAA55
