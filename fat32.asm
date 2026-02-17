%ifidn __OUTPUT_FORMAT__, elf64
    ; ELF-specific code (no ORG needed)
%else
    org 0x7e00  ; Binary output format - loaded at 0x7e00
%endif
[bits 16]

; FAT32 Driver - reads boot2.bin and kernel.elf from FAT32 filesystem
; Entry point: 0x7e00
; Exit: jumps to 0x9000:0x0000 (boot2.bin location)

start:
    ; Set up segment registers
    cld    
    xor ax, ax
    mov ds, ax          ; DS = 0
    mov es, ax          ; ES = 0 (temporarily)

    ; Now set ES for the directory
    mov ax, 0x0800
    mov es, ax
    xor di, di          ; ES:DI = 0x0800:0x0000

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
    mov ax, [es:di + 28]    ; File size in bytes (low word)
    mov bx, [es:di + 26]    ; Starting cluster (low word)
    ; Ignore high cluster word - assume cluster < 65536
    
    ; Calculate data area start
    push bx                     ; Save cluster
    mov bx, word [0x7C0E]       ; Reserved sectors
    mov cx, word [0x7C24]       ; Sectors per FAT (low word)
    mov dl, byte [0x7C10]       ; Number of FATs
    push ax
    mov al, dl
    mul cx
    add bx, ax                  ; BX = data area start LBA
    pop ax
    pop ax                      ; Restore cluster to AX
    
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
    mov ax, [es:di + 28]    ; File size in bytes (low word)
    ; calculate how many sectors we need
    add ax, 511             ; round up to nearest sector
    push bx
    mov bx, 512
    xor dx, dx
    div bx                  ; AX = sectors needed
    pop bx
    mov cx, ax              ; CX = sectors needed
    
    ; Get starting cluster (use low word only)
    mov ax, [es:di + 26]    ; Low cluster word
    ; Ignore high cluster word - assume cluster < 65536
    
    ; Calculate data area start
    push cx                 ; Save sector count
    push ax                 ; Save cluster
    mov bx, word [0x7C0E]   ; Reserved sectors
    mov cx, word [0x7C24]   ; Sectors per FAT (low word)
    mov dl, byte [0x7C10]   ; Number of FATs
    push ax
    mov al, dl
    mul cx
    add bx, ax              ; BX = data area start LBA
    pop ax
    pop ax                  ; Restore cluster
    
    ; Convert cluster to LBA
    sub ax, 2
    mov cl, byte [0x7C0D]   ; Sectors per cluster
    mul cl
    add ax, bx              ; AX = LBA
    mov si, ax              ; SI = starting LBA
    pop cx                  ; CX = total sectors to read
    
    mov ax, 0x2000
    mov es, ax
    xor bx, bx              ; ES:BX = 0x2000:0x0000
    
.read_loop:
    ; Calculate sectors remaining on current track
    mov ax, si              ; Current LBA
    xor dx, dx
    push bx
    mov bx, 18              ; Sectors per track
    div bx                  ; AX = track, DX = sector on track
    pop bx
    mov ax, 18
    sub ax, dx              ; AX = sectors left on track
    
    ; Read min(sectors_left_on_track, sectors_remaining)
    cmp ax, cx
    jbe .use_track_limit
    mov ax, cx
.use_track_limit:
    push ax                 ; Save sector count for this read
    push cx                 ; Save total remaining
    
    ; Convert current LBA to CHS
    mov ax, si
    xor dx, dx
    push bx
    mov bx, 18              ; Sectors per track
    div bx
    pop bx
    mov cx, dx
    inc cx                  ; Sector
    
    cwd
    push bx
    mov bx, 2               ; Number of heads
    div bx
    pop bx
    mov dh, dl              ; Head
    shl ah, 6
    xchg al, ah
    or cx, ax               ; Cylinder
    
    ; Read sectors
    pop di                  ; DI = total remaining
    pop ax                  ; AX = sectors to read this time
    
    ; Debug: Print dot for each loop iteration
    push ax
    mov ah, 0x0E
    mov al, '.'
    int 0x10
    pop ax
    
    push di
    push ax
    mov ah, 0x02
    mov dl, 0
    int 0x13
    jc .read_error
    
    pop ax                  ; Sectors just read
    pop di                  ; Total remaining
    mov cx, di              ; CX = total remaining for next iteration
    
    ; Update for next iteration
    add si, ax              ; Next LBA
    sub cx, ax              ; Remaining sectors
    
    ; Update ES:BX pointer
    push dx
    mov dx, ax              ; DX = sectors read
    shl dx, 5               ; DX = sectors * 32 (paragraphs)
    mov ax, es
    add ax, dx
    mov es, ax
    pop dx
    ; BX stays at 0
    
    test cx, cx
    jnz .read_loop
    jmp .read_done
    
.read_error:
    pop ax
    pop cx
    jmp exit
    
.read_done:
    
    ; Jump to BOOT2.BIN
    jmp 0x0900:0x0000
    
next_fat32_entry:    
    add di, 32
    dec cx
    cmp cx, 0
    jnz search_boot2_bin

exit:
    jmp exit

; FAT32 8.3 filename (11 bytes)
boot2_filename          db "BOOT2   BIN"
kernel_elf_filename     db "KERNEL  ELF"

; Pad to 512 bytes (one sector)
times 512 - ($ - $$) db 0
