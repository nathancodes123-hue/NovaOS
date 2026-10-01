CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy
CFLAGS=-m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -O2 -Wall -Wextra -Iinclude
LDFLAGS=-m elf_i386 -T linker.ld

KERNEL_OBJS=kernel/kernel.o kernel/mm.o kernel/paging.o kernel/interrupts.o kernel/process.o kernel/vfs.o kernel/gui.o

all: novaos.bin

boot.bin: boot/boot.asm
	nasm -f bin $< -o $@

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: $(KERNEL_OBJS) linker.ld
	$(LD) $(LDFLAGS) $(KERNEL_OBJS) -o kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

novaos.bin: boot.bin kernel.bin
	cat boot.bin kernel.bin > $@
	truncate -s 131072 $@

run: novaos.bin
	qemu-system-i386 -drive format=raw,file=novaos.bin

clean:
	rm -f boot.bin kernel/*.o kernel.elf kernel.bin novaos.bin
