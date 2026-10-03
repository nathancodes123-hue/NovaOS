#pragma once
#include "types.h"

typedef struct {
    char vendor[13];
    u32 features_ecx;
    u32 features_edx;
    u8 has_cpuid;
    u8 has_apic;
    u8 has_mmx;
    u8 has_sse;
} NovaCpuInfo;

int cpu_init(void);
int cpu_has_cpuid(void);
void cpu_get_info(NovaCpuInfo *info);
void cpu_reboot(void);
u32 cpu_vendor(char *out, u32 capacity);
