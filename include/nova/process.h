#pragma once
#include "types.h"

enum {
    NOVA_PROC_UNUSED = 0,
    NOVA_PROC_READY,
    NOVA_PROC_RUNNING,
    NOVA_PROC_BLOCKED,
    NOVA_PROC_ZOMBIE
};

int process_create(const char *name);
int process_exec_pe(const char *name);
void process_init(void);
void process_exit(int status);
void scheduler(void);

u32 process_current_pid(void);
u32 process_current_entry(void);
int process_find(u32 pid);
u32 process_count(void);
u32 process_ready_count(void);
int process_state(u32 pid);
const char *process_name(u32 pid);
int process_kill(u32 pid, int status);
