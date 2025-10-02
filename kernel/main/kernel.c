#include "kernel.h"
#include "pmm.h"
#include "arch/x86/tss.h"
#include "arch/x86/gdt.h"
#include "paging.h"
#include "proc.h"
#include "context_switch.h"

extern pcb_t *current_proc;

// =================================================================
// DEBUG Start
// =================================================================
void check_double_fault_breadcrumbs() {
    uint32_t *magic1 = (uint32_t*)0x15000;
    uint32_t *magic2 = (uint32_t*)0x15004;
    uint32_t *magic3 = (uint32_t*)0x15008;
    
    printk("Checking double fault breadcrumbs:\n");
    printk("  Magic1 (0x15000): 0x%08x %s\n", *magic1, (*magic1 == 0xDEADBEEF) ? "(FOUND)" : "(not found)");
    printk("  Magic2 (0x15004): 0x%08x %s\n", *magic2, (*magic2 == 0xCAFEBABE) ? "(FOUND)" : "(not found)");
    printk("  Magic3 (0x15008): 0x%08x %s\n", *magic3, (*magic3 == 0x12345678) ? "(FOUND)" : "(not found)");
}

void debug_idt_entry(int num) {
    extern idt_entry_t idt[IDT_ENTRIES];
    
    printk("IDT Entry %d:\n", num);
    printk("  base_low:  0x%04x\n", idt[num].base_low);
    printk("  base_high: 0x%04x\n", idt[num].base_high);
    printk("  sel:       0x%04x\n", idt[num].sel);
    printk("  always0:   0x%02x\n", idt[num].always0);
    printk("  flags:     0x%02x\n", idt[num].flags);
    
    // Decode flags
    if (idt[num].flags & 0x80) printk("    Present: YES\n");
    else printk("    Present: NO\n");
    
    uint8_t gate_type = idt[num].flags & 0x0F;
    if (gate_type == 0x5) printk("    Type: Task Gate\n");
    else if (gate_type == 0xE) printk("    Type: Interrupt Gate\n");
    else printk("    Type: 0x%x (unknown)\n", gate_type);
}

void debug_gdt_entry(int num) {
    extern struct gdt_entry gdt[4];
    
    printk("GDT Entry %d:\n", num);
    printk("  base: 0x%08x\n", 
           (gdt[num].base_high << 24) | (gdt[num].base_middle << 16) | gdt[num].base_low);
    printk("  limit: 0x%05x\n", 
           ((gdt[num].granularity & 0x0F) << 16) | gdt[num].limit_low);
    printk("  access: 0x%02x\n", gdt[num].access);
    printk("  granularity: 0x%02x\n", gdt[num].granularity);
    
    // Decode access byte
    if (gdt[num].access & 0x80) printk("    Present: YES\n");
    else printk("    Present: NO\n");
    
    uint8_t desc_type = (gdt[num].access >> 3) & 0x1;
    if (desc_type == 0) printk("    Type: System\n");
    else printk("    Type: Code/Data\n");
}

void debug_tss_contents() {
    printk("TSS Contents:\n");
    printk("  esp: 0x%08x\n", tss_df.esp);
    printk("  ss:  0x%04x\n", tss_df.ss);
    printk("  cs:  0x%04x\n", tss_df.cs);
    printk("  eip: 0x%08x\n", tss_df.eip);
    printk("  cr3: 0x%08x\n", tss_df.cr3);
    printk("  ds:  0x%04x\n", tss_df.ds);
}

void my_test_proc (void *arg) 
{
    panik("Test process is running!\n");
    while (1) { __asm__ __volatile__("hlt"); }
}
// =================================================================
// DEBUG End
// =================================================================

extern void switch_to_high_stack(uint32_t new_esp, void (*entry_func)());

void test_stack_overflow(int depth) {
    volatile uint8_t dummy[512]; // Forces at least 512 bytes stack usage per call
    dummy[0] = (uint8_t)depth;   // Actually touch the memory
    
    // Get current stack pointer
    uint32_t current_esp;
    asm volatile ("mov %%esp, %0" : "=r"(current_esp));
    
    printk("Stack depth: %d, ESP=0x%08x\n", depth, current_esp);
    
    // Check if we're getting close to the guard page
    if (current_esp <= KERNEL_STACK_BOTTOM_VIRT + PAGE_SIZE + 0x1000) {
        printk("WARNING: Approaching guard page at 0x%08x!\n", KERNEL_STACK_BOTTOM_VIRT);
        printk("Current ESP: 0x%08x, Guard page: 0x%08x\n", current_esp, KERNEL_STACK_BOTTOM_VIRT);
    }
    
    test_stack_overflow(depth + 1); // Recursive call
}

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

    // printk("\nTriggering page fault...\n");
    // volatile int *ptr = (int *)0xDEADBEEF;  // This address is not mapped
    // *ptr = 123;                             // Will cause interrupt 14 (page fault)

    printk("Testing stack overflow...\n");
    printk("Current page directory CR3: 0x%08x\n", tss_df.cr3);

    // Check if VGA memory is accessible
    volatile uint16_t* vga_test = (volatile uint16_t*)0xB8000;
    *vga_test = 0x4F41; // 'A' with white on red
    printk("VGA memory test: wrote to 0xB8000\n");


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
    
    // -------------------------------------------------------------------------
    // Kernel main execution complete - halt the system
    // -------------------------------------------------------------------------
    printk("\nKernel initialization complete. Halting system.\n");
    
    // Infinite halt loop - prevents undefined behavior from function "returning"
    while (1) {
        __asm__ __volatile__("cli; hlt");
    }
}

void kernel_main() {
    // -------------------------------------------------------------------------
    // Console and Logger Initialization
    // -------------------------------------------------------------------------
    printk_init();
    printk("%s v%s - Hello Devjit!\n", KERNEL_NAME, KERNEL_VERSION);
    printk("Kernel-V is running! Welcome to your custom kernel, Devjit!\n");

    check_double_fault_breadcrumbs();

    // -------------------------------------------------------------------------
    // Initializing IDT (Interrupt Descriptor Table)
    // -------------------------------------------------------------------------
    idt_init();
    init_tss();
    gdt_init();
    // Do NOT enable interrupts yet

    // Debug IDT and GDT setup
    printk("\n==================================================\n");
    printk("DEBUG: IDT and GDT Setup\n");
    debug_idt_entry(8);  // Double fault
    debug_gdt_entry(3);  // TSS entry
    debug_tss_contents();
    printk("==================================================\n");

    // Add this after TSS setup
    printk("Double fault stack: 0x%08x to 0x%08x\n", 
        (uint32_t)double_fault_stack, 
        (uint32_t)(double_fault_stack + DOUBLE_FAULT_STACK_SIZE));
    printk("Double fault stack size: %d bytes\n", DOUBLE_FAULT_STACK_SIZE);

    // Write a pattern to double fault stack to verify it's accessible
    for (int i = 0; i < 16; i++) {
        double_fault_stack[i] = 0xAA + i;
    }
    printk("Double fault stack test pattern written\n");

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
    debug_page_tables();
    pmm_reserve_memory_region(RESERVED_TYPE_PAGE_TABLE);

    update_tss_cr3();
    // Debug: Verify double fault handler setup
    printk("\n==================================================\n");
    printk("Double fault TSS configured:\n");
    printk("  TSS address: 0x%08x\n", (uint32_t)&tss_df);
    printk("  Handler EIP: 0x%08x\n", tss_df.eip);
    printk("  Handler ESP: 0x%08x\n", tss_df.esp);
    printk("  Handler CR3: 0x%08x\n", tss_df.cr3);
    printk("\n==================================================\n");
    

    // -------------------------------------------------------------------------
    // Process Control 
    // -------------------------------------------------------------------------
    proc_init ();
    pcb_t *test_proc = proc_create (my_test_proc, NULL, "test_proc");
    if (test_proc)
    {
        printk ("Test process created with PID %d, stack at %p\n", test_proc->pid, test_proc->stack_base);
        current_proc = NULL;
        switch_to (current_proc, test_proc);
    }
    else
    {
        printk ("Failed to create test process!\n");
    }


    // Map stack region: high virtual address -> physical address
    uint32_t stack_size = KERNEL_STACK_TOP_VIRT - KERNEL_STACK_BOTTOM_VIRT;
    printk("Mapping stack pages...\n");
    // Guard page at KERNEL_STACK_BOTTOM_VIRT (first page) is left unmapped
    // If the stack overflows, it will hit this unmapped page and cause a page fault
    for (uint32_t off = PAGE_SIZE; off < stack_size; off += PAGE_SIZE) {
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
    uint32_t new_stack_ptr = KERNEL_STACK_TOP_VIRT - 16; // Leave some space from the very top
    printk("New stack pointer: 0x%08x\n", new_stack_ptr);
    switch_to_high_stack(new_stack_ptr, high_stack_entry);
    printk("Switched to high virtual stack!\n");

    // Execution continues from high_stack_entry()
}

void debug_print_esp_args(uint32_t arg1, uint32_t arg2) {
    printk("switch_to_high_stack: [esp+4]=0x%08x [esp+8]=0x%08x\n", arg1, arg2);
}
