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
#define NSH_HISTORY 16

static char line[NSH_LINE];
static char history[NSH_HISTORY][NSH_LINE];
static u32 history_count,length,cwd;

static void out(const char*s){if(!s)return;while(*s)putc(*s++);}
static void out_u32(u32 n){char b[11];u32 i=0;if(!n){putc('0');return;}while(n&&i<sizeof(b)-1u){b[i++]=(char)('0'+n%10u);n/=10u;}while(i)putc(b[--i]);}
static int eq(const char*a,const char*b){u32 i=0;while(a[i]&&b[i]&&a[i]==b[i])++i;return a[i]==0&&b[i]==0;}
static const char*skip_space(const char*s){while(*s==' '||*s=='\t')++s;return s;}
static void copy_str(char*d,const char*s,u32 cap){if(!cap)return;u32 i=0;while(i+1u<cap&&s&&s[i]){d[i]=s[i];++i;}d[i]=0;}
static void next_word(const char**src,char*dst,u32 cap){const char*s=skip_space(*src);u32 n=0;if(*s=='"'||*s=='\''){char q=*s++;while(*s&&*s!=q){if(n+1u<cap)dst[n++]=*s;++s;}if(*s==q)++s;}else while(*s&&*s!=' '&&*s!='\t'){if(n+1u<cap)dst[n++]=*s;++s;}if(cap)dst[n]=0;*src=s;}
static u32 resolve(const char*p){return(!p||!p[0])?cwd:vfs_resolve(cwd,p);}

static int split_parent(const char*path,u32*parent,char*name,u32 cap){
    char tmp[128];copy_str(tmp,path,sizeof(tmp));u32 len=0;while(tmp[len])++len;while(len&&tmp[len-1]=='/')tmp[--len]=0;if(!len)return 0;
    u32 slash=len;while(slash&&tmp[slash-1]!='/')--slash;const char*base=tmp+slash;if(!*base)return 0;copy_str(name,base,cap);
    if(slash==0)*parent=cwd;else{if(slash==1&&tmp[0]=='/')tmp[1]=0;else tmp[slash-1]=0;*parent=resolve(tmp[0]?tmp:"/");}
    return *parent!=0;
}
static const char*state_name(int s){switch(s){case NOVA_PROC_READY:return "READY";case NOVA_PROC_RUNNING:return "RUNNING";case NOVA_PROC_BLOCKED:return "BLOCKED";case NOVA_PROC_ZOMBIE:return "ZOMBIE";default:return "UNUSED";}}

static void cmd_help(void){
    out("Nova Shell (nsh)\nBuilt-in commands:\n");
    out("  help clear cls echo pwd cd ls dir cat type touch mkdir rm rmdir\n");
    out("  write stat truncate uname date free ps kill sleep history env whoami\n");
    out("  true false exit\n");
}
static void cmd_ls(const char*arg){
    u32 node=arg&&arg[0]?resolve(arg):cwd,type,size,parent;
    if(!node||!vfs_stat(node,&type,&size,&parent)){out("ls: No such file or directory\n");return;}
    if(type!=NOVA_VFS_DIR){out(arg);putc('\n');return;}
    char name[64];u32 inode,t,i=0;
    while(vfs_list(node,i++,name,sizeof(name),&inode,&t)){out(name);if(t==NOVA_VFS_DIR)out("/");out("  ");}
    putc('\n');
}
static void cmd_stat(const char*a){
    u32 inode=resolve(a),type,size,parent;
    if(!inode||!vfs_stat(inode,&type,&size,&parent)){out("stat: ");out(a);out(": No such file\n");return;}
    out("File: ");out(a);putc('\n');out("Inode: ");out_u32(inode);putc('\n');out("Type: ");out(type==NOVA_VFS_DIR?"directory":"file");putc('\n');out("Size: ");out_u32(size);out(" bytes\n");out("Parent: ");out_u32(parent);putc('\n');
}
static void cmd_cat(const char*a){
    u32 inode=resolve(a),type,size,parent;
    if(!inode||!vfs_stat(inode,&type,&size,&parent)){out("cat: ");out(a);out(": No such file\n");return;}
    if(type!=NOVA_VFS_FILE){out("cat: ");out(a);out(": Is a directory\n");return;}
    for(u32 i=0;i<size;++i){u8 c;if(vfs_read_at(inode,i,&c,1)==1)putc((char)c);}putc('\n');
}
static void cmd_touch(const char*a){
    if(!a[0]){out("touch: missing file operand\n");return;}if(resolve(a))return;
    u32 parent;char name[64];if(!split_parent(a,&parent,name,sizeof(name))||!vfs_create(parent,name)){out("touch: cannot create '");out(a);out("'\n");}
}
static void cmd_mkdir(const char*a){
    if(!a[0]){out("mkdir: missing operand\n");return;}u32 parent;char name[64];
    if(!split_parent(a,&parent,name,sizeof(name))||!vfs_mkdir(parent,name)){out("mkdir: cannot create directory '");out(a);out("'\n");}
}
static void cmd_rm(const char*a,int dir_only){
    if(!a[0]){out(dir_only?"rmdir: missing operand\n":"rm: missing operand\n");return;}
    u32 inode=resolve(a),type,size,parent;
    if(!inode||!vfs_stat(inode,&type,&size,&parent)){out(dir_only?"rmdir: ":"rm: ");out(a);out(": No such file or directory\n");return;}
    if(inode==vfs_root()){out("rm: cannot remove root\n");return;}
    if(dir_only&&type!=NOVA_VFS_DIR){out("rmdir: ");out(a);out(": Not a directory\n");return;}
    if(!dir_only&&type==NOVA_VFS_DIR){char child[64];u32 ci,ct;if(vfs_list(inode,0,child,sizeof(child),&ci,&ct)){out("rm: ");out(a);out(": Is a directory\n");return;}}
    char name[64];u32 p;if(!split_parent(a,&p,name,sizeof(name))||!vfs_unlink(p,name)){out(dir_only?"rmdir: ":"rm: ");out(a);out(": cannot remove\n");}
}
static void cmd_write(const char*args){
    char name[64];const char*p=args;next_word(&p,name,sizeof(name));p=skip_space(p);
    if(!name[0]||!*p){out("write: usage: write FILE TEXT\n");return;}
    u32 inode=resolve(name);
    if(!inode){u32 parent;char base[64];if(!split_parent(name,&parent,base,sizeof(base))||!vfs_create(parent,base)){out("write: cannot create file\n");return;}inode=resolve(name);}
    u32 n=0;while(p[n]&&n<1024u)++n;
    if(vfs_write(inode,(const u8*)p,n)<0)out("write: failed\n");
}
static void cmd_free(void){out("Memory:\n  total frames: ");out_u32(mm_total_count());putc('\n');out("  free frames:  ");out_u32(mm_free_count());putc('\n');out("  heap free:    ");out_u32(mm_heap_free());out(" bytes\n");}
static void cmd_ps(void){out("PID  STATE    NAME\n");for(u32 i=1;i<=process_count()+4u;++i)if(process_find(i)>=0){out_u32(i);out("  ");out(state_name(process_state(i)));out("  ");out(process_name(i));putc('\n');}}

void nsh_execute(const char*input){
    const char*p=skip_space(input);char cmd[32],arg[128];next_word(&p,cmd,sizeof(cmd));p=skip_space(p);if(!cmd[0])return;
    if(eq(cmd,"help")||eq(cmd,"man")){cmd_help();return;}if(eq(cmd,"clear")||eq(cmd,"cls")){volatile u16*v=(volatile u16*)0xB8000;for(u32 i=0;i<2000;++i)v[i]=0x0720;return;}
    if(eq(cmd,"echo")){out(p);putc('\n');return;}if(eq(cmd,"pwd")){out("/\n");return;}
    if(eq(cmd,"cd")){next_word(&p,arg,sizeof(arg));u32 target=arg[0]?resolve(arg):vfs_root(),type,size,parent;if(!target||!vfs_stat(target,&type,&size,&parent)||type!=NOVA_VFS_DIR){out("cd: ");out(arg);out(": No such directory\n");}else cwd=target;return;}
    if(eq(cmd,"ls")||eq(cmd,"dir")){next_word(&p,arg,sizeof(arg));cmd_ls(arg);return;}if(eq(cmd,"cat")||eq(cmd,"type")){next_word(&p,arg,sizeof(arg));if(!arg[0])out("cat: missing file operand\n");else cmd_cat(arg);return;}
    if(eq(cmd,"touch")){next_word(&p,arg,sizeof(arg));cmd_touch(arg);return;}if(eq(cmd,"mkdir")){next_word(&p,arg,sizeof(arg));cmd_mkdir(arg);return;}
    if(eq(cmd,"rm")){next_word(&p,arg,sizeof(arg));cmd_rm(arg,0);return;}if(eq(cmd,"rmdir")){next_word(&p,arg,sizeof(arg));cmd_rm(arg,1);return;}
    if(eq(cmd,"stat")){next_word(&p,arg,sizeof(arg));if(!arg[0])out("stat: missing operand\n");else cmd_stat(arg);return;}if(eq(cmd,"write")){cmd_write(p);return;}
    if(eq(cmd,"truncate")){next_word(&p,arg,sizeof(arg));u32 inode=resolve(arg);if(!inode||!vfs_truncate(inode))out("truncate: cannot truncate file\n");return;}
    if(eq(cmd,"cp")||eq(cmd,"mv")){out(cmd);out(": not implemented yet\n");return;}
    if(eq(cmd,"uname")){out("NovaOS nova 0.3 i386 x86\n");return;}if(eq(cmd,"whoami")){out("root\n");return;}
    if(eq(cmd,"date")){NovaRtcTime t;if(!rtc_read(&t)){out("date: RTC unavailable\n");return;}out_u32(t.year);putc('-');if(t.month<10)putc('0');out_u32(t.month);putc('-');if(t.day<10)putc('0');out_u32(t.day);putc(' ');if(t.hour<10)putc('0');out_u32(t.hour);putc(':');if(t.minute<10)putc('0');out_u32(t.minute);putc(':');if(t.second<10)putc('0');out_u32(t.second);putc('\n');return;}
    if(eq(cmd,"free")){cmd_free();return;}if(eq(cmd,"ps")){cmd_ps();return;}
    if(eq(cmd,"sleep")){next_word(&p,arg,sizeof(arg));u32 ticks=0;for(u32 i=0;arg[i]>='0'&&arg[i]<='9';++i)ticks=ticks*10u+(arg[i]-'0');process_sleep(ticks);return;}
    if(eq(cmd,"kill")){next_word(&p,arg,sizeof(arg));u32 pid=0;for(u32 i=0;arg[i]>='0'&&arg[i]<='9';++i)pid=pid*10u+(arg[i]-'0');if(!pid||!process_kill(pid,1))out("kill: failed\n");return;}
    if(eq(cmd,"true")||eq(cmd,"false"))return;if(eq(cmd,"env")){out("USER=root\nHOME=/root\nSHELL=/bin/nsh\nPATH=/bin:/system/bin\nPWD=/\n");return;}
    if(eq(cmd,"history")){for(u32 i=0;i<history_count;++i){out_u32(i+1);out("  ");out(history[i]);putc('\n');}return;}if(eq(cmd,"exit")){out("nsh: exit requested\n");return;}
    out("nsh: command not found: ");out(cmd);putc('\n');
}
void nsh_init(void){length=0;line[0]=0;history_count=0;cwd=vfs_root();out("Nova Shell (nsh)\nType 'help' for commands.\n");}
void nsh_run(void){
    while(keyboard_available()){int c=keyboard_read();if(c<0)continue;
        if(c=='\n'){putc('\n');line[length]=0;if(length){if(history_count<NSH_HISTORY)copy_str(history[history_count++],line,NSH_LINE);else{for(u32 i=1;i<NSH_HISTORY;++i)copy_str(history[i-1],history[i],NSH_LINE);copy_str(history[NSH_HISTORY-1],line,NSH_LINE);}}nsh_execute(line);length=0;line[0]=0;out("nsh$ ");} 
        else if(c=='\b'){if(length){--length;putc('\b');}} else if(c>=32&&c<127&&length+1u<NSH_LINE){line[length++]=(char)c;putc((char)c);}
    }
}
