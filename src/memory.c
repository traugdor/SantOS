#include "../include/memory.h"
#include "../include/printf.h"
#include <stdint.h>

// E820 memory map from bootloader (at physical address 0x500)
// Layout: 0x500 = count (4 bytes), 0x504 = entries
#define E820_MAP_ADDR 0x500

static physical_memory_t pmem;
static e820_map_t e820_map;

// Parse E820 memory map from bootloader
const e820_map_t* e820_parse(void) {
    uint32_t* count_ptr = (uint32_t*)E820_MAP_ADDR;
    e820_entry_t* entries = (e820_entry_t*)(E820_MAP_ADDR + 4);  // Entries start at 0x504
    
    e820_map.count = *count_ptr;
    
    if (e820_map.count == 0 || e820_map.count > 32) {
        printf("ERROR: Invalid E820 entry count: %d\n", e820_map.count);
        return 0;
    }
    
    // Copy entries to our structure
    for (uint32_t i = 0; i < e820_map.count; i++) {
        e820_map.entries[i] = entries[i];
    }
    
    // Print memory map
    printf("E820 Memory Map (%d entries):\n", e820_map.count);
    for (uint32_t i = 0; i < e820_map.count; i++) {
        const char* type_str = "Unknown";
        switch (e820_map.entries[i].type) {
            case E820_RAM: type_str = "Usable RAM"; break;
            case E820_RESERVED: type_str = "Reserved"; break;
            case E820_ACPI: type_str = "ACPI"; break;
            case E820_NVS: type_str = "ACPI NVS"; break;
            case E820_UNUSABLE: type_str = "Unusable"; break;
        }
        
        printf("  [%d] Base: %x%x, Length: %x%x, Type: %s\n",
               i,
               (uint32_t)(e820_map.entries[i].base >> 32),
               (uint32_t)(e820_map.entries[i].base & 0xFFFFFFFF),
               (uint32_t)(e820_map.entries[i].length >> 32),
               (uint32_t)(e820_map.entries[i].length & 0xFFFFFFFF),
               type_str);
    }
    printf("\n");
    
    return &e820_map;
}

// Find the largest usable RAM region
int e820_find_largest_region(uint64_t* start, uint64_t* size) {
    uint64_t largest_size = 0;
    uint64_t largest_start = 0;
    
    for (uint32_t i = 0; i < e820_map.count; i++) {
        if (e820_map.entries[i].type == E820_RAM) {
            uint64_t region_start = e820_map.entries[i].base;
            uint64_t region_end = e820_map.entries[i].base + e820_map.entries[i].length;
            
            // Skip regions that end before 1MB (reserved for BIOS/VGA/etc)
            if (region_end <= 0x100000) {
                continue;
            }
            
            // If region starts below 1MB but extends above, adjust start to 1MB
            if (region_start < 0x100000) {
                region_start = 0x100000;
            }
            
            uint64_t usable_size = region_end - region_start;
            
            if (usable_size > largest_size) {
                largest_size = usable_size;
                largest_start = region_start;
            }
        }
    }
    
    if (largest_size == 0) {
        printf("ERROR: No usable RAM found!\n");
        return -1;
    }
    
    printf("Largest region: start=%x, size=%x\n", 
           (uint32_t)largest_start, (uint32_t)largest_size);
    
    *start = largest_start;
    *size = largest_size;
    return 0;
}

// Helper: Set a bit in the bitmap
static void bitmap_set(uint32_t page) {
    uint32_t byte_idx = page / 8;
    uint32_t bit_idx = page % 8;
    pmem.bitmap[byte_idx] |= (1 << bit_idx);
}

// Helper: Clear a bit in the bitmap
static void bitmap_clear(uint32_t page) {
    uint32_t byte_idx = page / 8;
    uint32_t bit_idx = page % 8;
    pmem.bitmap[byte_idx] &= ~(1 << bit_idx);
}

// Helper: Check if a bit is set
static int bitmap_is_set(uint32_t page) {
    uint32_t byte_idx = page / 8;
    uint32_t bit_idx = page % 8;
    return (pmem.bitmap[byte_idx] & (1 << bit_idx)) != 0;
}

// Helper: Find first free page starting from given index
static uint32_t find_free_page(uint32_t start) {
    for (uint32_t page = start; page < pmem.total_pages; page++) {
        if (!bitmap_is_set(page)) {
            return page;
        }
    }
    return 0xFFFFFFFF; // Not found
}

// Initialize physical memory manager
int pmem_init(uint32_t memory_start, uint32_t memory_size) {
    // Calculate number of pages (4KB each)
    pmem.total_pages = memory_size / 4096;
    
    // Calculate bitmap size (1 bit per page)
    pmem.bitmap_size = (pmem.total_pages + 7) / 8;
    
    // Bitmap is stored at the beginning of available memory
    pmem.bitmap = (uint8_t*)(uintptr_t)memory_start;
    
    // Clear bitmap (all pages free)
    for (uint32_t i = 0; i < pmem.bitmap_size; i++) {
        pmem.bitmap[i] = 0;
    }
    
    // Mark bitmap pages as used (they contain the bitmap itself)
    uint32_t bitmap_pages = (pmem.bitmap_size + 4095) / 4096;
    for (uint32_t i = 0; i < bitmap_pages; i++) {
        bitmap_set(i);
    }
    
    pmem.free_pages = pmem.total_pages - bitmap_pages;
    
    // Calculate MB values using integer math
    // (pages * 4096 bytes/page) / (1024*1024 bytes/MB)
    uint32_t total_kb = (pmem.total_pages * 4096) / 1024;
    uint32_t total_mb_int = total_kb / 1024;
    uint32_t total_mb_frac = (total_kb % 1024) * 10 / 1024;
    
    uint32_t free_kb = (pmem.free_pages * 4096) / 1024;
    uint32_t free_mb_int = free_kb / 1024;
    uint32_t free_mb_frac = (free_kb % 1024) * 10 / 1024;
    
    printf("Physical memory manager initialized:\n");
    printf("  Total pages: %d (%d.%d MB)\n", pmem.total_pages, total_mb_int, total_mb_frac);
    printf("  Free pages: %d (%d.%d MB)\n", pmem.free_pages, free_mb_int, free_mb_frac);
    printf("  Bitmap size: %d bytes\n\n", pmem.bitmap_size);
    
    return 0;
}

// Allocate a single physical page
uint32_t pmem_alloc_page(void) {
    if (pmem.free_pages == 0) {
        printf("ERROR: Out of physical memory!\n");
        return 0;
    }
    
    uint32_t page = find_free_page(0);
    if (page == 0xFFFFFFFF) {
        printf("ERROR: No free pages found!\n");
        return 0;
    }
    
    bitmap_set(page);
    pmem.free_pages--;
    
    return (page * 4096);
}

// Allocate multiple contiguous physical pages
uint32_t pmem_alloc_pages(uint32_t count) {
    if (count == 0) {
        return 0;
    }
    
    if (pmem.free_pages < count) {
        printf("ERROR: Not enough free pages! Need %d, have %d\n", count, pmem.free_pages);
        return 0;
    }
    
    // Find first free page
    uint32_t start_page = find_free_page(0);
    if (start_page == 0xFFFFFFFF) {
        printf("ERROR: No free pages found!\n");
        return 0;
    }
    
    // Check if we have 'count' contiguous free pages
    for (uint32_t i = 0; i < count; i++) {
        if (bitmap_is_set(start_page + i)) {
            // Not contiguous, try again from next page
            return pmem_alloc_pages(count);
        }
    }
    
    // Mark all pages as used
    for (uint32_t i = 0; i < count; i++) {
        bitmap_set(start_page + i);
    }
    
    pmem.free_pages -= count;
    
    return (start_page * 4096);
}

// Free a single physical page
void pmem_free_page(uint32_t addr) {
    if (addr == 0) {
        printf("WARNING: Attempted to free NULL address\n");
        return;
    }
    
    uint32_t page = addr / 4096;
    
    if (page >= pmem.total_pages) {
        printf("ERROR: Invalid page address: 0x%x\n", addr);
        return;
    }
    
    if (!bitmap_is_set(page)) {
        printf("WARNING: Double-free detected at address 0x%x\n", addr);
        return;
    }
    
    bitmap_clear(page);
    pmem.free_pages++;
}

// Free multiple contiguous physical pages
void pmem_free_pages(uint32_t addr, uint32_t count) {
    if (count == 0) {
        return;
    }
    
    for (uint32_t i = 0; i < count; i++) {
        pmem_free_page(addr + (i * 4096));
    }
}

// Get number of free pages
uint32_t pmem_get_free_pages(void) {
    return pmem.free_pages;
}

// Get total number of pages
uint32_t pmem_get_total_pages(void) {
    return pmem.total_pages;
}
