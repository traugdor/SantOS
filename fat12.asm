%ifidn __OUTPUT_FORMAT__, elf64
    ; ELF-specific code (no ORG needed)
%else
    org 0x7e00  ; Binary output format - loaded at 0x7e00
%endif
[bits 16]

; FAT12 Driver - reads boot2.bin from FAT12 filesystem and loads it into memory
; Entry point: 0x7e00
; Exit: jumps to 0x8000:0x0000 (boot2.bin location)

start:
    ; Set up segment registers
    cld    
    xor ax, ax
    mov ds, ax          ; DS = 0
    mov es, ax          ; ES = 0 (temporarily)

    ; Now set ES for the directory
    mov ax, 0x9000
    mov es, ax
    xor di, di          ; ES:DI = 0x9000:0x0000

search_boot2_bin:
    ; Initialize directory entry pointer
    mov di, 0
    mov cx, 16           ; Check up to 16 directory entries
 
.check_entry:
    ; Check if this is a file (not a directory or volume label)
    mov al, [es:di + 11]  ; File attributes
    and al, 0x18          ; Check for volume label (0x08) or directory (0x10)
    jnz .next_entry       ; Skip if either bit is set
 
    ; Compare filename (11 bytes)
    push di
    push si
    push cx
    
    mov si, boot2_filename
    mov cx, 11           ; Compare 11 bytes
    cld                  ; Clear direction flag for forward comparison
    ; print es:di all 11 bytes
    push di
    ; print new line
    mov ax, 0x0E00 | 0x0D
    int 0x10
    mov ax, 0x0E00 | 0x0A
    int 0x10
    .print_filename:
        mov ah, 0x0e
        mov al, [es:di]
        int 0x10
        inc di
        dec cx
        jnz .print_filename
    mov al, '|'
    int 0x10
    push si
    mov cx, 11
    .print_dssi:
        mov al, [ds:si]
        int 0x10
        inc si
        dec cx
        jnz .print_dssi
    pop si
    pop di
    mov cx, 11
    repe cmpsb           ; Compare [DS:SI] with [ES:DI]
    
    pop cx
    pop si
    pop di
    
    je .found_boot2      ; If all bytes matched, we found it
 
.next_entry:
    add di, 32           ; Move to next directory entry (32 bytes)
    loop .check_entry    ; Check next entry
 
    ; If we get here, we didn't find the file
    jmp .not_found
 
.not_found:
    ; Print 'N' for not found and halt
    mov ax, 0x0E00 | 'N'
    int 0x10
    jmp $                ; Halt


.found_boot2:
    ; Found BOOT2.BIN! Get file size and starting cluster
    mov ax, [es:di + 28]    ; File size in bytes
    mov bx, [es:di + 26]    ; Starting cluster
    
    ; Load BOOT2.BIN into memory at 0x8000:0x0000
    ; Get sector number from boot2.asm before final compile
    mov ax, bx          ; AX = cluster number
    sub ax, 2           ; Subtract 2
    add ax, 33          ; Add first data sector LBA
    
    ; Convert LBA to CHS
    xor  dx, dx
    mov  bx, 18
    div  bx
    mov  cx, dx
    inc  cx                          ; Sector
    cwd
    mov  bx, 2
    div  bx
    mov  dh, dl                      ; Head
    shl  ah, 6
    xchg al, ah
    or   cx, ax                      ; Cylinder

    pusha
    mov ax, 0x0E00 | 'C'
    mov bx, 0x0000
    mov cx, 1
    mov dx, 0x0000
    int 0x10
    popa

    push es
    push ds
    
    ; Read BOOT2.BIN
    mov ax, 0x0900
    mov es, ax
    mov bx, 0x0000
    
    mov ah, 0x02
    mov al, 4           ; Read 4 sectors ---- must match boot2.asm padding!!!
    mov dl, 0
    int 0x13

    ;find kernel.elf
    pop ds
    pop es
    xor bx, bx
    
    ; Debug: Print 'K' to show we reached kernel search
    pusha
    mov ax, 0x0E00 | 'K'
    int 0x10
    popa
    
search_kernel_elf:
    ; Initialize directory entry pointer
    mov di, 0
    mov cx, 16           ; Check up to 16 directory entries
 
.check_entry:
    ; Check if this is a file (not a directory or volume label)
    mov al, [es:di + 11]  ; File attributes
    and al, 0x18          ; Check for volume label (0x08) or directory (0x10)
    jnz .next_entry       ; Skip if either bit is set
 
    ; Compare filename (11 bytes)
    push di
    push si
    push cx
    
    mov si, kernel_elf_filename
    mov cx, 11           ; Compare 11 bytes
    cld                  ; Clear direction flag for forward comparison
    ; print es:di all 11 bytes
    push di
    ; print new line
    mov ax, 0x0E00 | 0x0D
    int 0x10
    mov ax, 0x0E00 | 0x0A
    int 0x10
    .print_filename:
        mov ah, 0x0e
        mov al, [es:di]
        int 0x10
        inc di
        dec cx
        jnz .print_filename
    mov al, '|'
    int 0x10
    push si
    mov cx, 11
    .print_dssi:
        mov al, [ds:si]
        int 0x10
        inc si
        dec cx
        jnz .print_dssi
    pop si
    pop di
    mov cx, 11
    repe cmpsb           ; Compare [DS:SI] with [ES:DI]
    
    pop cx
    pop si
    pop di
    
    je .found_kernel_elf      ; If all bytes matched, we found it
 
.next_entry:
    add di, 32           ; Move to next directory entry (32 bytes)
    loop .check_entry    ; Check next entry
 
    ; If we get here, we didn't find the file
    jmp .not_found
 
.not_found:
    ; Print 'N' for not found and halt
    mov ax, 0x0E00 | 'N'
    int 0x10
    jmp $                ; Halt


.found_kernel_elf:
    ; Found kernel.elf! Get file size and starting cluster
    mov ax, [es:di + 28]
    mov dx, [es:di + 30]    ; File size high word (DX:AX = 32-bit size)

    ; calculate how many sectors we need (round up)
    add ax, 511
    adc dx, 0               ; DX:AX += 511
    mov cx, 512
    div cx                  ; AX = (DX:AX) / 512 = sectors needed
    
    ; Initialize DMA tracking variables
    mov word [sectors_remaining], ax
    ; get lba of file
    mov ax, [es:di + 26]
    add ax, 31
    mov word [current_lba], ax

    ; setup es:bx
    mov ax, 0x2000
    mov es, ax
    mov bx, 0x0000
    mov word [bx_position], bx

    ; while sectors_remaining > 0
    ; print new line
    mov ax, 0x0E00 | 0x0D
    int 0x10
    mov ax, 0x0E00 | 0x0A
    int 0x10
.main_read_loop:    
    ; print a dot to show we're still reading. this is 0x7F4C btw
    mov ax, 0x0E00 | '.'
    int 0x10
    cmp word [sectors_remaining], 0
    jz .read_done

    ; Convert current LBA to CHS
    mov ax, word [current_lba]
    xor dx, dx
    mov bx, 18
    div bx
    inc dl
    mov byte [current_S], dl
    mov bx, 2
    xor dx, dx
    div bx
    mov byte [current_C], al
    mov byte [current_H], dl

    ; figure out how much we can safely read
    ; if current Sector is 1 then we can read a full track (18 sectors)
    ; otherwise we can only read up to the next track boundary
    mov al, 19
    sub al, byte [current_S]
    mov [safe_sectors_per_read], al

    mov al, 128
    sub al, byte [sectors_in_current_DMA]
    cmp al, byte [safe_sectors_per_read]
    ja .nextCmp
    mov byte [safe_sectors_per_read], al
.nextCmp:
    mov al, byte [sectors_remaining]
    cmp al, byte [safe_sectors_per_read]
    ja .use_remaining
    mov byte [safe_sectors_per_read], al
.use_remaining:
    ; no assignment needed. safesectors is already set to the minimum

    mov al, byte [safe_sectors_per_read]
    mov byte [sectors_to_read], al

    ; do disk read
    ; int 0x13 ah=0x02 read sectors from floppy
    mov bx, word [bx_position]
    mov ah, 0x02
    mov al, byte [sectors_to_read]  ; Number of sectors to read
    mov ch, byte [current_C]        ; Cylinder
    mov cl, byte [current_S]        ; Sector (1-18)
    mov dh, byte [current_H]        ; Head
    mov dl, 0                       ; Drive (0x00 = first floppy)
    int 0x13
    jc .read_error

    ; update bx_position
    xor ax, ax
    mov al, byte [sectors_to_read]
    mov bx, 512
    mul bx
    add word [bx_position], ax
    
    ; add sectors to current_lba
    xor ax, ax
    mov al, byte [sectors_to_read]
    add ax, word [current_lba]
    mov word [current_lba], ax
    
    ; add sectors to sectors_in_current_DMA
    mov al, byte [sectors_in_current_DMA]
    add al, byte [sectors_to_read]
    mov byte [sectors_in_current_DMA], al
    
    ; subtract sectors from sectors_remaining
    mov ax, word [sectors_remaining]
    xor bx, bx
    mov bl, byte [sectors_to_read]
    sub ax, bx
    mov word [sectors_remaining], ax

    ; if sectors_in_current_DMA >= max_sectors_per_read
    cmp byte [sectors_in_current_DMA], 128
    jae .dma_complete
    jmp .main_read_loop
.dma_complete:
    ; reset sectors_in_current_DMA
    mov byte [sectors_in_current_DMA], 0
    ; increment es
    mov ax, es
    add ax, 0x1000
    mov es, ax
    ; reset bx
    mov bx, 0x0000
    mov word [bx_position], bx
    
    jmp .main_read_loop

.read_error:
    ; print "error"
    mov ah, 0x0E
    mov al, 'e'
    int 0x10
    mov al, 'r'
    int 0x10
    mov al, 'r'
    int 0x10
    mov al, 'o'
    int 0x10
    mov al, 'r'
    int 0x10
    mov al, 0x0D
    int 0x10
    mov al, 0x0A
    int 0x10
    
.pause:
    jmp .pause
    
.read_done:
    ; print "done"
    mov ah, 0x0E
    mov al, 'd'
    int 0x10
    mov al, 'o'
    int 0x10
    mov al, 'n'
    int 0x10
    mov al, 'e'
    int 0x10
    mov al, 0x0D
    int 0x10
    mov al, 0x0A
    int 0x10

    ; Debug: Print total sectors read
    xor ax, ax
    mov ax, word [sectors_remaining]
    call print_number
    
    ; Debug: Print final ES segment
    mov ax, 0x0E00 | ' '
    int 0x10
    mov ax, es
    call print_hex_word

    ; ---------------
    ; debugging pause
    ; ---------------
    ; jmp .pause
    ; Jump to BOOT2.BIN
    jmp 0x0900:0x0000
    
next_fat12_entry:    
    add di, 32
    dec cx
    cmp cx, 0
    jnz search_boot2_bin

exit:
    jmp exit

; --------------------
; Print number in AX (decimal)
print_number:
    push ax
    push bx
    push cx
    push dx
    
    mov cx, 0              ; Digit counter
    mov bx, 10             ; Divide by 10

.divide_loop:
    xor dx, dx
    div bx                 ; AX = quotient, DX = remainder
    push dx                ; Save digit
    inc cx                 ; Count digits
    test ax, ax
    jnz .divide_loop
    
.print_loop:
    pop ax                 ; Get digit
    add al, '0'            ; Convert to ASCII
    mov ah, 0x0E
    int 0x10
    loop .print_loop
    
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; --------------------
; Print hex byte in AL
print_hex_byte:
    push ax
    
    ; high nibble
    mov ah, al
    shr ah, 4
    mov al, ah
    call .print_nibble
    
    ; low nibble
    pop ax
    push ax
    and al, 0x0F
    call .print_nibble
    
    pop ax
    ret

.print_nibble:
    cmp al, 9
    jbe .digit
    add al, 'A' - 10
    jmp .print
.digit:
    add al, '0'
.print:
    mov ah, 0x0E
    int 0x10
    ret

; --------------------
; print hex word
; --------------------
; Print hex word in AX
print_hex_word:
    push ax

    mov al, ah
    call print_hex_byte   ; high byte

    pop ax
    call print_hex_byte   ; low byte

    ret

; FAT12 8.3 filename (11 bytes)
boot2_filename          db "BOOT2   BIN"
kernel_elf_filename     db "KERNEL  ELF"

; DMA boundary variables for kernel loading more than 64kb
sectors_remaining       dw 0    ; Loop Control
sectors_in_current_DMA  dw 0    ; Sectors in current DMA transfer
current_lba             dw 0    ; Current LBA for recalculating CHS
sectors_to_read         db 0    ; Sectors to read in current batch
bx_position             dw 0    ; Position in BX for DMA
current_C               db 0    ; Current cylinder
current_H               db 0    ; Current head
current_S               db 0    ; Current sector
max_sectors_per_DMA     dw 128  ; Maximum sectors per DMA transfer
safe_sectors_per_read   db 0    ; Safe sectors to read at once

; CHS addressing variables
cylinder                db 0    ; Current cylinder
head                    db 0    ; Current head
sector                  db 0    ; Current sector

; Pad to 1024 bytes (two sectors)
times 1024 - ($ - $$) db 0
