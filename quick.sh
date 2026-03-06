#!/bin/bash

./build.sh
./create_disk.sh
qemu-system-x86_64 -m 512 -drive format=raw,file=disk.img,if=floppy