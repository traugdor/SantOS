# SantOS - x86_64 Operating System

A complete x86_64 operating system with multi-stage bootloader, FAT12 filesystem support, a 64-bit kernel with heap allocation, system call interface, interactive shell, and full-featured text editor.

## Features

### Bootloader System
- **Multi-filesystem support**: FAT12 (fully functional), FAT16 (WIP), and FAT32 (WIP) bootloaders
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
  - ATA/IDE disk controller (with timeout handling) (WIP)
- **Filesystem drivers**:
  - FAT12 (floppy disks) - fully functional with FDC
  - FAT16 (small partitions) (WIP)
  - FAT32 (large partitions) (WIP)
- **DMA Controller**: 8237 DMA setup for floppy disk transfers
- **Interrupt handling**: IDT setup with hardware interrupt support
- **Memory management**:
  - Physical memory manager (bitmap allocator) with E820 detection
  - Virtual memory manager (paging) with dynamic mapping up to 1GB
  - Kernel heap (malloc/free/realloc/calloc)
- **System call interface**: INT 0x80 for kernel-userspace communication
- **ELF Program loader**: Loads and executes 64-bit ELF programs from FAT12 filesystem
  - Parses ELF headers and program segments
  - Loads programs at 5MB (0x500000)
  - Provides dedicated stack at 7MB (0x700000)
  - Supports command-line arguments (argc/argv)
  - Zeroes BSS section automatically

### Userspace Programs
- **SantOS Shell v1.0**: Full-featured command interpreter
  - Built-in commands (ls, cat, read, echo, help, clear)
  - Program execution with arguments
  - Script execution (##/sosh)
  - Environment variables with arithmetic operations
  - Command piping (cat | read, ls | read)
  - Command history (10 commands)
  - Inline editing with arrow keys
- **SEdit v1.0.0**: Complete text editor
  - File operations (open, edit, save)
  - Vertical and horizontal scrolling
  - Truncation indicators for long lines
  - Status bar with version, filename, position, shortcuts
  - Ctrl+S save, Ctrl+Q quit
  - Supports up to 1000 lines, 256 chars per line
- **Hello v1.0**: Example program
  - Interactive calculator (4 operations)
  - Prototype spell checker (50-word dictionary)
  - Demonstrates userspace capabilities
- **System call wrappers**: Userspace libraries for stdio, stdlib, and string functions
- **Separate build system**: Programs built independently and linked with syscall libraries
- **ELF format**: All programs are 64-bit ELF executables

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
- SHELL.ELF interactive shell (loads at 1MB)
- SEDIT text editor (no extension for cleaner UI)
- HELLO.ELF example program
- TEST.SH sample script
- STORY.TXT sample text file

## Running

### QEMU
```bash
make run
# or
qemu-system-x86_64 -m 512 -fda disk.img
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
├── boot16.asm          # FAT16 MBR bootloader (WIP)
├── boot32.asm          # FAT32 MBR bootloader (WIP)
├── fat12.asm           # FAT12 filesystem driver (stage 1.5)
├── fat16.asm           # FAT16 filesystem driver (stage 1.5) (WIP)
├── fat32.asm           # FAT32 filesystem driver (stage 1.5) (WIP)
├── boot2.asm           # Second stage: 64-bit transition
├── kernel.c            # Kernel entry point
├── start.asm           # Kernel assembly entry
├── linker.ld           # Kernel linker script
├── Makefile            # Main build system
├── build.sh            # Full build script
├── quick.sh            # Quick build + run
├── quick_k.sh          # Quick kernel rebuild + run
├── create_disk.sh      # Disk image creation
├── include/            # Header files
│   ├── fat12.h
│   ├── fdc.h          # Floppy Disk Controller
│   ├── keyboard.h
│   ├── timer.h
│   ├── idt.h
│   ├── heap.h         # Kernel heap allocator
│   ├── syscall.h      # System call definitions
│   ├── pmem.h         # Physical memory manager
│   ├── vmem.h         # Virtual memory manager
│   └── ...
├── src/                # Kernel source files
│   ├── fat12.c
│   ├── fdc.c          # FDC with DMA support
│   ├── keyboard.c
│   ├── timer.c
│   ├── idt.c
│   ├── heap.c         # malloc/free/realloc/calloc
│   ├── pmem.c         # Physical memory bitmap allocator
│   ├── vmem.c         # Virtual memory / paging
│   ├── syscall.c      # Kernel syscall handler
│   ├── syscall_asm.asm # INT 0x80 assembly entry
│   ├── loader.c       # Program loader
│   └── ...
└── programs/           # Userspace programs
    ├── Makefile        # Programs build system
    ├── lib/            # Userspace syscall libraries
    │   ├── stdio.c    # printf, putchar, getchar, exec_program
    │   ├── stdlib.c   # malloc, free, realloc, calloc
    │   └── string.c   # strlen, strcmp, strcpy, memcpy, etc.
    ├── shell/          # Interactive shell
    │   ├── shell.c    # SantOS Shell v1.0
    │   └── Readme.MD  # Shell documentation
    ├── sedit/          # Text editor
    │   ├── SEdit.c    # SEdit v1.0.0
    │   └── Readme.MD  # Editor documentation
    └── hello/          # Example program
        ├── hello.c    # Calculator & spell checker
        └── Readme.MD  # Program documentation
```

## Technical Details

### Memory Layout
- `0x7C00`: Stage 1 bootloader (512 bytes)
- `0x7E00`: Stage 1.5 filesystem driver (512 bytes)
- `0x8000`: DMA buffer for floppy disk transfers
- `0x9000`: Stage 2 bootloader (boot2.bin, 2KB)
- `0x20000`: Kernel ELF file (loaded by FAT driver)
- `0x100000`: Shell program (SHELL.ELF, 1MB)
- `0x200000`: Kernel execution address (copied from ELF, 2MB)
- `0x500000`: Userspace programs load address (5MB)
- `0x700000`: Userspace program stack (7MB, grows down)
- `0x1000-0x3FFF`: Page tables (PML4, PDPT, PD)
- `0x80000`: Kernel stack (16KB)
- `0x90000`: Boot stack
- `0xB8000`: VGA text mode buffer
- `0x1000000`: Kernel heap (16MB+)
- Dynamic page tables map up to 1GB based on E820 memory map

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
5. **Kernel** initializes hardware, filesystem, heap, and syscall interface
6. **Shell** loaded from SHELL.ELF and executed at 1MB
7. **User programs** can be executed from shell (./SEdit, ./hello, etc.)

### System Call Interface
Userspace programs communicate with the kernel via `INT 0x80`:

| Syscall | Number | Description |
|---------|--------|-------------|
| PUTCHAR | 1 | Write character to screen |
| GETCHAR | 2 | Read character from keyboard |
| PRINTF  | 3 | Print formatted string |
| CLEAR   | 4 | Clear screen |
| SET_CURSOR | 5 | Set VGA cursor position |
| SET_COLOR | 6 | Set VGA text colors |
| MALLOC  | 10 | Allocate heap memory |
| FREE    | 11 | Free heap memory |
| REALLOC | 12 | Reallocate heap memory |
| CALLOC  | 13 | Allocate zeroed memory |
| STRLEN  | 20 | String length |
| STRCMP  | 21 | String compare |
| STRCPY  | 22 | String copy |
| STRCAT  | 23 | String concatenate |
| MEMCPY  | 24 | Memory copy |
| MEMSET  | 25 | Memory set |
| LIST_DIR | 30 | List root directory |
| LIST_DIR_CLUSTER | 31 | List directory by cluster |
| FIND_ENTRY | 32 | Find directory entry |
| READ_FILE | 33 | Read file from filesystem |
| CREATE_FILE | 34 | Create new file |
| WRITE_FILE | 35 | Write file to filesystem |
| EXEC_PROGRAM | 40 | Load and execute ELF program |

### Long Mode Transition
1. Enable A20 line (BIOS and keyboard controller methods)
2. Check CPUID support
3. Check long mode support (CPUID extended function)
4. Setup identity-mapped page tables
5. Load 32-bit GDT and enter protected mode
6. Enable PAE (CR4.PAE = 1)
7. Load CR3 with PML4 address
8. Set LM bit in EFER MSR
9. Enable paging (CR0.PG = 1) - activates long mode
10. Load 64-bit GDT
11. Far jump to 64-bit code segment

### FAT Filesystem Drivers

| Feature | FAT12 | FAT16 (WIP) | FAT32 (WIP) |
|---------|-------|-------------|-------------|
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
- **VirtualBox** (optional, alternative to QEMU) (WIP)

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

## Notes

***__It is not recommended to use this as a production bootloader. All code provided is untested in a real system and may contain bugs that could cause data loss or system instability. For example, I have only been able to get it to successfully run in QEMU, not in VirtualBox. Use at your own risk.__***

Feel free to fork and modify for your own projects. If you feel you have a significant improvement, consider contributing back to the project and explain in thorough detail what you have done and why so that I can learn from your improvements!