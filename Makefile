# SantOS Makefile

# Tools
NASM := nasm
CC := $(shell which x86_64-elf-gcc 2>/dev/null || echo /mingw64/bin/x86_64-elf-gcc)
LD := $(shell which x86_64-elf-ld 2>/dev/null || echo /mingw64/bin/x86_64-elf-ld)

# Flags
CFLAGS = -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -I.
LDFLAGS = -T linker.ld

# Source files
C_SOURCES = $(wildcard src/*.c) kernel.c
ASM_SOURCES = start.asm src/idt_asm.asm

# Object files
C_OBJECTS = $(C_SOURCES:.c=.o)
ASM_OBJECTS = $(ASM_SOURCES:.asm=.o)
ALL_OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

# Bootloader files
BOOT_BIN = boot.bin
BOOT2_BIN = boot2.bin

# Output
KERNEL_ELF = kernel.elf
IMAGE = bootloader.img

# Default target
all: $(IMAGE)

# Bootloader stage 1
$(BOOT_BIN): boot.asm
	@echo "Assembling boot.asm..."
	$(NASM) -f bin boot.asm -o $(BOOT_BIN)

# Bootloader stage 2
$(BOOT2_BIN): boot2.asm
	@echo "Assembling boot2.asm..."
	$(NASM) -f bin boot2.asm -o $(BOOT2_BIN)

# Assembly objects (64-bit ELF)
%.o: %.asm
	@echo "Assembling $<..."
	$(NASM) -f elf64 $< -o $@

# C objects
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Kernel ELF
$(KERNEL_ELF): $(ALL_OBJECTS)
	@echo "Linking kernel.elf..."
	$(LD) $(LDFLAGS) -o $(KERNEL_ELF) $(ALL_OBJECTS)

# Disk image
$(IMAGE): $(BOOT_BIN) $(BOOT2_BIN) $(KERNEL_ELF)
	@echo "Creating disk image..."
	@# Pad boot.bin to 512 bytes
	@truncate -s 512 $(BOOT_BIN)
	@# Pad boot2.bin to 2048 bytes (4 sectors)
	@truncate -s 2048 $(BOOT2_BIN)
	@# Concatenate: boot.bin + boot2.bin + kernel.elf
	cat $(BOOT_BIN) $(BOOT2_BIN) $(KERNEL_ELF) > $(IMAGE)
	@echo "Build complete: $(IMAGE)"
	@echo "  boot.bin:   $$(stat -c%s $(BOOT_BIN)) bytes"
	@echo "  boot2.bin:  $$(stat -c%s $(BOOT2_BIN)) bytes"
	@echo "  kernel.elf: $$(stat -c%s $(KERNEL_ELF)) bytes"

# Run in QEMU
run: $(IMAGE)
	qemu-system-x86_64 -drive format=raw,file=$(IMAGE)

# Clean
clean:
	@echo "Cleaning..."
	rm -f $(C_OBJECTS) $(ASM_OBJECTS) $(KERNEL_ELF) $(BOOT_BIN) $(BOOT2_BIN) $(IMAGE)

# Clean only kernel (keep bootloader)
clean-kernel:
	@echo "Cleaning kernel..."
	rm -f $(C_OBJECTS) $(ASM_OBJECTS) $(KERNEL_ELF)

# Rebuild kernel only
kernel: clean-kernel $(KERNEL_ELF)
	@echo "Kernel rebuilt"

# Rebuild bootloader only
bootloader: $(BOOT_BIN) $(BOOT2_BIN)
	@echo "Bootloader rebuilt"

# Phony targets
.PHONY: all run clean clean-kernel kernel bootloader

# Show help
help:
	@echo "SantOS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build everything (incremental)"
	@echo "  make run      - Build and run in QEMU"
	@echo "  make kernel   - Rebuild only kernel"
	@echo "  make bootloader - Rebuild only bootloader"
	@echo "  make clean    - Clean all build artifacts"
	@echo "  make clean-kernel - Clean only kernel objects"
