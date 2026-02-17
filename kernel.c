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
#include "include/fat12.h"

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
    
    // Initialize FDC (Floppy Disk Controller)
    fdc_init();
    
    // Initialize FAT12 filesystem
    printf("Initializing FAT12 filesystem...\n");
    if (fat12_init() == 0) {
        printf("FAT12 initialized successfully!\n\n");
        
        // Try to read TEST.TXT using FAT12 driver
        printf("Attempting to read TEST.TXT...\n");
        fat12_file_t file;
        
        if (fat12_open("TEST.TXT", &file) == 0) {
            printf("Successfully opened TEST.TXT (%d bytes)\n", file.size);
            printf("File contents:\n");
            printf("---\n");
            
            // Read and print file contents
            uint8_t buffer[512];
            int bytes_read = fat12_read(&file, buffer, sizeof(buffer));
            
            if (bytes_read > 0) {
                for (int i = 0; i < bytes_read; i++) {
                    putchar(buffer[i]);
                }
            }
            
            printf("\n---\n\n");
        } else {
            printf("Failed to open TEST.TXT\n\n");
        }
    } else {
        printf("FAT12 initialization failed!\n\n");
    }
    
    // Halt
    __asm__ volatile("1: hlt; jmp 1b");
}