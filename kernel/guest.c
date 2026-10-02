#include "../include/nova/types.h"
#include "../include/nova/io.h"
#include "../include/nova/guest.h"

#define QEMU_PORT 0x504u
#define BOCHS_PORT 0xE9u

static NovaGuestInfo info;

static int string_eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a++ != *b++) return 0;
    }
    return *a == 0 && *b == 0;
}

static u32 cpuid_vendor(void) {
    u32 eax, ebx, ecx, edx;
    __asm__ volatile(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );
    (void)eax;
    if (ebx == 0x756e6547u && edx == 0x49656e69u &&
        ecx == 0x6c65746eu)
        return 1; /* GenuineIntel */
    return 0;
}

static int hypervisor_present(void) {
    u32 eax, ebx, ecx, edx;
    __asm__ volatile(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );
    (void)eax; (void)ebx; (void)edx;
    return (ecx & (1u << 31)) != 0;
}

static void cpuid_hypervisor_leaf(char *out) {
    u32 a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0x40000000u));
    ((u32 *)out)[0] = b;
    ((u32 *)out)[1] = c;
    ((u32 *)out)[2] = d;
    out[12] = 0;
    (void)a;
}

void guest_debug_puts(const char *text) {
    if (!text) return;
    while (*text) {
        outb(BOCHS_PORT, (u8)*text++);
    }
}

void guest_tools_init(void) {
    info = (NovaGuestInfo){0};
    info.timer_hz = 100;
    info.display_width = 80;
    info.display_height = 25;

    if (!hypervisor_present())
        return;

    char vendor[13];
    cpuid_hypervisor_leaf(vendor);

    if (string_eq(vendor, "TCGTCGTCGTCG"))
        info.detected = NOVA_GUEST_QEMU;
    else if (string_eq(vendor, "Microsoft Hv"))
        info.detected = NOVA_GUEST_QEMU;
    else if (string_eq(vendor, "VBoxVBoxVBox"))
        info.detected = NOVA_GUEST_VIRTUALBOX;
    else if (string_eq(vendor, "VMwareVMware"))
        info.detected = NOVA_GUEST_VMWARE;
    else
        info.detected = NOVA_GUEST_QEMU;

    info.vendor = info.detected;
    info.version = 1;
    info.features = 1u; /* debug console support */

    /*
     * QEMU/Bochs debug output is intentionally passive. Guest tools never
     * depend on it for booting, so a real machine behaves exactly like a VM.
     */
    guest_debug_puts("NovaOS guest tools initialized\n");
}

const NovaGuestInfo *guest_info(void) {
    return &info;
}

int guest_is_virtualized(void) {
    return info.detected != NOVA_GUEST_NONE;
}

int guest_request_resolution(u32 width, u32 height) {
    /*
     * The current boot path is a VGA text-mode kernel. Resolution changes
     * become active once the VBE/framebuffer driver is installed. Keep the
     * request recorded here so the desktop and future guest agent have one
     * API instead of scattered hypervisor-specific code.
     */
    if (!width || !height || width > 4096 || height > 4096)
        return 0;
    info.display_width = width;
    info.display_height = height;
    return 1;
}
