#include "../include/vmm.h"
#include "../include/memory.h"
#include "../include/printf.h"
#include <stdint.h>

// The PML4 table set up by boot2.asm is at physical address 0x1000
// boot2 identity-maps first 8MB using 2MB huge pages:
//   PML4[0] -> PDPT at 0x2000
//   PDPT[0] -> PD at 0x3000
//   PD[0]: 0-2MB (huge), PD[1]: 2-4MB (huge), PD[2]: 4-6MB (huge), PD[3]: 6-8MB (huge)

static pte_t* pml4 = (pte_t*)0x1000;

// Read CR3 to get the current PML4 address
static uint64_t read_cr3(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

// Write CR3 to set the PML4 address
static void write_cr3(uint64_t cr3) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

// Flush a single TLB entry
void vmm_flush_tlb(uint64_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

// Flush entire TLB by reloading CR3
void vmm_flush_tlb_all(void) {
    write_cr3(read_cr3());
}

// Allocate a new page table (zeroed out)
// Returns the physical address of the new page table, or 0 on failure
static uint64_t alloc_page_table(void) {
    uint32_t page = pmem_alloc_page();
    if (page == 0) {
        return 0;
    }
    
    // Zero the page table
    uint8_t* ptr = (uint8_t*)(uint64_t)page;
    for (int i = 0; i < PAGE_SIZE; i++) {
        ptr[i] = 0;
    }
    
    return (uint64_t)page;
}

// Get or create a page table entry at a given level
// parent: pointer to the parent table
// index: index into the parent table
// create: if 1, create a new table if the entry doesn't exist
// Returns: pointer to the child table, or 0 on failure
static pte_t* get_or_create_table(pte_t* parent, int index, int create) {
    if (parent[index] & PAGE_PRESENT) {
        // Entry exists, return pointer to child table
        return (pte_t*)(parent[index] & PTE_ADDR_MASK);
    }
    
    if (!create) {
        return 0;
    }
    
    // Allocate a new page table
    uint64_t new_table = alloc_page_table();
    if (new_table == 0) {
        return 0;
    }
    
    // Set the entry to point to the new table
    parent[index] = new_table | PAGE_PRESENT | PAGE_WRITE;
    
    return (pte_t*)new_table;
}

// Initialize the virtual memory manager
int vmm_init(void) {
    // Read current CR3 to verify page table location
    uint64_t cr3 = read_cr3();
    pml4 = (pte_t*)(cr3 & PTE_ADDR_MASK);
    
    // Read highest mapped address from boot2 (stored at 0x8000)
    uint32_t* highest_addr_ptr = (uint32_t*)0x8000;
    uint32_t highest_addr = *highest_addr_ptr;
    
    // Convert to MB for display
    uint32_t mapped_mb = highest_addr / (1024 * 1024);
    
    printf("Virtual memory manager initialized:\n");
    printf("  PML4 at: %x\n", (uint32_t)(uint64_t)pml4);
    printf("  CR3: %x\n", (uint32_t)cr3);
    printf("  Identity map: 0-%dMB (2MB huge pages, dynamic from E820)\n\n", mapped_mb);
    
    return 0;
}

// Map a single 4KB virtual page to a physical page
int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    // Get indices into each level of page tables
    int pml4_idx = PML4_INDEX(virt);
    int pdpt_idx = PDPT_INDEX(virt);
    int pd_idx   = PD_INDEX(virt);
    int pt_idx   = PT_INDEX(virt);
    
    // Walk/create page table hierarchy: PML4 -> PDPT -> PD -> PT
    pte_t* pdpt = get_or_create_table(pml4, pml4_idx, 1);
    if (!pdpt) {
        printf("VMM: Failed to get/create PDPT\n");
        return -1;
    }
    
    pte_t* pd = get_or_create_table(pdpt, pdpt_idx, 1);
    if (!pd) {
        printf("VMM: Failed to get/create PD\n");
        return -1;
    }
    
    // Check if this PD entry is a 2MB huge page
    if (pd[pd_idx] & PAGE_HUGE) {
        printf("VMM: Cannot map 4KB page over 2MB huge page at %x\n", (uint32_t)virt);
        return -1;
    }
    
    pte_t* pt = get_or_create_table(pd, pd_idx, 1);
    if (!pt) {
        printf("VMM: Failed to get/create PT\n");
        return -1;
    }
    
    // Check if page is already mapped
    if (pt[pt_idx] & PAGE_PRESENT) {
        printf("VMM: Page already mapped at %x\n", (uint32_t)virt);
        return -1;
    }
    
    // Set the page table entry
    pt[pt_idx] = (phys & PTE_ADDR_MASK) | flags | PAGE_PRESENT;
    
    // Flush TLB for this address
    vmm_flush_tlb(virt);
    
    return 0;
}

// Unmap a single 4KB virtual page
int vmm_unmap_page(uint64_t virt) {
    int pml4_idx = PML4_INDEX(virt);
    int pdpt_idx = PDPT_INDEX(virt);
    int pd_idx   = PD_INDEX(virt);
    int pt_idx   = PT_INDEX(virt);
    
    // Walk page table hierarchy (don't create if missing)
    pte_t* pdpt = get_or_create_table(pml4, pml4_idx, 0);
    if (!pdpt) return -1;
    
    pte_t* pd = get_or_create_table(pdpt, pdpt_idx, 0);
    if (!pd) return -1;
    
    // Can't unmap a huge page with this function
    if (pd[pd_idx] & PAGE_HUGE) return -1;
    
    pte_t* pt = get_or_create_table(pd, pd_idx, 0);
    if (!pt) return -1;
    
    // Check if page is mapped
    if (!(pt[pt_idx] & PAGE_PRESENT)) {
        return -1;
    }
    
    // Clear the entry
    pt[pt_idx] = 0;
    
    // Flush TLB
    vmm_flush_tlb(virt);
    
    return 0;
}

// Map a contiguous range of pages
int vmm_map_pages(uint64_t virt, uint64_t phys, uint32_t count, uint64_t flags) {
    for (uint32_t i = 0; i < count; i++) {
        int result = vmm_map_page(
            virt + (uint64_t)i * PAGE_SIZE,
            phys + (uint64_t)i * PAGE_SIZE,
            flags
        );
        if (result != 0) {
            // Unmap any pages we already mapped
            for (uint32_t j = 0; j < i; j++) {
                vmm_unmap_page(virt + (uint64_t)j * PAGE_SIZE);
            }
            return -1;
        }
    }
    return 0;
}

// Unmap a contiguous range of pages
int vmm_unmap_pages(uint64_t virt, uint32_t count) {
    int result = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (vmm_unmap_page(virt + (uint64_t)i * PAGE_SIZE) != 0) {
            result = -1;
        }
    }
    return result;
}

// Look up the physical address for a virtual address
uint64_t vmm_get_physical(uint64_t virt) {
    int pml4_idx = PML4_INDEX(virt);
    int pdpt_idx = PDPT_INDEX(virt);
    int pd_idx   = PD_INDEX(virt);
    int pt_idx   = PT_INDEX(virt);
    
    // Walk page table hierarchy
    if (!(pml4[pml4_idx] & PAGE_PRESENT)) return 0;
    pte_t* pdpt = (pte_t*)(pml4[pml4_idx] & PTE_ADDR_MASK);
    
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;
    pte_t* pd = (pte_t*)(pdpt[pdpt_idx] & PTE_ADDR_MASK);
    
    // Check for 2MB huge page
    if (pd[pd_idx] & PAGE_HUGE) {
        uint64_t base = pd[pd_idx] & 0x000FFFFFFFE00000ULL;
        return base + (virt & 0x1FFFFF);
    }
    
    if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
    pte_t* pt = (pte_t*)(pd[pd_idx] & PTE_ADDR_MASK);
    
    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
    return (pt[pt_idx] & PTE_ADDR_MASK) + (virt & 0xFFF);
}
