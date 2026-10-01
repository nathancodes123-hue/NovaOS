#include "../include/nova/types.h"
extern u32 mm_alloc_frame(void);
#define PD_ENTRIES 1024
#define PT_ENTRIES 1024
#define PRESENT 1
#define WRITE 2
#define USER 4
static u32 page_directory[PD_ENTRIES] __attribute__((aligned(4096)));
static u32 first_table[PT_ENTRIES] __attribute__((aligned(4096)));
static u32 kernel_pd_phys;
static void zero(void*p,usize n){u8*b=p;while(n--)*b++=0;}
static void invlpg(u32 a){__asm__ volatile("invlpg (%0)"::"r"(a):"memory");}
static void load_cr3(u32 p){__asm__ volatile("mov %0,%%cr3"::"r"(p));}
static u32 read_cr0(void){u32 v;__asm__ volatile("mov %%cr0,%0":"=r"(v));return v;}
static void write_cr0(u32 v){__asm__ volatile("mov %0,%%cr0"::"r"(v));}
void paging_identity_map_first_4m(void){
 zero(page_directory,sizeof(page_directory));zero(first_table,sizeof(first_table));
 for(u32 i=0;i<PT_ENTRIES;i++)first_table[i]=(i*PAGE_SIZE)|PRESENT|WRITE;
 page_directory[0]=((u32)first_table)|PRESENT|WRITE;
 kernel_pd_phys=(u32)page_directory;
 load_cr3(kernel_pd_phys);
 write_cr0(read_cr0()|0x80000000u);
}
int paging_map(u32 virt,u32 phys,u32 flags){
 u32 pdi=virt>>22,pti=(virt>>12)&1023;
 u32*table;
 if(!(page_directory[pdi]&PRESENT)){
  u32 frame=mm_alloc_frame();if(!frame)return 0;
  table=(u32*)frame;zero(table,PAGE_SIZE);page_directory[pdi]=frame|PRESENT|WRITE|USER;
 }else table=(u32*)(page_directory[pdi]&PAGE_MASK);
 table[pti]=(phys&PAGE_MASK)|flags|PRESENT;invlpg(virt);return 1;
}
int paging_unmap(u32 virt){
 u32 pdi=virt>>22,pti=(virt>>12)&1023;
 if(!(page_directory[pdi]&PRESENT))return 0;
 u32*table=(u32*)(page_directory[pdi]&PAGE_MASK);
 if(!(table[pti]&PRESENT))return 0;
 table[pti]=0;invlpg(virt);return 1;
}
u32 paging_directory(void){return kernel_pd_phys;}
