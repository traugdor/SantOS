#include "../include/syscall.h"
#include "../include/stdio.h"
#include "../include/heap.h"
#include "../include/string.h"
#include "../include/vga.h"
#include "../include/fat12.h"
#include "../include/loader.h"
#include <stdarg.h>

// Kernel-side system call handler
// Called from syscall_asm.asm with arguments passed via C calling convention
uint64_t syscall_handler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    uint64_t result = 0;
    
    switch (syscall_num) {
        // I/O syscalls
        case SYSCALL_PUTCHAR:
            putchar((char)arg1);
            result = 0;
            break;
            
        case SYSCALL_GETCHAR:
            // Re-enable interrupts so keyboard IRQ can fire
            __asm__ volatile("sti");
            result = (uint64_t)getchar();
            break;
            
        case SYSCALL_PRINTF:
            printf("%s", (const char*)arg1);
            result = 0;
            break;
            
        case SYSCALL_CLEAR:
            vga_clear();
            result = 0;
            break;
            
        case SYSCALL_SET_COLOR:
            //printf("[SYSCALL_SET_COLOR] fg=%d bg=%d\n", (uint8_t)arg1, (uint8_t)arg2);
            vga_set_color((uint8_t)arg1, (uint8_t)arg2);  // fg, bg
            result = 0;
            break;
        
        // Memory syscalls
        case SYSCALL_MALLOC:
            result = (uint64_t)malloc((size_t)arg1);
            break;
            
        case SYSCALL_FREE:
            free((void*)arg1);
            result = 0;
            break;
            
        case SYSCALL_REALLOC:
            result = (uint64_t)realloc((void*)arg1, (size_t)arg2);
            break;
            
        case SYSCALL_CALLOC:
            result = (uint64_t)calloc((size_t)arg1, (size_t)arg2);
            break;
        
        // String syscalls
        case SYSCALL_STRLEN:
            result = (uint64_t)strlen((const char*)arg1);
            break;
            
        case SYSCALL_STRCMP:
            result = (uint64_t)(int64_t)strcmp((const char*)arg1, (const char*)arg2);
            break;
            
        case SYSCALL_STRCPY:
            result = (uint64_t)strcpy((char*)arg1, (const char*)arg2);
            break;
            
        case SYSCALL_STRCAT:
            result = (uint64_t)strcat((char*)arg1, (const char*)arg2);
            break;
            
        case SYSCALL_MEMCPY:
            result = (uint64_t)memcpy((void*)arg1, (const void*)arg2, (size_t)arg3);
            break;
            
        case SYSCALL_MEMSET:
            result = (uint64_t)memset((void*)arg1, (int)arg2, (size_t)arg3);
            break;
        
        // Filesystem syscalls
        case SYSCALL_LIST_DIR:
            result = (uint64_t)fat12_list_root();
            break;
            
        case SYSCALL_LIST_DIR_CLUSTER:
            result = (uint64_t)fat12_list_dir((uint16_t)arg1);
            break;
            
        case SYSCALL_FIND_ENTRY:
            result = (uint64_t)fat12_find_entry((uint16_t)arg1, (const char*)arg2, (int*)arg3);
            break;
        
        // Program load syscall - loads ELF and returns entry point
        // Does NOT execute - caller must invoke the entry point from userspace
        case SYSCALL_EXEC_PROGRAM: {
            const char* filename = (const char*)arg1;
            
            // Prevent shell from executing itself (would overwrite its own code)
            if (strcmp(filename, "SHELL.ELF") == 0 || strcmp(filename, "shell.elf") == 0) {
                printf("Error: Cannot execute shell from within shell\n");
                result = 0;
                break;
            }
            
            // First, open the file and check if it's a valid executable
            fat12_file_t file;
            if (fat12_open(filename, &file) != 0) {
                printf("Error: Failed to open %s\n", filename);
                result = 0;
                break;
            }
            
            // Read first 4 bytes to check for ELF magic number (0x7F 'E' 'L' 'F')
            uint8_t header[4];
            if (fat12_read(&file, header, 4) != 4) {
                printf("Error: Failed to read file header\n");
                result = 0;
                break;
            }
            
            // Check for ELF magic bytes
            if (header[0] != 0x7F || header[1] != 'E' || header[2] != 'L' || header[3] != 'F') {
                printf("Error: %s is not a valid executable (not ELF format)\n", filename);
                result = 0;
                break;
            }
            
            void* load_addr = (void*)0x500000;  // Load at 5MB (after kernel at 2MB)
            
            // Load the program and return the entry point to userspace
            result = load_program(filename, load_addr);
            break;
        }
            
        default:
            result = (uint64_t)-1;  // Invalid syscall
            break;
    }
    
    return result;
}
