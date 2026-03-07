#include "../include/loader.h"
#include "../include/fat12.h"
#include "../include/printf.h"
#include "../include/heap.h"
#include <stdint.h>

// ELF64 header structure (minimal fields we need)
typedef struct {
    uint8_t  e_ident[16];     // ELF identification
    uint16_t e_type;          // Object file type
    uint16_t e_machine;       // Machine type
    uint32_t e_version;       // Object file version
    uint64_t e_entry;         // Entry point address
    uint64_t e_phoff;         // Program header offset
    uint64_t e_shoff;         // Section header offset
    uint32_t e_flags;         // Processor-specific flags
    uint16_t e_ehsize;        // ELF header size
    uint16_t e_phentsize;     // Program header entry size
    uint16_t e_phnum;         // Number of program header entries
    uint16_t e_shentsize;     // Section header entry size
    uint16_t e_shnum;         // Number of section header entries
    uint16_t e_shstrndx;      // Section name string table index
} __attribute__((packed)) elf64_header_t;

// ELF64 program header
typedef struct {
    uint32_t p_type;          // Segment type
    uint32_t p_flags;         // Segment flags
    uint64_t p_offset;        // Offset in file
    uint64_t p_vaddr;         // Virtual address
    uint64_t p_paddr;         // Physical address
    uint64_t p_filesz;        // Size in file
    uint64_t p_memsz;         // Size in memory
    uint64_t p_align;         // Alignment
} __attribute__((packed)) elf64_program_header_t;

#define PT_LOAD 1  // Loadable segment

// Load a program from disk into memory
// Returns entry point address on success, 0 on failure
uint64_t load_program(const char* filename, void* load_addr) {
    fat12_file_t file;
    
    printf("Loading program: %s\n", filename);
    
    // Open the file
    if (fat12_open(filename, &file) != 0) {
        printf("ERROR: Failed to open %s\n", filename);
        return 0;
    }
    
    printf("  File size: %d bytes\n", file.size);
    
    // Read the entire ELF file into a temporary buffer
    uint8_t* elf_buffer = (uint8_t*)malloc(file.size);
    if (!elf_buffer) {
        printf("ERROR: Failed to allocate memory for ELF file\n");
        return 0;
    }
    
    int bytes_read = fat12_read(&file, elf_buffer, file.size);
    if (bytes_read < 0 || (uint32_t)bytes_read != file.size) {
        printf("ERROR: Failed to read %s (read %d of %d bytes)\n", filename, bytes_read, file.size);
        free(elf_buffer);
        return 0;
    }
    
    // Parse ELF header
    elf64_header_t* elf_header = (elf64_header_t*)elf_buffer;
    
    // Verify ELF magic number
    if (elf_header->e_ident[0] != 0x7F || 
        elf_header->e_ident[1] != 'E' || 
        elf_header->e_ident[2] != 'L' || 
        elf_header->e_ident[3] != 'F') {
        printf("ERROR: Not a valid ELF file\n");
        free(elf_buffer);
        return 0;
    }
    
    //printf("  Entry point: 0x%x\n", (uint32_t)elf_header->e_entry);
    //printf("  Program headers: %d\n", elf_header->e_phnum);
    
    // Load each PT_LOAD segment at its virtual address
    elf64_program_header_t* ph = (elf64_program_header_t*)(elf_buffer + elf_header->e_phoff);
    for (int i = 0; i < elf_header->e_phnum; i++) {
        if (ph[i].p_type == PT_LOAD) {
            //printf("  Loading segment %d: vaddr=0x%x size=%d\n", 
            //       i, (uint32_t)ph[i].p_vaddr, (uint32_t)ph[i].p_filesz);
            
            // Copy segment from ELF file to its virtual address
            uint8_t* dest = (uint8_t*)ph[i].p_vaddr;
            uint8_t* src = elf_buffer + ph[i].p_offset;
            
            for (uint64_t j = 0; j < ph[i].p_filesz; j++) {
                dest[j] = src[j];
            }
            
            // Zero out any remaining memory (p_memsz > p_filesz for BSS)
            for (uint64_t j = ph[i].p_filesz; j < ph[i].p_memsz; j++) {
                dest[j] = 0;
            }
        }
    }
    
    uint64_t entry_point = elf_header->e_entry;
    free(elf_buffer);
    
    //printf("  Program loaded successfully!\n");
    return entry_point;
}

// Execute a loaded program (only used by kernel to launch initial program like shell)
void execute_program(uint64_t entry_point, const char* program_name, int kernel_mode) {
    if (program_name) {
        printf("Executing program: %s\n", program_name);
    } else {
        printf("Executing program...\n");
    }
    
    // Cast entry point to function pointer and call it
    void (*program_main)(void) = (void (*)(void))entry_point;
    program_main();
    
    // Program returned
    if (kernel_mode) {
        if (program_name) {
            printf("\n%s exited. Halting.\n", program_name);
        } else {
            printf("\nProgram exited. Halting.\n");
        }
        __asm__ volatile("cli; hlt");
    }
}

