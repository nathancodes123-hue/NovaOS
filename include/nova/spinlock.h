#pragma once
#include "types.h"

typedef struct {
    volatile u32 value;
} NovaSpinlock;

void spinlock_init(NovaSpinlock *lock);
void spin_lock(NovaSpinlock *lock);
void spin_unlock(NovaSpinlock *lock);
int spin_trylock(NovaSpinlock *lock);
