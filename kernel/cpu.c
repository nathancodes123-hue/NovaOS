#include "../include/nova/types.h"
#include "../include/nova/cpu.h"
#include "../include/nova/io.h"

static NovaCpuInfo cpu_info;

static void cpuid(u32 leaf, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx) {
    __asm__ volatile("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf));
}

int cpu_has_cpuid(void) {
    u32 original, modified;
    __asm__ volatile("pushfl; popl %0" : "=r"(original));
    modified = original ^ (1u << 21);
    __asm__ volatile("pushl %0; popfl" :: "r"(modified));
    __asm__ volatile("pushfl; popl %0" : "=r"(modified));
    __asm__ volatile("pushl %0; popfl" :: "r"(original));
    return ((original ^ modified) & (1u << 21)) != 0;
}

static void copy_vendor(char *out, u32 capacity, u32 ebx, u32 edx, u32 ecx) {
    u32 regs[3] = {ebx, edx, ecx};
    u32 pos = 0;
    if (!out || !capacity)
        return;
    for (u32 r = 0; r < 3; ++r) {
        for (u32 i = 0; i < 4 && pos + 1u < capacity; ++i)
            out[pos++] = (char)((regs[r] >> (i * 8u)) & 0xFFu);
    }
    out[pos] = 0;
}

int cpu_init(void) {
    cpu_info.has_cpuid = (u8)cpu_has_cpuid();
    cpu_info.vendor[0] = 0;
    cpu_info.features_ecx = 0;
    cpu_info.features_edx = 0;
    cpu_info.has_apic = 0;
    cpu_info.has_mmx = 0;
    cpu_info.has_sse = 0;

    if (!cpu_info.has_cpuid)
        return 0;

    u32 max_leaf, ebx, ecx, edx;
    cpuid(0, &max_leaf, &ebx, &ecx, &edx);
    copy_vendor(cpu_info.vendor, sizeof(cpu_info.vendor), ebx, edx, ecx);
    if (max_leaf < 1u)
        return 1;

    u32 eax;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    cpu_info.features_ecx = ecx;
    cpu_info.features_edx = edx;
    cpu_info.has_apic = (u8)((edx >> 9) & 1u);
    cpu_info.has_mmx = (u8)((edx >> 23) & 1u);
    cpu_info.has_sse = (u8)((edx >> 25) & 1u);
    return 1;
}

void cpu_get_info(NovaCpuInfo *info) {
    if (!info)
        return;
    *info = cpu_info;
}

u32 cpu_vendor(char *out, u32 capacity) {
    u32 i = 0;
    if (!out || !capacity)
        return 0;
    while (cpu_info.vendor[i] && i + 1u < capacity) {
        out[i] = cpu_info.vendor[i];
        ++i;
    }
    out[i] = 0;
    return i;
}

void cpu_reboot(void) {
    cli();
    for (u32 i = 0; i < 100000u; ++i)
        if (!(inb(0x64) & 0x02u))
            break;
    outb(0x64, 0xFE);
    for (;;) {
        hlt();
    }
}
