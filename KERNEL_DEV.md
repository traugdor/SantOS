# Kernel Development Guide

## Overview

Your bootloader now loads and executes a 64-bit ELF kernel! Here's how it works:

## Boot Process

1. **boot.asm** (Stage 1 - 512 bytes)
   - Initializes system
   - Enables A20 line
   - Loads stage 2 from disk (sectors 2-5)
   - Loads kernel.elf from disk (sectors 6+) to 0x10000
   - Jumps to stage 2

2. **boot2.asm** (Stage 2 - 2048 bytes)
   - Checks CPU features (CPUID, long mode)
   - Sets up page tables
   - Transitions to 64-bit long mode
   - Parses ELF header at 0x10000
   - Loads ELF segments to their target addresses
   - Jumps to kernel entry point

3. **kernel.elf** (Your OS kernel)
   - Loaded at 1MB (0x100000) by default
   - Runs in 64-bit long mode
   - Full access to system

## Building

```bash
./build.sh
```

This will:
- Assemble bootloader stages
- Compile kernel.c
- Link kernel.elf
- Create bootloader.img with everything

## Running

```bash
qemu-system-x86_64 -drive format=raw,file=bootloader.img
```

## Kernel Structure

### Memory Layout
- `0x00000 - 0x00400`: Real mode IVT
- `0x00500 - 0x07BFF`: Free
- `0x07C00 - 0x07DFF`: Boot sector (stage 1)
- `0x08000 - 0x08FFF`: Stage 2 bootloader
- `0x10000 - 0x1FFFF`: Kernel.elf file (temporary)
- `0x100000+`: Kernel code/data (1MB+)

### What's Available

Your kernel has:
- **64-bit long mode** enabled
- **Paging** enabled (identity mapped first 4MB)
- **VGA text mode** at 0xB8000
- **No interrupts** (you need to set up IDT)
- **No standard library** (freestanding)

### Kernel Entry

The bootloader calls your `kernel_main()` function directly. You can:
- Write to VGA memory (0xB8000)
- Access all memory
- Set up your own IDT/GDT
- Initialize devices

## Expanding Your Kernel

### Add More Features

```c
// Example: Set up a simple IDT
void setup_idt(void) {
    // Your IDT code here
}

// Example: Handle keyboard input
void keyboard_handler(void) {
    unsigned char scancode = inb(0x60);
    // Process scancode
}

void kernel_main(void) {
    setup_idt();
    // Your OS code
}
```

### Increase Kernel Size

Edit `boot.asm` line 53 to load more sectors:
```asm
mov al, 128  ; Load 128 sectors (64KB) instead of 64
```

### Change Kernel Load Address

Edit `linker.ld` line 8:
```ld
. = 0x200000;  /* Load at 2MB instead of 1MB */
```

## Debugging Tips

1. **Use VGA text mode** for debugging output
2. **QEMU monitor**: Press `Ctrl+Alt+2` for QEMU console
3. **QEMU debug**: Add `-d int,cpu_reset` to see faults
4. **GDB**: Use `qemu-system-x86_64 -s -S` and connect with GDB

## Next Steps

- Set up interrupt descriptor table (IDT)
- Handle keyboard/timer interrupts
- Implement memory management
- Add file system support
- Create a shell/command interpreter
- Build your DOS-like interface!

## Resources

- [OSDev Wiki](https://wiki.osdev.org/)
- [x86_64 Programming](https://wiki.osdev.org/X86-64)
- [ELF Format](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)
