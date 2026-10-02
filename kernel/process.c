#include "../include/nova/types.h"
#include "../include/nova/pe.h"
#include "../include/nova/interrupts.h"
#include "../include/nova/process.h"

extern void *kmalloc(usize);
extern void kfree(void *);

#define INITIAL_PROCESS_CAPACITY 32u
#define PROCESS_NAME_MAX 32u
#define STACK_SIZE 8192u

enum { PROC_UNUSED, PROC_READY, PROC_RUNNING, PROC_BLOCKED, PROC_ZOMBIE };

typedef struct {
    u32 pid,ppid,esp,ebp,cr3,entry,wake_tick,exit_status;
    u8 state;
    char name[PROCESS_NAME_MAX];
    u8 *stack;
} Process;

/*
 * The process table is dynamically sized. There is deliberately no
 * MAX_PROCESSES constant: the practical limit is available kernel memory.
 */
static Process *proc;
static u32 proc_capacity;
static u32 next_pid=1;
static u32 current=0;

void scheduler(void);

static void copy_name(char*d,const char*s){
    u32 i=0;
    if(!s){d[0]=0;return;}
    for(;i<PROCESS_NAME_MAX-1u&&s[i];++i)d[i]=s[i];
    d[i]=0;
}

static int process_table_grow(void){
    u32 old_capacity=proc_capacity;
    u32 new_capacity=old_capacity?old_capacity*2u:INITIAL_PROCESS_CAPACITY;
    if(new_capacity<=old_capacity)return 0;

    Process *new_proc=(Process*)kmalloc((usize)new_capacity*sizeof(Process));
    if(!new_proc)return 0;

    for(u32 i=0;i<new_capacity;++i){
        new_proc[i].pid=0; new_proc[i].ppid=0;
        new_proc[i].esp=0; new_proc[i].ebp=0; new_proc[i].cr3=0;
        new_proc[i].entry=0; new_proc[i].wake_tick=0; new_proc[i].exit_status=0;
        new_proc[i].state=PROC_UNUSED; new_proc[i].name[0]=0; new_proc[i].stack=NULL;
    }

    if(proc){
        for(u32 i=0;i<old_capacity;++i)new_proc[i]=proc[i];
        kfree(proc);
    }
    proc=new_proc;
    proc_capacity=new_capacity;
    return 1;
}

static int process_find_free_slot(void){
    for(u32 i=0;i<proc_capacity;++i)
        if(proc[i].state==PROC_UNUSED)return (int)i;

    u32 old_capacity=proc_capacity;
    if(!process_table_grow())return -1;
    return (int)old_capacity;
}

int process_create(const char*name){
    int slot=process_find_free_slot();
    if(slot<0)return -1;

    u8*stack=(u8*)kmalloc(STACK_SIZE);
    if(!stack)return -1;

    Process *p=&proc[(u32)slot];
    p->pid=next_pid++;
    if(!next_pid)next_pid=1;
    p->ppid=(current<proc_capacity&&proc[current].state!=PROC_UNUSED)?proc[current].pid:0;
    p->state=PROC_READY;
    p->entry=0; p->wake_tick=0; p->exit_status=0;
    p->stack=stack;
    p->esp=(u32)(stack+STACK_SIZE-16u);
    p->ebp=p->esp; p->cr3=0;
    copy_name(p->name,name);
    return (int)p->pid;
}

int process_exec_pe(const char*name){
    PEInfo info;
    u32 entry_rva=0;
    if(pe_load(name,&info,&entry_rva)!=0)return -1;
    int pid=process_create(name);
    if(pid<0)return -1;
    int i=process_find((u32)pid);
    if(i>=0)proc[(u32)i].entry=info.image_base+entry_rva;
    return pid;
}

void process_exit(int status){
    if(current<proc_capacity&&proc[current].state==PROC_RUNNING){
        proc[current].exit_status=(u32)status;
        proc[current].state=PROC_ZOMBIE;
    }
    scheduler();
}

int process_sleep(u32 ticks){
    if(current>=proc_capacity||proc[current].state!=PROC_RUNNING)return 0;
    if(!ticks)return 1;
    proc[current].wake_tick=interrupt_ticks()+ticks;
    proc[current].state=PROC_BLOCKED;
    scheduler();
    return 1;
}

void process_wake(u32 pid){
    int i=process_find(pid);
    if(i>=0&&proc[(u32)i].state==PROC_BLOCKED){
        proc[(u32)i].wake_tick=0;
        proc[(u32)i].state=PROC_READY;
    }
}

void process_tick(void){
    u32 now=interrupt_ticks();
    for(u32 i=0;i<proc_capacity;++i)
        if(proc[i].state==PROC_BLOCKED &&
           (u32)(now-proc[i].wake_tick)<0x80000000u)
            process_wake(proc[i].pid);

    if(current<proc_capacity&&proc[current].state==PROC_RUNNING)
        scheduler();
}

void process_init(void){
    proc=NULL; proc_capacity=0; current=0; next_pid=1;
    if(!process_table_grow())return;

    if(process_create("kernel")>0)proc[0].state=PROC_RUNNING;

    int init_pid=process_create("init");
    if(init_pid>0){
        int init_index=process_find((u32)init_pid);
        if(init_index>=0){
            u32 saved=current;
            current=(u32)init_index;
            process_create("nsh");
            current=saved;
        }
    }
}

void scheduler(void){
    if(!proc_capacity)return;
    u32 start=current;
    for(u32 n=1;n<=proc_capacity;++n){
        u32 i=(start+n)%proc_capacity;
        if(proc[i].state!=PROC_READY)continue;
        if(current<proc_capacity&&proc[current].state==PROC_RUNNING)
            proc[current].state=PROC_READY;
        current=i;
        proc[i].state=PROC_RUNNING;
        return;
    }
}

u32 process_current_pid(void){return current<proc_capacity?proc[current].pid:0;}
u32 process_current_entry(void){return current<proc_capacity?proc[current].entry:0;}

int process_find(u32 pid){
    for(u32 i=0;i<proc_capacity;++i)
        if(proc[i].state!=PROC_UNUSED&&proc[i].pid==pid)return (int)i;
    return -1;
}

u32 process_count(void){
    u32 c=0;
    for(u32 i=0;i<proc_capacity;++i)if(proc[i].state!=PROC_UNUSED)++c;
    return c;
}

u32 process_ready_count(void){
    u32 c=0;
    for(u32 i=0;i<proc_capacity;++i)if(proc[i].state==PROC_READY)++c;
    return c;
}

int process_state(u32 pid){
    int i=process_find(pid);
    return i<0?PROC_UNUSED:proc[(u32)i].state;
}

const char*process_name(u32 pid){
    int i=process_find(pid);
    return i<0?NULL:proc[(u32)i].name;
}

int process_kill(u32 pid,int status){
    int i=process_find(pid);
    if(i<0||(u32)i==current)return 0;
    Process *p=&proc[(u32)i];
    p->exit_status=(u32)status;
    if(p->stack)kfree(p->stack);
    p->stack=NULL;
    p->state=PROC_ZOMBIE;
    return 1;
}
