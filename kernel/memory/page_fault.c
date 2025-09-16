#include "page_fault.h"
#include "paging.h"
#include "pmm.h"
#include "printk.h"
#include "panik.h"
#include <stdint.h>

void page_fault_handler (page_fault_stack_t* frame)
{
    // cr2 holds the fault linear address for the most recent page fault
    uint32_t fault_address;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(fault_address));

    printk("[PAGE FAULT] at address: 0x%x, error code: 0x%x [eip=0x%x]\n", 
            fault_address, 
            frame->error_code, 
            frame->eip);

    // Check if the fault_address is in the kernel heap range
    if (fault_address >= KERNEL_HEAP_START && fault_address < KERNEL_HEAP_END) 
    {
        printk("[PAGE FAULT] Address within kernel heap region: allocating and mapping new page.\n");

        void* new_frame = pmm_alloc_frame();
        if(!new_frame)
        {
            panik("Out of memory: Unable to allocate frame for page fault at address 0x%x", fault_address);
        }
        paging_map_page(fault_address, (uint32_t)new_frame, PAGE_PRESENT | PAGE_WRITE);
        return;
    }

    // Todo: similarly for kernel stack growth
    // Todo: user space page fault handling - signal the fault back to the process

    /*
    Error Code for Page Fault:
    Bit 0 (P)   : (0 = Page not present)    (1 = Protection Violation)
    Bit 1 (W/R) : (0 = Fault on Read)       (1 = Fault on Write) 
    Bit 2 (U/s) : (0 = Fault in Kernel Mode)(1 = Fault in User Mode)
    Bit 3 (R)   : (0 = Reserved bit not set)(1 = Reserved bit set)
    */

    if (!(frame->error_code & 0x1))
    {
        printk("[PAGE FAULT] Page not present.\n");
    }
    if (frame->error_code & 0x2)
    {
        printk("[PAGE FAULT] Write access.\n");
    }
    if (frame->error_code & 0x4)
    {
        printk("[PAGE FAULT] User mode access.\n");
    }
    if (frame->error_code & 0x8)
    {
        printk("[PAGE FAULT] Reserved bit set.\n");
    }
    if (frame->error_code & 0x10)
    {
        printk("[PAGE FAULT] Instruction fetch.\n");
    }

    // halt or implement fault recovery
    while (1) {
        __asm__ __volatile__("hlt");
    }
}