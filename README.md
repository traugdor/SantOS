# SantOS - x86_64 Operating System

A complete x86_64 operating system with multi-stage bootloader, FAT12/16/32 filesystem support, and a 64-bit kernel.

## Features

### Bootloader System
- **Multi-filesystem support**: FAT12, FAT16, and FAT32 bootloaders
- **Three-stage boot process**:
  - **Stage 1** (boot12/16/32.asm): MBR bootloader (512 bytes)
  - **Stage 1.5** (fat12/16/32.asm): Filesystem driver (512 bytes)
  - **Stage 2** (boot2.asm): 64-bit transition and kernel loader
- **Track boundary handling**: Proper BIOS INT 0x13 disk reads respecting track boundaries
- **A20 line enabling**: Required for accessing memory above 1MB

### Kernel Features
- **64-bit long mode**: Full x86_64 support
- **Hardware drivers**:
  - VGA text mode display
  - PS/2 keyboard input
  - PIT timer (1ms resolution)
  - FDC (Floppy Disk Controller) with DMA support
  - ATA/IDE disk controller (with timeout handling)
- **Filesystem drivers**:
  - FAT12 (floppy disks) - fully functional with FDC
  - FAT16 (small partitions)
  - FAT32 (large partitions)
- **DMA Controller**: 8237 DMA setup for floppy disk transfers
- **Interrupt handling**: IDT setup with hardware interrupt support

## Building

### Using Make (Recommended)
```bash
make clean
make all
```

### Using Build Script
```bash
chmod +x build.sh create_disk.sh
./build.sh
./create_disk.sh
```

This creates a 1.44MB FAT12 floppy disk image (`disk.img`) with:
- Bootloader in MBR
- FAT12.SYS filesystem driver
- BOOT2.BIN second-stage loader
- KERNEL.ELF 64-bit kernel
- TEST.TXT sample file

## Running

### QEMU
```bash
make run
# or
qemu-system-x86_64 -fda disk.img
```

### QEMU with GDB Debugging
```bash
make debug
```

### VirtualBox
1. Create new VM (Type: Other, Version: Other/Unknown 64-bit)
2. Attach `disk.img` as floppy disk
3. Boot!

## Project Structure

```
bootloader/
├── boot12.asm          # FAT12 MBR bootloader
├── boot16.asm          # FAT16 MBR bootloader
├── boot32.asm          # FAT32 MBR bootloader
├── fat12.asm           # FAT12 filesystem driver (stage 1.5)
├── fat16.asm           # FAT16 filesystem driver (stage 1.5)
├── fat32.asm           # FAT32 filesystem driver (stage 1.5)
├── boot2.asm           # Second stage: 64-bit transition
├── kernel.c            # Kernel entry point
├── start.asm           # Kernel assembly entry
├── linker.ld           # Kernel linker script
├── include/            # Header files
│   ├── fat12.h
│   ├── fat16.h
│   ├── fat32.h
│   ├── fdc.h          # Floppy Disk Controller
│   ├── ata.h
│   ├── keyboard.h
│   ├── timer.h
│   ├── idt.h
│   └── ...
└── src/                # Kernel source files
    ├── fat12.c
    ├── fat16.c
    ├── fat32.c
    ├── fdc.c          # FDC with DMA support
    ├── ata.c
    ├── keyboard.c
    ├── timer.c
    ├── idt.c
    └── ...
```

## Technical Details

### Memory Layout
- `0x7C00`: Stage 1 bootloader (512 bytes)
- `0x7E00`: Stage 1.5 filesystem driver (512 bytes)
- `0x9000`: Stage 2 bootloader (boot2.bin, 2KB)
- `0x20000`: Kernel ELF file (loaded by FAT driver)
- `0x200000`: Kernel execution address (copied from ELF)
- `0x1000-0x3FFF`: Page tables (PML4, PDPT, PD)
- `0x80000`: Kernel stack (16KB)
- `0x90000`: Boot stack
- `0xB8000`: VGA text mode buffer

### Boot Process
1. **BIOS** loads MBR (boot12.asm) to 0x7C00
2. **Stage 1** loads FAT driver (fat12.asm) to 0x7E00
3. **FAT driver** searches root directory for:
   - BOOT2.BIN → loads to 0x9000
   - KERNEL.ELF → loads to 0x20000
4. **Stage 2** (boot2.asm):
   - Checks CPU features (CPUID, long mode)
   - Sets up identity-mapped page tables
   - Enters 32-bit protected mode
   - Enables PAE and long mode
   - Enters 64-bit long mode
   - Parses ELF header and copies kernel to 0x200000
   - Jumps to kernel entry point
5. **Kernel** initializes hardware and filesystem

### Long Mode Transition
1. Enable A20 line (BIOS and keyboard controller methods)
2. Check CPUID support
3. Check long mode support (CPUID extended function)
4. Setup identity-mapped page tables (8MB mapped)
5. Load 32-bit GDT and enter protected mode
6. Enable PAE (CR4.PAE = 1)
7. Load CR3 with PML4 address
8. Set LM bit in EFER MSR
9. Enable paging (CR0.PG = 1) - activates long mode
10. Load 64-bit GDT
11. Far jump to 64-bit code segment

### FAT Filesystem Drivers

| Feature | FAT12 | FAT16 | FAT32 |
|---------|-------|-------|-------|
| FAT Entry Size | 12 bits | 16 bits | 32 bits (28 used) |
| Max Clusters | ~4,084 | ~65,524 | ~268M |
| Root Directory | Fixed location | Fixed location | Cluster chain |
| Typical Use | Floppy disks | Small partitions | Large partitions |

All filesystem drivers support:
- File search by name (8.3 format)
- File reading with cluster chain following
- Directory listing
- Track boundary handling for BIOS disk reads

## Requirements

### Build Tools
- **NASM** (Netwide Assembler)
- **x86_64-elf-gcc** (cross-compiler)
- **x86_64-elf-ld** (cross-linker)
- **objcopy** (binary conversion)
- **make** (build system)

### Disk Tools
- **mkfs.vfat** (FAT filesystem creation)
- **mtools** (mcopy, mmd, mdir - FAT file operations)
- **dd** (disk image creation)

### Testing
- **QEMU** (qemu-system-x86_64)
- **GDB** (optional, for debugging)
- **VirtualBox** (optional, alternative to QEMU)

## Development Notes

### Track Boundary Handling
BIOS INT 0x13 has critical limitations:
- Cannot read across track boundaries (18 sectors/track on 1.44MB floppy)
- Cannot cross 64KB DMA boundaries

The bootloader splits reads at track boundaries:
```
sectors_left_on_track = 18 - (LBA % 18)
read_count = min(sectors_left_on_track, sectors_remaining)
```

### Floppy Disk Controller (FDC) & DMA
The kernel includes a full FDC driver with DMA support:
- **DMA Channel 2**: Used for floppy disk transfers
- **DMA Buffer**: Located at 0x8000 (safe memory, doesn't conflict with page tables)
- **Transfer Mode**: Single-transfer mode, 512 bytes per sector
- **Commands**: Reset, recalibrate, seek, read data
- **Result Handling**: Reads 7-byte result after each operation
- **Motor Control**: Automatic motor on/off for power management

**Critical Memory Layout:**
- 0x1000-0x3FFF: Page tables (PML4, PDPT, PD) - **DO NOT USE**
- 0x7C00-0x7DFF: Boot sector
- 0x7E00-0x7FFF: Stage 1.5 loader
- 0x8000+: Safe for DMA buffer and other uses

### Register Usage
All bootloader stages use **16-bit registers only** (AX, BX, CX, DX, SI, DI) for real mode compatibility, even when handling 32-bit values in FAT32.

### ELF Loading
The kernel is compiled as an ELF64 executable. Boot2.asm:
1. Verifies ELF magic number (0x7F 'E' 'L' 'F')
2. Reads entry point from ELF header
3. Parses program headers
4. Copies loadable segments to their virtual addresses
5. Jumps to entry point

## License

This project is provided as-is for educational purposes.
