#!/bin/bash
# Create a 32MB FAT16 disk image with bootloader and kernel

set -e  # Exit on error

echo "Creating 32MB FAT16 disk image..."

# Size in MB
DISK_SIZE=32

# Create empty disk image
dd if=/dev/zero of=disk.img bs=1M count=$DISK_SIZE status=progress

echo "Formatting as FAT16..."
# Format as FAT16 (let mkfs.vfat choose reserved sectors)
# -F 16 = FAT16, -n = volume label
# mkfs.vfat typically uses 8 reserved sectors for FAT16
mkfs.vfat -F 16 -n "SANTOS" disk.img

echo "Copying files to FAT16 filesystem..."

# Try mtools first
if command -v mcopy &> /dev/null; then
    echo "Using mtools..."
    
    # Copy boot2 and kernel to FAT16 filesystem (they'll be in the data area)
    mcopy -i disk.img boot2.bin ::BOOT2.BIN
    mcopy -i disk.img kernel.elf ::KERNEL.ELF
    
    echo "Creating test file..."
    echo "Hello from SantOS filesystem!" > test.txt
    echo "This file was read from the FAT16 disk." >> test.txt
    echo "If you can see this, the filesystem driver works!" >> test.txt
    mcopy -i disk.img test.txt ::TEST.TXT
    
    echo "Listing files on disk..."
    mdir -i disk.img ::
    
    echo ""
    echo "Installing bootloader with BPB preservation..."
    
    # Save the FAT16 BPB (bytes 3-61, 59 bytes)
    # This includes OEM name, bytes per sector, sectors per cluster, etc.
    dd if=disk.img of=/tmp/fat16_bpb.bin bs=1 skip=3 count=59 2>/dev/null
    
    # Write boot.bin to sector 0 (overwrites BPB temporarily)
    dd if=boot.bin of=disk.img conv=notrunc bs=512 count=1 2>/dev/null
    
    # Restore the FAT16 BPB (bytes 3-61)
    dd if=/tmp/fat16_bpb.bin of=disk.img bs=1 seek=3 count=59 conv=notrunc 2>/dev/null
    
    echo "✓ boot.bin at sector 0 (with BPB preserved)"
    echo "✓ boot2.bin in FAT filesystem as BOOT2.BIN"
    echo "✓ kernel.elf in FAT filesystem as KERNEL.ELF"
    echo "✓ test.txt in FAT filesystem as TEST.TXT"
    echo ""
    echo "Finding file locations in FAT filesystem..."
    
    # Root directory is at sector 130, 32 sectors = 16KB
    # Each entry is 32 bytes
    echo "Root directory (first 1024 bytes):"
    hexdump -C disk.img -s $((130*512)) -n 1024
    
    echo ""
    echo "Parsing file locations..."
    python3 << 'PARSE_FAT'
import struct

# Read the disk image
with open('disk.img', 'rb') as f:
    # Read BPB to get filesystem parameters
    f.seek(0x0B)
    bytes_per_sector = struct.unpack('<H', f.read(2))[0]
    sectors_per_cluster = struct.unpack('B', f.read(1))[0]
    reserved_sectors = struct.unpack('<H', f.read(2))[0]
    num_fats = struct.unpack('B', f.read(1))[0]
    root_entries = struct.unpack('<H', f.read(2))[0]
    total_sectors_16 = struct.unpack('<H', f.read(2))[0]
    media_type = struct.unpack('B', f.read(1))[0]
    sectors_per_fat = struct.unpack('<H', f.read(2))[0]
    
    # Calculate sector positions
    fat_start = reserved_sectors
    root_dir_start = fat_start + (num_fats * sectors_per_fat)
    data_start = root_dir_start + ((root_entries * 32 + bytes_per_sector - 1) // bytes_per_sector)
    
    print(f"Filesystem parameters:")
    print(f"  Bytes per sector: {bytes_per_sector}")
    print(f"  Sectors per cluster: {sectors_per_cluster}")
    print(f"  Reserved sectors: {reserved_sectors}")
    print(f"  FAT start: sector {fat_start}")
    print(f"  Root dir start: sector {root_dir_start}")
    print(f"  Data area start: sector {data_start}")
    print()
    
    # Read root directory
    f.seek(root_dir_start * bytes_per_sector)
    root_data = f.read(root_entries * 32)
    
    # Parse directory entries
    print("Files in root directory:")
    for i in range(root_entries):
        entry = root_data[i*32:(i+1)*32]
        if entry[0] == 0:
            break
        if entry[0] == 0xE5:  # Deleted entry
            continue
        if entry[11] == 0x0F:  # LFN entry
            continue
        
        filename = entry[0:8].decode('ascii', errors='ignore').strip()
        ext = entry[8:11].decode('ascii', errors='ignore').strip()
        attr = entry[11]
        cluster = struct.unpack('<H', entry[26:28])[0]
        size = struct.unpack('<I', entry[28:32])[0]
        
        if filename or ext:
            full_name = f"{filename}.{ext}" if ext else filename
            sector = data_start + (cluster - 2) * sectors_per_cluster
            print(f"  {full_name}: cluster {cluster}, sector {sector}, size {size}")

PARSE_FAT
else
    echo "mtools not found, using Python fallback..."
    python3 << 'PYTHON_SCRIPT'
import os
import struct

# Mount offset calculation for FAT16
# This is a simplified approach - just append files to the data area
# For a proper implementation, we'd parse the FAT and update it

print("Note: Python fallback is limited. Install mtools for full functionality:")
print("  pacman -S mtools")
print("")
print("For now, kernel will be embedded in bootloader.img")
print("FAT16 filesystem will be empty (but formatted correctly)")
PYTHON_SCRIPT
    
    echo ""
    echo "⚠ Warning: Could not copy files to disk. Install mtools:"
    echo "   pacman -S mtools"
    echo ""
    echo "Disk image created but files not copied."
fi

hexdump -C disk.img | head -n 300 > imgdump

echo ""
echo "✓ Disk image created: disk.img (${DISK_SIZE}MB)"
echo ""
echo "To test in QEMU:"
echo "  qemu-system-x86_64 -drive format=raw,file=disk.img"
echo ""
echo "To test in VirtualBox:"
echo "  1. Create new VM (Type: Other, Version: Other/Unknown 64-bit)"
echo "  2. Attach disk.img as IDE hard disk"
echo "  3. Boot!"
echo ""
echo "To test in VMware:"
echo "  1. Create new VM (Other 64-bit)"
echo "  2. Use existing disk: disk.img"
echo "  3. Boot!"
