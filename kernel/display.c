#include "../include/nova/types.h"
#include "../include/nova/display.h"
#include "../include/nova/pci.h"
#include "../include/nova/paging.h"

#define BGA_INDEX 0x1CE
#define BGA_DATA  0x1CF
#define BGA_ID 0x0B0C5u
#define BGA_REG_ID 0
#define BGA_REG_XRES 1
#define BGA_REG_YRES 2
#define BGA_REG_BPP 3
#define BGA_REG_ENABLE 4
#define BGA_REG_VRAM 0x0Au

#define BGA_ENABLE 0x41u
#define FRAMEBUFFER_VIRT 0xE0000000u

static NovaDisplayInfo info;

static void outw(u16 port, u16 value)
{
    __asm__ volatile ("outw %0,%1" :: "a"(value), "Nd"(port));
}

static u16 inw(u16 port)
{
    u16 value;
    __asm__ volatile ("inw %1,%0" : "=a"(value) : "Nd"(port));
    return value;
}

static void bga_write(u16 reg, u16 value)
{
    outw(BGA_INDEX, reg);
    outw(BGA_DATA, value);
}

static u16 bga_read(u16 reg)
{
    outw(BGA_INDEX, reg);
    return inw(BGA_DATA);
}

static u32 bga_framebuffer_phys(void)
{
    for (u32 i = 0; i < pci_device_count(); ++i) {
        NovaPciDevice dev;
        if (!pci_get(i, &dev))
            continue;

        if (dev.vendor == 0x1234 && dev.device == 0x1111) {
            u32 bar = pci_config_read32(dev.bus, dev.slot, dev.function, 0x10);
            if (!(bar & 1u))
                return bar & 0xFFFFFFF0u;
        }
    }

    return 0;
}

static u32 framebuffer_bytes(u32 width, u32 height)
{
    u64 bytes = (u64)width * (u64)height * 4u;
    return bytes > 0xFFFFFFFFull ? 0 : (u32)bytes;
}

static int map_framebuffer(u32 phys, u32 size)
{
    u32 pages = (size + PAGE_SIZE - 1u) / PAGE_SIZE;
    if (!pages)
        return 0;

    return paging_map_range(FRAMEBUFFER_VIRT, phys, pages,
                            PAGING_PRESENT | PAGING_WRITE);
}

static int bga_set(u32 width, u32 height)
{
    u32 bytes = framebuffer_bytes(width, height);
    if (!bytes || bytes > info.vram_size)
        return 0;

    if (width > NOVA_DISPLAY_MAX_WIDTH || height > NOVA_DISPLAY_MAX_HEIGHT)
        return 0;

    bga_write(BGA_REG_ENABLE, 0);
    bga_write(BGA_REG_XRES, (u16)width);
    bga_write(BGA_REG_YRES, (u16)height);
    bga_write(BGA_REG_BPP, NOVA_DISPLAY_BPP);
    bga_write(BGA_REG_ENABLE, BGA_ENABLE);

    info.width = width;
    info.height = height;
    info.pitch = width * 4u;
    info.bpp = NOVA_DISPLAY_BPP;
    info.framebuffer = FRAMEBUFFER_VIRT;
    info.framebuffer_size = bytes;
    info.graphics = 1;

    return 1;
}

int display_set_resolution(u32 width, u32 height)
{
    if (!info.graphics)
        return 0;
    return bga_set(width, height);
}

int display_init(void)
{
    info = (NovaDisplayInfo){0};

    /*
     * The standard QEMU VGA device exposes the Bochs/QEMU BGA register
     * interface. This is our first real framebuffer backend; physical GPU
     * drivers will be added separately.
     */
    if (bga_read(BGA_REG_ID) != BGA_ID)
        return 0;

    u32 vram_kb = (u32)bga_read(BGA_REG_VRAM) * 64u;
    info.vram_size = vram_kb * 1024u;

    u32 phys = bga_framebuffer_phys();
    if (!phys || info.vram_size < framebuffer_bytes(
            NOVA_DISPLAY_DEFAULT_WIDTH, NOVA_DISPLAY_DEFAULT_HEIGHT))
        return 0;

    if (!map_framebuffer(phys, info.vram_size))
        return 0;

    return bga_set(NOVA_DISPLAY_DEFAULT_WIDTH,
                   NOVA_DISPLAY_DEFAULT_HEIGHT);
}

const NovaDisplayInfo *display_info(void)
{
    return &info;
}

void display_clear(u32 pixel)
{
    if (!info.graphics)
        return;

    volatile u32 *fb = (volatile u32 *)info.framebuffer;
    u32 pixels = info.width * info.height;

    for (u32 i = 0; i < pixels; ++i)
        fb[i] = pixel;
}

void display_pixel(u32 x, u32 y, u32 pixel)
{
    if (!info.graphics || x >= info.width || y >= info.height)
        return;

    ((volatile u32 *)info.framebuffer)[y * info.width + x] = pixel;
}
