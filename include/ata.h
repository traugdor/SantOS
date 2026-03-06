#ifndef ATA_H
#define ATA_H

#include <stdint.h>

// Detect if ATA drive is available
// Returns: 1 if ATA drive detected, 0 if not available
int ata_detect(void);

// ATA disk driver - PIO mode (Programmed I/O)
int ata_init(void);
int ata_read_sectors(uint32_t lba, uint8_t sector_count, uint8_t* buffer);
int ata_write_sectors(uint32_t lba, uint8_t sector_count, const uint8_t* buffer);

#endif
