#ifndef FAT12_H
#define FAT12_H

#include <stdint.h>

// FAT12 filesystem driver

// Initialize FAT12 filesystem
int fat12_init(void);

// File operations
typedef struct {
    char name[12];          // 8.3 filename
    uint32_t size;          // File size in bytes
    uint32_t first_cluster; // First cluster number
    uint8_t is_directory;   // 1 if directory, 0 if file
} fat12_file_t;

// Open a file (returns 0 on success, -1 on error)
int fat12_open(const char* filename, fat12_file_t* file);

// Read from a file
int fat12_read(fat12_file_t* file, uint8_t* buffer, uint32_t size);

// List files in root directory
int fat12_list_root(void);

#endif
