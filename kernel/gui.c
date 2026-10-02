#include "../include/nova/types.h"
#include "../include/nova/display.h"

#define MAX_WINDOWS 16

typedef struct {
    u32 x, y, w, h;
    u8 visible, active;
    u32 color;
    const char *title;
} Window;

static Window windows[MAX_WINDOWS];
static u32 count;

static void fill(u32 x, u32 y, u32 w, u32 h, u32 color)
{
    const NovaDisplayInfo *d = display_info();
    if (!d->graphics) return;

    if (x >= d->width || y >= d->height) return;
    if (w > d->width - x) w = d->width - x;
    if (h > d->height - y) h = d->height - y;

    volatile u32 *fb = (volatile u32 *)d->framebuffer;

    for (u32 yy = 0; yy < h; ++yy)
        for (u32 xx = 0; xx < w; ++xx)
            fb[(y + yy) * d->width + (x + xx)] = color;
}

static void border(u32 x, u32 y, u32 w, u32 h, u32 color)
{
    if (w < 2 || h < 2) return;

    fill(x, y, w, 3, color);
    fill(x, y + h - 3, w, 3, color);
    fill(x, y, 3, h, color);
    fill(x + w - 3, y, 3, h, color);
}

static void window_draw(const Window *w)
{
    if (!w || !w->visible) return;

    fill(w->x, w->y, w->w, w->h, 0xFF20242Bu);
    fill(w->x, w->y, w->w, 42, 0xFF303640u);
    border(w->x, w->y, w->w, w->h, 0xFFE0E5ECu);
}

void gui_open(u32 x, u32 y, u32 w, u32 h, const char *title)
{
    if (count >= MAX_WINDOWS) return;
    windows[count++] = (Window){x, y, w, h, 1, 1, 0xFF20242Bu, title};
}

void gui_init(void)
{
    count = 0;

    if (!display_init())
        return;

    gui_open(80, 60, 1120, 600, "Nova Desktop");
}

void gui_render(void)
{
    const NovaDisplayInfo *d = display_info();
    if (!d->graphics) return;

    fill(0, 0, d->width, d->height, 0xFF101820u);

    /* 720p desktop layout scales naturally to the selected framebuffer. */
    u32 taskbar_h = d->height / 14u;
    if (taskbar_h < 48u) taskbar_h = 48u;
    if (taskbar_h > 120u) taskbar_h = 120u;

    fill(0, d->height - taskbar_h, d->width, taskbar_h, 0xFF202830u);

    for (u32 i = 0; i < count; ++i)
        window_draw(&windows[i]);
}
