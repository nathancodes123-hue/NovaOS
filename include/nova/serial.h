#pragma once
#include "types.h"

void serial_init(void);
int serial_ready(void);
void serial_putc(char c);
void serial_write(const char *text);
