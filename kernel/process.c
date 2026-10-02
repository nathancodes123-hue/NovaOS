#include "../include/nova/types.h"
#include "../include/nova/pe.h"

extern void *kmalloc(usize);\nextern void kfree(void *);

#define MAX_PROCESSES 32
#define STACK_SIZE 8192

enum {
    PROC_UNUSED,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_ZOMBIE
};

typedef struct {
    u32 pid;
    u32 ppid;
    u8 state;
    u32 esp;
    u32 ebp;
    u32 cr3;
    u32 entry;
    char name[32];
    u8 *stack;
} Process;

static Process proc[MAX_PROCESSES];
static u32 next_pid = 1;
static u32 current = 0;

void scheduler(void);

static void copy_name(char *dst, const char *src) {
    u32 i = 0;
    if (!src) {
        dst[0] = 0;
        return;
    }
    for (; i < 31 && src[i]; ++i)
        dst[i] = src[i];
    dst[i] = 0;
}

int process_create(const char *name) {
    for (u32 i = 0; i < MAX_PROCESSES; ++i) {
        if (proc[i].state != PROC_UNUSED)
            continue;

        u8 *stack = (u8 *)kmalloc(STACK_SIZE);
        if (!stack)
            return -1;

        proc[i].pid = next_pid++;
        proc[i].ppid = (current < MAX_PROCESSES) ? proc[current].pid : 0;
        proc[i].state = PROC_READY;
        proc[i].entry = 0;
        proc[i].stack = stack;
        proc[i].esp = (u32)(stack + STACK_SIZE - 16);
        proc[i].ebp = proc[i].esp;
        proc[i].cr3 = 0;
        copy_name(proc[i].name, name);
        return (int)proc[i].pid;
    }
    return -1;
}

int process_exec_pe(const char *name) {
    PEInfo info;
    u32 entry_rva = 0;

    if (pe_load(name, &info, &entry_rva) != 0)
        return -1;

    int pid = process_create(name);
    if (pid < 0)
        return -1;

    for (u32 i = 0; i < MAX_PROCESSES; ++i) {
        if (proc[i].pid == (u32)pid) {
            proc[i].entry = info.image_base + entry_rva;
            break;
        }
    }

    return pid;
}

void process_exit(int status) {
    (void)status;
    if (current < MAX_PROCESSES && proc[current].state == PROC_RUNNING)
        proc[current].state = PROC_ZOMBIE;
    scheduler();
}

void process_init(void) {
    for (u32 i = 0; i < MAX_PROCESSES; ++i)
        proc[i].state = PROC_UNUSED;

    current = 0;
    next_pid = 1;

    if (process_create("kernel") > 0)
        proc[0].state = PROC_RUNNING;

    process_create("desktop");
}

void scheduler(void) {
    u32 start = current;

    for (u32 n = 1; n <= MAX_PROCESSES; ++n) {
        u32 i = (start + n) % MAX_PROCESSES;
        if (proc[i].state != PROC_READY)
            continue;

        if (current < MAX_PROCESSES && proc[current].state == PROC_RUNNING)
            proc[current].state = PROC_READY;

        current = i;
        proc[i].state = PROC_RUNNING;
        return;
    }
}

u32 process_current_pid(void) {
    return current < MAX_PROCESSES ? proc[current].pid : 0;
}

u32 process_current_entry(void) {
    return current < MAX_PROCESSES ? proc[current].entry : 0;
}

int process_find(u32 pid) {
    for (u32 i = 0; i < MAX_PROCESSES; ++i)
        if (proc[i].state != PROC_UNUSED && proc[i].pid == pid)
            return (int)i;
    return -1;
}

u32 process_count(void) {
    u32 count = 0;
    for (u32 i = 0; i < MAX_PROCESSES; ++i)
        if (proc[i].state != PROC_UNUSED)
            ++count;
    return count;
}

u32 process_ready_count(void) {
    u32 count = 0;
    for (u32 i = 0; i < MAX_PROCESSES; ++i)
        if (proc[i].state == PROC_READY)
            ++count;
    return count;
}

int process_state(u32 pid) {
    int index = process_find(pid);
    return index < 0 ? PROC_UNUSED : proc[index].state;
}

const char *process_name(u32 pid) {
    int index = process_find(pid);
    return index < 0 ? NULL : proc[index].name;
}

int process_kill(u32 pid, int status) {
    (void)status;

    int index = process_find(pid);
    if (index < 0 || (u32)index == current)
        return 0;

    if (proc[index].stack)
        kfree(proc[index].stack);

    proc[index].stack = NULL;
    proc[index].state = PROC_ZOMBIE;
    return 1;
}
