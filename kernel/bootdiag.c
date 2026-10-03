#include "../include/nova/types.h"
#include "../include/nova/cpu.h"
#include "../include/nova/mm.h"
#include "../include/nova/paging.h"
#include "../include/nova/process.h"
#include "../include/nova/interrupts.h"
#include "../include/nova/pagefile.h"

void kernel_diag_print(void (*print)(const char *), void (*print_u32)(u32)) {
    NovaCpuInfo cpu;
    cpu_get_info(&cpu);
    print("CPU vendor: "); print(cpu.vendor[0] ? cpu.vendor : "unknown"); print("\\n");
    print("CPU CPUID: "); print(cpu.has_cpuid ? "yes" : "no"); print("\\n");
    print("CPU APIC: "); print(cpu.has_apic ? "yes" : "no"); print("\\n");
    print("CPU MMX/SSE: "); print(cpu.has_mmx ? "MMX " : ""); print(cpu.has_sse ? "SSE" : "none"); print("\\n");
    print("Memory: "); print_u32(mm_free_count()); print(" free / "); print_u32(mm_total_count()); print(" frames\\n");
    print("Heap: "); print_u32(mm_heap_used()); print(" used / "); print_u32(mm_heap_capacity()); print(" bytes\\n");
    print("Heap allocations: "); print_u32(mm_heap_allocations()); print("\\n");
    print("Page tables: "); print_u32(paging_table_count()); print("\\n");
    print("Processes: "); print_u32(process_count()); print(" (ready "); print_u32(process_ready_count()); print(")\\n");
    print("Ticks: "); print_u32(interrupt_ticks()); print("\\n");
    print("Pagefile: "); print(pagefile_available() ? "available" : "unavailable");
    if (pagefile_available()) { print(" (free "); print_u32(pagefile_free_pages()); print(" pages)"); }
    print("\\n");
}
