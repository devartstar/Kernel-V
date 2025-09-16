#include "paging.h"
#include "printk.h"

static uint32_t* page_directory         = (uint32_t*)PAGE_DIR_START_ADDR;
static uint32_t* first_page_table       = (uint32_t*)PAGE_TABLE_START_ADDR;

//
// Initialize paging by setting up first entry in page directory 
// to a simple identity-mapped page table
//
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

//
// Walk the page directory and page table for the given virtual address
// allocate a new page table entry if not present
// map the page table entry to physical frames with given flags
//
void paging_map_page (uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags)
{
    // Extract page directory index (bits 22 - 31) - top 10 bits
    uint32_t pdir_index = virtual_addr >> 22;

    // Extract page table index (bits 12 - 21) - next 10 bits
    uint32_t ptable_index = (virtual_addr >> 12) & 0x03FF;

    // If already present - get the address of the page table
    uint32_t* page_table;
    if (page_directory[pdir_index] & PAGE_PRESENT)
    {
        // if page table already exists, get its address
        page_table = (uint32_t*)(page_directory[pdir_index] & 0xFFFFF000);
    }
    else 
    {
        // Allocate a new Page Table

        // allocate a 4Kb frame
        page_table = (uint32_t*)pmm_alloc_frame(); 
        if (!page_table)
        {
            panik("Out of memory: Unable to allocate frame for new page table");
        }
        for (uint32_t entry = 0; entry < PAGE_ENTRIES; entry++)
        {
            page_table[entry] = 0;
        }

        // Link the new page table to the page directory
        page_directory[pdir_index] = ((uint32_t)page_table) | PAGE_PRESENT | PAGE_WRITE;
    }

    // Now set the page table entry to point to the physical frame with given flags
    page_table[ptable_index] = (physical_addr & 0xFFFFF000) | (flags & 0xFFF);

    // TLB - Translation Look aside buffer. It is a small cache inside the CPU.
    // stores the recent virtual to physical address translations. 
    // Flush TLB for new page, rest unchanged (to ensure hardware picks up new mapping)
    __asm__ __volatile__ (
        "invlpg (%0)"
        :
        : "r"(virtual_addr)
        : "memory"
    );
}