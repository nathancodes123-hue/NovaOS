CC ?= gcc
LD ?= ld
CFLAGS=-m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -O2 -Wall -Wextra
LDFLAGS=-m elf_i386 -T linker.ld

all: novaos.bin

boot.bin: boot/boot.asm
	nasm -f bin $< -o $@

kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: kernel.o linker.ld
	$(LD) $(LDFLAGS) kernel.o -o kernel.elf
	objcopy -O binary kernel.elf $@

novaos.bin: boot.bin kernel.bin
	cat boot.bin kernel.bin > $@
	truncate -s 65536 $@

run: novaos.bin
	qemu-system-i386 -drive format=raw,file=novaos.bin

clean:
	rm -f boot.bin kernel.o kernel.elf kernel.bin novaos.bin
