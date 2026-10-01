#include "../include/nova/types.h"
#include "../include/nova/pe.h"

extern u32 vfs_find(const char *name);
extern u32 vfs_size(u32 inode);
extern int vfs_read(u32 inode, u8 *dst, u32 n);
extern void *kmalloc(usize size);
extern void kfree(void *ptr);

#define PE_DOS_MAGIC 0x5A4Du
#define PE_NT_MAGIC 0x00004550u
#define PE32_MAGIC 0x010Bu
#define IMAGE_FILE_MACHINE_I386 0x014Cu
#define IMAGE_FILE_EXECUTABLE_IMAGE 0x0002u

typedef struct __attribute__((packed)) {
    u16 magic;
    u8 pad[58];
    u32 pe_offset;
} DOSHeader;

typedef struct __attribute__((packed)) {
    u32 signature;
    u16 machine;
    u16 sections;
    u32 timestamp;
    u32 symbol_table;
    u32 symbols;
    u16 optional_size;
    u16 characteristics;
} COFFHeader;

typedef struct __attribute__((packed)) {
    u16 magic;
    u8 linker_major, linker_minor;
    u32 code_size, init_data, uninit_data;
    u32 entry_rva, code_base, data_base;
    u32 image_base, section_align, file_align;
    u16 os_major, os_minor, image_major, image_minor, subsystem_major, subsystem_minor;
    u32 win32_version, image_size, headers_size, checksum;
    u16 subsystem, dll_characteristics;
    u32 stack_reserve, stack_commit, heap_reserve, heap_commit;
    u32 loader_flags, directories;
} PE32Optional;

typedef struct __attribute__((packed)) {
    u8 name[8];
    u32 virtual_size, virtual_address, raw_size, raw_offset;
    u32 reloc, line, reloc_count, line_count;
    u32 characteristics;
} PESection;

static u32 rd32(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

int pe_load(const char *name, PEInfo *info, u32 *entry) {
    if (!name || !info || !entry) return -1;

    u32 inode = vfs_find(name);
    u32 size = inode ? vfs_size(inode) : 0;
    if (!inode || size < sizeof(DOSHeader) || size > 1024) return -1;

    u8 *image = (u8 *)kmalloc(size);
    if (!image) return -1;
    if (vfs_read(inode, image, size) != (int)size) {
        kfree(image);
        return -1;
    }

    DOSHeader *dos = (DOSHeader *)image;
    if (dos->magic != PE_DOS_MAGIC || dos->pe_offset > size - sizeof(COFFHeader)) {
        kfree(image);
        return -1;
    }

    COFFHeader *coff = (COFFHeader *)(image + dos->pe_offset);
    if (coff->signature != PE_NT_MAGIC ||
        coff->machine != IMAGE_FILE_MACHINE_I386 ||
        !(coff->characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) ||
        coff->sections == 0 || coff->sections > 96 ||
        coff->optional_size < sizeof(PE32Optional)) {
        kfree(image);
        return -1;
    }

    u32 opt_off = dos->pe_offset + sizeof(COFFHeader);
    if (opt_off > size - coff->optional_size) {
        kfree(image);
        return -1;
    }

    PE32Optional *opt = (PE32Optional *)(image + opt_off);
    if (opt->magic != PE32_MAGIC || !opt->image_size ||
        opt->headers_size > opt->image_size ||
        opt->entry_rva >= opt->image_size) {
        kfree(image);
        return -1;
    }

    u32 sec_off = opt_off + coff->optional_size;
    u32 sec_bytes = (u32)coff->sections * sizeof(PESection);
    if (sec_off > size || sec_bytes > size - sec_off) {
        kfree(image);
        return -1;
    }

    for (u32 i = 0; i < coff->sections; ++i) {
        PESection *s = (PESection *)(image + sec_off + i * sizeof(PESection));
        if (s->raw_size) {
            if (s->raw_offset > size || s->raw_size > size - s->raw_offset ||
                s->virtual_address > opt->image_size ||
                s->raw_size > opt->image_size - s->virtual_address) {
                kfree(image);
                return -1;
            }
        }
    }

    info->machine = coff->machine;
    info->sections = coff->sections;
    info->entry_rva = opt->entry_rva;
    info->image_base = opt->image_base;
    info->image_size = opt->image_size;
    info->headers_size = opt->headers_size;
    info->characteristics = coff->characteristics;

    /*
     * Execution is intentionally not enabled yet: PE images need a real
     * per-process address space, section mapping, relocations/imports, and
     * user-mode entry setup before the entry point can safely run.
     */
    *entry = opt->entry_rva;
    kfree(image);
    return 0;
}
