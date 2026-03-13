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

cylinder db 0
sector db 0
head db 0

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
; Enable A20 line (required for x86_64)
    call enable_a20

; --------------------
; Read FAT12.BIN from FAT12 filesystem

    ; Load BOOT folder contents directly from LBA 33
    mov ax, 33              ; LBA of BOOT folder on FAT12 floppy
    xor dx, dx
    mov bx, 18
    div bx
    inc dx
    mov [sector], dl        ; save 1-based sector number
    cwd                     ; clear dx
    mov bx, 2
    div bx
    mov [head], dl          ; save 0-based head number
    mov [cylinder], al      ; save cylinder number

    mov ax, 0x9000
    mov es, ax
    xor bx, bx
    
    mov ah, 0x02
    mov al, 1               ; Read 1 sector
    mov dl, 0
    mov ch, [cylinder]
    mov cl, [sector]
    mov dh, [head]
    int 0x13
    jc end
    
    ; Search for FAT12.BIN in BOOT folder
    mov di, 0
    mov cx, 16
.search_fat12:
    cmp byte [es:di], 0
    je end
    mov al, [es:di + 11]
    and al, 0x10
    jnz .next_fat12
    push di
    push cx
    mov si, filename_fat12
    mov cx, 11
    repe cmpsb
    pop cx
    pop di
    je .found_fat12
.next_fat12:
    add di, 32
    dec cx
    jnz .search_fat12
    jmp end
    
.found_fat12:
    ; Load FAT12.BIN to 0x7E00
    mov ax, [es:di + 26]    ; Get cluster
    sub ax, 2
    add ax, 33              ; LBA of BOOT folder on FAT12 floppy
    xor dx, dx
    mov bx, 18
    div bx
    inc dx
    mov [sector], dl        ; save 1-based sector number
    cwd                     ; clear dx
    mov bx, 2
    div bx
    mov [head], dl          ; save 0-based head number
    mov [cylinder], al      ; save cylinder number

    mov ax, 0x0000
    mov es, ax
    mov bx, 0x7E00
    
    mov ah, 0x02
    mov al, 2               ; Might need to read more if fat12.bin grows
    mov dl, 0
    mov ch, [cylinder]
    mov cl, [sector]
    mov dh, [head]
    int 0x13
    
    ; Jump to FAT12.BIN
    jmp 0x0000:0x7E00

end:
    hlt
    jmp $

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

; FAT12 8.3 filenames (11 bytes each, padded with spaces)
filename_fat12      db "FAT12   SYS"

; --------------------
; Boot sector padding 
times 510 - ($ - $$) db 0
dw 0xAA55