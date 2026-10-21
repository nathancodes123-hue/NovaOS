#include "../include/nova/types.h"

void *memset(void *dst, int value, u32 n)
{
    u8 *p = (u8 *)dst;
    for (u32 i = 0; i < n; i++)
        p[i] = (u8)value;
    return dst;
}
