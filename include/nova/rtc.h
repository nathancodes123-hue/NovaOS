#pragma once
#include "types.h"

typedef struct {
    u8 second;
    u8 minute;
    u8 hour;
    u8 day;
    u8 month;
    u16 year;
} NovaRtcTime;

void rtc_init(void);
int rtc_read(NovaRtcTime *out);
u32 rtc_unix_seconds(void);
