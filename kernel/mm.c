#include "../include/nova/types.h"
#include "../include/nova/mm.h"

#define MAX_FRAMES 32768u
#define MAX_REGIONS 32u
#define MAX_RESERVED 128u
#define BLOCK_MIN 16u
#define BLOCK_CLASSES 12u
#define HEAP_SIZE (1024u * 1024u)
#define ALLOC_MAGIC 0x4E4F5641u

typedef struct { u32 base, length, type; } MemoryRegion;
typedef struct { u32 start, end; } ReservedRange;
typedef struct FreeBlock { u32 magic, size; struct FreeBlock *next; } FreeBlock;
typedef struct { u32 magic, size, class_index; } AllocHeader;

static u32 frame_bits[MAX_FRAMES / 32u];
static u32 reserved_bits[MAX_FRAMES / 32u];
static MemoryRegion memory_map[MAX_REGIONS];
static ReservedRange reserved[MAX_RESERVED];
static FreeBlock *free_lists[BLOCK_CLASSES];
static u8 kernel_heap[HEAP_SIZE];

static u32 memory_regions, reserved_count, total_frames, free_frames;
static u32 heap_cursor, heap_used_bytes, heap_free_bytes, heap_allocations;

static void mm_zero(void *ptr, usize size) {
    u8 *p = (u8 *)ptr;
    while (size--) *p++ = 0;
}

static u32 bit_word(u32 frame) { return frame >> 5; }
static u32 bit_mask(u32 frame) { return 1u << (frame & 31u); }

static void bitmap_set(u32 *bitmap, u32 frame) {
    bitmap[bit_word(frame)] |= bit_mask(frame);
}

static void bitmap_clear(u32 *bitmap, u32 frame) {
    bitmap[bit_word(frame)] &= ~bit_mask(frame);
}

static int bitmap_test(const u32 *bitmap, u32 frame) {
    return (bitmap[bit_word(frame)] & bit_mask(frame)) != 0;
}

static int region_contains_page(const MemoryRegion *region, u32 address) {
    if (!region || region->type != 1 || !region->length || address < region->base)
        return 0;
    return address < region->base + region->length;
}

static int frame_usable(u32 frame) {
    u32 address = frame * PAGE_SIZE;
    if (frame == 0 || bitmap_test(reserved_bits, frame))
        return 0;

    for (u32 i = 0; i < memory_regions; ++i)
        if (region_contains_page(&memory_map[i], address))
            return 1;
    return 0;
}

static u32 class_for(u32 size) {
    u32 c = 0, s = BLOCK_MIN;
    while (c < BLOCK_CLASSES - 1u && s < size) {
        s <<= 1;
        ++c;
    }
    return c;
}

static u32 class_size(u32 c) {
    return BLOCK_MIN << c;
}

void mm_add_region(u32 base, u32 length, u32 type) {
    if (!length || length > 0xFFFFFFFFu - base || memory_regions >= MAX_REGIONS)
        return;
    memory_map[memory_regions++] = (MemoryRegion){base, length, type};
}

void mm_reserve(u32 start, u32 end) {
    if (end <= start)
        return;

    if (reserved_count < MAX_RESERVED)
        reserved[reserved_count++] = (ReservedRange){start, end};

    u32 first = start & PAGE_MASK;
    u32 last = end;
    if (last > 0xFFFFFFFFu - (PAGE_SIZE - 1u))
        last = 0xFFFFFFFFu;
    else
        last = (last + PAGE_SIZE - 1u) & PAGE_MASK;

    for (u32 address = first; address < last; address += PAGE_SIZE) {
        u32 frame = address / PAGE_SIZE;
        if (frame < total_frames)
            bitmap_set(reserved_bits, frame);
        if (address > 0xFFFFFFFFu - PAGE_SIZE)
            break;
    }
}

u32 mm_alloc_frame(void) {
    for (u32 f = 1; f < total_frames; ++f) {
        if (!frame_usable(f) || bitmap_test(frame_bits, f))
            continue;

        bitmap_set(frame_bits, f);
        if (free_frames)
            --free_frames;
        return f * PAGE_SIZE;
    }
    return 0;
}

int mm_alloc_frames(u32 count, u32 *base) {
    if (!count || !base || count > total_frames)
        return 0;

    u32 run = 0, first = 0;
    for (u32 f = 1; f < total_frames; ++f) {
        if (frame_usable(f) && !bitmap_test(frame_bits, f)) {
            if (!run)
                first = f;
            if (++run == count) {
                for (u32 x = first; x < first + count; ++x)
                    bitmap_set(frame_bits, x);
                if (free_frames >= count)
                    free_frames -= count;
                else
                    free_frames = 0;
                *base = first * PAGE_SIZE;
                return 1;
            }
        } else {
            run = 0;
        }
    }
    return 0;
}

u32 mm_alloc_zeroed_frame(void) {
    u32 address = mm_alloc_frame();
    if (!address)
        return 0;

    /*
     * Physical memory above the first identity-mapped 4 MiB cannot safely
     * be touched as a C pointer yet. Keep allocation separate from clearing;
     * paging_alloc_page() clears a frame through its virtual mapping.
     */
    return address;
}

void mm_free_frame(u32 address) {
    u32 f = address / PAGE_SIZE;
    if (!address || (address & (PAGE_SIZE - 1u)) ||
        f >= total_frames || bitmap_test(reserved_bits, f))
        return;

    if (bitmap_test(frame_bits, f)) {
        bitmap_clear(frame_bits, f);
        ++free_frames;
    }
}

void mm_free_frames(u32 base, u32 count) {
    if (!base || !count || (base & (PAGE_SIZE - 1u)))
        return;

    for (u32 i = 0; i < count; ++i) {
        if (base > 0xFFFFFFFFu - i * PAGE_SIZE)
            break;
        mm_free_frame(base + i * PAGE_SIZE);
    }
}

void *kmalloc(usize size) {
    if (!size || size > HEAP_SIZE - sizeof(AllocHeader))
        return NULL;

    u32 required = (u32)size + sizeof(AllocHeader);
    u32 c = class_for(required);
    u32 slot = class_size(c);

    if (free_lists[c]) {
        FreeBlock *block = free_lists[c];
        free_lists[c] = block->next;

        AllocHeader *h = (AllocHeader *)block;
        h->magic = ALLOC_MAGIC;
        h->size = size;
        h->class_index = c;

        heap_free_bytes -= slot;
        heap_used_bytes += slot;
        ++heap_allocations;
        return (u8 *)h + sizeof(AllocHeader);
    }

    /*
     * Allocate whole size-class slots from the bump area. The old allocator
     * advanced by a smaller alignment than the class size, which made the
     * accounting disagree with the actual free-list slot size.
     */
    if (heap_cursor > HEAP_SIZE || slot > HEAP_SIZE - heap_cursor)
        return NULL;

    AllocHeader *h = (AllocHeader *)(kernel_heap + heap_cursor);
    h->magic = ALLOC_MAGIC;
    h->size = size;
    h->class_index = c;

    heap_cursor += slot;
    heap_used_bytes += slot;
    ++heap_allocations;
    return (u8 *)h + sizeof(AllocHeader);
}

void *kcalloc(usize count, usize size) {
    if (!count || !size || count > 0xFFFFFFFFu / size)
        return NULL;

    usize total = count * size;
    u8 *p = (u8 *)kmalloc(total);
    if (!p)
        return NULL;

    for (usize i = 0; i < total; ++i)
        p[i] = 0;
    return p;
}

void kfree(void *ptr) {
    if (!ptr)
        return;

    u8 *raw = (u8 *)ptr;
    if (raw < kernel_heap + sizeof(AllocHeader) ||
        raw >= kernel_heap + HEAP_SIZE)
        return;

    AllocHeader *h = (AllocHeader *)(raw - sizeof(AllocHeader));
    if (h->magic != ALLOC_MAGIC || h->class_index >= BLOCK_CLASSES)
        return;

    u32 slot = class_size(h->class_index);
    FreeBlock *block = (FreeBlock *)h;
    block->magic = ALLOC_MAGIC;
    block->size = h->size;
    block->next = free_lists[h->class_index];
    free_lists[h->class_index] = block;

    h->magic = 0;
    if (heap_used_bytes >= slot)
        heap_used_bytes -= slot;
    heap_free_bytes += slot;
    if (heap_allocations)
        --heap_allocations;
}

int mm_region(u32 index, NovaMemoryRegion *out) {
    if (!out || index >= memory_regions)
        return 0;

    out->base = memory_map[index].base;
    out->length = memory_map[index].length;
    out->type = memory_map[index].type;
    return 1;
}

int mm_is_reserved(u32 address) {
    u32 frame;
    if (address & (PAGE_SIZE - 1u))
        return 1;

    frame = address / PAGE_SIZE;
    if (frame >= total_frames)
        return 1;
    return bitmap_test(reserved_bits, frame);
}

u32 mm_used_count(void) {
    return total_frames - free_frames;
}

u32 mm_region_count(void) {
    return memory_regions;
}

u32 mm_heap_capacity(void) {
    return HEAP_SIZE;
}

u32 mm_heap_allocations(void) {
    return heap_allocations;
}

int mm_validate(void) {
    u32 calculated_free = 0;

    for (u32 f = 1; f < total_frames; ++f)
        if (frame_usable(f) && !bitmap_test(frame_bits, f))
            ++calculated_free;

    return calculated_free == free_frames;
}

void mm_init(void) {
    mm_zero(frame_bits, sizeof(frame_bits));
    mm_zero(reserved_bits, sizeof(reserved_bits));
    mm_zero(memory_map, sizeof(memory_map));
    mm_zero(reserved, sizeof(reserved));
    mm_zero(free_lists, sizeof(free_lists));
    mm_zero(kernel_heap, sizeof(kernel_heap));

    memory_regions = 0;
    reserved_count = 0;
    total_frames = MAX_FRAMES;
    free_frames = 0;
    heap_cursor = 0;
    heap_used_bytes = 0;
    heap_free_bytes = 0;
    heap_allocations = 0;

    /* Temporary map until the native bootloader supplies E820/UEFI data. */
    mm_add_region(0x00000000u, 0x0009FC00u, 1);
    mm_add_region(0x00100000u, 0x03F00000u, 1);

    /*
     * Keep the low 1 MiB and VGA/ROM area unavailable to the frame allocator.
     * The kernel image itself begins at 0x10000 inside this reserved range.
     */
    mm_reserve(0x00000000u, 0x00100000u);
    mm_reserve(0x000A0000u, 0x00100000u);

    for (u32 f = 0; f < total_frames; ++f)
        if (frame_usable(f) && !bitmap_test(frame_bits, f))
            ++free_frames;
}

u32 mm_free_count(void) { return free_frames; }
u32 mm_total_count(void) { return total_frames; }
u32 mm_reserved_count(void) { return reserved_count; }
u32 mm_heap_used(void) { return heap_used_bytes; }
u32 mm_heap_free(void) { return HEAP_SIZE - heap_used_bytes; }
