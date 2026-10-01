#pragma once
#include "types.h"

typedef struct {
    u16 machine;
    u16 sections;
    u32 entry_rva;
    u32 image_base;
    u32 image_size;
    u32 headers_size;
    u16 characteristics;
} PEInfo;

int pe_load(const char *name, PEInfo *info, u32 *entry);
