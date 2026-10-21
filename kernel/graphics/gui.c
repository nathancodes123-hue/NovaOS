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
static u8 dirty;

static void fill(u32 x, u32 y, u32 w, u32 h, u32 color)
{
    const NovaDisplayInfo *d = display_info();
    if (!d->graphics || x >= d->width || y >= d->height)
        return;

    if (w > d->width - x) w = d->width - x;
    if (h > d->height - y) h = d->height - y;

    /*
     * Draw complete scanlines. The compiler can turn the contiguous
     * 32-bit stores into very fast stores, avoiding per-pixel function
     * overhead.
     */
    volatile u32 *fb = (volatile u32 *)d->framebuffer;

    for (u32 yy = 0; yy < h; ++yy) {
        volatile u32 *row = fb + (y + yy) * d->width + x;
        for (u32 xx = 0; xx < w; ++xx)
            row[xx] = color;
    }
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
    dirty = 1;
}

void gui_init(void)
{
    count = 0;
    dirty = 0;

    if (!display_init())
        return;

    gui_open(80, 60, 1120, 600, "Nova Desktop");
}

/*
 * Mark the desktop as needing a new frame. Future window manager,
 * compositor, mouse and keyboard code can call this after changing
 * something visible.
 */
void gui_mark_dirty(void)
{
    dirty = 1;
}

void gui_render(void)
{
    const NovaDisplayInfo *d = display_info();
    if (!d->graphics || !dirty)
        return;

    /*
     * Render one complete frame only when the scene changed. The old
     * implementation rendered continuously while the CPU was idle,
     * repeatedly rewriting the whole framebuffer and making the scanout
     * visibly reveal the rendering order.
     */
    fill(0, 0, d->width, d->height, 0xFF101820u);

    u32 taskbar_h = d->height / 14u;
    if (taskbar_h < 48u) taskbar_h = 48u;
    if (taskbar_h > 120u) taskbar_h = 120u;

    fill(0, d->height - taskbar_h, d->width, taskbar_h, 0xFF202830u);

    for (u32 i = 0; i < count; ++i)
        window_draw(&windows[i]);

    /*
     * Everything for this frame has now been written. Do not touch the
     * framebuffer again until the next visible change.
     */
    dirty = 0;
}
