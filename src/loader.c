#include "../include/loader.h"
#include "../include/fat12.h"
#include "../include/printf.h"
#include "../include/heap.h"
#include <stdint.h>

// Load a program from disk into memory
int load_program(const char* filename, void* load_addr) {
    fat12_file_t file;
    
    printf("Loading program: %s\n", filename);
    
    // Open the file
    if (fat12_open(filename, &file) != 0) {
        printf("ERROR: Failed to open %s\n", filename);
        return -1;
    }
    
    printf("  File size: %d bytes\n", file.size);
    printf("  Loading to: %x\n", (uint32_t)(uint64_t)load_addr);
    
    // Read the entire file into memory
    int bytes_read = fat12_read(&file, (uint8_t*)load_addr, file.size);
    if (bytes_read < 0 || (uint32_t)bytes_read != file.size) {
        printf("ERROR: Failed to read %s (read %d of %d bytes)\n", filename, bytes_read, file.size);
        return -1;
    }
    
    printf("  Program loaded successfully! (%d bytes)\n", bytes_read);
    return 0;
}

// Execute a loaded program
void execute_program(void* entry_point) {
    printf("Executing program at %x\n\n", (uint32_t)(uint64_t)entry_point);
    
    // Cast entry point to function pointer and call it
    void (*program_main)(void) = (void (*)(void))entry_point;
    program_main();
    
    // If the program returns (it shouldn't), halt
    printf("\nProgram exited. Halting.\n");
    while(1) {
        __asm__ volatile("hlt");
    }
}
