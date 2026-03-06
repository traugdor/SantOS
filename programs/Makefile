# SantOS Programs Makefile
# Build userspace programs separately from kernel

# Tools
CC := $(shell which x86_64-elf-gcc 2>/dev/null || echo ../cross/bin/x86_64-elf-gcc)
LD := $(shell which x86_64-elf-ld 2>/dev/null || echo ../cross/bin/x86_64-elf-ld)
OBJCOPY := $(shell which objcopy 2>/dev/null || echo ../cross/bin/objcopy)

# Flags for userspace programs
CFLAGS = -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -I..
LDFLAGS = -nostdlib

# Programs
PROGRAMS = shell/shell.bin

.PHONY: all clean

all: $(PROGRAMS)
	@echo "✓ All programs built successfully"

# Shell program
shell/shell.o: shell/shell.c
	@echo "Compiling shell.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build userspace libraries
lib/stdio.o: lib/stdio.c
	@echo "Compiling userspace stdio..."
	$(CC) $(CFLAGS) -c $< -o $@

lib/stdlib.o: lib/stdlib.c
	@echo "Compiling userspace stdlib..."
	$(CC) $(CFLAGS) -c $< -o $@

lib/string.o: lib/string.c
	@echo "Compiling userspace string..."
	$(CC) $(CFLAGS) -c $< -o $@

shell/shell.elf: shell/shell.o lib/stdio.o lib/stdlib.o lib/string.o
	@echo "Linking shell.elf with userspace libraries..."
	$(LD) $(LDFLAGS) -Ttext=0x100000 -e shell_main -o $@ $^

shell/shell.bin: shell/shell.elf
	@echo "Converting to flat binary..."
	$(OBJCOPY) -O binary $< $@
	@echo "✓ Shell built successfully"

clean:
	@echo "Cleaning programs..."
	rm -f shell/*.o shell/*.bin
	@echo "✓ Programs cleaned"
