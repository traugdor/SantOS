#include "../include/ata.h"

// ATA PIO ports (Primary bus)
#define ATA_PRIMARY_DATA        0x1F0
#define ATA_PRIMARY_ERROR       0x1F1
#define ATA_PRIMARY_SECTOR_COUNT 0x1F2
#define ATA_PRIMARY_LBA_LOW     0x1F3
#define ATA_PRIMARY_LBA_MID     0x1F4
#define ATA_PRIMARY_LBA_HIGH    0x1F5
#define ATA_PRIMARY_DRIVE       0x1F6
#define ATA_PRIMARY_STATUS      0x1F7
#define ATA_PRIMARY_COMMAND     0x1F7

// ATA Commands
#define ATA_CMD_READ_SECTORS    0x20
#define ATA_CMD_WRITE_SECTORS   0x30

// Status bits
#define ATA_STATUS_BSY  0x80  // Busy
#define ATA_STATUS_DRQ  0x08  // Data request ready
#define ATA_STATUS_ERR  0x01  // Error

// I/O functions
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

// Wait for drive to be ready
static void ata_wait_ready(void) {
    while (inb(ATA_PRIMARY_STATUS) & ATA_STATUS_BSY);
}

// Wait for data to be ready
static void ata_wait_drq(void) {
    while (!(inb(ATA_PRIMARY_STATUS) & ATA_STATUS_DRQ));
}

// Initialize ATA
void ata_init(void) {
    // Select master drive (drive 0)
    outb(ATA_PRIMARY_DRIVE, 0xA0);
    ata_wait_ready();
}

// Read sectors from disk
int ata_read_sectors(uint32_t lba, uint8_t sector_count, uint8_t* buffer) {
    if (sector_count == 0) return -1;
    
    ata_wait_ready();
    
    // Select drive and set LBA mode
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    
    // Send sector count and LBA
    outb(ATA_PRIMARY_SECTOR_COUNT, sector_count);
    outb(ATA_PRIMARY_LBA_LOW, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HIGH, (uint8_t)(lba >> 16));
    
    // Send read command
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_SECTORS);
    
    // Read data
    for (int i = 0; i < sector_count; i++) {
        ata_wait_drq();
        
        // Check for errors
        if (inb(ATA_PRIMARY_STATUS) & ATA_STATUS_ERR) {
            return -1;
        }
        
        // Read 256 words (512 bytes) per sector
        uint16_t* buf16 = (uint16_t*)(buffer + i * 512);
        for (int j = 0; j < 256; j++) {
            buf16[j] = inw(ATA_PRIMARY_DATA);
        }
    }
    
    return 0;
}

// Write sectors to disk
int ata_write_sectors(uint32_t lba, uint8_t sector_count, const uint8_t* buffer) {
    if (sector_count == 0) return -1;
    
    ata_wait_ready();
    
    // Select drive and set LBA mode
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    
    // Send sector count and LBA
    outb(ATA_PRIMARY_SECTOR_COUNT, sector_count);
    outb(ATA_PRIMARY_LBA_LOW, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HIGH, (uint8_t)(lba >> 16));
    
    // Send write command
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_SECTORS);
    
    // Write data
    for (int i = 0; i < sector_count; i++) {
        ata_wait_drq();
        
        // Check for errors
        if (inb(ATA_PRIMARY_STATUS) & ATA_STATUS_ERR) {
            return -1;
        }
        
        // Write 256 words (512 bytes) per sector
        const uint16_t* buf16 = (const uint16_t*)(buffer + i * 512);
        for (int j = 0; j < 256; j++) {
            outw(ATA_PRIMARY_DATA, buf16[j]);
        }
    }
    
    return 0;
}
