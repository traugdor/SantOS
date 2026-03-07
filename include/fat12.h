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

// Write to a file
int fat12_write(fat12_file_t* file, const uint8_t* buffer, uint32_t size);

// Update file size in directory entry (call after writing)
int fat12_update_size(const char* filename, uint32_t new_size);

// Create a new file
int fat12_create(const char* filename, fat12_file_t* file);

// Delete a file
int fat12_delete(const char* filename);

// List files in root directory
int fat12_list_root(void);

// List files in a specific directory by cluster
int fat12_list_dir(uint16_t cluster);

// Find a directory entry by name in current directory (0 = root)
// Returns cluster number of entry, or 0 if not found
// Sets is_directory to 1 if entry is a directory
uint16_t fat12_find_entry(uint16_t dir_cluster, const char* name, int* is_directory);

#endif
