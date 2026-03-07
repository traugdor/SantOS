// SantOS Kernel - Delivered by ELFs!

#include "include/stdio.h"
#include "include/stdlib.h"
#include "include/string.h"
#include "include/ctype.h"
#include "include/vga.h"
#include "include/idt.h"
#include "include/timer.h"
#include "include/keyboard.h"
#include "include/fdc.h"
#include "include/ata.h"
#include "include/fat12.h"
#include "include/memory.h"
#include "include/vmm.h"
#include "include/heap.h"
#include "include/loader.h"

// Disk type enumeration
typedef enum {
    DISK_NONE = 0,
    DISK_FLOPPY,
    DISK_ATA
} disk_type_t;

static disk_type_t active_disk = DISK_NONE;

void kernel_main(void) {
    // Initialize VGA driver
    vga_init();
    
    // Print welcome message
    printf("Welcome to SantOS!\n\n");
    
    // Initialize interrupts, timer, and keyboard
    idt_init();
    timer_init(1000);  // 1000 Hz = 1ms per tick
    keyboard_init();
    printf("Keyboard ready!\n\n");
    
    // Parse E820 memory map and initialize memory managers
    const e820_map_t* e820 = e820_parse();
    
    if (!e820) {
        pmem_init(0x400000, 0x400000);
    } else {
        uint64_t mem_start, mem_size;
        if (e820_find_largest_region(&mem_start, &mem_size) == 0) {
            uint64_t pmem_start = 0x800000;  // 8MB
            if (mem_start > pmem_start) {
                pmem_start = mem_start;
            }
            
            uint64_t pmem_size = (mem_start + mem_size) - pmem_start;
            
            // Cap at 1GB (boot2 maps up to 1GB with 512 × 2MB pages)
            if (pmem_size > 0x40000000) {
                pmem_size = 0x40000000;
            }
            
            pmem_init((uint32_t)pmem_start, (uint32_t)pmem_size);
        } else {
            pmem_init(0x400000, 0x400000);
        }
    }
    
    vmm_init();
    heap_init((void*)0x1000000, 1024 * 1024);
    
    // Test heap allocation
    printf("Testing heap allocation...\n");
    int* test_array = (int*)malloc(10 * sizeof(int));
    if (test_array) {
        printf("  malloc test: SUCCESS (allocated %d bytes)\n", (int)(10 * sizeof(int)));
        for (int i = 0; i < 10; i++) {
            test_array[i] = i * 100;
        }
        printf("  Array values: %d, %d, %d, %d, %d\n", 
               test_array[0], test_array[1], test_array[2], test_array[3], test_array[4]);
        free(test_array);
        printf("  free test: SUCCESS\n\n");
    } else {
        printf("  malloc test: FAILED\n\n");
    }
    
    // Detect available disk drives
    printf("Detecting disk drives...\n");
    
    // Try FDC first (floppy)
    if (fdc_detect()) {
        printf("Floppy disk controller detected\n");
        if (fdc_init() == 0) {
            active_disk = DISK_FLOPPY;
            printf("Using floppy disk\n\n");
        } else {
            printf("FDC initialization failed\n");
        }
    } else {
        printf("No floppy disk detected\n");
    }
    
    // Try ATA if no floppy
    if (active_disk == DISK_NONE && ata_detect()) {
        printf("ATA hard disk detected\n");
        if (ata_init() == 0) {
            active_disk = DISK_ATA;
            printf("Using ATA hard disk\n\n");
        } else {
            printf("ATA initialization failed\n");
        }
    } else if (active_disk == DISK_NONE) {
        printf("No ATA disk detected\n");
    }
    
    // Check if we have any disk
    if (active_disk == DISK_NONE) {
        printf("\nERROR: No disk drives available!\n");
        printf("Cannot access filesystem.\n\n");
        printf("Halting.\n");
        __asm__ volatile("1: hlt; jmp 1b");
    }
    
    // Initialize FAT12 filesystem
    printf("Initializing FAT12 filesystem...\n");
    if (fat12_init() != 0) {
        printf("FAT12 initialization failed!\n\n");
        printf("Halting.\n");
        __asm__ volatile("1: hlt; jmp 1b");
    }
    printf("FAT12 initialized successfully!\n\n");
    
    // Load and execute shell program
    void* shell_addr = (void*)0x100000;  // Load at 1MB
    uint64_t entry_point = load_program("SHELL.ELF", shell_addr);
    if (entry_point != 0) {
        execute_program(entry_point, "SHELL.ELF", 1);  // kernel_mode=1 (halt on exit)
    } else {
        printf("Failed to load shell. Halting.\n");
        __asm__ volatile("1: hlt; jmp 1b");
    }

    //if we get here, notify user to power off computer
    printf("It is now safe to turn off your computer.\n");
    __asm__ volatile("hlt");
}