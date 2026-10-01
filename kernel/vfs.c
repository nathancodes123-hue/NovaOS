#include "../include/nova/types.h"
#include "../include/nova/io.h"
#define MAX_FILES 128
#define MAX_NAME 48
enum{VFS_DIR,VFS_FILE};
typedef struct{u32 inode,parent,size,type;u8 used;char name[MAX_NAME];u8 data[1024];} VNode;
static VNode nodes[MAX_FILES];static u32 next_inode=1;
static void cp(char*d,const char*s){u32 i=0;while(i<MAX_NAME-1&&s[i]){d[i]=s[i];i++;}d[i]=0;}
static VNode*find_child(u32 parent,const char*n){for(u32 i=0;i<MAX_FILES;i++)if(nodes[i].used&&nodes[i].parent==parent){u32 j=0;while(nodes[i].name[j]&&n[j]&&nodes[i].name[j]==n[j])j++;if(!nodes[i].name[j]&&!n[j])return &nodes[i];}return NULL;}
static VNode*alloc(u32 parent,u32 type,const char*n){for(u32 i=0;i<MAX_FILES;i++)if(!nodes[i].used){nodes[i]=(VNode){.inode=next_inode++,.parent=parent,.size=0,.type=type,.used=1};cp(nodes[i].name,n);return &nodes[i];}return NULL;}
void vfs_init(void){for(u32 i=0;i<MAX_FILES;i++)nodes[i].used=0;next_inode=1;VNode*r=alloc(0,VFS_DIR,"/");if(!r)return;alloc(r->inode,VFS_DIR,"bin");alloc(r->inode,VFS_DIR,"home");alloc(r->inode,VFS_DIR,"system");}
int vfs_mkdir(u32 parent,const char*n){if(!n||find_child(parent,n))return 0;return alloc(parent,VFS_DIR,n)!=NULL;}
int vfs_create(u32 parent,const char*n){if(!n||find_child(parent,n))return 0;return alloc(parent,VFS_FILE,n)!=NULL;}
int vfs_write(u32 inode,const u8*d,u32 n){for(u32 i=0;i<MAX_FILES;i++)if(nodes[i].used&&nodes[i].inode==inode&&nodes[i].type==VFS_FILE){if(n>sizeof(nodes[i].data))n=sizeof(nodes[i].data);for(u32 j=0;j<n;j++)nodes[i].data[j]=d[j];nodes[i].size=n;return n;}return -1;}
int vfs_read(u32 inode,u8*d,u32 n){for(u32 i=0;i<MAX_FILES;i++)if(nodes[i].used&&nodes[i].inode==inode){if(n>nodes[i].size)n=nodes[i].size;for(u32 j=0;j<n;j++)d[j]=nodes[i].data[j];return n;}return -1;}


u32 vfs_find(const char *name) {
    if (!name) return 0;
    VNode *node = find_child(1, name);
    return node ? node->inode : 0;
}

u32 vfs_size(u32 inode) {
    for (u32 i = 0; i < MAX_FILES; ++i)
        if (nodes[i].used && nodes[i].inode == inode)
            return nodes[i].size;
    return 0;
}
