#include "paging.h"
#include "printk.h"

static uint32_t* page_directory         = (uint32_t*)PAGE_DIR_START_ADDR;
static uint32_t* first_page_table       = (uint32_t*)PAGE_TABLE_START_ADDR;

void paging_init()
{
    printk("[PAGING] Initializing Paging structures...\n");

    // clear the page directory table
    for (uint32_t entry = 0; entry < PAGE_ENTRIES; entry++)
    {
        // marking each entry as supervisor, Read/Write, not present
        page_directory[entry] = 0x00000002;
    }

    // identity map first 4MB (0x00000000 to 0x003FFFFF)
    for (uint32_t entry = 0; entry < PAGE_ENTRIES; entry++)
    {
        // [0  - 12] bits: flags (present, writable, user)
        // [12 - 31] bits: physical address of the page frame
        first_page_table[entry] = (entry * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }

    // LINK first page table to the first entry of the page directory
    page_directory[0] = (uint32_t)first_page_table | PAGE_PRESENT | PAGE_WRITE;

    printk ("[PAGING] Directory at %p, Table[0] at %p\n", page_directory, first_page_table);

    // Load the page directory address into CR3
    __asm__ __volatile__ (
        "mov %0, %%cr3"
        :
        : "r"(page_directory)
    );

    // Store the cr0 register value int variable
    uint32_t cr0;
    __asm__ __volatile__ (
        "mov %%cr0, %0"
        : "=r"(cr0)  
    );

    // Set the paging bit cr0.pg (bit 31)
    cr0 |= 0x80000000; 

    // Write the value back to cr0
    __asm__ __volatile__ (
        "mov %0, %%cr0"
        :
        : "r"(cr0)
    );

    // From now on all memory access will be virtual
    printk("[PAGING] Paging enabled successfully!\n");
}

// need to register this handler in the Interrupt Descriptor Table (IDT) for vector 14
void page_fault_handler()
{
    printk("[PAGING] !!! PAGE FAULT !!!\n");

    while(1) { __asm__ __volatile__("hlt"); }
}
