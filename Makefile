CC ?= gcc
LD := $(shell if command -v ld.lld >/dev/null 2>&1; then command -v ld.lld; elif [ "$$(uname -s 2>/dev/null)" = "Darwin" ] && [ -x /opt/local/libexec/llvm-19/bin/ld.lld ]; then echo /opt/local/libexec/llvm-19/bin/ld.lld; elif [ "$$(uname -s 2>/dev/null)" != "Darwin" ] && command -v ld >/dev/null 2>&1; then command -v ld; else echo ld.lld; fi)
OBJCOPY := $(shell if command -v objcopy >/dev/null 2>&1; then command -v objcopy; elif command -v llvm-objcopy >/dev/null 2>&1; then command -v llvm-objcopy; elif [ -x /opt/local/libexec/llvm-19/bin/llvm-objcopy ]; then echo /opt/local/libexec/llvm-19/bin/llvm-objcopy; else echo llvm-objcopy; fi)
CFLAGS=-m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -Oz -ffunction-sections -fdata-sections -Wall -Wextra -mgeneral-regs-only -Iinclude

ifeq ($(shell uname -s 2>/dev/null),Darwin)
CFLAGS += --target=i386-unknown-none-elf
endif

KERNEL_OBJS=kernel/start_bss.o kernel/kernel.o kernel/mm.o kernel/paging.o kernel/interrupts.o kernel/interrupts_asm.o kernel/process.o kernel/vfs.o kernel/string.o kernel/pe.o kernel/gui.o kernel/display.o kernel/nsh.o kernel/guest.o kernel/pci.o kernel/ata.o kernel/fd.o kernel/block.o kernel/ext2.o kernel/spinlock.o kernel/serial.o kernel/rtc.o kernel/pagefile.o

all: novaos.bin

boot.bin: boot/boot.asm
	nasm -f bin $< -o $@
	test $$(stat -c %s $@ 2>/dev/null || stat -f %z $@) -eq 512

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/start_bss.o: kernel/start_bss.asm
	nasm -f elf32 $< -o $@

kernel/interrupts_asm.o: kernel/interrupts_asm.asm
	nasm -f elf32 $< -o $@

kernel.bin: $(KERNEL_OBJS) linker.ld
	$(LD) -m elf_i386 --gc-sections -T linker.ld $(KERNEL_OBJS) -o kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin
	test $$(stat -c %s kernel.bin 2>/dev/null || stat -f %z $@) -le 61440

novaos.bin: boot.bin kernel.bin
	cat boot.bin kernel.bin > $@
	python3 -c 'import os,sys; os.truncate(sys.argv[1], 16 * 1024 * 1024)' $@

run: novaos.bin
	qemu-system-i386 -drive format=raw,file=novaos.bin

clean:
	rm -f boot.bin kernel/*.o kernel.elf kernel.bin novaos.bin
