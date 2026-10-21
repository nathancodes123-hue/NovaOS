#include "../include/nova/types.h"
#include "../include/nova/rtc.h"

#define CMOS_INDEX 0x70
#define CMOS_DATA 0x71

static int ready;

static void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0,%1" :: "a"(value), "Nd"(port));
}

static u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1,%0" : "=a"(value) : "Nd"(port));
    return value;
}

static u8 cmos_read(u8 index) {
    outb(CMOS_INDEX, index);
    return inb(CMOS_DATA);
}

static u8 bcd_to_bin(u8 value) {
    return (u8)((value & 0x0Fu) + ((value >> 4) * 10u));
}

static int update_in_progress(void) {
    return (cmos_read(0x0A) & 0x80u) != 0;
}

void rtc_init(void) {
    ready = cmos_read(0x0B) != 0xFFu;
}

int rtc_read(NovaRtcTime *out) {
    if (!ready || !out)
        return 0;

    NovaRtcTime a, b;
    for (;;) {
        while (update_in_progress())
            ;

        a.second = cmos_read(0x00);
        a.minute = cmos_read(0x02);
        a.hour = cmos_read(0x04);
        a.day = cmos_read(0x07);
        a.month = cmos_read(0x08);
        a.year = cmos_read(0x09);

        while (update_in_progress())
            ;

        b.second = cmos_read(0x00);
        b.minute = cmos_read(0x02);
        b.hour = cmos_read(0x04);
        b.day = cmos_read(0x07);
        b.month = cmos_read(0x08);
        b.year = cmos_read(0x09);

        if (a.second == b.second && a.minute == b.minute &&
            a.hour == b.hour && a.day == b.day &&
            a.month == b.month && a.year == b.year)
            break;
    }

    u8 status_b = cmos_read(0x0B);
    if (!(status_b & 0x04u)) {
        a.second = bcd_to_bin(a.second);
        a.minute = bcd_to_bin(a.minute);
        a.hour = bcd_to_bin(a.hour);
        a.day = bcd_to_bin(a.day);
        a.month = bcd_to_bin(a.month);
        a.year = bcd_to_bin(a.year);
    }

    if (!(status_b & 0x02u) && (a.hour & 0x80u)) {
        a.hour = (u8)(((a.hour & 0x7Fu) + 12u) % 24u);
    }

    a.year = (u16)(2000u + a.year);
    *out = a;
    return 1;
}

u32 rtc_unix_seconds(void) {
    NovaRtcTime t;
    if (!rtc_read(&t))
        return 0;

    static const u16 days_before_month[] =
        {0,31,59,90,120,151,181,212,243,273,304,334};

    u32 days = 0;
    for (u16 y = 1970; y < t.year; ++y)
        days += 365u + ((y % 4u == 0 && (y % 100u != 0 || y % 400u == 0)) ? 1u : 0u);

    if (t.month >= 1 && t.month <= 12)
        days += days_before_month[t.month - 1u];

    if (t.month > 2 && (t.year % 4u == 0 && (t.year % 100u != 0 || t.year % 400u == 0)))
        ++days;

    if (t.day)
        days += t.day - 1u;

    return days * 86400u + (u32)t.hour * 3600u +
           (u32)t.minute * 60u + t.second;
}
