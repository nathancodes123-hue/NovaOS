#pragma once
#include "types.h"

#define NOVA_DISPLAY_DEFAULT_WIDTH 1280u
#define NOVA_DISPLAY_DEFAULT_HEIGHT 720u
#define NOVA_DISPLAY_MAX_WIDTH 3840u
#define NOVA_DISPLAY_MAX_HEIGHT 2160u
#define NOVA_DISPLAY_BPP 32u

typedef struct {
    u32 width;
    u32 height;
    u32 pitch;
    u32 bpp;
    u32 framebuffer;
    u32 framebuffer_size;
    u32 vram_size;
    u8 graphics;
} NovaDisplayInfo;

int display_init(void);
int display_set_resolution(u32 width, u32 height);
const NovaDisplayInfo *display_info(void);
void display_clear(u32 pixel);
void display_pixel(u32 x, u32 y, u32 pixel);
