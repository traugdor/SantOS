#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

// FAT32 filesystem driver

// Initialize FAT32 filesystem
int fat32_init(void);

// File operations
typedef struct {
    char name[12];          // 8.3 filename
    uint32_t size;          // File size in bytes
    uint32_t first_cluster; // First cluster number
    uint8_t is_directory;   // 1 if directory, 0 if file
} fat32_file_t;

// Open a file (returns 0 on success, -1 on error)
int fat32_open(const char* filename, fat32_file_t* file);

// Read from a file
int fat32_read(fat32_file_t* file, uint8_t* buffer, uint32_t size);

// List files in root directory
int fat32_list_root(void);

#endif
