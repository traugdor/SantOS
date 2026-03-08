#include "../include/fat12.h"
#include "../include/fdc.h"
#include "../include/stdio.h"
#include "../include/string.h"

// FAT12 Boot Sector structure
typedef struct __attribute__((packed)) {
    uint8_t  jump[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} fat12_boot_sector_t;

// FAT12 Directory Entry
typedef struct __attribute__((packed)) {
    char     filename[8];
    char     extension[3];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} fat12_dir_entry_t;

// Filesystem info
static fat12_boot_sector_t boot_sector;
static uint32_t fat_start_sector;
static uint32_t root_dir_start_sector;
static uint32_t data_start_sector;
static uint8_t fat_buffer[512 * 9]; // FAT12 typically uses 9 sectors for FAT

// Read FAT12 entry (12-bit values packed in bytes)
static uint16_t get_fat_entry(uint16_t cluster) {
    // FAT12 entries are 12 bits, so 2 entries per 3 bytes
    uint32_t fat_offset = cluster + (cluster / 2); // cluster * 1.5
    uint16_t entry;
    
    // Read the 16-bit value containing our 12-bit entry
    entry = *(uint16_t*)&fat_buffer[fat_offset];
    
    // Extract the 12-bit value
    if (cluster & 1) {
        entry >>= 4; // Odd cluster: high 12 bits
    } else {
        entry &= 0x0FFF; // Even cluster: low 12 bits
    }
    
    return entry;
}

// Write FAT12 entry (12-bit values packed in bytes)
static void set_fat_entry(uint16_t cluster, uint16_t value) {
    uint32_t fat_offset = cluster + (cluster / 2);
    uint16_t* entry_ptr = (uint16_t*)&fat_buffer[fat_offset];
    
    if (cluster & 1) {
        // Odd cluster: high 12 bits
        *entry_ptr = (*entry_ptr & 0x000F) | (value << 4);
    } else {
        // Even cluster: low 12 bits
        *entry_ptr = (*entry_ptr & 0xF000) | (value & 0x0FFF);
    }
}

// Find free cluster in FAT
static uint16_t find_free_cluster(void) {
    // Start from cluster 2 (0 and 1 are reserved)
    for (uint16_t cluster = 2; cluster < 0xFF0; cluster++) {
        if (get_fat_entry(cluster) == 0) {
            return cluster;
        }
    }
    return 0; // No free clusters
}

// Write FAT table back to disk
static int write_fat_table(void) {
    for (int i = 0; i < boot_sector.sectors_per_fat; i++) {
        if (fdc_write_sectors(fat_start_sector + i, 1, fat_buffer + (i * 512)) != 0) {
            return -1;
        }
    }
    return 0;
}

// Update directory entry file size
int fat12_update_size(const char* filename, uint32_t new_size) {
    uint8_t buffer[512];
    uint32_t entries_per_sector = 512 / sizeof(fat12_dir_entry_t);
    uint32_t total_sectors = ((boot_sector.root_entries * 32) + 511) / 512;
    
    // Convert filename to 8.3 format (uppercase)
    char name[9] = "        ";
    char ext[4] = "   ";
    int i = 0, j = 0;
    
    while (filename[i] && filename[i] != '.' && j < 8) {
        char c = filename[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        name[j++] = c;
    }
    
    if (filename[i] == '.') {
        i++;
        j = 0;
        while (filename[i] && j < 3) {
            char c = filename[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            ext[j++] = c;
        }
    }
    
    // Search for file in root directory
    for (uint32_t sector = 0; sector < total_sectors; sector++) {
        if (fdc_read_sectors(root_dir_start_sector + sector, 1, buffer) != 0) {
            return -1;
        }
        
        fat12_dir_entry_t* entries = (fat12_dir_entry_t*)buffer;
        
        for (uint32_t idx = 0; idx < entries_per_sector; idx++) {
            if (entries[idx].filename[0] == 0x00) {
                return -1; // File not found
            }
            
            if ((uint8_t)entries[idx].filename[0] == 0xE5) {
                continue;
            }
            
            // Compare filename
            int match = 1;
            for (int k = 0; k < 8; k++) {
                if (name[k] != entries[idx].filename[k]) {
                    match = 0;
                    break;
                }
            }
            for (int k = 0; k < 3 && match; k++) {
                if (ext[k] != entries[idx].extension[k]) {
                    match = 0;
                    break;
                }
            }
            
            if (match) {
                // Update file size
                entries[idx].file_size = new_size;
                
                // Write directory entry back
                if (fdc_write_sectors(root_dir_start_sector + sector, 1, buffer) != 0) {
                    return -1;
                }
                
                return 0;
            }
        }
    }
    
    return -1; // File not found
}

// Initialize FAT12
int fat12_init(void) {
    // Read boot sector (LBA 0)
    uint8_t buffer[512];
    if (fdc_read_sectors(0, 1, buffer) != 0) {
        printf("Error: Failed to read boot sector\n");
        return -1;
    }
    
    // Copy boot sector
    for (int i = 0; i < sizeof(fat12_boot_sector_t); i++) {
        ((uint8_t*)&boot_sector)[i] = buffer[i];
    }
    
    // Debug: Show fs_type field
    // printf("Debug: fs_type = '%.8s'\n", boot_sector.fs_type);
    // printf("Debug: OEM = '%.8s'\n", boot_sector.oem_name);
    
    // Verify it's FAT (check is optional since we know it's FAT12)
    // Some FAT12 implementations don't set fs_type correctly
    if (boot_sector.bytes_per_sector == 0 || boot_sector.sectors_per_cluster == 0) {
        printf("Error: Invalid boot sector\n");
        return -1;
    }
    
    // Calculate important sectors
    fat_start_sector = boot_sector.reserved_sectors;
    root_dir_start_sector = fat_start_sector + (boot_sector.num_fats * boot_sector.sectors_per_fat);
    uint32_t root_dir_sectors = ((boot_sector.root_entries * 32) + (boot_sector.bytes_per_sector - 1)) / boot_sector.bytes_per_sector;
    data_start_sector = root_dir_start_sector + root_dir_sectors;
    
    // Read FAT table into memory
    for (int i = 0; i < boot_sector.sectors_per_fat; i++) {
        if (fdc_read_sectors(fat_start_sector + i, 1, fat_buffer + (i * 512)) != 0) {
            printf("Error: Failed to read FAT table\n");
            return -1;
        }
    }
    
    // printf("FAT12 initialized:\n");
    // printf("  Bytes per sector: %d\n", boot_sector.bytes_per_sector);
    // printf("  Sectors per cluster: %d\n", boot_sector.sectors_per_cluster);
    // printf("  Root entries: %d\n", boot_sector.root_entries);
    // printf("  FAT start: %d\n", fat_start_sector);
    // printf("  Root dir start: %d\n", root_dir_start_sector);
    // printf("  Data start: %d\n\n", data_start_sector);
    
    return 0;
}

// Convert 8.3 filename to normal format
static void format_filename(const fat12_dir_entry_t* entry, char* output) {
    int i, j = 0;
    
    // Copy filename (trim spaces)
    for (i = 0; i < 8 && entry->filename[i] != ' '; i++) {
        output[j++] = entry->filename[i];
    }
    
    // Add extension if present
    if (entry->extension[0] != ' ') {
        output[j++] = '.';
        for (i = 0; i < 3 && entry->extension[i] != ' '; i++) {
            output[j++] = entry->extension[i];
        }
    }
    
    output[j] = '\0';
}

// List files in root directory
int fat12_list_root(void) {
    uint8_t buffer[512];
    uint32_t entries_per_sector = 512 / sizeof(fat12_dir_entry_t);
    uint32_t total_sectors = ((boot_sector.root_entries * 32) + 511) / 512;
    
    printf("Root directory:\n");
    printf("%-12s %10s\n", "Name", "Size");
    printf("------------------------\n");
    
    for (uint32_t sector = 0; sector < total_sectors; sector++) {
        if (fdc_read_sectors(root_dir_start_sector + sector, 1, buffer) != 0) {
            return -1;
        }
        
        fat12_dir_entry_t* entries = (fat12_dir_entry_t*)buffer;
        
        for (uint32_t i = 0; i < entries_per_sector; i++) {
            // End of directory
            if (entries[i].filename[0] == 0x00) {
                return 0;
            }
            
            // Deleted file
            if (entries[i].filename[0] == 0xE5) {
                continue;
            }
            
            // Skip volume label and long filenames
            if (entries[i].attributes & 0x08 || entries[i].attributes == 0x0F) {
                continue;
            }
            
            char name[13];
            format_filename(&entries[i], name);
            
            if (entries[i].attributes & 0x10) {
                printf("%-12s %10s\n", name, "<DIR>");
            } else {
                printf("%-12s %10d\n", name, entries[i].file_size);
            }
        }
    }
    
    return 0;
}

// List files in a specific directory by cluster
int fat12_list_dir(uint16_t cluster) {
    uint8_t buffer[512];
    
    // If cluster is 0, list root directory
    if (cluster == 0) {
        return fat12_list_root();
    }
    
    printf("Directory contents:\n");
    printf("%-12s %10s\n", "Name", "Size");
    printf("------------------------\n");
    
    // Read directory clusters
    while (cluster >= 2 && cluster < 0xFF8) {
        uint32_t sector = data_start_sector + ((cluster - 2) * boot_sector.sectors_per_cluster);
        
        for (int s = 0; s < boot_sector.sectors_per_cluster; s++) {
            if (fdc_read_sectors(sector + s, 1, buffer) != 0) {
                return -1;
            }
            
            fat12_dir_entry_t* entries = (fat12_dir_entry_t*)buffer;
            uint32_t entries_per_sector = 512 / sizeof(fat12_dir_entry_t);
            
            for (uint32_t i = 0; i < entries_per_sector; i++) {
                // End of directory
                if (entries[i].filename[0] == 0x00) {
                    return 0;
                }
                
                // Deleted file
                if (entries[i].filename[0] == 0xE5) {
                    continue;
                }
                
                // Skip volume label and long filenames
                if (entries[i].attributes & 0x08 || entries[i].attributes == 0x0F) {
                    continue;
                }
                
                // Skip . and .. entries in display (but they exist)
                if (entries[i].filename[0] == '.') {
                    continue;
                }
                
                char name[13];
                format_filename(&entries[i], name);
                
                if (entries[i].attributes & 0x10) {
                    printf("%-12s %10s\n", name, "<DIR>");
                } else {
                    printf("%-12s %10d\n", name, entries[i].file_size);
                }
            }
        }
        
        // Get next cluster
        cluster = get_fat_entry(cluster);
    }
    
    return 0;
}

// Find a directory entry by name in current directory (0 = root)
uint16_t fat12_find_entry(uint16_t dir_cluster, const char* name, int* is_directory) {
    uint8_t buffer[512];
    
    // Convert filename to 8.3 format (uppercase)
    char fname[9] = "        ";
    char ext[4] = "   ";
    int i = 0, j = 0;
    
    while (name[i] && name[i] != '.' && j < 8) {
        char c = name[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        fname[j++] = c;
    }
    
    if (name[i] == '.') {
        i++;
        j = 0;
        while (name[i] && j < 3) {
            char c = name[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            ext[j++] = c;
        }
    }
    
    // Search in root directory
    if (dir_cluster == 0) {
        uint32_t entries_per_sector = 512 / sizeof(fat12_dir_entry_t);
        uint32_t total_sectors = ((boot_sector.root_entries * 32) + 511) / 512;
        
        for (uint32_t sector = 0; sector < total_sectors; sector++) {
            if (fdc_read_sectors(root_dir_start_sector + sector, 1, buffer) != 0) {
                return 0;
            }
            
            fat12_dir_entry_t* entries = (fat12_dir_entry_t*)buffer;
            
            for (uint32_t idx = 0; idx < entries_per_sector; idx++) {
                if (entries[idx].filename[0] == 0x00) {
                    return 0;
                }
                
                if (entries[idx].filename[0] == 0xE5) {
                    continue;
                }
                
                // Compare filename
                int match = 1;
                for (int k = 0; k < 8; k++) {
                    if (fname[k] != entries[idx].filename[k]) {
                        match = 0;
                        break;
                    }
                }
                for (int k = 0; k < 3 && match; k++) {
                    if (ext[k] != entries[idx].extension[k]) {
                        match = 0;
                        break;
                    }
                }
                
                if (match) {
                    *is_directory = (entries[idx].attributes & 0x10) ? 1 : 0;
                    return entries[idx].first_cluster_low;
                }
            }
        }
    } else {
        // Search in subdirectory
        while (dir_cluster >= 2 && dir_cluster < 0xFF8) {
            uint32_t sector = data_start_sector + ((dir_cluster - 2) * boot_sector.sectors_per_cluster);
            
            for (int s = 0; s < boot_sector.sectors_per_cluster; s++) {
                if (fdc_read_sectors(sector + s, 1, buffer) != 0) {
                    return 0;
                }
                
                fat12_dir_entry_t* entries = (fat12_dir_entry_t*)buffer;
                uint32_t entries_per_sector = 512 / sizeof(fat12_dir_entry_t);
                
                for (uint32_t idx = 0; idx < entries_per_sector; idx++) {
                    if (entries[idx].filename[0] == 0x00) {
                        return 0;
                    }
                    
                    if (entries[idx].filename[0] == 0xE5) {
                        continue;
                    }
                    
                    // Compare filename
                    int match = 1;
                    for (int k = 0; k < 8; k++) {
                        if (fname[k] != entries[idx].filename[k]) {
                            match = 0;
                            break;
                        }
                    }
                    for (int k = 0; k < 3 && match; k++) {
                        if (ext[k] != entries[idx].extension[k]) {
                            match = 0;
                            break;
                        }
                    }
                    
                    if (match) {
                        *is_directory = (entries[idx].attributes & 0x10) ? 1 : 0;
                        return entries[idx].first_cluster_low;
                    }
                }
            }
            
            dir_cluster = get_fat_entry(dir_cluster);
        }
    }
    
    return 0;
}

// Open a file
int fat12_open(const char* filename, fat12_file_t* file) {
    uint8_t buffer[512];
    uint32_t entries_per_sector = 512 / sizeof(fat12_dir_entry_t);
    uint32_t total_sectors = ((boot_sector.root_entries * 32) + 511) / 512;
    
    // Convert filename to 8.3 format (uppercase)
    char name[9] = "        ";
    char ext[4] = "   ";
    int i = 0, j = 0;
    
    while (filename[i] && filename[i] != '.' && j < 8) {
        char c = filename[i++];
        if (c >= 'a' && c <= 'z') c -= 32; // Convert to uppercase
        name[j++] = c;
    }
    
    if (filename[i] == '.') {
        i++;
        j = 0;
        while (filename[i] && j < 3) {
            char c = filename[i++];
            if (c >= 'a' && c <= 'z') c -= 32; // Convert to uppercase
            ext[j++] = c;
        }
    }
    
    // Search root directory
    for (uint32_t sector = 0; sector < total_sectors; sector++) {
        if (fdc_read_sectors(root_dir_start_sector + sector, 1, buffer) != 0) {
            return -1;
        }
        
        fat12_dir_entry_t* entries = (fat12_dir_entry_t*)buffer;
        
        for (uint32_t i = 0; i < entries_per_sector; i++) {
            if (entries[i].filename[0] == 0x00) {
                return -1; // File not found
            }
            
            if (entries[i].filename[0] == 0xE5) {
                continue;
            }
            
            // Compare filename
            int match = 1;
            for (int k = 0; k < 8; k++) {
                if (name[k] != entries[i].filename[k]) {
                    match = 0;
                    break;
                }
            }
            for (int k = 0; k < 3 && match; k++) {
                if (ext[k] != entries[i].extension[k]) {
                    match = 0;
                    break;
                }
            }
            
            if (match) {
                // Found the file!
                format_filename(&entries[i], file->name);
                file->size = entries[i].file_size;
                file->first_cluster = entries[i].first_cluster_low;
                file->is_directory = (entries[i].attributes & 0x10) ? 1 : 0;
                return 0;
            }
        }
    }
    
    return -1; // File not found
}

// Read from a file
int fat12_read(fat12_file_t* file, uint8_t* buffer, uint32_t size) {
    if (size > file->size) {
        size = file->size;
    }
    
    uint32_t bytes_read = 0;
    uint16_t cluster = file->first_cluster;
    
    while (bytes_read < size && cluster >= 2 && cluster < 0xFF8) {
        // Calculate sector from cluster
        uint32_t sector = data_start_sector + ((cluster - 2) * boot_sector.sectors_per_cluster);
        
        // Read entire cluster at once (1 cluster = sectors_per_cluster sectors)
        uint8_t cluster_buffer[512 * 2]; // FAT12 clusters are typically 1-2 sectors
        if (fdc_read_sectors(sector, boot_sector.sectors_per_cluster, cluster_buffer) != 0) {
            return -1;
        }
        
        // Copy data from cluster buffer to output buffer
        uint32_t cluster_size = boot_sector.sectors_per_cluster * 512;
        uint32_t to_copy = (size - bytes_read > cluster_size) ? cluster_size : (size - bytes_read);
        for (uint32_t j = 0; j < to_copy; j++) {
            buffer[bytes_read++] = cluster_buffer[j];
        }
        
        // Get next cluster from FAT
        cluster = get_fat_entry(cluster);
    }
    
    return bytes_read;
}

// Write to a file
int fat12_write(fat12_file_t* file, const uint8_t* buffer, uint32_t size) {
    uint32_t bytes_written = 0;
    uint16_t cluster = file->first_cluster;
    uint8_t sector_buffer[512];
    
    while (bytes_written < size && cluster >= 2 && cluster < 0xFF8) {
        // Calculate sector from cluster
        uint32_t sector = data_start_sector + ((cluster - 2) * boot_sector.sectors_per_cluster);
        
        // Write cluster
        for (int i = 0; i < boot_sector.sectors_per_cluster && bytes_written < size; i++) {
            // Fill sector buffer
            uint32_t to_write = (size - bytes_written > 512) ? 512 : (size - bytes_written);
            for (uint32_t j = 0; j < to_write; j++) {
                sector_buffer[j] = buffer[bytes_written++];
            }
            
            // Pad rest of sector with zeros if needed
            for (uint32_t j = to_write; j < 512; j++) {
                sector_buffer[j] = 0;
            }
            
            // Write sector
            if (fdc_write_sectors(sector + i, 1, sector_buffer) != 0) {
                return -1;
            }
        }
        
        // Get next cluster or allocate new one if needed
        uint16_t next_cluster = get_fat_entry(cluster);
        if (bytes_written < size && (next_cluster >= 0xFF8 || next_cluster == 0)) {
            // Need to allocate new cluster
            next_cluster = find_free_cluster();
            if (next_cluster == 0) {
                return -1; // Disk full
            }
            
            // Link clusters
            set_fat_entry(cluster, next_cluster);
            set_fat_entry(next_cluster, 0xFFF); // Mark as end of chain
            
            // Write FAT table
            if (write_fat_table() != 0) {
                return -1;
            }
        }
        
        cluster = next_cluster;
    }
    
    return bytes_written;
}

// Create a new file
int fat12_create(const char* filename, fat12_file_t* file) {
    uint8_t buffer[512];
    uint32_t entries_per_sector = 512 / sizeof(fat12_dir_entry_t);
    uint32_t total_sectors = ((boot_sector.root_entries * 32) + 511) / 512;
    
    // Convert filename to 8.3 format (uppercase)
    char name[9] = "        ";
    char ext[4] = "   ";
    int i = 0, j = 0;
    
    while (filename[i] && filename[i] != '.' && j < 8) {
        char c = filename[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        name[j++] = c;
    }
    
    if (filename[i] == '.') {
        i++;
        j = 0;
        while (filename[i] && j < 3) {
            char c = filename[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            ext[j++] = c;
        }
    }
    
    // Find free directory entry
    for (uint32_t sector = 0; sector < total_sectors; sector++) {
        if (fdc_read_sectors(root_dir_start_sector + sector, 1, buffer) != 0) {
            return -1;
        }
        
        fat12_dir_entry_t* entries = (fat12_dir_entry_t*)buffer;
        
        for (uint32_t idx = 0; idx < entries_per_sector; idx++) {
            // Found free entry (0x00 or 0xE5)
            if (entries[idx].filename[0] == 0x00 || (uint8_t)entries[idx].filename[0] == 0xE5) {
                // Allocate first cluster
                uint16_t first_cluster = find_free_cluster();
                if (first_cluster == 0) {
                    return -1; // Disk full
                }
                
                // Mark cluster as end of chain
                set_fat_entry(first_cluster, 0xFFF);
                
                // Write FAT table
                if (write_fat_table() != 0) {
                    return -1;
                }
                
                // Create directory entry
                for (int k = 0; k < 8; k++) entries[idx].filename[k] = name[k];
                for (int k = 0; k < 3; k++) entries[idx].extension[k] = ext[k];
                entries[idx].attributes = 0x20; // Archive attribute
                entries[idx].reserved = 0;
                entries[idx].first_cluster_high = 0;
                entries[idx].first_cluster_low = first_cluster;
                entries[idx].file_size = 0;
                
                // Write directory entry back
                if (fdc_write_sectors(root_dir_start_sector + sector, 1, buffer) != 0) {
                    return -1;
                }
                
                // Fill file structure
                for (int k = 0; k < 8; k++) file->name[k] = name[k];
                file->size = 0;
                file->first_cluster = first_cluster;
                file->is_directory = 0;
                
                return 0;
            }
        }
    }
    
    return -1; // No free directory entries
}

// Delete a file
int fat12_delete(const char* filename) {
    uint8_t buffer[512];
    uint32_t entries_per_sector = 512 / sizeof(fat12_dir_entry_t);
    uint32_t total_sectors = ((boot_sector.root_entries * 32) + 511) / 512;
    
    // Convert filename to 8.3 format (uppercase)
    char name[9] = "        ";
    char ext[4] = "   ";
    int i = 0, j = 0;
    
    while (filename[i] && filename[i] != '.' && j < 8) {
        char c = filename[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        name[j++] = c;
    }
    
    if (filename[i] == '.') {
        i++;
        j = 0;
        while (filename[i] && j < 3) {
            char c = filename[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            ext[j++] = c;
        }
    }
    
    // Search for file in root directory
    for (uint32_t sector = 0; sector < total_sectors; sector++) {
        if (fdc_read_sectors(root_dir_start_sector + sector, 1, buffer) != 0) {
            return -1;
        }
        
        fat12_dir_entry_t* entries = (fat12_dir_entry_t*)buffer;
        
        for (uint32_t idx = 0; idx < entries_per_sector; idx++) {
            if (entries[idx].filename[0] == 0x00) {
                return -1; // File not found
            }
            
            if ((uint8_t)entries[idx].filename[0] == 0xE5) {
                continue;
            }
            
            // Compare filename
            int match = 1;
            for (int k = 0; k < 8; k++) {
                if (name[k] != entries[idx].filename[k]) {
                    match = 0;
                    break;
                }
            }
            for (int k = 0; k < 3 && match; k++) {
                if (ext[k] != entries[idx].extension[k]) {
                    match = 0;
                    break;
                }
            }
            
            if (match) {
                // Free cluster chain
                uint16_t cluster = entries[idx].first_cluster_low;
                while (cluster >= 2 && cluster < 0xFF8) {
                    uint16_t next = get_fat_entry(cluster);
                    set_fat_entry(cluster, 0); // Mark as free
                    cluster = next;
                }
                
                // Write FAT table
                if (write_fat_table() != 0) {
                    return -1;
                }
                
                // Mark directory entry as deleted
                entries[idx].filename[0] = (char)0xE5;
                
                // Write directory entry back
                if (fdc_write_sectors(root_dir_start_sector + sector, 1, buffer) != 0) {
                    return -1;
                }
                
                return 0;
            }
        }
    }
    
    return -1; // File not found
}
