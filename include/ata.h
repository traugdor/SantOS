#ifndef ATA_H
#define ATA_H

#include <stdint.h>

// ATA disk driver - PIO mode (Programmed I/O)
void ata_init(void);
int ata_read_sectors(uint32_t lba, uint8_t sector_count, uint8_t* buffer);
int ata_write_sectors(uint32_t lba, uint8_t sector_count, const uint8_t* buffer);

#endif
