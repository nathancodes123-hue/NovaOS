#include "../include/nova/types.h"

#define TEXT_WIDTH 80
#define TEXT_HEIGHT 25
#define MAX_WINDOWS 16

typedef struct {
    int x, y, w, h;
    u8 visible, active;
    u32 color;
    const char *title;
} Window;

static Window windows[MAX_WINDOWS];
static u32 count;
static volatile u16 *const vga = (volatile u16 *)0xB8000;

static void text_cell(int x, int y, char c, u8 attr)
{
    if (x < 0 || x >= TEXT_WIDTH || y < 0 || y >= TEXT_HEIGHT)
        return;
    vga[(u32)y * TEXT_WIDTH + (u32)x] = ((u16)attr << 8) | (u8)c;
}

static void text_fill(int x, int y, int w, int h, char c, u8 attr)
{
    for (int yy = 0; yy < h; ++yy)
        for (int xx = 0; xx < w; ++xx)
            text_cell(x + xx, y + yy, c, attr);
}

static void text_label(int x, int y, const char *s, u8 attr)
{
    if (!s) return;
    while (*s && x < TEXT_WIDTH)
        text_cell(x++, y, *s++, attr);
}

static void text_window(const Window *w)
{
    if (!w || !w->visible || w->w < 2 || w->h < 2)
        return;

    int right = w->x + w->w - 1;
    int bottom = w->y + w->h - 1;

    text_fill(w->x, w->y, w->w, w->h, ' ', 0x07);

    for (int x = w->x; x <= right; ++x) {
        text_cell(x, w->y, '-', 0x0F);
        text_cell(x, bottom, '-', 0x0F);
    }

    for (int y = w->y; y <= bottom; ++y) {
        text_cell(w->x, y, '|', 0x0F);
        text_cell(right, y, '|', 0x0F);
    }

    text_cell(w->x, w->y, '+', 0x0F);
    text_cell(right, w->y, '+', 0x0F);
    text_cell(w->x, bottom, '+', 0x0F);
    text_cell(right, bottom, '+', 0x0F);
    text_label(w->x + 2, w->y, w->title, 0x0F);
}

void gui_open(int x, int y, int w, int h, const char *title)
{
    if (count >= MAX_WINDOWS)
        return;
    windows[count++] = (Window){x, y, w, h, 1, 1, 7, title};
}

void gui_init(void)
{
    count = 0;

    /*
     * NovaOS remains in VGA text mode at this stage. Never treat 0xA0000
     * as a linear framebuffer until a real graphics mode is configured.
     */
    gui_open(8, 4, 64, 14, "Nova Desktop");
}

void gui_render(void)
{
    /*
     * Text-mode-safe GUI placeholder. The old renderer wrote a 320x200
     * image directly to 0xA0000 without switching the VGA hardware mode.
     */
    for (int y = 3; y < 19; ++y)
        text_fill(7, y, 66, 16, ' ', 0x07);

    for (u32 i = 0; i < count; ++i)
        text_window(&windows[i]);
}
