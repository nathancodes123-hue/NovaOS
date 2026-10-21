#include "../include/nova/types.h"
#include "../include/nova/pci.h"

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define MAX_PCI_DEVICES 128u

static NovaPciDevice devices[MAX_PCI_DEVICES];
static u32 device_count;

static void pci_outl(u16 port, u32 value) {
    __asm__ volatile ("outl %0,%1" :: "a"(value), "Nd"(port));
}

static u32 pci_inl(u16 port) {
    u32 value;
    __asm__ volatile ("inl %1,%0" : "=a"(value) : "Nd"(port));
    return value;
}

u32 pci_config_read32(u8 bus, u8 slot, u8 function, u8 offset) {
    u32 address = 0x80000000u |
                  ((u32)bus << 16) |
                  ((u32)slot << 11) |
                  ((u32)function << 8) |
                  (offset & 0xFCu);
    pci_outl(PCI_CONFIG_ADDR, address);
    return pci_inl(PCI_CONFIG_DATA);
}

void pci_init(void) {
    device_count = 0;

    for (u32 bus = 0; bus < 256u; ++bus) {
        for (u32 slot = 0; slot < 32u; ++slot) {
            for (u32 function = 0; function < 8u; ++function) {
                u32 id = pci_config_read32((u8)bus, (u8)slot, (u8)function, 0);
                if ((id & 0xFFFFu) == 0xFFFFu)
                    continue;
                if (device_count >= MAX_PCI_DEVICES)
                    return;

                u32 class = pci_config_read32((u8)bus, (u8)slot, (u8)function, 8);
                devices[device_count++] = (NovaPciDevice){
                    (u8)bus, (u8)slot, (u8)function,
                    (u16)(id & 0xFFFFu), (u16)(id >> 16),
                    (u8)(class >> 24), (u8)(class >> 16),
                    (u8)(class >> 8), (u8)class
                };
            }
        }
    }
}

u32 pci_device_count(void) {
    return device_count;
}

int pci_get(u32 index, NovaPciDevice *out) {
    if (!out || index >= device_count)
        return 0;
    *out = devices[index];
    return 1;
}
