#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>

// FAT16 filesystem driver

// Initialize FAT16 filesystem
int fat16_init(void);

// File operations
typedef struct {
    char name[12];          // 8.3 filename
    uint32_t size;          // File size in bytes
    uint32_t first_cluster; // First cluster number
    uint8_t is_directory;   // 1 if directory, 0 if file
} fat16_file_t;

// Open a file (returns 0 on success, -1 on error)
int fat16_open(const char* filename, fat16_file_t* file);

// Read from a file
int fat16_read(fat16_file_t* file, uint8_t* buffer, uint32_t size);

// List files in root directory
int fat16_list_root(void);

#endif
