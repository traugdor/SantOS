# SantOS Bootloader Makefile

# Tools
NASM := nasm
CC := $(shell which x86_64-elf-gcc 2>/dev/null || echo ./cross/bin/x86_64-elf-gcc)
LD := $(shell which x86_64-elf-ld 2>/dev/null || echo ./cross/bin/x86_64-elf-ld)
OBJCOPY := $(shell which objcopy 2>/dev/null || echo ./cross/bin/objcopy)

# Flags
CFLAGS = -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -I.
LDFLAGS = -T linker.ld
NASMFLAGS = -f elf64 -g -F dwarf

# Source files
C_SOURCES = $(wildcard src/*.c) kernel.c
ASM_SOURCES = start.asm src/idt_asm.asm

# Object files
C_OBJECTS = $(C_SOURCES:.c=.o)
ASM_OBJECTS = $(ASM_SOURCES:.asm=.o)
ALL_OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

# Bootloader files
BOOT12_BIN = boot12.bin
BOOT12_ELF = boot12.elf
BOOT12_OBJ = boot12.elf.o

BOOT16_BIN = boot16.bin
BOOT16_ELF = boot16.elf
BOOT16_OBJ = boot16.elf.o

BOOT32_BIN = boot32.bin
BOOT32_ELF = boot32.elf
BOOT32_OBJ = boot32.elf.o

BOOT2_BIN = boot2.bin
BOOT2_ELF = boot2.elf
BOOT2_OBJ = boot2.elf.o

# Output
KERNEL_ELF = kernel.elf
DISK_IMG = disk.img

# Default target
.PHONY: all clean build bootloader stage2 kernel disk run help

all: clean build disk

build: bootloader stage2 kernel

bootloader: $(BOOT12_BIN) $(BOOT16_BIN) $(BOOT32_BIN)
	@echo "✓ Bootloader built successfully"

stage2: $(BOOT2_BIN)
	@echo "✓ Stage 2 bootloader built successfully"

kernel: $(KERNEL_ELF)
	@echo "✓ Kernel built successfully"

disk: build
	@echo "Creating 1.44MB FAT12 floppy disk image..."
	@bash create_disk.sh
	@echo "✓ Disk image created successfully"

# Boot12 (FAT12 bootloader)
$(BOOT12_OBJ): boot12.asm
	@echo "Assembling boot12.asm..."
	$(NASM) $(NASMFLAGS) -o $@ $<

$(BOOT12_ELF): $(BOOT12_OBJ)
	@echo "Linking boot12.elf..."
	$(LD) -Ttext=0x7C00 -nostdlib -o $@ $<

$(BOOT12_BIN): $(BOOT12_ELF)
	@echo "Converting boot12.elf to binary..."
	$(OBJCOPY) -O binary $< $@

# Boot16 (FAT16 bootloader - optional)
$(BOOT16_OBJ): boot16.asm
	@echo "Assembling boot16.asm..."
	$(NASM) $(NASMFLAGS) -o $@ $<

$(BOOT16_ELF): $(BOOT16_OBJ)
	@echo "Linking boot16.elf..."
	$(LD) -Ttext=0x7C00 -nostdlib -o $@ $<

$(BOOT16_BIN): $(BOOT16_ELF)
	@echo "Converting boot16.elf to binary..."
	$(OBJCOPY) -O binary $< $@

# Boot32 (32-bit bootloader - optional)
$(BOOT32_OBJ): boot32.asm
	@echo "Assembling boot32.asm..."
	$(NASM) $(NASMFLAGS) -o $@ $<

$(BOOT32_ELF): $(BOOT32_OBJ)
	@echo "Linking boot32.elf..."
	$(LD) -Ttext=0x7C00 -nostdlib -o $@ $<

$(BOOT32_BIN): $(BOOT32_ELF)
	@echo "Converting boot32.elf to binary..."
	$(OBJCOPY) -O binary $< $@

# Boot2 (Second stage bootloader)
$(BOOT2_OBJ): boot2.asm
	@echo "Assembling boot2.asm..."
	$(NASM) $(NASMFLAGS) -o $@ $<

$(BOOT2_ELF): $(BOOT2_OBJ)
	@echo "Linking boot2.elf..."
	$(LD) -Ttext=0x8000 -nostdlib -o $@ $<

$(BOOT2_BIN): $(BOOT2_ELF)
	@echo "Converting boot2.elf to binary..."
	$(OBJCOPY) -O binary $< $@

# Assembly objects (64-bit ELF for kernel)
%.o: %.asm
	@echo "Assembling $<..."
	$(NASM) -f elf64 $< -o $@

# C objects (kernel)
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Kernel ELF
$(KERNEL_ELF): $(ALL_OBJECTS)
	@echo "Linking kernel.elf..."
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJECTS)

# Run in QEMU
run: clean build disk
	@echo "Launching QEMU..."
	qemu-system-x86_64 -fda $(DISK_IMG)

# Debug in QEMU with GDB
debug: clean build disk
	@echo "Launching QEMU with GDB debugging..."
	@echo "Starting QEMU on port 1234..."
	qemu-system-x86_64 -fda $(DISK_IMG) -s -S &
	@sleep 1
	@echo "Attaching GDB..."
	@gdb -x gdb_init.script

# Clean
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(BOOT12_OBJ) $(BOOT12_ELF) $(BOOT12_BIN)
	rm -f $(BOOT16_OBJ) $(BOOT16_ELF) $(BOOT16_BIN)
	rm -f $(BOOT32_OBJ) $(BOOT32_ELF) $(BOOT32_BIN)
	rm -f $(BOOT2_OBJ) $(BOOT2_ELF) $(BOOT2_BIN)
	rm -f $(C_OBJECTS) $(ASM_OBJECTS) $(KERNEL_ELF)
	rm -f $(DISK_IMG)
	rm -f *.o
	rm -f src/*.o
	rm -f *.img
	rm -f imgdump
	@echo "✓ Clean complete"

# Help
help:
	@echo "SantOS Bootloader Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make clean      - Clean all build artifacts"
	@echo "  make all        - Clean, build bootloader/stage2/kernel, and create disk"
	@echo "  make build      - Build bootloader, stage2, and kernel"
	@echo "  make bootloader - Compile boot12/boot16/boot32.asm files"
	@echo "  make stage2     - Compile boot2.asm"
	@echo "  make kernel     - Compile kernel.elf"
	@echo "  make disk       - Create 1.44MB FAT12 floppy disk image"
	@echo "  make run        - Clean, build, create disk, and run in QEMU"
	@echo "  make debug      - Clean, build, create disk, run QEMU with GDB, and attach debugger"
	@echo "  make help       - Show this help message"
