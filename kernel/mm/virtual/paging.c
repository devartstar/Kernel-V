#include "mm/paging.h"
#include "core/debug.h"
#include "core/panik.h"
#include "lib/printk.h"
#include "mm/pmm.h"

/*
static uint32_t *page_directory = (uint32_t *)PAGE_DIR_START_ADDR;
static uint32_t *first_page_table = (uint32_t *)PAGE_TABLE_START_ADDR;
*/

uint32_t *kernel_page_directory_virt = kernel_page_directory;
uint32_t kernel_page_directory_phys = (uint32_t)kernel_page_directory;

//
//  Initialize paging by setting up first entry in page directory
//  to a simple identity-mapped page table
//
void paging_init() {
    debug_module(PAGING, "[PAGING] Initializing Paging structures...\n");
    debug_module(PAGING,
                 "\n==================================================\n");
    debug_module(PAGING, "Initializing Paging...\n");

    //  clear the page directory table
    for (uint32_t entry = 0; entry < PAGE_ENTRIES; entry++) {
        //  marking each entry as supervisor, Read/Write, not present
        kernel_page_directory[entry] = 0x00000002;
    }

    //  identity map first 4MB (0x00000000 to 0x003FFFFF)
    for (uint32_t entry = 0; entry < PAGE_ENTRIES; entry++) {
        //  [0  - 12] bits: flags (present, writable, user)
        //  [12 - 31] bits: physical address of the page frame
        first_page_table[entry] =
            (entry * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }

    //  LINK first page table to the first entry of the page directory
    kernel_page_directory[0] =
        (uint32_t)first_page_table | PAGE_PRESENT | PAGE_WRITE;

    debug_module(PAGING, "[PAGING] Directory at %p, Table[0] at %p\n",
                 kernel_page_directory, first_page_table);
    debug_module(PAGING,
                 "\n==================================================\n");

    //  Load the page directory address into CR3
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(kernel_page_directory));

    //  Store the cr0 register value int variable
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));

    //  Set the paging bit cr0.pg (bit 31)
    cr0 |= 0x80000000;

    //  Write the value back to cr0
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0));

    //  From now on all memory access will be virtual
    pr_info("[PAGING] Paging enabled successfully!\n");
}

void debug_dump_pte(uint32_t virtual_addr) {
    uint32_t pdir_index = virtual_addr >> 22;             /* bits 22-31 */
    uint32_t ptable_index = (virtual_addr >> 12) & 0x3FF; /* bits 21-12 */

    /* check if page directory entry present */
    if (!(page_directory[pdir_index] & PAGE_PRESENT)) {
        KLOG_VERBOSE("PAGE_TABLE",
                     "Page directory entry for 0x%08x not presnet.\n",
                     virtual_addr);
        return;
    }

    /* Get the address of the page table */
    uint32_t *page_table =
        (uint32_t *)(page_directory[pdir_index] & 0xFFFFF000);
    uint32_t pte = page_table[ptable_index];

    char flags[64];
    if (pte & PAGE_PRESENT)
        strappend(flags, "PRESENT ");
    if (pte & PAGE_USER)
        strappend(flags, "USER ");
    if (pte & PAGE_WRITE)
        strappend(flags, "WRITE ");

    KLOG_VERBOSE("PAGE_TABLE",
                 "PTE for 0x%08x: 0x%08x [physical=0x%08x]\tflags=%s\n",
                 virtual_addr, pte, pte & 0xFFFFF000, flags);
}

void paging_map_page_in_pd(uint32_t *pd_virt, uint32_t virt_addr,
                           uint32_t phys_addr, uint32_t flags) {
    uint32_t pdir_index = PD_INDEX(virt_addr);
    uint32_t ptable_index = PT_INDEX(virt_addr);

    if (!pd_virt) {
        panik("paging_map_page_in_pd: pd_virt is null");
    }

    uint32_t *page_table;

    if (pd_virt[pdir_index] & PAGE_PRESENT) {
        page_table = (uint32_t *)(pd_virt[pdir_index] & 0xFFFFF000);
    } else {
        page_table = (uint32_t *)mm_alloc_frame();
        if (!page_table) {
            panik("paging_map_page_in_pd: Unable to allocate frame for new "
                  "page table");
        }

        for (uint32_t entry = 0; entry < PAGE_ENTRIES; entry++) {
            page_table[entry] = 0;
        }

        pd_virt[pdir_index] = ((uint32_t)page_table) | PAGE_PRESENT |
                              PAGE_WRITE | (flags & PAGE_USER);
    }

    page_table[ptable_index] = (phys_addr & 0xFFFFF000) | (flags | 0xFFF);

    /*
     * IMPORTANT:
     * Only flush TLB if this page directory is currently active.
     * For now you may flush unconditionally, but technically that only affects
     * current CR3.
     */
    __asm__ __volatile__("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

//
//  Walk the page directory and page table for the given virtual address
//  allocate a new page table entry if not present
//  map the page table entry to physical frames with given flags
//
void paging_map_page(uint32_t virtual_addr, uint32_t physical_addr,
                     uint32_t flags) {
    /* Map the virtual address in the current process page directory */
    paging_map_page_in_pd(kernel_page_directory_virt, virtual_addr,
                          physical_addr, flags);
}

uint32_t paging_get_physical_address_in_pd(uint32_t *pd_virt, uint32_t virt) {
    uint32_t pdir_index = PD_INDEX(virt);
    uint32_t ptable_index = PT_INDEX(virt);

    if (!pd_virt) {
        return 0;
    }

    if (!(pd_virt[pdir_index] & PAGE_PRESENT)) {
        return 0;
    }

    uint32_t *page_table = (uint32_t *)(pd_virt[pdir_index] & 0xFFFFF000);

    if (!(page_table[ptable_index] & PAGE_PRESENT)) {
        return 0;
    }

    uint32_t phy_base = page_table[ptable_index] & 0xFFFFF000;
    uint32_t offset = page_table[ptable_index] & 0xFFF;

    return phy_base + offset;
}

uint32_t paging_get_physical_address(uint32_t virt) {
    // 32 bit address ->
    // each entry of pagetable/directory = 12 LSB are for flags, 20 MSB is the
    // address
    // ---
    // => 10 MSB -> 31-22 -> page dir index in (1024) entries of page directory
    // => 10 MSB -> 21-12 -> page table index in (1024) entries of page table
    //
    return paging_get_physical_address_in_pd(kernel_page_directory_virt, virt);
}

void paging_unmap_page_in_pd(uint32_t *pd_virt, uint32_t virt) {
    uint32_t pdir_index = PD_INDEX(virt);
    uint32_t ptable_index = PT_INDEX(virt);

    if (!pd_virt) {
        return;
    }

    if (!(pd_virt[pdir_index] & PAGE_PRESENT)) {
        return;
    }

    uint32_t *page_table = (uint32_t *)(pd_virt[pdir_index] & 0xFFFFF000);

    if (!(page_table[ptable_index] & PAGE_PRESENT)) {
        return;
    }

    page_table[ptable_index] = 0;

    __asm__ __volatile__("invlpg (%0)" : : "r"(virt) : "memory");
}

void paging_unmap_page(uint32_t virt) {
    paging_unmap_page_in_pd(kernel_page_directory_virt, virt);
}

uint32_t paging_get_current_cr3(void) {
    uint32_t cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

void paging_switch_address_space(uint32_t pd_phys) {
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(pd_phys) : "memory");
}

int paging_create_address_space(uint32_t **out_pd_virt, uint32_t *out_pd_phys) {
    if (!out_pd_virt || !out_pd_phys) {
        return -1;
    }

    uint32_t *new_pd = (uint32_t *)pmm_alloc_frame();
    if (!new_pd) {
        return -1;
    }

    for (uint32_t i = 0; i < PAGE_ENTRIES; i++) {
        new_pd[i] = 0;
    }

    /* Copy kernel side page dir entries from master kernel page dir */
    for (uint32_t i = KERNEL_PDE_START; i < PAGE_ENTRIES; i++) {
        new_pd[i] = kernel_page_directory_virt[i];
    }

    *out_pd_virt = new_pd;
    *out_pd_phys = (uint32_t)new_pd;

    return 0;
}

void paging_free_region_in_pd(uint32_t *pd_virt, uint32_t start,
                              uint32_t size) {
    uint32_t addr = PAGE_ALIGN_DOWN(start);
    uint32_t end = PAGE_ALIGN_UP(start + size);

    for (; addr < end; addr += PAGE_SIZE) {
        uint32_t phys = paging_get_physical_address_in_pd(pd_virt, addr);

        if (phys) {
            pmm_free_frame((void *)(phys & 0xFFFFF000));
        }

        paging_unmap_page_in_pd(pd_virt, addr);
    }
}

void paging_free_region(uint32_t start, uint32_t size) {
    paging_free_region_in_pd(kernel_page_directory_virt, start, size);
}
