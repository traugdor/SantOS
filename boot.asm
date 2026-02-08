[org 0x7C00]
[bits 16]

; Jump over BPB (BIOS Parameter Block) - mkfs.vfat will fill bytes 3-61
jmp short boot_start
nop

; FAT16 BIOS Parameter Block (BPB) - bytes 3-61
; These will be filled by mkfs.vfat when formatting the disk
OEMLabel:           db "MSWIN4.1"      ; Offset 3, 8 bytes
BytesPerSector:     dw 512             ; Offset 11
SectorsPerCluster:  db 1               ; Offset 13
ReservedSectors:    dw 1               ; Offset 14
NumberOfFATs:       db 2               ; Offset 16
RootEntries:        dw 512             ; Offset 17
TotalSectors16:     dw 0               ; Offset 19
MediaType:          db 0xF8            ; Offset 21
SectorsPerFAT:      dw 0               ; Offset 22
SectorsPerTrack:    dw 0               ; Offset 24
NumberOfHeads:      dw 0               ; Offset 26
HiddenSectors:      dd 0               ; Offset 28
TotalSectors32:     dd 0               ; Offset 32
DriveNumber:        db 0               ; Offset 36
Reserved:           db 0               ; Offset 37
BootSignature:      db 0x29            ; Offset 38
VolumeID:           dd 0               ; Offset 39
VolumeLabel:        db "SANTOS     "   ; Offset 43, 11 bytes
FileSystem:         db "FAT16   "      ; Offset 54, 8 bytes

; Pad to byte 62 (BPB ends at byte 61, code starts at 62)
times 62-($-$$) db 0

boot_start:
    xor ax, ax 
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

; --------------------
; Reset disk
reset_disk:
    mov ah, 0
    int 0x13
    jc reset_disk

; --------------------
; Display first message
    mov si, msg1
    call print_string

; --------------------
; Display second message
    mov si, msg2
    call print_string

; --------------------
; Display third message
    mov si, msg3
    call print_string

; --------------------
; Enable A20 line (required for x86_64)
    call enable_a20

; --------------------
; Read stage2 (load second stage from disk)
; LBA sector 164 (mcopy placed BOOT2.BIN here) = CHS (0, 2, 39)
; Calculation: C=164/(255*63)=0, H=(164/63)%255=2, S=(164%63)+1=39
read_stage2:
    mov ah, 0x02           ; BIOS read sectors function
    mov al, 4              ; Number of sectors to read (boot2.bin = 2048 bytes = 4 sectors)
    mov ch, 0              ; Cylinder 0
    mov dh, 2              ; Head 2
    mov cl, 39             ; Sector 39 (LBA 164 in CHS)
    mov bx, 0x8000         ; Load stage2 at 0x8000
    int 0x13
    jc read_stage2         ; Retry on error

; --------------------
; Read kernel.elf from disk  
; LBA sector 168 (mcopy placed KERNEL.ELF here) = CHS (0, 2, 43)
; Calculation: C=168/(255*63)=0, H=(168/63)%255=2, S=(168%63)+1=43
read_kernel:
    push es
    mov ax, 0x1000         ; Set ES to 0x1000
    mov es, ax
    
    mov ah, 0x02           ; BIOS read sectors function
    mov al, 64             ; Number of sectors (32KB max kernel)
    mov ch, 0              ; Cylinder 0
    mov dh, 2              ; Head 2
    mov cl, 43             ; Sector 43 (LBA 168 in CHS)
    xor bx, bx             ; ES:BX = 0x1000:0x0000 = 0x10000
    int 0x13
    jc read_kernel         ; Retry on error
    
    pop es                 ; Restore ES

; Jump to stage2
jmp 0x0000:0x8000

; --------------------
; Function: print a null-terminated string at [SI]
print_string:
    mov ah, 0x0E
.next_char:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .next_char
.done:
    ret

; --------------------
; Enable A20 line using BIOS and keyboard controller
enable_a20:
    ; Try BIOS method first
    mov ax, 0x2401
    int 0x15
    jnc .done
    
    ; Try keyboard controller method
    call .wait_input
    mov al, 0xAD
    out 0x64, al        ; Disable keyboard
    
    call .wait_input
    mov al, 0xD0
    out 0x64, al        ; Read output port
    
    call .wait_output
    in al, 0x60
    push ax
    
    call .wait_input
    mov al, 0xD1
    out 0x64, al        ; Write output port
    
    call .wait_input
    pop ax
    or al, 2            ; Set A20 bit
    out 0x60, al
    
    call .wait_input
    mov al, 0xAE
    out 0x64, al        ; Enable keyboard
    
    call .wait_input
.done:
    ret

.wait_input:
    in al, 0x64
    test al, 2
    jnz .wait_input
    ret

.wait_output:
    in al, 0x64
    test al, 1
    jz .wait_output
    ret

; --------------------
; Messages to display
msg1 db "Bootloader started successfully!", 0
msg2 db 0x0D, 0x0A, "Disk reset completed!", 0
msg3 db 0x0D, 0x0A, "Loading operating system...", 0

; --------------------
; Boot sector padding 
times 510 - ($ - $$) db 0
dw 0xAA55