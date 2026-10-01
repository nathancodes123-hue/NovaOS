# NovaOS

NovaOS is a from-scratch x86 operating-system project. This initial tree contains a GUI-first freestanding kernel foundation.

## Build

Requires NASM, GCC/Clang with 32-bit freestanding support, GNU ld, and QEMU.

```sh
make
make run
```

The current milestone is a bootable kernel foundation with VGA graphics, desktop/window/widget structures, keyboard and mouse initialization, a tiny heap, event queue, scheduler scaffolding, and a Nova shell bootstrap. Hardware and filesystem subsystems will be split into drivers/modules as the project grows.
