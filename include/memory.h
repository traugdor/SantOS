#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

// E820 Memory Map Entry (from BIOS)
#define E820_RAM        1  // Usable RAM
#define E820_RESERVED   2  // Reserved, do not use
#define E820_ACPI       3  // ACPI reclaimable
#define E820_NVS        4  // ACPI NVS (non-volatile storage)
#define E820_UNUSABLE   5  // Unusable memory

typedef struct {
    uint64_t base;      // Base address
    uint64_t length;    // Length in bytes
    uint32_t type;      // Memory type
    uint32_t extended;  // Extended attributes (optional)
} __attribute__((packed)) e820_entry_t;

typedef struct {
    uint32_t count;     // Number of entries
    e820_entry_t entries[32];  // Up to 32 entries
} e820_map_t;

// Parse E820 memory map from bootloader (stored at 0x500)
// Returns: pointer to parsed E820 map, or NULL on failure
const e820_map_t* e820_parse(void);

// Find the largest usable memory region from E820 map
// start: output pointer for region start address
// size: output pointer for region size
// Returns: 0 on success, -1 on failure
int e820_find_largest_region(uint64_t* start, uint64_t* size);

// Physical memory manager - bitmap-based page allocator
// Manages allocation and deallocation of physical pages (4KB each)

typedef struct {
    uint8_t* bitmap;           // Bitmap: 1 bit per page (1=used, 0=free)
    uint32_t total_pages;      // Total number of pages available
    uint32_t free_pages;       // Number of free pages
    uint32_t bitmap_size;      // Size of bitmap in bytes
} physical_memory_t;

// Initialize physical memory manager
// memory_start: start of available physical memory (usually after kernel)
// memory_size: total size of available memory in bytes
int pmem_init(uint32_t memory_start, uint32_t memory_size);

// Allocate a single physical page (4KB)
// Returns: physical address of allocated page, or 0 on failure
uint32_t pmem_alloc_page(void);

// Allocate multiple contiguous physical pages
// count: number of pages to allocate
// Returns: physical address of first page, or 0 on failure
uint32_t pmem_alloc_pages(uint32_t count);

// Free a single physical page
// addr: physical address of page to free
void pmem_free_page(uint32_t addr);

// Free multiple contiguous physical pages
// addr: physical address of first page
// count: number of pages to free
void pmem_free_pages(uint32_t addr, uint32_t count);

// Get number of free pages
uint32_t pmem_get_free_pages(void);

// Get total number of pages
uint32_t pmem_get_total_pages(void);

#endif
