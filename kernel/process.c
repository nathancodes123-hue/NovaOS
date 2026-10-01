#include "../include/nova/types.h"
#include "../include/nova/io.h"
extern void*kmalloc(usize);
#define MAX_PROCESSES 32
#define STACK_SIZE 8192
enum{PROC_UNUSED,PROC_READY,PROC_RUNNING,PROC_BLOCKED,PROC_ZOMBIE};
typedef struct{u32 pid,ppid;u8 state;u32 esp,ebp,cr3,entry;char name[32];u8*stack;} Process;
static Process proc[MAX_PROCESSES];static u32 next_pid=1,current=0;
static void copy_name(char*d,const char*s){u32 i=0;if(!s){d[0]=0;return;}for(;i<31&&s[i];i++)d[i]=s[i];d[i]=0;}
int process_create(const char*name){
 for(u32 i=0;i<MAX_PROCESSES;i++)if(proc[i].state==PROC_UNUSED){
  proc[i].pid=next_pid++;proc[i].ppid=current?proc[current].pid:0;proc[i].state=PROC_READY;proc[i].entry=0;
  proc[i].stack=kmalloc(STACK_SIZE);if(!proc[i].stack){proc[i].state=PROC_UNUSED;return -1;}
  proc[i].esp=(u32)(proc[i].stack+STACK_SIZE-16);proc[i].ebp=proc[i].esp;proc[i].cr3=0;copy_name(proc[i].name,name);return proc[i].pid;
 }return -1;
}
int process_exec(const char*name,u32 entry){
 int pid=process_create(name);if(pid<0)return -1;
 for(u32 i=0;i<MAX_PROCESSES;i++)if(proc[i].pid==(u32)pid){proc[i].entry=entry;break;}
 return pid;
}
void process_exit(int status){(void)status;if(current<MAX_PROCESSES){proc[current].state=PROC_ZOMBIE;scheduler();}}
void process_init(void){for(u32 i=0;i<MAX_PROCESSES;i++)proc[i].state=PROC_UNUSED;process_create("kernel");process_create("desktop");}
void scheduler(void){u32 start=current;for(u32 n=0;n<MAX_PROCESSES;n++){u32 i=(start+n+1)%MAX_PROCESSES;if(proc[i].state==PROC_READY){if(current<MAX_PROCESSES&&proc[current].state==PROC_RUNNING)proc[current].state=PROC_READY;current=i;proc[i].state=PROC_RUNNING;return;}}}
u32 process_current_pid(void){return current<MAX_PROCESSES?proc[current].pid:0;}
u32 process_current_entry(void){return current<MAX_PROCESSES?proc[current].entry:0;}
