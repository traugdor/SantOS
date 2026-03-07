# SantOS Programs Makefile
# Build userspace programs separately from kernel

# Tools
CC := $(shell which x86_64-elf-gcc 2>/dev/null || echo ../cross/bin/x86_64-elf-gcc)
LD := $(shell which x86_64-elf-ld 2>/dev/null || echo ../cross/bin/x86_64-elf-ld)
OBJCOPY := $(shell which objcopy 2>/dev/null || echo ../cross/bin/objcopy)

# Flags for userspace programs
CFLAGS = -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -I..
LDFLAGS = -nostdlib

# Auto-discover program directories (exclude lib)
PROG_DIRS := $(filter-out lib, $(patsubst %/,%,$(dir $(wildcard */))))
PROG_SOURCES := $(foreach dir,$(PROG_DIRS),$(wildcard $(dir)/*.c))
PROG_ELFS := $(patsubst %.c,%.elf,$(PROG_SOURCES))
PROG_OBJS := $(patsubst %.c,%.o,$(PROG_SOURCES))

# ELF files to copy to root
ROOT_ELFS := $(notdir $(PROG_ELFS))

# Library objects
LIB_OBJS = lib/stdio.o lib/stdlib.o lib/string.o

.PHONY: all clean

all: $(LIB_OBJS) $(PROG_ELFS) $(ROOT_ELFS)
	@echo "✓ All programs built successfully"

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

# Generic rule to compile any program's .c file to .o
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Special rule for shell (has shell_main entry point, loads at 1MB)
shell/shell.elf: shell/shell.o $(LIB_OBJS)
	@echo "Linking shell.elf..."
	$(LD) $(LDFLAGS) -Ttext=0x100000 -e shell_main -o $@ $< $(LIB_OBJS)
	@echo "✓ shell.elf built successfully"

# Generic rule to link any other program's .o file to .elf (loads at 2MB to avoid shell)
%.elf: %.o $(LIB_OBJS)
	@echo "Linking $@..."
	$(LD) $(LDFLAGS) -Ttext=0x500000 -e main -o $@ $< $(LIB_OBJS)
	@echo "✓ $(notdir $@) built successfully"

# Copy ELF files to programs root directory
$(ROOT_ELFS): $(PROG_ELFS)
	@echo "Copying ELF files to programs root..."
	@for elf in $(PROG_ELFS); do \
		cp $$elf . ; \
		echo "  Copied $$(basename $$elf)"; \
	done

clean:
	@echo "Cleaning programs..."
	rm -f $(PROG_OBJS) $(PROG_ELFS) $(LIB_OBJS) *.elf
	@echo "✓ Programs cleaned"
