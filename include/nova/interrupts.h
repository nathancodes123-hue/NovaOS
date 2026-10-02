#pragma once
#include "types.h"

void interrupts_init(void);
void interrupts_start(void);
u32 interrupt_ticks(void);

int keyboard_available(void);
int keyboard_read(void);
