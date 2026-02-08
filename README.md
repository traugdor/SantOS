# x86_64 Bootloader

A two-stage bootloader that boots into 64-bit long mode on x86_64 systems.

## Features

- **Stage 1 (boot.asm)**: 
  - 16-bit real mode bootloader
  - Disk initialization and reset
  - A20 line enabling (required for x86_64)
  - Loads stage 2 from disk

- **Stage 2 (boot2.asm)**:
  - CPU feature detection (CPUID and long mode support)
  - Identity-mapped page tables (PML4, PDPT, PD)
  - Transition through 32-bit protected mode to 64-bit long mode
  - VGA text mode display with progress bar
  - Number conversion and display demo

## Building

```bash
chmod +x build.sh
./build.sh
```

This will create `bootloader.img` containing both stages.

## Running in QEMU

```bash
qemu-system-x86_64 -drive format=raw,file=bootloader.img
```

## Technical Details

### Memory Layout
- `0x7C00`: Stage 1 bootloader (boot.bin)
- `0x8000`: Stage 2 bootloader (boot2.bin)
- `0x1000`: PML4 (Page Map Level 4)
- `0x2000`: PDPT (Page Directory Pointer Table)
- `0x3000`: PD (Page Directory)
- `0x90000`: Stack pointer
- `0xB8000`: VGA text mode buffer

### Long Mode Transition
1. Enable A20 line
2. Check CPU features (CPUID, long mode)
3. Setup identity-mapped page tables
4. Load 32-bit GDT and enter protected mode
5. Enable PAE (Physical Address Extension)
6. Set LM bit in EFER MSR
7. Enable paging (activates long mode)
8. Load 64-bit GDT
9. Far jump to 64-bit code

## Requirements

- NASM assembler
- QEMU (qemu-system-x86_64)
- x86_64 CPU support (or emulation)
