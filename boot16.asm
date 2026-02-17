%ifidn __OUTPUT_FORMAT__, elf64
    ; ELF-specific code (no ORG needed)
%else
    org 0x7C00  ; Binary output format
%endif
[bits 16]

; Jump over BPB (BIOS Parameter Block) - mkfs.vfat will fill bytes 3-61
jmp short boot_start
nop

; FAT16 BIOS Parameter Block (BPB) - bytes 3-61
; These will be filled by mkfs.vfat when formatting the disk
; thus they can be ignored here and left empty

; Pad to byte 62 (BPB ends at byte 61, code starts at 62)
times 62-($-$$) db 0

boot_start:
    cli
    cld
    xor ax, ax 
    mov ds, ax
    mov es, ax
    mov ax, 0x7000
    mov ss, ax
    mov sp, 0x0100
    sti
    push dx

; --------------------
; Reset disk
reset_disk:
    mov ah, 0
    int 0x13
    jc reset_disk

; --------------------
; Display first message
    mov si, bootloader_started
    call print_string

; --------------------
; Display second message
    mov si, disk_reset
    call print_string

; --------------------
; Display third message
    mov si, loading_os
    call print_string

; --------------------
; Enable A20 line (required for x86_64)
    call enable_a20

; --------------------
; Read FAT16.SYS from FAT16 filesystem
; FAT16 has different layout than FAT12
; Root directory starts after FATs
; For typical FAT16: Reserved + (FATs * FAT_size) = root dir start

    ; Read BPB to get filesystem parameters
    mov ax, word [0x7C0E]   ; Reserved sectors
    mov bx, word [0x7C16]   ; Sectors per FAT
    mov cl, byte [0x7C10]   ; Number of FATs
    
    ; Calculate root directory LBA: reserved + (FATs * sectors_per_FAT)
    push ax
    mov al, cl
    mul bx                  ; AX = FATs * sectors_per_FAT
    pop bx
    add ax, bx              ; AX = root directory LBA
    
    ; Load root directory to 0x0800:0x0000
    mov bx, 0x0800
    mov es, bx
    xor bx, bx
    
    ; Convert LBA to CHS
    xor dx, dx
    push bx
    mov bx, 18              ; Sectors per track (assuming standard)
    div bx
    pop bx
    mov cx, dx
    inc cx                  ; Sector
    cwd
    push bx
    mov bx, 2               ; Heads
    div bx
    pop bx
    mov dh, dl              ; Head
    shl ah, 6
    xchg al, ah
    or cx, ax               ; Cylinder
    
    mov ah, 0x02
    mov al, 1               ; Read 1 sector of root directory
    mov dl, 0
    int 0x13
    jc end
    
    ; Search for FAT16.SYS in root directory
    mov di, 0
    mov cx, 16
.search_fat16:
    cmp byte [es:di], 0
    je end
    mov al, [es:di + 11]
    and al, 0x10
    jnz .next_fat16
    push di
    push cx
    mov si, filename_fat16
    mov cx, 11
    repe cmpsb
    pop cx
    pop di
    je .found_fat16
.next_fat16:
    add di, 32
    dec cx
    jnz .search_fat16
    jmp end
    
.found_fat16:
    ; Load FAT16.SYS to 0x7E00
    ; Get cluster and convert to LBA
    mov ax, [es:di + 26]    ; Get starting cluster
    
    ; Calculate data area start
    ; data_start = reserved + (FATs * sectors_per_FAT) + root_dir_sectors
    mov bx, word [0x7C0E]   ; Reserved sectors
    push ax
    mov ax, word [0x7C16]   ; Sectors per FAT
    mov cl, byte [0x7C10]   ; Number of FATs
    mul cl
    add bx, ax              ; BX = reserved + FAT area
    
    ; Add root directory sectors
    mov ax, word [0x7C11]   ; Root entry count
    shl ax, 5               ; * 32 bytes per entry
    mov cx, word [0x7C0B]   ; Bytes per sector
    xor dx, dx
    div cx                  ; AX = root dir sectors
    add bx, ax              ; BX = data area start LBA
    
    pop ax                  ; Restore cluster number
    sub ax, 2               ; Cluster 2 is first data cluster
    mov cl, byte [0x7C0D]   ; Sectors per cluster
    mul cl                  ; AX = cluster offset in sectors
    add ax, bx              ; AX = LBA of cluster
    
    ; Convert LBA to CHS
    xor dx, dx
    push bx
    mov bx, 18
    div bx
    pop bx
    mov cx, dx
    inc cx                  ; Sector
    cwd
    push bx
    mov bx, 2
    div bx
    pop bx
    mov dh, dl              ; Head
    shl ah, 6
    xchg al, ah
    or cx, ax               ; Cylinder
    
    mov ax, 0x0000
    mov es, ax
    mov bx, 0x7E00
    mov ah, 0x02
    mov al, 1
    mov dl, 0
    int 0x13
    
    ; Jump to FAT16.SYS
    jmp 0x0000:0x7E00

end:
    hlt
    jmp $

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
; Print newline
print_newline:
    mov al, 0x0D
    mov ah, 0x0E
    int 0x10
    mov al, 0x0A
    mov ah, 0x0E
    int 0x10
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
bootloader_started  db "Bootloader started!", 0
disk_reset          db 0x0D, 0x0A, "Disk reset!", 0
loading_os          db 0x0D, 0x0A, "Loading OS...", 0

; FAT16 8.3 filenames (11 bytes each, padded with spaces)
filename_fat16      db "FAT16   SYS"

; --------------------
; Boot sector padding 
times 510 - ($ - $$) db 0
dw 0xAA55
