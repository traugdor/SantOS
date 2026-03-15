#include "../include/syscall.h"
#include "../include/stdio.h"
#include "../include/heap.h"
#include "../include/string.h"
#include "../include/vga.h"
#include "../include/fat12.h"
#include "../include/loader.h"
#include <stdarg.h>

// I/O port helpers for VGA cursor position
static inline void outb_vga(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb_vga(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// Get current cursor position from VGA hardware
static void get_cursor_pos(uint8_t* x, uint8_t* y) {
    uint16_t pos = 0;
    
    // Read cursor position from VGA registers
    outb_vga(0x3D4, 0x0F);  // Low byte
    pos |= inb_vga(0x3D5);
    outb_vga(0x3D4, 0x0E);  // High byte
    pos |= ((uint16_t)inb_vga(0x3D5)) << 8;
    
    *x = pos % 80;
    *y = pos / 80;
}

// VGA buffer backup (shared between save/restore syscalls)
static uint16_t vga_backup[2000];  // 80x25 screen buffer
static uint8_t saved_cursor_x = 0;
static uint8_t saved_cursor_y = 0;

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
            
        case SYSCALL_SET_CURSOR:
            vga_set_cursor_pos((uint8_t)arg1, (uint8_t)arg2);  // x, y
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
            
        // File read syscall - reads entire file into buffer
        // arg1 = filename, arg2 = buffer, arg3 = buffer size
        // Returns bytes read, or 0 on error
        case SYSCALL_READ_FILE: {
            const char* fname = (const char*)arg1;
            uint8_t* buf = (uint8_t*)arg2;
            uint32_t buf_size = (uint32_t)arg3;
            
            fat12_file_t file;
            if (fat12_open(fname, &file) != 0) {
                result = 0;
                break;
            }
            
            uint32_t to_read = file.size < buf_size ? file.size : buf_size;
            int bytes = fat12_read(&file, buf, to_read);
            result = (uint64_t)(bytes > 0 ? bytes : 0);
            break;
        }
        
        // File create syscall - creates an empty file
        // arg1 = filename
        // Returns 0 on success, -1 on error
        case SYSCALL_CREATE_FILE: {
            const char* fname = (const char*)arg1;
            fat12_file_t file;
            result = (uint64_t)(int64_t)fat12_create(fname, &file);
            break;
        }
        
        // File write syscall - writes buffer to existing file
        // arg1 = filename, arg2 = buffer, arg3 = size
        // Returns bytes written, or 0 on error
        case SYSCALL_WRITE_FILE: {
            const char* fname = (const char*)arg1;
            const uint8_t* buf = (const uint8_t*)arg2;
            uint32_t size = (uint32_t)arg3;
            
            fat12_file_t file;
            if (fat12_open(fname, &file) != 0) {
                // File doesn't exist - try to create it
                if (fat12_create(fname, &file) != 0) {
                    result = 0;
                    break;
                }
            }
            
            int bytes = fat12_write(&file, buf, size);
            if (bytes > 0) {
                // Update directory entry with new file size
                if (fat12_update_size(fname, (uint32_t)bytes) != 0) {
                    result = 0;
                    break;
                }
            }
            result = (uint64_t)(bytes > 0 ? bytes : 0);
            break;
        }
        
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
            
            // Read first sector to check for ELF magic number (0x7F 'E' 'L' 'F')
            // Must be 512 bytes because fdc_read_sectors copies full sectors
            uint8_t header[512];
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
        
        // VGA buffer save/restore syscalls
        case SYSCALL_SAVE_VGA: {
            uint16_t* vga = (uint16_t*)0xB8000;
            for (int i = 0; i < 2000; i++) {
                vga_backup[i] = vga[i];
            }
            // Save cursor position
            get_cursor_pos(&saved_cursor_x, &saved_cursor_y);
            result = 0;
            break;
        }
        
        case SYSCALL_RESTORE_VGA: {
            uint16_t* vga = (uint16_t*)0xB8000;
            for (int i = 0; i < 2000; i++) {
                vga[i] = vga_backup[i];
            }
            // Restore cursor position
            vga_set_cursor_pos(saved_cursor_x, saved_cursor_y);
            result = 0;
            break;
        }
            
        default:
            result = (uint64_t)-1;  // Invalid syscall
            break;
    }
    
    return result;
}
