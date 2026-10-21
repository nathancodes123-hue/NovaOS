CC ?= gcc
LD := $(shell if command -v ld.lld >/dev/null 2>&1; then command -v ld.lld; elif [ "$$(uname -s 2>/dev/null)" = "Darwin" ] && [ -x /opt/local/libexec/llvm-19/bin/ld.lld ]; then echo /opt/local/libexec/llvm-19/bin/ld.lld; elif [ "$$(uname -s 2>/dev/null)" != "Darwin" ] && command -v ld >/dev/null 2>&1; then command -v ld; else echo ld.lld; fi)
OBJCOPY := $(shell if command -v objcopy >/dev/null 2>&1; then command -v objcopy; elif command -v llvm-objcopy >/dev/null 2>&1; then command -v llvm-objcopy; elif [ -x /opt/local/libexec/llvm-19/bin/llvm-objcopy ]; then echo /opt/local/libexec/llvm-19/bin/llvm-objcopy; else echo llvm-objcopy; fi)
CFLAGS=-m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -Oz -ffunction-sections -fdata-sections -Wall -Wextra -mgeneral-regs-only -Iinclude

ifeq ($(shell uname -s 2>/dev/null),Darwin)
CFLAGS += --target=i386-unknown-none-elf
endif

KERNEL_OBJS= \
 kernel/arch/i386/start_bss.o \
 kernel/core/kernel.o \
 kernel/memory/mm.o \
 kernel/memory/paging.o \
 kernel/interrupts/interrupts.o \
 kernel/arch/i386/interrupts_asm.o \
 kernel/process/process.o \
 kernel/fs/vfs.o \
 kernel/lib/string.o \
 kernel/exec/pe.o \
 kernel/graphics/gui.o \
 kernel/graphics/display.o \
 kernel/shell/nsh.o \
 kernel/virtualization/guest.o \
 kernel/drivers/pci/pci.o \
 kernel/drivers/storage/ata.o \
 kernel/io/fd.o \
 kernel/drivers/storage/block.o \
 kernel/fs/ext2.o \
 kernel/locking/spinlock.o \
 kernel/drivers/serial/serial.o \
 kernel/time/rtc.o \
 kernel/memory/pagefile.o \
 kernel/cpu/cpu.o \
 kernel/diagnostics/bootdiag.o

all: novaos.bin

boot.bin: boot/boot.asm
	nasm -f bin $< -o $@
	test $$(stat -c %s $@ 2>/dev/null || stat -f %z $@) -eq 512

kernel/%.o: kernel/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

kernel/arch/i386/%.o: kernel/arch/i386/%.asm
	mkdir -p $(dir $@)
	nasm -f elf32 $< -o $@

kernel.bin: $(KERNEL_OBJS) linker.ld
	$(LD) -m elf_i386 --gc-sections -T linker.ld $(KERNEL_OBJS) -o kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin
	test $$(stat -c %s kernel.bin 2>/dev/null || stat -f %z $@) -le 122880

novaos.bin: boot.bin kernel.bin
	cat boot.bin kernel.bin > $@
	python3 -c 'import os,sys; os.truncate(sys.argv[1], 16 * 1024 * 1024)' $@

run: novaos.bin
	qemu-system-i386 -drive format=raw,file=novaos.bin

clean:
	rm -f boot.bin kernel/*.o $(KERNEL_OBJS) kernel.elf kernel.bin novaos.bin
