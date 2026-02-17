#include "../include/fat32.h"
#include "../include/ata.h"
#include "../include/stdio.h"
#include "../include/string.h"

// FAT32 Boot Sector structure
typedef struct __attribute__((packed)) {
    uint8_t  jump[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;          // 0 for FAT32
    uint16_t total_sectors_16;      // 0 for FAT32
    uint8_t  media_type;
    uint16_t sectors_per_fat_16;    // 0 for FAT32
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    // FAT32 extended fields
    uint32_t sectors_per_fat_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} fat32_boot_sector_t;

// FAT32 Directory Entry
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
} fat32_dir_entry_t;

// Filesystem info
static fat32_boot_sector_t boot_sector;
static uint32_t fat_start_sector;
static uint32_t data_start_sector;
static uint32_t root_cluster;

// Read FAT32 entry (32-bit values)
static uint32_t get_fat_entry(uint32_t cluster) {
    // Calculate which sector contains this FAT entry
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_sector + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    // Read the FAT sector
    uint8_t buffer[512];
    if (ata_read_sectors(fat_sector, 1, buffer) != 0) {
        return 0x0FFFFFFF; // Error - return end of chain
    }
    
    // Extract the 32-bit entry (only use low 28 bits)
    uint32_t entry = *(uint32_t*)&buffer[entry_offset];
    return entry & 0x0FFFFFFF;
}

// Initialize FAT32
int fat32_init(void) {
    // Read boot sector (LBA 0)
    uint8_t buffer[512];
    if (ata_read_sectors(0, 1, buffer) != 0) {
        printf("Error: Failed to read boot sector\n");
        return -1;
    }
    
    // Copy boot sector
    for (int i = 0; i < sizeof(fat32_boot_sector_t); i++) {
        ((uint8_t*)&boot_sector)[i] = buffer[i];
    }
    
    // Verify it's FAT32
    if (boot_sector.fs_type[0] != 'F' || boot_sector.fs_type[1] != 'A' || boot_sector.fs_type[2] != 'T') {
        printf("Error: Not a FAT filesystem\n");
        return -1;
    }
    
    // Calculate important sectors
    fat_start_sector = boot_sector.reserved_sectors;
    data_start_sector = fat_start_sector + (boot_sector.num_fats * boot_sector.sectors_per_fat_32);
    root_cluster = boot_sector.root_cluster;
    
    printf("FAT32 initialized:\n");
    printf("  Bytes per sector: %d\n", boot_sector.bytes_per_sector);
    printf("  Sectors per cluster: %d\n", boot_sector.sectors_per_cluster);
    printf("  Root cluster: %d\n", root_cluster);
    printf("  FAT start: %d\n", fat_start_sector);
    printf("  Data start: %d\n\n", data_start_sector);
    
    return 0;
}

// Convert 8.3 filename to normal format
static void format_filename(const fat32_dir_entry_t* entry, char* output) {
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
int fat32_list_root(void) {
    uint8_t buffer[512];
    uint32_t entries_per_sector = 512 / sizeof(fat32_dir_entry_t);
    uint32_t cluster = root_cluster;
    
    printf("Root directory:\n");
    printf("%-12s %10s\n", "Name", "Size");
    printf("------------------------\n");
    
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        // Calculate sector from cluster
        uint32_t sector = data_start_sector + ((cluster - 2) * boot_sector.sectors_per_cluster);
        
        // Read cluster
        for (int s = 0; s < boot_sector.sectors_per_cluster; s++) {
            if (ata_read_sectors(sector + s, 1, buffer) != 0) {
                return -1;
            }
            
            fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buffer;
            
            for (uint32_t i = 0; i < entries_per_sector; i++) {
                // End of directory
                if (entries[i].filename[0] == 0x00) {
                    return 0;
                }
                
                // Deleted file
                if ((uint8_t)entries[i].filename[0] == 0xE5) {
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
        
        // Get next cluster
        cluster = get_fat_entry(cluster);
    }
    
    return 0;
}

// Open a file
int fat32_open(const char* filename, fat32_file_t* file) {
    uint8_t buffer[512];
    uint32_t entries_per_sector = 512 / sizeof(fat32_dir_entry_t);
    uint32_t cluster = root_cluster;
    
    // Convert filename to 8.3 format
    char name[9] = "        ";
    char ext[4] = "   ";
    int i = 0, j = 0;
    
    while (filename[i] && filename[i] != '.' && j < 8) {
        name[j++] = filename[i++];
    }
    
    if (filename[i] == '.') {
        i++;
        j = 0;
        while (filename[i] && j < 3) {
            ext[j++] = filename[i++];
        }
    }
    
    // Search root directory
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        // Calculate sector from cluster
        uint32_t sector = data_start_sector + ((cluster - 2) * boot_sector.sectors_per_cluster);
        
        // Read cluster
        for (int s = 0; s < boot_sector.sectors_per_cluster; s++) {
            if (ata_read_sectors(sector + s, 1, buffer) != 0) {
                return -1;
            }
            
            fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buffer;
            
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
                    // Found the file!
                    format_filename(&entries[idx], file->name);
                    file->size = entries[idx].file_size;
                    file->first_cluster = ((uint32_t)entries[idx].first_cluster_high << 16) | entries[idx].first_cluster_low;
                    file->is_directory = (entries[idx].attributes & 0x10) ? 1 : 0;
                    return 0;
                }
            }
        }
        
        // Get next cluster
        cluster = get_fat_entry(cluster);
    }
    
    return -1; // File not found
}

// Read from a file
int fat32_read(fat32_file_t* file, uint8_t* buffer, uint32_t size) {
    if (size > file->size) {
        size = file->size;
    }
    
    uint32_t bytes_read = 0;
    uint32_t cluster = file->first_cluster;
    uint8_t sector_buffer[512];
    
    while (bytes_read < size && cluster >= 2 && cluster < 0x0FFFFFF8) {
        // Calculate sector from cluster
        uint32_t sector = data_start_sector + ((cluster - 2) * boot_sector.sectors_per_cluster);
        
        // Read cluster
        for (int i = 0; i < boot_sector.sectors_per_cluster && bytes_read < size; i++) {
            if (ata_read_sectors(sector + i, 1, sector_buffer) != 0) {
                return -1;
            }
            
            uint32_t to_copy = (size - bytes_read > 512) ? 512 : (size - bytes_read);
            for (uint32_t j = 0; j < to_copy; j++) {
                buffer[bytes_read++] = sector_buffer[j];
            }
        }
        
        // Get next cluster from FAT
        cluster = get_fat_entry(cluster);
    }
    
    return bytes_read;
}
