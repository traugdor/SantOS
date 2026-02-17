%ifidn __OUTPUT_FORMAT__, elf64
    ; ELF-specific code (no ORG needed)
%else
    org 0x7C00  ; Binary output format
%endif
[bits 16]

; Jump over BPB (BIOS Parameter Block)
jmp short boot_start
nop

; FAT32 BIOS Parameter Block (BPB) - bytes 3-89
; These will be filled by mkfs.vfat when formatting the disk
; thus they can be ignored here and left empty

; Pad to byte 90 (FAT32 BPB ends at byte 89, code starts at 90)
times 90-($-$$) db 0

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
; Read FAT32.SYS from FAT32 filesystem
; FAT32 has root directory in a cluster chain, not fixed location
; Root directory cluster is stored in BPB at offset 0x2C

    ; Get root directory cluster from BPB (32-bit value, use only low 16 bits)
    mov ax, word [0x7C2C]   ; Root directory cluster (low word)
    ; Ignore high word for simplicity - assume cluster < 65536
    
    ; Calculate data area start
    ; data_start = reserved + (FATs * sectors_per_FAT)
    mov bx, word [0x7C0E]       ; Reserved sectors
    mov cx, word [0x7C24]       ; Sectors per FAT (use low word)
    mov dl, byte [0x7C10]       ; Number of FATs
    push ax
    mov al, dl
    mul cx                      ; AX = FATs * sectors_per_FAT (low word)
    add bx, ax                  ; BX = data area start LBA
    pop ax
    
    ; Convert cluster to LBA
    ; LBA = data_start + (cluster - 2) * sectors_per_cluster
    sub ax, 2                   ; Cluster 2 is first data cluster
    mov cl, byte [0x7C0D]       ; Sectors per cluster
    mul cl                      ; AX = cluster offset in sectors
    add ax, bx                  ; AX = LBA of root directory
    
    ; Load root directory to 0x0800:0x0000
    mov bx, 0x0800
    mov es, bx
    xor bx, bx
    
    ; Convert LBA (in AX) to CHS
    xor dx, dx
    push bx
    mov bx, 18                  ; Sectors per track
    div bx                      ; AX = track, DX = sector
    pop bx
    mov cx, dx
    inc cx                      ; Sector (1-based)
    
    cwd
    push bx
    mov bx, 2                   ; Number of heads
    div bx                      ; AX = cylinder, DX = head
    pop bx
    mov dh, dl                  ; Head
    shl ah, 6
    xchg al, ah
    or cx, ax                   ; Cylinder
    
    mov ah, 0x02
    mov al, 1               ; Read 1 sector
    mov dl, 0
    int 0x13
    jc end
    
    ; Search for FAT32.SYS in root directory
    mov di, 0
    mov cx, 16
.search_fat32:
    cmp byte [es:di], 0
    je end
    mov al, [es:di + 11]
    and al, 0x10
    jnz .next_fat32
    push di
    push cx
    mov si, filename_fat32
    mov cx, 11
    repe cmpsb
    pop cx
    pop di
    je .found_fat32
.next_fat32:
    add di, 32
    dec cx
    jnz .search_fat32
    jmp end
    
.found_fat32:
    ; Load FAT32.SYS to 0x7E00
    ; Get starting cluster (use only low word for simplicity)
    mov ax, [es:di + 26]        ; Low cluster word
    ; Ignore high cluster word - assume cluster < 65536
    
    ; Calculate data area start (same as before)
    mov bx, word [0x7C0E]       ; Reserved sectors
    mov cx, word [0x7C24]       ; Sectors per FAT (low word)
    mov dl, byte [0x7C10]       ; Number of FATs
    push ax
    mov al, dl
    mul cx
    add bx, ax                  ; BX = data area start LBA
    pop ax
    
    ; Convert cluster to LBA
    sub ax, 2
    mov cl, byte [0x7C0D]       ; Sectors per cluster
    mul cl
    add ax, bx                  ; AX = LBA
    
    ; Convert LBA to CHS
    xor dx, dx
    push bx
    mov bx, 18                  ; Sectors per track
    div bx
    pop bx
    mov cx, dx
    inc cx                      ; Sector
    
    cwd
    push bx
    mov bx, 2                   ; Number of heads
    div bx
    pop bx
    mov dh, dl                  ; Head
    shl ah, 6
    xchg al, ah
    or cx, ax                   ; Cylinder
    
    mov ax, 0x0000
    mov es, ax
    mov bx, 0x7E00
    mov ah, 0x02
    mov al, 1
    mov dl, 0
    int 0x13
    
    ; Jump to FAT32.SYS
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

; FAT32 8.3 filenames (11 bytes each, padded with spaces)
filename_fat32      db "FAT32   SYS"

; --------------------
; Boot sector padding 
times 510 - ($ - $$) db 0
dw 0xAA55
