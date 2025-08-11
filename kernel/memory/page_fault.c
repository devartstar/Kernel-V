#include <stdint.h>
#include "printk.h"

void page_fault_handler (uint32_t error_code)
{
    // cr2 holds the fault linear address for the most recent page fault
    uint32_t fault_address;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(fault_address));

    printk("[PAGE FAULT] at address: 0x%x, error code: 0x%x]\n", fault_address, error_code);

    /*
    Error Code:
    Bit 0 (P)   : (0 = Page not present)    (1 = Protection Violation)
    Bit 1 (W/R) : (0 = Fault on Read)       (1 = Fault on Write) 
    Bit 2 (U/s) : (0 = Fault in Kernel Mode)(1 = Fault in User Mode)
    Bit 3 (R)   : (0 = Reserved bit not set)(1 = Reserved bit set)
    */

    if (!(error_code & 0x1))
    {
        printk("[PAGE FAULT] Page not present.\n");
    }
    if (error_code & 0x2)
    {
        printk("[PAGE FAULT] Write access.\n");
    }
    if (error_code & 0x4)
    {
        printk("[PAGE FAULT] User mode access.\n");
    }
    if (error_code & 0x8)
    {
        printk("[PAGE FAULT] Reserved bit set.\n");
    }
    if (error_code & 0x10)
    {
        printk("[PAGE FAULT] Instruction fetch.\n");
    }

    // halt or implement fault recovery
    while (1) {
        __asm__ __volatile__("hlt");
    }
}