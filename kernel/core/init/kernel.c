#include "arch/x86/tss.h"
#include "arch/x86/gdt.h"
#include "core/kernel.h"
#include "core/debug.h"
#include "core/debug_funcs.h"
#include "tests/test_runner.h"
#include "tests/proc_tests.h"
#include "mm/pmm.h"
#include "mm/paging.h"
#include "proc/proc.h"
#include "proc/context_switch.h"
#include "time/timer.h"

extern void switch_to_high_stack(uint32_t new_esp, void (*entry_func)());

__attribute__((noreturn))
void high_stack_entry() {
    printk("Switched to high virtual stack!\n");
    
    uint32_t cur_esp;
    __asm__ __volatile__ ("mov %%esp, %0" : "=r"(cur_esp));
    debug_verbose("ESP after stack switch: 0x%08x\n", cur_esp);

    // Test demand-paged heap access
    debug_print("Triggering demand-paged heap access...\n");
    volatile int *heap_ptr = (int *)(KERNEL_HEAP_START + 0x1234);
    *heap_ptr = 42;
    debug_print("Heap page mapped and write succeeded!\n");

    // Stack overflow testing (debug only)
    if (DEBUG_STACK) {
        debug_print("Testing stack overflow detection...\n");
        printk("Current page directory CR3: 0x%08x\n", tss_df.cr3);
    }

    // VGA memory test
    volatile uint16_t* vga_test = (volatile uint16_t*)0xB8000;
    *vga_test = 0x4F41; // 'A' with white on red
    debug_verbose("VGA memory test: wrote to 0xB8000\n");

    // Initialize Process Management
    proc_init();
    printk("Initialized Process Management...\n");

    // Create test processes only if tests are enabled
    #ifdef KERNEL_TESTS
        // Run kernel tests first
        run_kernel_tests();
    #else
        printk("Production build - testing disabled\n");
    #endif
    
    // Main kernel loop
    printk("\nKernel initialization complete. Entering main loop.\n");
    while (1) {
        __asm__ __volatile__("cli; hlt");
    }
}

void kernel_main() {
    // Console and Logger Initialization
    printk_init();
    printk("%s v%s - Hello Devjit!\n", KERNEL_NAME, KERNEL_VERSION);
    printk("Kernel-V is running! Welcome to your custom kernel, Devjit!\n");

    // Debug breadcrumbs (debug build only)
    check_double_fault_breadcrumbs();

    // Initialize core systems
    idt_init();
    init_tss();
    gdt_init();

    // Debug system state
    DEBUG_IDT_GDT_SETUP();

    // Debug double fault stack info
    if (DEBUG_ENABLED) {
        printk("Double fault stack: 0x%08x to 0x%08x\n", 
            (uint32_t)double_fault_stack, 
            (uint32_t)(double_fault_stack + DOUBLE_FAULT_STACK_SIZE));
        printk("Double fault stack size: %d bytes\n", DOUBLE_FAULT_STACK_SIZE);

        // Write test pattern to double fault stack
        for (int i = 0; i < 16; i++) {
            double_fault_stack[i] = 0xAA + i;
        }
        debug_verbose("Double fault stack test pattern written\n");
    }

    // Initialize Timer
    pit_init(PIT_DEFAULT_HZ);

    // Memory Management Setup
    printk("\n==================================================\n");
    printk("Parsing BIOS Memory Map (E820)...\n");
    parse_and_print_e820_map();

    // Physical Memory Manager
    pmm_init();
    pmm_reserve_memory_region(RESERVED_TYPE_INIT);
    pmm_reserve_memory_region(RESERVED_TYPE_KERNEL);
    pmm_reserve_memory_region(RESERVED_TYPE_BITMAP);

    void* frame1 = pmm_alloc_frame();
    if (frame1) {
        debug_verbose("Allocated frame at address: %p\n", frame1);
    } else {
        debug_verbose("Failed to allocate frame\n");
    }

    void* frame2 = pmm_alloc_frame();
    if (frame2) {
        debug_verbose("Allocated another frame at address: %p\n", frame2);
    } else {
        debug_verbose("Failed to allocate another frame\n");
    }

    // Virtual Memory & Paging
    printk("\n==================================================\n");
    printk("Initializing Paging...\n");
    paging_init();
    
    if (DEBUG_PAGING) {
        debug_page_tables();
    }
    
    pmm_reserve_memory_region(RESERVED_TYPE_PAGE_TABLE);
    update_tss_cr3();

    // Debug double fault handler setup
    DEBUG_DOUBLE_FAULT_SETUP();

    // Map stack region
    uint32_t stack_size = KERNEL_STACK_TOP_VIRT - KERNEL_STACK_BOTTOM_VIRT;
    debug_verbose("Mapping stack pages...\n");
    
    for (uint32_t off = PAGE_SIZE; off < stack_size; off += PAGE_SIZE) {
        uint32_t virt = KERNEL_STACK_BOTTOM_VIRT + off;
        void* phys_frame = pmm_alloc_frame();
        if (!phys_frame) {
            printk("Failed to allocate stack frame for virt=0x%08x\n", virt);
            panik("Stack frame allocation failed");
        }
        debug_verbose("Mapping stack page: virt=0x%08x phys=0x%08x\n", virt, (uint32_t)phys_frame);
        paging_map_page(virt, (uint32_t)phys_frame, PAGE_PRESENT | PAGE_WRITE);
        pmm_set_frame_bitmap((uint32_t)phys_frame, (uint32_t)phys_frame + PAGE_SIZE);
    }

    printk("Paging initialized successfully!\n");

    // Switch to high virtual stack
    debug_verbose("About to switch to high virtual stack...\n");
    uint32_t new_stack_ptr = KERNEL_STACK_TOP_VIRT - 16;
    debug_verbose("New stack pointer: 0x%08x\n", new_stack_ptr);
    switch_to_high_stack(new_stack_ptr, high_stack_entry);
}