#pragma once
#include "types.h"

enum {
    NOVA_GUEST_NONE = 0,
    NOVA_GUEST_QEMU = 1,
    NOVA_GUEST_BOCHS = 2,
    NOVA_GUEST_VIRTUALBOX = 3,
    NOVA_GUEST_VMWARE = 4
};

typedef struct {
    u32 vendor;
    u32 version;
    u32 features;
    u32 detected;
    u32 timer_hz;
    u32 display_width;
    u32 display_height;
} NovaGuestInfo;

void guest_tools_init(void);
const NovaGuestInfo *guest_info(void);
int guest_is_virtualized(void);
int guest_request_resolution(u32 width, u32 height);
void guest_debug_puts(const char *text);
