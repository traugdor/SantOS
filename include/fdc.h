#ifndef FDC_H
#define FDC_H

#include <stdint.h>

// Floppy Disk Controller driver for 1.44MB floppy disks

// Initialize the FDC
void fdc_init(void);

// Read sectors from floppy disk
// lba: Logical block address (sector number)
// count: Number of sectors to read
// buffer: Destination buffer (must be at least count * 512 bytes)
// Returns: 0 on success, -1 on error
int fdc_read_sectors(uint32_t lba, uint8_t count, uint8_t* buffer);

// Write sectors to floppy disk
// lba: Logical block address (sector number)
// count: Number of sectors to write
// buffer: Source buffer (must be at least count * 512 bytes)
// Returns: 0 on success, -1 on error
int fdc_write_sectors(uint32_t lba, uint8_t count, const uint8_t* buffer);

#endif
