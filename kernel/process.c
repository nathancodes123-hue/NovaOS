#include "../include/nova/types.h"
#include "../include/nova/pe.h"
#include "../include/nova/interrupts.h"
#include "../include/nova/process.h"

extern void *kmalloc(usize);
extern void kfree(void *);

#define MAX_PROCESSES 32
#define STACK_SIZE 8192
enum { PROC_UNUSED, PROC_READY, PROC_RUNNING, PROC_BLOCKED, PROC_ZOMBIE };

typedef struct { u32 pid,ppid,esp,ebp,cr3,entry,wake_tick,exit_status; u8 state; char name[32]; u8 *stack; } Process;
static Process proc[MAX_PROCESSES]; static u32 next_pid=1,current=0;
void scheduler(void);

static void copy_name(char*d,const char*s){u32 i=0;if(!s){d[0]=0;return;}for(;i<31&&s[i];++i)d[i]=s[i];d[i]=0;}

int process_create(const char*name){
    for(u32 i=0;i<MAX_PROCESSES;++i){if(proc[i].state!=PROC_UNUSED)continue;u8*stack=(u8*)kmalloc(STACK_SIZE);if(!stack)return -1;
        proc[i].pid=next_pid++;proc[i].ppid=(current<MAX_PROCESSES)?proc[current].pid:0;proc[i].state=PROC_READY;proc[i].entry=0;proc[i].wake_tick=0;proc[i].exit_status=0;
        proc[i].stack=stack;proc[i].esp=(u32)(stack+STACK_SIZE-16);proc[i].ebp=proc[i].esp;proc[i].cr3=0;copy_name(proc[i].name,name);return (int)proc[i].pid;}
    return -1;
}
int process_exec_pe(const char*name){PEInfo info;u32 entry_rva=0;if(pe_load(name,&info,&entry_rva)!=0)return -1;int pid=process_create(name);if(pid<0)return -1;for(u32 i=0;i<MAX_PROCESSES;++i)if(proc[i].pid==(u32)pid){proc[i].entry=info.image_base+entry_rva;break;}return pid;}
void process_exit(int status){if(current<MAX_PROCESSES&&proc[current].state==PROC_RUNNING){proc[current].exit_status=(u32)status;proc[current].state=PROC_ZOMBIE;}scheduler();}
int process_sleep(u32 ticks){if(current>=MAX_PROCESSES||proc[current].state!=PROC_RUNNING)return 0;if(!ticks)return 1;proc[current].wake_tick=interrupt_ticks()+ticks;proc[current].state=PROC_BLOCKED;scheduler();return 1;}
void process_wake(u32 pid){int i=process_find(pid);if(i>=0&&proc[i].state==PROC_BLOCKED){proc[i].wake_tick=0;proc[i].state=PROC_READY;}}
void process_tick(void){u32 now=interrupt_ticks();for(u32 i=0;i<MAX_PROCESSES;++i)if(proc[i].state==PROC_BLOCKED&&(u32)(now-proc[i].wake_tick)<0x80000000u)process_wake(proc[i].pid);if(proc[current].state==PROC_RUNNING)scheduler();}

void process_init(void){
    for(u32 i=0;i<MAX_PROCESSES;++i)proc[i].state=PROC_UNUSED;
    current=0;next_pid=1;
    if(process_create("kernel")>0)proc[0].state=PROC_RUNNING;
    /* PID 2: init owns system services. */
    int init_pid=process_create("init");
    if(init_pid>0){
        int init_index=process_find((u32)init_pid);
        /* Temporarily make init the parent while registering nsh. */
        if(init_index>=0){u32 saved=current;current=(u32)init_index;process_create("nsh");current=saved;}
    }
}

void scheduler(void){
    u32 start=current;
    for(u32 n=1;n<=MAX_PROCESSES;++n){u32 i=(start+n)%MAX_PROCESSES;if(proc[i].state!=PROC_READY)continue;if(current<MAX_PROCESSES&&proc[current].state==PROC_RUNNING)proc[current].state=PROC_READY;current=i;proc[i].state=PROC_RUNNING;return;}
}
u32 process_current_pid(void){return current<MAX_PROCESSES?proc[current].pid:0;}
u32 process_current_entry(void){return current<MAX_PROCESSES?proc[current].entry:0;}
int process_find(u32 pid){for(u32 i=0;i<MAX_PROCESSES;++i)if(proc[i].state!=PROC_UNUSED&&proc[i].pid==pid)return (int)i;return -1;}
u32 process_count(void){u32 c=0;for(u32 i=0;i<MAX_PROCESSES;++i)if(proc[i].state!=PROC_UNUSED)++c;return c;}
u32 process_ready_count(void){u32 c=0;for(u32 i=0;i<MAX_PROCESSES;++i)if(proc[i].state==PROC_READY)++c;return c;}
int process_state(u32 pid){int i=process_find(pid);return i<0?PROC_UNUSED:proc[i].state;}
const char*process_name(u32 pid){int i=process_find(pid);return i<0?NULL:proc[i].name;}
int process_kill(u32 pid,int status){int i=process_find(pid);if(i<0||(u32)i==current)return 0;proc[i].exit_status=(u32)status;if(proc[i].stack)kfree(proc[i].stack);proc[i].stack=NULL;proc[i].state=PROC_ZOMBIE;return 1;}
