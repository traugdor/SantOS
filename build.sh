#!/bin/bash
# Build script for x86_64 bootloader + kernel

echo "Building bootloader..."

# For boot.asm (first stage)
nasm -f elf64 -g -F dwarf -o boot12.elf.o boot12.asm
x86_64-elf-ld -Ttext=0x7C00 -nostdlib -o boot12.elf boot12.elf.o
objcopy -O binary boot12.elf boot12.bin
if [ $? -ne 0 ]; then
    echo "Error assembling boot12.asm"
    exit 1
fi

nasm -f elf64 -g -F dwarf -o boot16.elf.o boot16.asm
x86_64-elf-ld -Ttext=0x7C00 -nostdlib -o boot16.elf boot16.elf.o
objcopy -O binary boot16.elf boot16.bin
if [ $? -ne 0 ]; then
    echo "Error assembling boot16.asm"
    #exit 1
fi

nasm -f elf64 -g -F dwarf -o boot32.elf.o boot32.asm
x86_64-elf-ld -Ttext=0x7C00 -nostdlib -o boot32.elf boot32.elf.o
objcopy -O binary boot32.elf boot32.bin
if [ $? -ne 0 ]; then
    echo "Error assembling boot32.asm"
    #exit 1
fi

nasm -f elf64 -g -F dwarf -o fat12.elf.o fat12.asm
x86_64-elf-ld -Ttext=0x7E00 -nostdlib -o fat12.elf fat12.elf.o
objcopy -O binary fat12.elf fat12.bin
if [ $? -ne 0 ]; then
    echo "Error assembling fat12.asm"
    exit 1
fi

nasm -f elf64 -g -F dwarf -o fat16.elf.o fat16.asm
x86_64-elf-ld -Ttext=0x7E00 -nostdlib -o fat16.elf fat16.elf.o
objcopy -O binary fat16.elf fat16.bin
if [ $? -ne 0 ]; then
    echo "Error assembling fat16.asm"
    #exit 1
fi

nasm -f elf64 -g -F dwarf -o fat32.elf.o fat32.asm
x86_64-elf-ld -Ttext=0x7E00 -nostdlib -o fat32.elf fat32.elf.o
objcopy -O binary fat32.elf fat32.bin
if [ $? -ne 0 ]; then
    echo "Error assembling fat32.asm"
    #exit 1
fi

# For boot2.asm (second stage)
nasm -f elf64 -g -F dwarf -o boot2.elf.o boot2.asm
x86_64-elf-ld -Ttext=0x9000 -nostdlib -o boot2.elf boot2.elf.o
objcopy -O binary boot2.elf boot2.bin
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

echo "Build complete!"
echo "To run in QEMU, use:"
echo "./create_disk.sh && qemu-system-x86_64 -drive format=raw,if=floppy,file=disk.img"
