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
; Read fat12.sys from FAT12 filesystem


end:
    ; Jump to second stage
    hlt
    jmp 0x8000

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

; print_decimal:
;     cmp ax, 0
;     jne .convert
;     mov al, '0'
;     mov ah, 0x0E
;     int 0x10
;     ret

; .convert:
;     xor cx, cx          ; digit counter = 0
;     mov bx, 10

; .loop:
;     xor dx, dx
;     div bx
;     push dx
;     inc cx
;     test ax, ax
;     jnz .loop

; .print_loop:
;     pop ax              ; Pop digit into AL
;     add al, '0'         ; Convert to ASCII
;     mov ah, 0x0E
;     int 0x10
;     loop .print_loop
;     ret

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
filename_fat12      db "FAT12   SYS"  ; 8.3 filename for FAT12 MUST BE 11 BYTES


; --------------------
; Boot sector padding 
times 510 - ($ - $$) db 0
dw 0xAA55