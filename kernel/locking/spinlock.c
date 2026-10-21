#include "../include/nova/types.h"
#include "../include/nova/spinlock.h"

void spinlock_init(NovaSpinlock *lock) {
    if (lock)
        lock->value = 0;
}

void spin_lock(NovaSpinlock *lock) {
    if (!lock)
        return;

    u32 expected;
    do {
        expected = 0;
        __asm__ volatile (
            "lock cmpxchgl %2, %1"
            : "+a"(expected), "+m"(lock->value)
            : "r"(1u)
            : "memory"
        );
    } while (expected != 0);
}

void spin_unlock(NovaSpinlock *lock) {
    if (!lock)
        return;

    __asm__ volatile ("movl $0, %0" : "=m"(lock->value) :: "memory");
}

int spin_trylock(NovaSpinlock *lock) {
    if (!lock)
        return 0;

    u32 expected = 0;
    __asm__ volatile (
        "lock cmpxchgl %2, %1"
        : "+a"(expected), "+m"(lock->value)
        : "r"(1u)
        : "memory"
    );
    return expected == 0;
}
