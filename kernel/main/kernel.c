#include "kernel.h"
#include "pmm.h"
#include "paging.h"

extern void switch_to_high_stack(uint32_t new_esp, void (*entry_func)());

__attribute__((noreturn))
void high_stack_entry() {
    printk("Switched to high virtual stack!\n");
    uint32_t cur_esp;
    asm volatile ("mov %%esp, %0" : "=r"(cur_esp));
    printk("ESP after stack switch: 0x%08x\n", cur_esp);

    // IMP: Enabling Interrupts is causing the kernel to reboot infinitely. WHY ???
    // Now enable interrupts 
    // __asm__ __volatile__("sti");

    
    // -------------------------------------------------------------------------
    // Optional: Trigger a page fault for testing
    // -------------------------------------------------------------------------
    printk("Triggering demand-paged heap access...\n");
    printk("Before accessing heap");
    volatile int *heap_ptr = (int *)(KERNEL_HEAP_START + 0x1234);
    *heap_ptr = 42;
    printk("Heap page mapped and write succeeded!\n");

    printk("\nTriggering page fault...\n");
    volatile int *ptr = (int *)0xDEADBEEF;  // This address is not mapped
    *ptr = 123;                             // Will cause interrupt 14 (page fault)

    // while (1) { __asm__ __volatile__("hlt"); }

    // -------------------------------------------------------------------------
    // Optional Unit Tests
    // -------------------------------------------------------------------------
    #ifdef KERNEL_TESTS
    printk("\n==================================================\n");
    printk("Tests Running...\n");
    run_printk_tests();
    run_printk_scrolling_test();
    run_panik_unit_tests();
    printk("==================================================\n");
    #endif
}

void kernel_main() {
    // -------------------------------------------------------------------------
    // Console and Logger Initialization
    // -------------------------------------------------------------------------
    printk_init();
    printk("%s v%s - Hello Devjit!\n", KERNEL_NAME, KERNEL_VERSION);
    printk("Kernel-V is running! Welcome to your custom kernel, Devjit!\n");

    // -------------------------------------------------------------------------
    // Initializing IDT (Interrupt Descriptor Table)
    // -------------------------------------------------------------------------
    idt_init();
    // Do NOT enable interrupts yet

    // -------------------------------------------------------------------------
    // Display BIOS Memory Map (E820)
    // -------------------------------------------------------------------------
    printk("\n==================================================\n");
    printk("Parsing BIOS Memory Map (E820)...\n");
    parse_and_print_e820_map();

    // -------------------------------------------------------------------------
    // Physical Memory Manager Setup
    // -------------------------------------------------------------------------
    pmm_init();
    pmm_reserve_memory_region(RESERVED_TYPE_INIT);
    pmm_reserve_memory_region(RESERVED_TYPE_KERNEL);
    pmm_reserve_memory_region(RESERVED_TYPE_BITMAP);

    void* frame1 = pmm_alloc_frame();
    printk(frame1 ? "Allocated frame at address: %p\n" : "Failed to allocate frame\n", frame1);

    void* frame2 = pmm_alloc_frame();
    printk(frame2 ? "Allocated another frame at address: %p\n" : "Failed to allocate another frame\n", frame2);

    // -------------------------------------------------------------------------
    // Virtual Memory & Paging Setup
    // -------------------------------------------------------------------------
    printk("\n==================================================\n");
    printk("Initializing Paging...\n");
    paging_init();
    pmm_reserve_memory_region(RESERVED_TYPE_PAGE_TABLE);

    // Map stack region: high virtual address -> physical address
    uint32_t stack_size = KERNEL_STACK_TOP_VIRT - KERNEL_STACK_BOTTOM_VIRT;
    printk("Mapping stack pages...\n");
    for (uint32_t off = 0; off < stack_size; off += 0x1000) {
        uint32_t virt = KERNEL_STACK_BOTTOM_VIRT + off;
        void* phys_frame = pmm_alloc_frame();
        if (!phys_frame) {
            printk("Failed to allocate stack frame for virt=0x%08x\n", virt);
            panik("Stack frame allocation failed");
        }
        printk("Mapping stack page: virt=0x%08x phys=0x%08x\n", virt, (uint32_t)phys_frame);
        paging_map_page(virt, (uint32_t)phys_frame, PAGE_PRESENT | PAGE_WRITE);
        pmm_set_frame_bitmap((uint32_t)phys_frame, (uint32_t)phys_frame + PAGE_SIZE);
    }

    printk("Paging initialized successfully!\n");

    // Switch ESP to high virtual address (inside mapped page, not at page boundary)
    printk("About to switch to high virtual stack...\n");
    // Prepare the top of the new stack:
    uint32_t* stack = (uint32_t*)KERNEL_STACK_TOP_VIRT;
    *(--stack) = (uint32_t)0;                // Fake return address
    switch_to_high_stack(stack, high_stack_entry);
    printk("Switched to high virtual stack!\n");

    // Execution continues from high_stack_entry()
}

void debug_print_esp_args(uint32_t arg1, uint32_t arg2) {
    printk("switch_to_high_stack: [esp+4]=0x%08x [esp+8]=0x%08x\n", arg1, arg2);
}
