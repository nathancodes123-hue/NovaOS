#include "../include/nova/types.h"
#include "../include/nova/io.h"
#define MAX_FILES 128
#define MAX_NAME 48
enum{VFS_DIR,VFS_FILE};
typedef struct{u32 inode,parent,size,type,mode,uid,gid;u8 used;char name[MAX_NAME];u8 data[1024];} VNode;
static VNode nodes[MAX_FILES];static u32 next_inode=1;
static void cp(char*d,const char*s){u32 i=0;while(i<MAX_NAME-1&&s[i]){d[i]=s[i];i++;}d[i]=0;}
static VNode*find_child(u32 parent,const char*n){for(u32 i=0;i<MAX_FILES;i++)if(nodes[i].used&&nodes[i].parent==parent){u32 j=0;while(nodes[i].name[j]&&n[j]&&nodes[i].name[j]==n[j])j++;if(!nodes[i].name[j]&&!n[j])return &nodes[i];}return NULL;}
static VNode*alloc(u32 parent,u32 type,const char*n){for(u32 i=0;i<MAX_FILES;i++)if(!nodes[i].used){nodes[i]=(VNode){.inode=next_inode++,.parent=parent,.size=0,.type=type,.mode=(type==VFS_DIR?0755u:0644u),.uid=0,.gid=0,.used=1};cp(nodes[i].name,n);return &nodes[i];}return NULL;}
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

int vfs_stat(u32 inode, u32 *type, u32 *size, u32 *parent) {
    for (u32 i = 0; i < MAX_FILES; ++i) {
        if (!nodes[i].used || nodes[i].inode != inode)
            continue;
        if (type) *type = nodes[i].type;
        if (size) *size = nodes[i].size;
        if (parent) *parent = nodes[i].parent;
        return 1;
    }
    return 0;
}

int vfs_write_at(u32 inode, u32 offset, const u8 *data, u32 n) {
    if (!data)
        return -1;

    for (u32 i = 0; i < MAX_FILES; ++i) {
        if (!nodes[i].used || nodes[i].inode != inode || nodes[i].type != VFS_FILE)
            continue;
        if (offset >= sizeof(nodes[i].data))
            return 0;
        if (n > sizeof(nodes[i].data) - offset)
            n = sizeof(nodes[i].data) - offset;

        for (u32 j = 0; j < n; ++j)
            nodes[i].data[offset + j] = data[j];

        if (offset + n > nodes[i].size)
            nodes[i].size = offset + n;
        return (int)n;
    }
    return -1;
}

int vfs_read_at(u32 inode, u32 offset, u8 *data, u32 n) {
    if (!data)
        return -1;

    for (u32 i = 0; i < MAX_FILES; ++i) {
        if (!nodes[i].used || nodes[i].inode != inode)
            continue;
        if (offset >= nodes[i].size)
            return 0;
        if (n > nodes[i].size - offset)
            n = nodes[i].size - offset;

        for (u32 j = 0; j < n; ++j)
            data[j] = nodes[i].data[offset + j];
        return (int)n;
    }
    return -1;
}

int vfs_truncate(u32 inode) {
    for (u32 i = 0; i < MAX_FILES; ++i) {
        if (!nodes[i].used || nodes[i].inode != inode || nodes[i].type != VFS_FILE)
            continue;
        nodes[i].size = 0;
        for (u32 j = 0; j < sizeof(nodes[i].data); ++j)
            nodes[i].data[j] = 0;
        return 1;
    }
    return 0;
}

int vfs_list(u32 parent, u32 index, char *name, u32 name_size, u32 *inode, u32 *type) {
    if (!name || !name_size)
        return 0;

    u32 seen = 0;
    for (u32 i = 0; i < MAX_FILES; ++i) {
        if (!nodes[i].used || nodes[i].parent != parent)
            continue;
        if (seen++ != index)
            continue;

        u32 j = 0;
        while (j + 1 < name_size && nodes[i].name[j]) {
            name[j] = nodes[i].name[j];
            ++j;
        }
        name[j] = 0;
        if (inode) *inode = nodes[i].inode;
        if (type) *type = nodes[i].type;
        return 1;
    }
    return 0;
}

u32 vfs_root(void) {
    return 1;
}

static int path_name_eq(const char *a, const char *b) {
    u32 i=0; while(a[i]&&b[i]&&a[i]==b[i])++i; return a[i]==0&&b[i]==0;
}
static u32 path_child(u32 parent,const char *name){
    for(u32 i=0;i<MAX_FILES;++i) if(nodes[i].used&&nodes[i].parent==parent&&path_name_eq(nodes[i].name,name)) return nodes[i].inode;
    return 0;
}
u32 vfs_resolve(u32 cwd,const char *path){
    if(!path||!path[0])return cwd;
    u32 current=path[0]=='/'?vfs_root():cwd; char part[MAX_NAME]; u32 pos=path[0]=='/'?1u:0u;
    while(1){
        u32 n=0; while(path[pos]=='/')++pos;
        while(path[pos]&&path[pos]!='/'&&n+1u<MAX_NAME)part[n++]=path[pos++];
        part[n]=0; if(!part[0])return current;
        if(path_name_eq(part,".")){}
        else if(path_name_eq(part,"..")){u32 parent=0;if(vfs_stat(current,NULL,NULL,&parent)&&parent)current=parent;}
        else{u32 child=path_child(current,part);if(!child)return 0;current=child;}
        if(!path[pos])return current;
    }
}
int vfs_unlink(u32 parent,const char *name){
    u32 inode=path_child(parent,name); if(!inode||inode==vfs_root())return 0;
    u32 type=0;if(!vfs_stat(inode,&type,NULL,NULL))return 0;
    if(type==VFS_DIR){char child[48];u32 ci,ct;if(vfs_list(inode,0,child,sizeof(child),&ci,&ct))return 0;}
    for(u32 i=0;i<MAX_FILES;++i)if(nodes[i].used&&nodes[i].inode==inode){nodes[i].used=0;return 1;}
    return 0;
}

int vfs_chmod(u32 inode, u32 mode) {
    for (u32 i=0;i<MAX_FILES;++i)
        if (nodes[i].used && nodes[i].inode==inode) { nodes[i].mode=mode&0777u; return 1; }
    return 0;
}
int vfs_chown(u32 inode, u32 uid, u32 gid) {
    for (u32 i=0;i<MAX_FILES;++i)
        if (nodes[i].used && nodes[i].inode==inode) { nodes[i].uid=uid; nodes[i].gid=gid; return 1; }
    return 0;
}
int vfs_get_permissions(u32 inode, u32 *mode, u32 *uid, u32 *gid) {
    for (u32 i=0;i<MAX_FILES;++i) if (nodes[i].used && nodes[i].inode==inode) {
        if(mode)*mode=nodes[i].mode; if(uid)*uid=nodes[i].uid; if(gid)*gid=nodes[i].gid; return 1;
    }
    return 0;
}
static void remove_children(u32 inode) {
    for (u32 i=0;i<MAX_FILES;++i) if(nodes[i].used && nodes[i].parent==inode) {
        u32 child=nodes[i].inode;
        if(nodes[i].type==VFS_DIR) remove_children(child);
        nodes[i].used=0;
    }
}
int vfs_remove_tree(u32 inode, int preserve_root) {
    if (!inode || !vfs_stat(inode,NULL,NULL,NULL)) return 0;
    if (inode==vfs_root()) {
        remove_children(inode);
        return preserve_root ? 1 : 1;
    }
    remove_children(inode);
    for(u32 i=0;i<MAX_FILES;++i)
        if(nodes[i].used && nodes[i].inode==inode){nodes[i].used=0;return 1;}
    return 0;
}
int vfs_no_preserve_root(void) {
    return vfs_remove_tree(vfs_root(), 1);
}
