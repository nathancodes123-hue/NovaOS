#pragma once
#include "types.h"

typedef struct {
    u8 bus;
    u8 slot;
    u8 function;
    u16 vendor;
    u16 device;
    u8 class_code;
    u8 subclass;
    u8 prog_if;
    u8 revision;
} NovaPciDevice;

void pci_init(void);
u32 pci_device_count(void);
int pci_get(u32 index, NovaPciDevice *out);
u32 pci_config_read32(u8 bus, u8 slot, u8 function, u8 offset);
