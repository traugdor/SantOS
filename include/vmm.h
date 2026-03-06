#ifndef VMM_H
#define VMM_H

#include <stdint.h>

// x86-64 paging constants
#define PAGE_SIZE       4096
#define PAGE_PRESENT    (1ULL << 0)
#define PAGE_WRITE      (1ULL << 1)
#define PAGE_USER       (1ULL << 2)
#define PAGE_HUGE       (1ULL << 7)
#define PAGE_NO_EXECUTE (1ULL << 63)

// Page table entry type
typedef uint64_t pte_t;

// Page table structure (512 entries per table, each 8 bytes = 4KB)
typedef struct {
    pte_t entries[512];
} __attribute__((packed)) page_table_t;

// Extract page table indices from virtual address
#define PML4_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr) (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)   (((addr) >> 12) & 0x1FF)

// Address mask for page table entries (bits 12-51)
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

// Initialize virtual memory manager
// Sets up the VMM to work with the existing page tables from boot2
int vmm_init(void);

// Map a virtual address to a physical address
// virt: virtual address to map
// phys: physical address to map to
// flags: page flags (PAGE_PRESENT, PAGE_WRITE, PAGE_USER, etc.)
// Returns: 0 on success, -1 on failure
int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);

// Unmap a virtual address
// virt: virtual address to unmap
// Returns: 0 on success, -1 on failure
int vmm_unmap_page(uint64_t virt);

// Map a range of pages (contiguous virtual to contiguous physical)
// virt: starting virtual address
// phys: starting physical address
// count: number of pages to map
// flags: page flags
// Returns: 0 on success, -1 on failure
int vmm_map_pages(uint64_t virt, uint64_t phys, uint32_t count, uint64_t flags);

// Unmap a range of pages
// virt: starting virtual address
// count: number of pages to unmap
// Returns: 0 on success, -1 on failure
int vmm_unmap_pages(uint64_t virt, uint32_t count);

// Get the physical address mapped to a virtual address
// virt: virtual address to look up
// Returns: physical address, or 0 on failure (unmapped)
uint64_t vmm_get_physical(uint64_t virt);

// Flush the TLB entry for a given virtual address
void vmm_flush_tlb(uint64_t virt);

// Flush the entire TLB
void vmm_flush_tlb_all(void);

#endif
