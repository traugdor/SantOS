#include "../include/fdc.h"
#include "../include/stdio.h"

// FDC I/O Ports
#define FDC_DOR     0x3F2  // Digital Output Register
#define FDC_MSR     0x3F4  // Main Status Register
#define FDC_FIFO    0x3F5  // Data FIFO
#define FDC_CCR     0x3F7  // Configuration Control Register

// DMA Controller Ports (Channel 2 for FDC)
#define DMA_ADDR_2      0x04  // Channel 2 address
#define DMA_COUNT_2     0x05  // Channel 2 count
#define DMA_PAGE_2      0x81  // Channel 2 page
#define DMA_STATUS      0x08  // DMA status register
#define DMA_COMMAND     0x08  // DMA command register
#define DMA_REQUEST     0x09  // DMA request register
#define DMA_SINGLE_MASK 0x0A  // DMA single channel mask
#define DMA_MODE        0x0B  // DMA mode register
#define DMA_FLIPFLOP    0x0C  // DMA flip-flop reset
#define DMA_MASTER_CLEAR 0x0D // DMA master clear

// FDC Commands
#define FDC_CMD_READ_DATA       0x06
#define FDC_CMD_WRITE_DATA      0x05
#define FDC_CMD_RECALIBRATE     0x07
#define FDC_CMD_SENSE_INTERRUPT 0x08
#define FDC_CMD_SPECIFY         0x03
#define FDC_CMD_SEEK            0x0F

// MSR bits
#define MSR_RQM     0x80  // Request for Master (ready for command)
#define MSR_DIO     0x40  // Data Input/Output (1=read, 0=write)
#define MSR_BUSY    0x10  // FDC is busy

// DOR bits
#define DOR_MOTOR_D 0x80  // Motor D on
#define DOR_MOTOR_C 0x40  // Motor C on
#define DOR_MOTOR_B 0x20  // Motor B on
#define DOR_MOTOR_A 0x10  // Motor A on
#define DOR_IRQ     0x08  // Enable IRQ
#define DOR_RESET   0x04  // Reset (active low)
#define DOR_DRIVE_SEL 0x03 // Drive select mask

// Floppy disk geometry (1.44MB)
#define SECTORS_PER_TRACK 18
#define HEADS 2
#define TRACKS 80

// I/O functions
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// DMA buffer (must be in low memory, below 16MB, must not cross 64KB boundary)
// 0x1000-0x3FFF: Page tables (PML4, PDPT, PD) - ACTIVE, do not touch
// 0x7C00-0x7DFF: Boot sector (no longer needed)
// 0x7E00-0x81FF: fat12.asm (no longer needed)
// 0x9000-0x97FF: boot2.asm + GDT - ACTIVE, do not touch!
// 0x20000+: Old kernel ELF raw data (no longer needed after ELF parsing)
#define DMA_BUFFER_ADDR 0x20000 // Safe: old kernel ELF area, within ISA DMA range
static uint8_t* dma_buffer = (uint8_t*)DMA_BUFFER_ADDR;

// FDC interrupt flag
static volatile int fdc_irq_received = 0;

// Setup DMA for floppy read (supports multi-sector reads)
static void dma_setup_read(uint8_t sector_count) {
    uintptr_t addr = (uintptr_t)dma_buffer;
    uint16_t count = (sector_count * 512) - 1; // Count is length - 1
    
    // Disable DMA channel 2
    outb(DMA_SINGLE_MASK, 0x06); // Mask channel 2
    
    // Reset flip-flop
    outb(DMA_FLIPFLOP, 0xFF);
    
    // Set DMA mode: single transfer, address increment, read from memory to device
    outb(DMA_MODE, 0x46); // Channel 2, read from memory, single mode
    
    // Set address (low byte, high byte)
    outb(DMA_ADDR_2, (uint8_t)(addr & 0xFF));
    outb(DMA_ADDR_2, (uint8_t)((addr >> 8) & 0xFF));
    
    // Set page (bits 16-23 of address)
    outb(DMA_PAGE_2, (uint8_t)((addr >> 16) & 0xFF));
    
    // Set count (low byte, high byte)
    outb(DMA_COUNT_2, (uint8_t)(count & 0xFF));
    outb(DMA_COUNT_2, (uint8_t)((count >> 8) & 0xFF));
    
    // Enable DMA channel 2
    outb(DMA_SINGLE_MASK, 0x02); // Unmask channel 2
}

// Setup DMA for floppy write (supports multi-sector writes)
static void dma_setup_write(uint8_t sector_count) {
    uintptr_t addr = (uintptr_t)dma_buffer;
    uint16_t count = (sector_count * 512) - 1; // Count is length - 1
    
    // Disable DMA channel 2
    outb(DMA_SINGLE_MASK, 0x06); // Mask channel 2
    
    // Reset flip-flop
    outb(DMA_FLIPFLOP, 0xFF);
    
    // Set DMA mode: single transfer, address increment, write to memory from device
    outb(DMA_MODE, 0x4A); // Channel 2, write to memory, single mode
    
    // Set address (low byte, high byte)
    outb(DMA_ADDR_2, (uint8_t)(addr & 0xFF));
    outb(DMA_ADDR_2, (uint8_t)((addr >> 8) & 0xFF));
    
    // Set page (bits 16-23 of address)
    outb(DMA_PAGE_2, (uint8_t)((addr >> 16) & 0xFF));
    
    // Set count (low byte, high byte)
    outb(DMA_COUNT_2, (uint8_t)(count & 0xFF));
    outb(DMA_COUNT_2, (uint8_t)((count >> 8) & 0xFF));
    
    // Enable DMA channel 2
    outb(DMA_SINGLE_MASK, 0x02); // Unmask channel 2
}

// Wait for FDC to be ready
static int fdc_wait_ready(void) {
    uint32_t timeout = 100000;
    while (timeout-- && !(inb(FDC_MSR) & MSR_RQM));
    return (timeout > 0) ? 0 : -1;
}

// Send a byte to FDC
static int fdc_write_byte(uint8_t byte) {
    if (fdc_wait_ready() != 0) return -1;
    outb(FDC_FIFO, byte);
    return 0;
}

// Read a byte from FDC
static int fdc_read_byte(uint8_t* byte) {
    uint32_t timeout = 100000;
    while (timeout-- && !(inb(FDC_MSR) & MSR_RQM));
    if (timeout == 0) return -1;
    *byte = inb(FDC_FIFO);
    return 0;
}

// Motor control
static void fdc_motor_on(void) {
    outb(FDC_DOR, DOR_MOTOR_A | DOR_IRQ | DOR_RESET | 0); // Drive 0
    // Wait for motor to spin up (500ms in real hardware, we'll skip for emulation)
}

static void fdc_motor_off(void) {
    outb(FDC_DOR, DOR_IRQ | DOR_RESET | 0);
}

// Reset FDC
static int fdc_reset(void) {
    // Disable controller
    outb(FDC_DOR, 0);
    
    // Re-enable controller
    outb(FDC_DOR, DOR_IRQ | DOR_RESET);
    
    // Wait for interrupt (we'll skip this for now)
    // In a real driver, you'd wait for IRQ 6
    
    // Send SENSE_INTERRUPT for each drive
    for (int i = 0; i < 4; i++) {
        if (fdc_write_byte(FDC_CMD_SENSE_INTERRUPT) != 0) return -1;
        uint8_t st0, cyl;
        if (fdc_read_byte(&st0) != 0) return -1;
        if (fdc_read_byte(&cyl) != 0) return -1;
    }
    
    // Set data rate (500 Kbps for 1.44MB)
    outb(FDC_CCR, 0);
    
    // Specify command (step rate, head load/unload times)
    if (fdc_write_byte(FDC_CMD_SPECIFY) != 0) return -1;
    if (fdc_write_byte(0xDF) != 0) return -1; // SRT=3ms, HUT=240ms
    if (fdc_write_byte(0x02) != 0) return -1; // HLT=16ms, DMA mode
    
    return 0;
}

// Recalibrate (seek to track 0)
static int fdc_recalibrate(void) {
    fdc_motor_on();
    
    if (fdc_write_byte(FDC_CMD_RECALIBRATE) != 0) return -1;
    if (fdc_write_byte(0) != 0) return -1; // Drive 0
    
    // Wait for recalibrate to complete (drive 0 busy bit clears)
    uint32_t timeout = 1000000;
    while (timeout-- && (inb(FDC_MSR) & 0x01));
    if (timeout == 0) return -1;
    if (fdc_wait_ready() != 0) return -1;
    
    // Sense interrupt
    if (fdc_write_byte(FDC_CMD_SENSE_INTERRUPT) != 0) return -1;
    uint8_t st0, cyl;
    if (fdc_read_byte(&st0) != 0) return -1;
    if (fdc_read_byte(&cyl) != 0) return -1;
    
    return (cyl == 0) ? 0 : -1;
}

// Seek to a specific cylinder
static int fdc_seek(uint8_t cylinder, uint8_t head) {
    if (fdc_write_byte(FDC_CMD_SEEK) != 0) return -1;
    if (fdc_write_byte((head << 2) | 0) != 0) return -1; // Drive 0
    if (fdc_write_byte(cylinder) != 0) return -1;
    
    // Wait for seek to complete (drive 0 busy bit clears)
    uint32_t timeout = 1000000;
    while (timeout-- && (inb(FDC_MSR) & 0x01));
    if (timeout == 0) return -1;
    if (fdc_wait_ready() != 0) return -1;
    
    // Sense interrupt
    if (fdc_write_byte(FDC_CMD_SENSE_INTERRUPT) != 0) return -1;
    uint8_t st0, cyl;
    if (fdc_read_byte(&st0) != 0) return -1;
    if (fdc_read_byte(&cyl) != 0) return -1;
    
    return 0;
}

// Convert LBA to CHS
static void lba_to_chs(uint32_t lba, uint8_t* cylinder, uint8_t* head, uint8_t* sector) {
    *cylinder = lba / (HEADS * SECTORS_PER_TRACK);
    *head = (lba / SECTORS_PER_TRACK) % HEADS;
    *sector = (lba % SECTORS_PER_TRACK) + 1; // Sectors are 1-based
}

// Detect if FDC is available
int fdc_detect(void) {
    // Check if FDC exists by reading the MSR
    // If all bits are 1 (0xFF), likely no FDC present
    uint8_t msr = inb(FDC_MSR);
    if (msr == 0xFF) {
        return 0; // No FDC detected
    }
    
    // Try to reset and see if we get a response
    outb(FDC_DOR, 0);
    outb(FDC_DOR, DOR_IRQ | DOR_RESET);
    
    // Wait a bit
    for (volatile int i = 0; i < 10000; i++);
    
    // Check if MSR shows ready state
    msr = inb(FDC_MSR);
    if (msr == 0xFF || msr == 0x00) {
        return 0; // No FDC detected
    }
    
    return 1; // FDC detected
}

// Initialize FDC
int fdc_init(void) {
    printf("Initializing FDC...\n");
    
    if (fdc_reset() != 0) {
        printf("FDC reset failed\n");
        return -1;
    }
    
    if (fdc_recalibrate() != 0) {
        printf("FDC recalibrate failed\n");
        return -1;
    }
    
    printf("FDC initialized successfully\n");
    return 0;
}

// Read sectors with DMA (optimized for multi-sector reads)
int fdc_read_sectors(uint32_t lba, uint8_t count, uint8_t* buffer) {
    uint8_t done = 0;
    
    fdc_motor_on();
    
    while (done < count) {
        uint8_t c, h, s;
        lba_to_chs(lba + done, &c, &h, &s);
        
        // Read up to end of track
        uint8_t n = SECTORS_PER_TRACK - s + 1;
        if (n > count - done) n = count - done;
        
        if (fdc_seek(c, h) != 0) { fdc_motor_off(); return -1; }
        
        dma_setup_read(n);
        fdc_irq_received = 0;
        
        if (fdc_write_byte(FDC_CMD_READ_DATA | 0x40) != 0 ||
            fdc_write_byte((h << 2)) != 0 ||
            fdc_write_byte(c) != 0 ||
            fdc_write_byte(h) != 0 ||
            fdc_write_byte(s) != 0 ||
            fdc_write_byte(2) != 0 ||
            fdc_write_byte(s + n - 1) != 0 ||
            fdc_write_byte(0x1B) != 0 ||
            fdc_write_byte(0xFF) != 0) {
            fdc_motor_off();
            return -1;
        }
        
        uint32_t t = 1000000;
        while (t-- && (inb(FDC_MSR) & MSR_BUSY));
        if (!t) { fdc_motor_off(); return -1; }
        
        uint8_t st0, st1, st2, r[4];
        if (fdc_read_byte(&st0) | fdc_read_byte(&st1) | fdc_read_byte(&st2) |
            fdc_read_byte(&r[0]) | fdc_read_byte(&r[1]) | fdc_read_byte(&r[2]) | fdc_read_byte(&r[3])) {
            fdc_motor_off();
            return -1;
        }
        
        if (st0 & 0xC0) { fdc_motor_off(); return -1; }
        
        for (int j = 0; j < n * 512; j++) buffer[done * 512 + j] = dma_buffer[j];
        done += n;
    }
    
    fdc_motor_off();
    return 0;
}

// Write sectors with DMA
int fdc_write_sectors(uint32_t lba, uint8_t count, const uint8_t* buffer) {
    for (uint8_t i = 0; i < count; i++) {
        uint8_t cylinder, head, sector;
        lba_to_chs(lba + i, &cylinder, &head, &sector);
        
        fdc_motor_on();
        
        // Seek to cylinder
        if (fdc_seek(cylinder, head) != 0) {
            fdc_motor_off();
            return -1;
        }
        
        // Copy user data to DMA buffer
        for (int j = 0; j < 512; j++) {
            dma_buffer[j] = buffer[i * 512 + j];
        }
        
        // Setup DMA for write
        dma_setup_write(1);
        
        // Reset IRQ flag
        fdc_irq_received = 0;
        
        // Write command (MT=0, MFM=1, SK=0)
        if (fdc_write_byte(FDC_CMD_WRITE_DATA | 0x40) != 0) {
            fdc_motor_off();
            return -1;
        }
        if (fdc_write_byte((head << 2) | 0) != 0) return -1; // Drive 0
        if (fdc_write_byte(cylinder) != 0) return -1;
        if (fdc_write_byte(head) != 0) return -1;
        if (fdc_write_byte(sector) != 0) return -1;
        if (fdc_write_byte(2) != 0) return -1; // 512 bytes per sector
        if (fdc_write_byte(SECTORS_PER_TRACK) != 0) return -1;
        if (fdc_write_byte(0x1B) != 0) return -1; // GAP3 length
        if (fdc_write_byte(0xFF) != 0) return -1; // Data length
        
        // Wait for operation to complete (polling instead of IRQ for simplicity)
        uint32_t timeout = 1000000;
        while (timeout-- && (inb(FDC_MSR) & MSR_BUSY));
        
        if (timeout == 0) {
            fdc_motor_off();
            return -1;
        }
        
        // Read result bytes (7 bytes)
        uint8_t st0, st1, st2, cyl, h, sec, bps;
        if (fdc_read_byte(&st0) != 0) return -1;
        if (fdc_read_byte(&st1) != 0) return -1;
        if (fdc_read_byte(&st2) != 0) return -1;
        if (fdc_read_byte(&cyl) != 0) return -1;
        if (fdc_read_byte(&h) != 0) return -1;
        if (fdc_read_byte(&sec) != 0) return -1;
        if (fdc_read_byte(&bps) != 0) return -1;
        
        // Check for errors in ST0
        if ((st0 & 0xC0) != 0) {
            fdc_motor_off();
            return -1;
        }
    }
    
    fdc_motor_off();
    return 0;
}
