#!/bin/bash
# Create a 1.44MB FAT12 floppy disk image with bootloader and kernel

set -e  # Exit on error

echo "Creating 1.44MB FAT12 disk image..."

# Clean up from previous runs
rm -f disk.img

# Create empty disk image (1.44MB = 2880 sectors × 512 bytes)
dd if=/dev/zero of=disk.img bs=512 count=2880 status=progress

echo "Formatting as FAT12..."
mkfs.vfat -F 12 -n "SANTOS" disk.img

echo "Copying files to FAT12 filesystem..."
mmd -i disk.img ::/BOOT2
mcopy -i disk.img boot2.bin ::/BOOT2/
mcopy -i disk.img kernel.elf ::/KERNEL.ELF

echo "Creating test file..."
cat > test.txt << 'EOF'
Hello from SantOS filesystem!
This file was read from the FAT12 disk.
If you can see this, the filesystem driver works!
EOF

mcopy -i disk.img test.txt ::/TEST.TXT

echo -e "\nFiles on disk:"
mdir -i disk.img ::

echo -e "\nInstalling bootloader with BPB preservation..."

# Create a temporary file in the current directory
BPB_TEMP="fat12_bpb.tmp"

# Save the FAT12 BPB (bytes 3-61, 59 bytes)
dd if=disk.img of="$BPB_TEMP" bs=1 skip=3 count=59 2>/dev/null

# Write boot.bin to sector 0 (overwrites BPB temporarily)
dd if=boot.bin of=disk.img conv=notrunc bs=512 count=1 2>/dev/null

# Restore the FAT12 BPB (bytes 3-61)
dd if="$BPB_TEMP" of=disk.img bs=1 seek=3 count=59 conv=notrunc 2>/dev/null
rm -f "$BPB_TEMP"  # Clean up

echo -e "\n✓ boot.bin at sector 0 (with BPB preserved)"
echo "✓ boot2.bin in FAT filesystem as /BOOT2.BIN"
echo "✓ kernel.elf in FAT filesystem as /KERNEL.ELF"
echo "✓ test.txt in FAT filesystem as /TEST.TXT"

# Create hexdump with LBA and CHS numbers for disk image debugging
hexdump -vC disk.img | awk '
    BEGIN { 
        line_count = 0
        sectors_per_track = 18
        heads = 2
    }
    /^[0-9a-f]/ {  # Only process lines with hex data
        # Calculate LBA: (line_count / 32) since each LBA is 32 lines
        lba = int(line_count / 32)
        
        # Calculate CHS from LBA
        cylinder = int(lba / (sectors_per_track * heads))
        temp = lba % (sectors_per_track * heads)
        head = int(temp / sectors_per_track)
        sector = (temp % sectors_per_track) + 1  # Sectors are 1-based
        
        # Print with LBA and CHS at the end
        printf "%s  LBA:%-4d  CHS:%d/%d/%d\n", $0, lba, cylinder, head, sector
        line_count++
        next
    }
    { print }  # Print non-hex lines as-is
' > imgdump

# Calculate actual disk size in KB
DISK_KB=$(( ( $(stat -c%s disk.img) + 1023 ) / 1024 ))

echo -e "\n✓ Disk image created: disk.img (${DISK_KB}KB)"
echo -e "\nTo test in QEMU:"
echo "  qemu-system-x86_64 -fda disk.img"
echo -e "\nTo test in VirtualBox:"
echo "  1. Create new VM (Type: Other, Version: Other/Unknown 64-bit)"
echo "  2. Attach disk.img as floppy disk"
echo "  3. Boot!"