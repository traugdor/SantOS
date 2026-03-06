#!/bin/bash

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

# Assemble syscall handler
nasm -f elf64 src/syscall_asm.asm -o syscall_asm.o
if [ $? -ne 0 ]; then
    echo "Error assembling syscall_asm.asm"
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
x86_64-elf-ld -T linker.ld -o kernel.elf start.o $OBJS idt_asm.o syscall_asm.o
if [ $? -ne 0 ]; then
    echo "Error linking kernel"
    exit 1
fi

echo "Building userspace programs..."
make -C programs
if [ $? -ne 0 ]; then
    echo "Error building programs"
    exit 1
fi

./create_disk.sh
qemu-system-x86_64 -m 512 -drive format=raw,file=disk.img,if=floppy