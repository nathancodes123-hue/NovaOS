CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy
CFLAGS=-m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -O2 -Wall -Wextra -mgeneral-regs-only -Iinclude
ASFLAGS=-m32
LDFLAGS=-m elf_i386 -T linker.ld

KERNEL_OBJS=kernel/kernel.o kernel/mm.o kernel/paging.o kernel/interrupts.o kernel/interrupts_asm.o kernel/process.o kernel/vfs.o kernel/gui.o

all: novaos.bin

boot.bin: boot/boot.asm
	nasm -f bin $< -o $@
	test $$(stat -c %s $@ 2>/dev/null || stat -f %z $@) -eq 512

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/%.o: kernel/%.S
	$(CC) $(ASFLAGS) -c $< -o $@

kernel.bin: $(KERNEL_OBJS) linker.ld
	$(LD) $(LDFLAGS) $(KERNEL_OBJS) -o kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin
	test $$(stat -c %s kernel.bin 2>/dev/null || stat -f %z kernel.bin) -le 122880

novaos.bin: boot.bin kernel.bin
	cat boot.bin kernel.bin > $@
	truncate -s 131072 $@

run: novaos.bin
	qemu-system-i386 -drive format=raw,file=novaos.bin

clean:
	rm -f boot.bin kernel/*.o kernel.elf kernel.bin novaos.bin
