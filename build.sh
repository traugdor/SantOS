#!/bin/bash
# Build script for x86_64 bootloader + kernel

echo "Building bootloader..."

# Assemble boot.asm (first stage bootloader)
nasm -f bin boot.asm -o boot.bin
if [ $? -ne 0 ]; then
    echo "Error assembling boot.asm"
    exit 1
fi

# Assemble boot2.asm (second stage bootloader)
nasm -f bin boot2.asm -o boot2.bin
if [ $? -ne 0 ]; then
    echo "Error assembling boot2.asm"
    exit 1
fi

# Assemble IDT assembly code
nasm -f elf64 src/idt_asm.asm -o idt_asm.o
if [ $? -ne 0 ]; then
    echo "Error assembling idt_asm.asm"
    exit 1
fi

# Assemble start.asm (entry point)
nasm -f elf64 start.asm -o start.o
if [ $? -ne 0 ]; then
    echo "Error assembling start.asm"
    exit 1
fi

echo "Building kernel..."

# Compiler settings
CC="x86_64-elf-gcc"
CFLAGS="-ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -I."

# Compile kernel.c
echo "  Compiling kernel.c..."
$CC $CFLAGS -c kernel.c -o kernel.o
if [ $? -ne 0 ]; then
    echo "Error compiling kernel.c"
    exit 1
fi

# Compile all source files in src/
OBJS="kernel.o"
for src in src/*.c; do
    if [ -f "$src" ]; then
        obj=$(basename "$src" .c).o
        echo "  Compiling $src..."
        $CC $CFLAGS -c "$src" -o "$obj"
        if [ $? -ne 0 ]; then
            echo "Error compiling $src"
            exit 1
        fi
        OBJS="$OBJS $obj"
    fi
done

# Link kernel
echo "  Linking kernel.elf..."
x86_64-elf-ld -T linker.ld -o kernel.elf start.o $OBJS idt_asm.o
if [ $? -ne 0 ]; then
    echo "Error linking kernel"
    exit 1
fi

echo "Creating disk image..."

# Check actual sizes before padding
echo "Before padding:"
echo "  - boot.bin: $(stat -c%s boot.bin) bytes"
echo "  - boot2.bin: $(stat -c%s boot2.bin) bytes"

# Pad boot.bin to exactly 512 bytes (1 sector)
truncate -s 512 boot.bin

# Pad boot2.bin to exactly 2048 bytes (4 sectors)
truncate -s 2048 boot2.bin

echo "After padding:"
echo "  - boot.bin: $(stat -c%s boot.bin) bytes"
echo "  - boot2.bin: $(stat -c%s boot2.bin) bytes"

# Create disk image - bootloader is now exactly 2560 bytes (5 sectors)
cat boot.bin boot2.bin > bootloader.img

# Verify bootloader size
BOOTLOADER_SIZE=$(stat -c%s bootloader.img)
echo "  - bootloader.img: $BOOTLOADER_SIZE bytes (should be 2560)"

if [ $BOOTLOADER_SIZE -ne 2560 ]; then
    echo "ERROR: Bootloader is not exactly 2560 bytes!"
    exit 1
fi

# Append kernel (starting at sector 6)
cat kernel.elf >> bootloader.img

# Pad to at least 64KB
truncate -s 65536 bootloader.img

hexdump -C bootloader.img | head -n 170 > hexdump
readelf -l kernel.elf > elfdump

echo "Build complete! bootloader.img created."
echo "Hexdump output saved to hexdump file"
echo "readelf output saved to elfdump file"
echo "  - kernel.elf size: $(stat -c%s kernel.elf) bytes"
echo ""
echo "To run in QEMU, use:"
echo "qemu-system-x86_64 -drive format=raw,file=bootloader.img"
