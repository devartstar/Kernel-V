#include "arch/x86/page_fault.h"
#include "core/debug.h"
#include "core/panik.h"
#include "lib/printk.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include <stdint.h>

void pagefault_interrupt_handler(uint32_t idt_index, regs_t *regs) {
    // Todo: Remove the idt_index param and use (regs->int_no) instead
    (void)idt_index;

    //  Disable interrupts to prevent nested faults
    __asm__ __volatile__("cli");

    //  cr2 holds the fault linear address for the most recent page fault
    uint32_t fault_address;
    uint32_t esp, ebp;
    __asm__ __volatile__("mov %%esp, %0" : "=r"(esp));
    __asm__ __volatile__("mov %%ebp, %0" : "=r"(ebp));
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(fault_address));

    //  Check to see if accessing guard page
    //  Fault accessing Guard Page: Stack Overflow
    if (fault_address >= KERNEL_STACK_BOTTOM_VIRT &&
        fault_address < KERNEL_STACK_BOTTOM_VIRT + PAGE_SIZE) {
        //  DON'T use panik() - it will use the corrupted stack!
        //  Instead, write directly to VGA and halt
        volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
        char *msg = "STACK OVERFLOW DETECTED!";

        //  Clear screen first
        for (int i = 0; i < 80 * 25; i++) {
            vga[i] = 0x4F20; //  White space on red
        }

        //  Write message
        for (int i = 0; msg[i] != '\0'; i++) {
            vga[i] = 0x4F00 | msg[i]; //  White text on red
        }

        //  Force double fault for proper handling
        volatile int *bad_ptr = (int *)0x00000000;
        *bad_ptr = 123;
    }

    debug_module(PAGING,
                 "[PAGE FAULT] at address: 0x%x, error code: 0x%x [eip=0x%x, "
                 "esp=0x%x, ebp=0x%x]\n",
                 PRINT_UINT32(fault_address), PRINT_UINT32(regs->error_code),
                 PRINT_UINT32(regs->eip), PRINT_UINT32(esp), PRINT_UINT32(ebp));
    //  Check if the fault_address is in the kernel heap range
    if (fault_address >= KERNEL_HEAP_START && fault_address < KERNEL_HEAP_END) {
        debug_module(PAGING, "[PAGE FAULT] Address within kernel heap region: "
                             "allocating and mapping new page.\n");

        void *new_frame = pmm_alloc_frame();
        if (!new_frame) {
            panik("Out of memory: Unable to allocate frame for page fault at "
                  "address "
                  "0x%x",
                  PRINT_UINT32(fault_address));
        }
        paging_map_page(fault_address, (uint32_t)new_frame,
                        PAGE_PRESENT | PAGE_WRITE);
        return;
    }

    //  Check if the fault_address is in the kernel stack range and faulting
    //  within a small gap below ESP Typical stack growth threshold: only map if
    //  faulting within N bytes below ESP
    const uint32_t STACK_GROWTH_GAP = 32; //  or 128, or 0
    if (fault_address >= KERNEL_STACK_BOTTOM_VIRT + PAGE_SIZE &&
        fault_address < KERNEL_STACK_TOP_VIRT) {
        if (fault_address >= regs->esp - STACK_GROWTH_GAP &&
            fault_address < regs->esp) {
            debug_module(
                PAGING,
                "[PAGE FAULT] Stack growth: mapping new stack page at 0x%x "
                "(esp=0x%x)\n",
                PRINT_UINT32(fault_address), PRINT_UINT32(regs->esp));
            void *new_frame = pmm_alloc_frame();
            if (!new_frame)
                panik("Out of memory in stack PF recovery");
            paging_map_page(fault_address, (uint32_t)new_frame,
                            PAGE_PRESENT | PAGE_WRITE);
            return;
        }
    }

    //  Todo: similarly for kernel stack growth
    //  Todo: user space page fault handling - signal the fault back to the
    //  process

    /*
    Error Code for Page Fault:
    Bit 0 (P)   : (0 = Page not present)    (1 = Protection Violation)
    Bit 1 (W/R) : (0 = Fault on Read)       (1 = Fault on Write)
    Bit 2 (U/s) : (0 = Fault in Kernel Mode)(1 = Fault in User Mode)
    Bit 3 (R)   : (0 = Reserved bit not set)(1 = Reserved bit set)
    Bit 4 (I)   : (0 = Normal fault)        (1 = Instruction fetch fault)
    */

    if (!(regs->error_code & 0x1)) {
        debug_module(PAGING, "[PAGE FAULT] Page not present.\n");
    }
    if (regs->error_code & 0x2) {
        debug_module(PAGING, "[PAGE FAULT] Write access.\n");
    }
    if (regs->error_code & 0x4) {
        debug_module(PAGING, "[PAGE FAULT] User mode access.\n");
    }
    if (regs->error_code & 0x8) {
        debug_module(PAGING, "[PAGE FAULT] Reserved bit set.\n");
    }
    if (regs->error_code & 0x10) {
        debug_module(PAGING, "[PAGE FAULT] Instruction fetch.\n");
    }

    //  halt or implement fault recovery
    while (1) {
        __asm__ __volatile__("hlt");
    }
}
