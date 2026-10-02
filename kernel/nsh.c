#include "../include/nova/types.h"
#include "../include/nova/nsh.h"
#include "../include/nova/io.h"
#include "../include/nova/vfs.h"
#include "../include/nova/fd.h"
#include "../include/nova/mm.h"
#include "../include/nova/process.h"
#include "../include/nova/interrupts.h"
#include "../include/nova/rtc.h"

#define NSH_LINE 256
static char line[NSH_LINE];
static u32 length;

static void out(const char *s){if(!s)return;while(*s)putc(*s++);}
static void out_u32(u32 n){char b[11];u32 i=0;if(!n){putc('0');return;}while(n&&i<sizeof(b)-1u){b[i++]=(char)('0'+n%10u);n/=10u;}while(i)putc(b[--i]);}
static int eq(const char*a,const char*b){u32 i=0;while(a[i]&&b[i]&&a[i]==b[i])++i;return a[i]==0&&b[i]==0;}
static const char*skip_space(const char*s){while(*s==' '||*s=='\t')++s;return s;}

static void next_word(const char**src,char*dst,u32 cap){
    const char*s=skip_space(*src);u32 n=0;
    if(*s=='"'||*s=='\''){char q=*s++;while(*s&&*s!=q){if(n+1u<cap)dst[n++]=*s;++s;}if(*s==q)++s;}
    else while(*s&&*s!=' '&&*s!='\t'){if(n+1u<cap)dst[n++]=*s;++s;}
    dst[n]=0;*src=s;
}

static const char*state_name(int state){
    switch(state){
        case NOVA_PROC_READY:return "READY";
        case NOVA_PROC_RUNNING:return "RUNNING";
        case NOVA_PROC_BLOCKED:return "BLOCKED";
        case NOVA_PROC_ZOMBIE:return "ZOMBIE";
        default:return "UNUSED";
    }
}

static void cmd_help(void){
    out("Nova Shell (nsh)\nBuilt-in commands:\n");
    out("  help man clear cls echo pwd ls dir cat type touch mkdir rm stat write\n");
    out("  uname date free ps kill sleep history env whoami true false exit\n");
    out("Pipes, redirection, globbing, variables, scripts, aliases, job control,\n");
    out("and external programs are being built into NSH.\n");
}

static void cmd_ls(void){
    char name[64];u32 inode,type,i=0;
    while(vfs_list(vfs_root(),i++,name,sizeof(name),&inode,&type)){out(name);if(type==0)out("/");out("  ");}
    putc('\n');
}

static void cmd_stat(const char*arg){
    u32 inode=vfs_find(arg),type,size,parent;
    if(!inode||!vfs_stat(inode,&type,&size,&parent)){out("stat: ");out(arg);out(": No such file\n");return;}
    out("File: ");out(arg);putc('\n');out("Inode: ");out_u32(inode);putc('\n');
    out("Type: ");out(type==0?"directory":"file");putc('\n');out("Size: ");out_u32(size);out(" bytes\n");
    out("Parent: ");out_u32(parent);putc('\n');
}

static void cmd_cat(const char*arg){
    u32 inode=vfs_find(arg);if(!inode){out("cat: ");out(arg);out(": No such file\n");return;}
    u32 size=vfs_size(inode);
    for(u32 offset=0;offset<size;++offset){u8 c;if(vfs_read_at(inode,offset,&c,1)==1)putc((char)c);}
    putc('\n');
}

static void cmd_touch(const char*arg){
    if(!arg[0]){out("touch: missing file operand\n");return;}
    if(!vfs_create(vfs_root(),arg)){out("touch: cannot create '");out(arg);out("'\n");}
}

static void cmd_mkdir(const char*arg){
    if(!arg[0]){out("mkdir: missing operand\n");return;}
    if(!vfs_mkdir(vfs_root(),arg)){out("mkdir: cannot create directory '");out(arg);out("'\n");}
}

static void cmd_write(const char*args){
    char name[64];const char*p=args;next_word(&p,name,sizeof(name));p=skip_space(p);
    if(!name[0]||!*p){out("write: usage: write FILE TEXT\n");return;}
    u32 inode=vfs_find(name);
    if(!inode){if(!vfs_create(vfs_root(),name)){out("write: cannot create file\n");return;}inode=vfs_find(name);}
    u32 n=0;while(p[n]&&n<1023u)++n;
    if(vfs_write(inode,(const u8*)p,n)<0)out("write: failed\n");
}

static void cmd_free(void){
    out("Memory:\n  total frames: ");out_u32(mm_total_count());putc('\n');
    out("  free frames:  ");out_u32(mm_free_count());putc('\n');
    out("  heap free:    ");out_u32(mm_heap_free());out(" bytes\n");
}

static void cmd_ps(void){
    u32 n=process_count();out("PID  STATE    NAME\n");
    for(u32 i=1;i<=n;++i){
        int pid=process_find(i);if(pid<0)continue;
        out_u32((u32)pid);out("  ");out(state_name(process_state((u32)pid)));out("  ");
        out(process_name((u32)pid));putc('\n');
    }
}

void nsh_execute(const char*input){
    const char*p=skip_space(input);char cmd[32],arg[128];next_word(&p,cmd,sizeof(cmd));p=skip_space(p);
    if(!cmd[0])return;

    if(eq(cmd,"help")||eq(cmd,"man")){cmd_help();return;}
    if(eq(cmd,"clear")||eq(cmd,"cls")){volatile u16*v=(volatile u16*)0xB8000;for(u32 i=0;i<80u*25u;++i)v[i]=0x0720;return;}
    if(eq(cmd,"echo")){out(p);putc('\n');return;}
    if(eq(cmd,"pwd")){out("/\n");return;}
    if(eq(cmd,"ls")||eq(cmd,"dir")){cmd_ls();return;}
    if(eq(cmd,"cat")||eq(cmd,"type")){next_word(&p,arg,sizeof(arg));if(!arg[0])out("cat: missing file operand\n");else cmd_cat(arg);return;}
    if(eq(cmd,"touch")){next_word(&p,arg,sizeof(arg));cmd_touch(arg);return;}
    if(eq(cmd,"mkdir")){next_word(&p,arg,sizeof(arg));cmd_mkdir(arg);return;}
    if(eq(cmd,"stat")){next_word(&p,arg,sizeof(arg));if(!arg[0])out("stat: missing operand\n");else cmd_stat(arg);return;}
    if(eq(cmd,"write")){cmd_write(p);return;}
    if(eq(cmd,"uname")){out("NovaOS nova 0.3 i386 x86\n");return;}
    if(eq(cmd,"whoami")){out("root\n");return;}
    if(eq(cmd,"date")){NovaRtcTime t;if(!rtc_read(&t)){out("date: RTC unavailable\n");return;}out_u32(t.year);putc('-');if(t.month<10)putc('0');out_u32(t.month);putc('-');if(t.day<10)putc('0');out_u32(t.day);putc(' ');if(t.hour<10)putc('0');out_u32(t.hour);putc(':');if(t.minute<10)putc('0');out_u32(t.minute);putc(':');if(t.second<10)putc('0');out_u32(t.second);putc('\n');return;}
    if(eq(cmd,"free")){cmd_free();return;}
    if(eq(cmd,"ps")){cmd_ps();return;}
    if(eq(cmd,"sleep")){next_word(&p,arg,sizeof(arg));u32 ticks=0;for(u32 i=0;arg[i]>='0'&&arg[i]<='9';++i)ticks=ticks*10u+(arg[i]-'0');process_sleep(ticks);return;}
    if(eq(cmd,"true")||eq(cmd,"false"))return;
    if(eq(cmd,"env")){out("USER=root\nHOME=/root\nSHELL=/bin/nsh\nPATH=/bin:/system/bin\n");return;}
    if(eq(cmd,"history")){out("NSH history storage is not implemented yet.\n");return;}
    if(eq(cmd,"rm")){out("rm: deletion is not implemented in the current VFS yet\n");return;}

    out("nsh: command not found: ");out(cmd);putc('\n');
}

void nsh_init(void){length=0;line[0]=0;out("Nova Shell (nsh)\nType 'help' for commands.\n");}
void nsh_run(void){
    while(keyboard_available()){
        int c=keyboard_read();if(c<0)continue;
        if(c=='\n'){putc('\n');line[length]=0;nsh_execute(line);length=0;line[0]=0;out("nsh$ ");}
        else if(c=='\b'){if(length){--length;putc('\b');}}
        else if(c>=32&&c<127&&length+1u<NSH_LINE){line[length++]=(char)c;putc((char)c);}
    }
}
