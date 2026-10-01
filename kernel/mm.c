#include "../include/nova/types.h"
#define MAX_FRAMES 32768
#define MAX_REGIONS 32
#define BLOCK_MIN 16
#define BLOCK_CLASSES 12

typedef struct {u32 base,length,type;} MemoryRegion;
typedef struct {u32 start,end;u8 used;} Range;
typedef struct FreeBlock {u32 size;struct FreeBlock*next;} FreeBlock;

static u32 frame_bits[MAX_FRAMES/32];
static MemoryRegion memory_map[MAX_REGIONS];
static u32 memory_regions;
static u32 total_frames,free_frames;
static Range reserved[128];
static u32 reserved_count;
static FreeBlock*free_lists[BLOCK_CLASSES];
static u8 kernel_heap[1024*1024];

static void mm_zero(void*p,usize n){u8*b=p;while(n--)*b++=0;}
static u32 bit_word(u32 f){return f>>5;}
static u32 bit_mask(u32 f){return 1u<<(f&31);}
static void frame_set(u32 f){frame_bits[bit_word(f)]|=bit_mask(f);}
static void frame_clear(u32 f){frame_bits[bit_word(f)]&=~bit_mask(f);}
static int frame_test(u32 f){return (frame_bits[bit_word(f)]&bit_mask(f))!=0;}

void mm_reserve(u32 start,u32 end){
 if(reserved_count<128){reserved[reserved_count++]=(Range){start,end,1};}
 for(u32 a=start&PAGE_MASK;a<end;a+=PAGE_SIZE) frame_set(a/PAGE_SIZE);
}
void mm_add_region(u32 base,u32 length,u32 type){
 if(memory_regions<MAX_REGIONS)memory_map[memory_regions++]=(MemoryRegion){base,length,type};
}
static int usable(u32 frame){
 u32 addr=frame*PAGE_SIZE;
 for(u32 i=0;i<memory_regions;i++){
  MemoryRegion*r=&memory_map[i];
  if(r->type==1&&addr>=r->base&&addr+PAGE_SIZE<=r->base+r->length)return 1;
 }
 return 0;
}
u32 mm_alloc_frame(void){
 for(u32 f=1;f<total_frames;f++)if(usable(f)&&!frame_test(f)){frame_set(f);if(free_frames)free_frames--;return f*PAGE_SIZE;}
 return 0;
}
void mm_free_frame(u32 addr){
 u32 f=addr/PAGE_SIZE;
 if(f<total_frames&&frame_test(f)){frame_clear(f);free_frames++;}
}
static u32 class_for(u32 size){u32 c=0,s=BLOCK_MIN;while(c<BLOCK_CLASSES-1&&s<size){s<<=1;c++;}return c;}
void* kmalloc(usize size){
 if(!size)return NULL;
 size+=sizeof(u32);u32 c=class_for(size);
 if(free_lists[c]){FreeBlock*b=free_lists[c];free_lists[c]=b->next;return (u8*)b+sizeof(u32);}
 static usize cursor=0;
 usize align=(size+15)&~15u;
 if(cursor+align>sizeof(kernel_heap))return NULL;
 u32*p=(u32*)(kernel_heap+cursor);*p=size;cursor+=align;return p+1;
}
void kfree(void*p){
 if(!p)return;
 u32*raw=(u32*)p-1;u32 size=*raw;u32 c=class_for(size);
 FreeBlock*b=(FreeBlock*)raw;b->size=size;b->next=free_lists[c];free_lists[c]=b;
}
void mm_init(void){
 mm_zero(frame_bits,sizeof(frame_bits));memory_regions=0;reserved_count=0;
 mm_add_region(0x00000000,0x0009FC00,1);
 mm_add_region(0x00100000,0x03F00000,1);
 total_frames=MAX_FRAMES;free_frames=0;
 mm_reserve(0,0x00100000);
 mm_reserve(0x000A0000,0x00100000);
 for(u32 f=0;f<total_frames;f++)if(usable(f)&&!frame_test(f))free_frames++;
}
u32 mm_free_count(void){return free_frames;}
u32 mm_total_count(void){return total_frames;}
