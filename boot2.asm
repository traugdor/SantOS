[bits 16]
%ifidn __OUTPUT_FORMAT__, elf64
    ; ELF-specific code (no ORG needed)
%else
    org 0x8000  ; Binary output format
%endif

start:
    ; Set up stack for boot2
    xor ax, ax
    mov ss, ax
    mov sp, 0x8000
    
    cli                     ; Disable interrupts
    
    ; Check for CPUID support
    call check_cpuid
    
    ; Check for long mode support
    call check_long_mode
    
    ; Print success message
    mov si, msg_checks_ok
    call print_string_16
    call delay_16
    
    ; Setup page tables for identity mapping
    call setup_page_tables
    
    mov si, msg_page_tables_ok
    call print_string_16
    call delay_16
    
    ; Load kernel from disk using BIOS INT 0x13 (after page tables, before protected mode)
    sti                     ; Re-enable interrupts for BIOS calls
    call load_kernel_16
    cli                     ; Disable interrupts again
    
    ; Load GDT
    lgdt [gdt_descriptor]
    
    mov si, msg_entering_pm
    call print_string_16
    call delay_16
    
    ; Enter protected mode
    mov eax, cr0
    or eax, 0x1             ; Set PE bit (Protection Enable)
    mov cr0, eax

    ; Far jump to 32-bit protected mode
    jmp CODE_SEG_32:init_pm
    jmp $

.kernel_load_failed:
    mov si, msg_kernel_load_failed
    call print_string_16
    jmp $

; Check if CPUID is supported
check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21        ; Flip ID bit
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .no_cpuid
    ret
.no_cpuid:
    mov si, error_no_cpuid
    call print_string_16
    jmp $

; Check if long mode is supported
check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29       ; Check LM bit
    jz .no_long_mode
    ret
.no_long_mode:
    mov si, error_no_long_mode
    call print_string_16
    jmp $

; Print string in 16-bit real mode
print_string_16:
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    ret

; Delay function for 16-bit mode (~3 seconds)
delay_16:
    push cx
    push dx
    mov cx, 0x30        ; Outer loop (increased from 0x0F)
.outer:
    mov dx, 0xFFFF      ; Inner loop
.inner:
    dec dx
    jnz .inner
    dec cx
    jnz .outer
    pop dx
    pop cx
    ret

; Setup identity-mapped page tables
setup_page_tables:
    ; Clear page table area (4KB * 3 = 12KB)
    mov edi, 0x1000
    xor eax, eax
    mov ecx, 3072           ; Clear 3 pages (12KB / 4 bytes)
    rep stosd
    
    ; Setup PML4[0] -> PDPT at 0x2000 (64-bit entry)
    mov dword [0x1000], 0x2003 ; Present, writable
    mov dword [0x1004], 0x0000 ; Upper 32 bits
    
    ; Setup PDPT[0] -> PD at 0x3000 (64-bit entry)
    mov dword [0x2000], 0x3003 ; Present, writable
    mov dword [0x2004], 0x0000 ; Upper 32 bits
    
    ; Setup PD entries - map first 8MB with 2MB pages (64-bit entries)
    mov dword [0x3000], 0x00000083  ; PD[0]: 0-2MB, Present, writable, huge
    mov dword [0x3004], 0x00000000  ; Upper 32 bits
    mov dword [0x3008], 0x00200083  ; PD[1]: 2-4MB
    mov dword [0x300C], 0x00000000  ; Upper 32 bits
    mov dword [0x3010], 0x00400083  ; PD[2]: 4-6MB
    mov dword [0x3014], 0x00000000  ; Upper 32 bits
    mov dword [0x3018], 0x00600083  ; PD[3]: 6-8MB
    mov dword [0x301C], 0x00000000  ; Upper 32 bits
    
    ; Set CR3 to PML4 address
    mov eax, 0x1000
    mov cr3, eax
    
    ret

; Load kernel from disk using BIOS INT 0x13
load_kernel_16:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds
    
    ; Reset disk controller
    mov ah, 0
    mov dl, 0           ; Drive 0 (floppy)
    int 0x13
    
; Load root directory (sectors 19-32, 14 sectors)
    mov ax, 0x09C0
    mov es, ax
    xor bx, bx

    mov ah, 0x02           ; Read sectors
    mov al, 14             ; Read 14 sectors (root directory)
    mov ch, 0              ; Cylinder 0
    mov dh, 1              ; Head 1
    mov cl, 2              ; Sector 2 (first sector of root directory)
    int 0x13
    jc .disk_error

; Search for "BOOT2  BIN" in root directory
    mov di, 0              ; Include volume label
    mov cx, 224            ; Max root entries (14 sectors * 16 entries/sector)

.search_loop:
    cmp byte [es:di], 0    ; End of directory
    je .kernel_not_found
    mov si, filename_kernel ; "KERNEL  ELF"
    mov cx, 11             ; Filename length
    push di
    repe cmpsb
    pop di
    je .found_kernel
    add di, 32             ; Next directory entry (32 bytes per entry)
    jmp .search_loop
    
.found_kernel:
    ; Debug: print 'K' for kernel found
    mov ah, 0x0E
    mov al, 'K'
    int 0x10
    
    ; Read kernel - calculate total sectors needed
    mov ax, [es:di + 28] ; File size in bytes
    add ax, 511
    mov cx, 512
    xor dx, dx
    div cx               ; ax = total sectors
    mov cx, ax           ; CX = total sectors to read (use CX instead of BP)
    
    ; Debug: print total sectors as hex
    call print_hex_word  ; Print AX as 4 hex digits
    
    ; Get first sector from directory entry
    mov ax, [es:di + 26] ; First sector
    sub ax, 2
    add ax, 33          ; LBA = (sector - 2) + 33
    mov si, ax          ; SI = current LBA
    
    ; Debug: print 'L' and LBA
    mov ah, 0x0E
    mov al, 'L'
    int 0x10
    mov ax, si
    call print_hex_word
    
    ; Load kernel to 0x1000:0x0000 (physical address 0x10000) like FAT16 version
    mov ax, 0x1000
    mov es, ax
    xor bx, bx         ; BX = buffer offset (0x0000)
    
    ; Save ES before BIOS call (BIOS might clobber it)
    push es
    
    ; Reset DS to 0 for BIOS call
    xor ax, ax
    mov ds, ax

    ; Read kernel respecting track boundaries
    mov ax, 0x1000
    mov es, ax
    xor bx, bx         ; BX = buffer offset
    
    ; CX contains total sectors to read
.read_kernel_loop:
    cmp cx, 0           ; Check if all sectors read
    je .kernel_read_ok
    
    ; Debug: print 'R' for read loop iteration
    mov ah, 0x0E
    mov al, 'R'
    int 0x10
    
    ; Convert current LBA to CHS
    mov ax, si          ; Current LBA
    xor dx, dx
    mov cx, 18
    div cx              ; ax = track, dx = sector within track (0-based)
    
    ; Save sector and track
    mov cl, dl          ; CL = sector within track (0-based)
    inc cl              ; CL = Sector (1-based for BIOS)
    mov bx, ax          ; BX = track
    
    ; Debug: print current LBA
    mov ax, si
    call print_hex_word
    mov ax, bx          ; AX = track (restore)
    
    xor dx, dx
    mov cx, 2
    div cx              ; ax = cylinder, dx = head
    
    mov ch, al          ; Cylinder
    mov dh, dl          ; Head
    
    ; CL still has the sector value from before
    ; (CL wasn't modified by the second division since we used CX=2)
    
    ; Debug: print CHS (C:H:S)
    mov ah, 0x0E
    mov al, ':'
    int 0x10
    mov al, ch          ; Cylinder
    call print_hex_byte
    mov al, ':'
    int 0x10
    mov al, dh          ; Head
    call print_hex_byte
    mov al, ':'
    int 0x10
    mov al, cl          ; Sector
    call print_hex_byte
    
    ; Calculate sectors remaining on this track
    ; Remaining = 18 - sector + 1 (sectors are 1-based)
    ; so if we're at sector 2, we can read 17 sectors (2-18)
    mov al, 19
    sub al, cl          ; AL = sectors remaining on track (19 - sector)
    
    ; Read min(remaining_on_track, sectors_needed)
    cmp al, cl          ; Compare remaining on track with sectors needed (CL = low byte of CX)
    jle .read_track_end ; If remaining on track <= needed, read to end of track
    
    ; Otherwise read only what's needed
    mov al, cl
    
.read_track_end:
    ; AL = sectors to read
    mov bl, al          ; Save in BL
    
    ; Debug: print sectors to read
    mov ah, 0x0E
    mov al, '='
    int 0x10
    mov al, bl
    call print_hex_byte
    
    pop es              ; Restore ES
    
    ; Debug: print registers before INT 0x13
    mov ah, 0x0E
    mov al, '['
    int 0x10
    mov al, ch          ; Cylinder
    call print_hex_byte
    mov al, dh          ; Head
    call print_hex_byte
    mov al, cl          ; Sector
    call print_hex_byte
    mov al, ']'
    int 0x10
    
    mov ah, 0x02        ; Read sectors command
    mov al, bl          ; Sector count
    mov dl, 0           ; Drive 0
    
    int 0x13
    jc .kernel_read_error_debug
    
    ; Debug: print 'S' for successful read
    mov ah, 0x0E
    mov al, 'S'
    int 0x10
    
    ; Update counters
    movzx ax, bl        ; AX = sectors we requested (from BL)
    sub cx, ax          ; Decrease remaining sectors in CX
    add si, ax          ; Increase LBA
    
    ; Update buffer address
    mov ax, 0x200
    mul al              ; AX = bytes read
    add bx, ax
    
    ; Check for 64KB boundary crossing
    cmp bx, 0xFFFF
    jle .read_kernel_loop
    
    ; Wrap to next segment
    mov ax, es
    add ax, 0x1000      ; Add 64KB
    mov es, ax
    xor bx, bx
    
    jmp .read_kernel_loop

.kernel_read_ok:
    
    ; Verify kernel was loaded correctly (check ELF magic)
    mov ax, 0x1000
    mov es, ax
    xor bx, bx
    mov ax, [es:bx]         ; Read from ES:BX
    
    ; Check for ELF magic
    cmp ax, 0x457F          ; Check first 2 bytes of "\x7FELF"
    jne .kernel_load_failed
    
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

.root_dir_read_error:
    mov ah, 0x0E
    mov al, 'R'
    int 0x10
    jmp $

.kernel_not_found:
    mov ah, 0x0E
    mov al, 'N'
    int 0x10
    jmp $

.kernel_read_error_debug:
    ; Print error code in AH
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    
    ; Print error code as hex
    mov al, ah          ; AH has error code from INT 0x13
    shr al, 4
    cmp al, 10
    jl .error_digit_0_9
    add al, 'A' - 10
    jmp .error_print_digit
.error_digit_0_9:
    add al, '0'
.error_print_digit:
    mov ah, 0x0E
    int 0x10
    jmp $

.kernel_read_error:
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    jmp $

.kernel_load_failed:
    mov ah, 0x0E
    mov al, 'F'
    int 0x10
    jmp $

.disk_error:
    push si
    mov si, msg_disk_error
    call print_string_16
    pop si
    jmp $

; Data storage
kernel_sectors_remaining db 0

; Messages
error_no_cpuid db 'ERROR: CPUID not supported', 0
error_no_long_mode db 0x0D, 0x0A, 'ERROR: Long mode not supported', 0
msg_checks_ok db 0x0D, 0x0A, 'CPU checks passed', 0x0D, 0x0A, 0
msg_page_tables_ok db 'Page tables OK', 0x0D, 0x0A, 0
msg_entering_pm db 'Entering PM', 0x0D, 0x0A, 0
msg_entering_64bit db 'Entering 64-bit', 0x0D, 0x0A, 0
msg_kernel_load_failed db 'Kernel load failed', 0
msg_kernel_entry db 'Kernel entry: 0x', 0
filename_kernel db 'KERNEL  ELF', 0


[bits 32]
init_pm:
    ; Set segment registers to DATA segment
    mov ax, DATA_SEG_32
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000        ; Set stack pointer to high memory
    
    ; Enable PAE (Physical Address Extension)
    mov eax, cr4
    or eax, 1 << 5          ; Set PAE bit
    mov cr4, eax
    
    ; Load CR3 with page table address
    mov eax, 0x1000
    mov cr3, eax
    
    ; Set long mode bit in EFER MSR
    mov ecx, 0xC0000080     ; EFER MSR
    rdmsr
    or eax, 1 << 8          ; Set LM bit
    wrmsr
    
    ; Enable paging (this activates long mode)
    mov eax, cr0
    or eax, 1 << 31         ; Set PG bit
    mov cr0, eax
    
    ; Load 64-bit GDT
    lgdt [gdt64_descriptor]
    
    ; Far jump to 64-bit code segment
    jmp CODE_SEG_64:long_mode_start

[bits 64]
long_mode_start:
    ; Set up segment registers for 64-bit
    mov ax, DATA_SEG_64
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Clear the screen
    call clear_screen
    
    ; Display welcome message
    mov rsi, msg
    mov rdi, 0xB8000        ; VGA text mode buffer
    mov ah, 0x0F            ; Text attribute: white on black
    call print_string_pm
    
    ; Display loading kernel message
    mov rdi, 0xB8000 + 160  ; Second line
    mov rsi, msg_loading_kernel
    mov ah, 0x0F
    call print_string_pm

    ; Kernel is already loaded at 0x10000 by boot2 in 16-bit mode
    ; Just verify the ELF magic number
    mov rax, [0x10000]
    mov rdi, 0xB8000 + 320  ; Third line
    call print_hex
    
    ; Check ELF magic number
    cmp dword [0x10000], 0x464C457F  ; "\x7FELF"
    jne .elf_error
    
    ; Get entry point from ELF header
    mov rax, [0x10018]      ; e_entry field in ELF64 header
    mov [kernel_entry], rax
    
    ; Debug: Display kernel entry point
    mov rdi, 0xB8000 + 480  ; Fourth line
    mov rsi, msg_kernel_entry
    call print_string_pm
    mov rax, [kernel_entry]
    mov rdi, 0xB8000 + 480 + 40
    call print_hex
    
    ; Get program header info
    movzx rcx, word [0x10038]  ; e_phnum
    mov rsi, [0x10020]         ; e_phoff
    add rsi, 0x10000           ; Add base address to get program header location
    
    ; Loop through program headers
.load_segments:
    ; Check if this is a loadable segment (p_type = 1)
    cmp dword [rsi], 1
    jne .next_segment
    
    ; Debug: Show segment info
    push rsi
    mov rdi, 0xB8000 + 480  ; Fourth line
    mov rax, [rsi + 0x10]   ; p_paddr
    call print_hex
    pop rsi
    
    ; Load segment into memory
    mov rdi, [rsi + 0x10]   ; p_paddr (physical address)
    mov rcx, [rsi + 0x28]   ; p_filesz
    mov rdx, [rsi + 0x08]   ; p_offset
    add rdx, 0x10000        ; Add base address
    push rsi
    mov rsi, rdx
    rep movsb
    pop rsi
    
.next_segment:
    add rsi, 0x38           ; Move to next program header (size = 0x38)
    dec rcx
    jnz .load_segments
    
    ; Set up stack for kernel (16KB stack at 0x80000)
    mov rsp, 0x80000
    
    ; Debug: Show we're about to jump to kernel
    mov rdi, 0xB8000 + 640  ; Fifth line
    mov rsi, msg_jumping_kernel
    mov ah, 0x0A
    call print_string_pm
    
    ; Small delay to see the message
    mov rcx, 0x1000000
.delay:
    dec rcx
    jnz .delay
    
    ; Jump to kernel entry point
    mov rax, [kernel_entry]
    jmp rax

.elf_error:
    ; Display error message
    mov rdi, 0xB8000 + 480
    mov rsi, msg_elf_error
    mov ah, 0x4C            ; Red on black
    call print_string_pm
    hlt
    jmp $

; Clear the screen (fills with spaces)
clear_screen:
    mov rdi, 0xB8000
    mov rcx, 80*25          ; Total screen characters
    mov ax, 0x0F20          ; Space character with white-on-black attribute
    rep stosw
    ret

; Print string in protected mode
print_string_pm:
    lodsb                   ; Load byte from [RSI] to AL
    test al, al
    jz .done    
    stosw                   ; Store AL and AH to [RDI]
    jmp print_string_pm
.done:
    ret

; Print 64-bit hex value in RAX at RDI
print_hex:
    push rax
    push rbx
    push rcx
    push rdx
    
    mov rcx, 16             ; 16 hex digits
    mov rbx, rax
.loop:
    rol rbx, 4              ; Rotate left 4 bits
    mov rax, rbx
    and rax, 0xF            ; Get lowest 4 bits
    add al, '0'
    cmp al, '9'
    jle .digit
    add al, 7               ; Convert A-F
.digit:
    mov ah, 0x0F            ; White on black
    stosw
    dec rcx
    jnz .loop
    
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; Delay for 64-bit mode
delay_64:
    push rcx
.loop:
    dec rcx
    jnz .loop
    pop rcx
    ret


; Data
msg db 'Hello, World from 64-bit long mode!', 0
msg_success db 'Successfully booted into x86_64 long mode!', 0
msg_loading_kernel db 'Loading kernel.elf...', 0
msg_in_load_kernel db 'In load_kernel', 0
msg_kernel_loaded db 'Kernel loaded!', 0
msg_before_10000 db 'At 0xFFFE: ', 0
msg_elf_at_10000 db 'ELF at 0x10000: ', 0
msg_jumping_kernel db 'Jumping to kernel...', 0
msg_entry_point db 'Entry: ', 0
msg_copy_src db 'Copy src: ', 0
msg_copy_dst db 'Copy dst: ', 0
msg_code_check db 'Code at entry: ', 0
msg_elf_error db 'ERROR: Invalid ELF file!', 0
msg_disk_error db 'DISK ERR: 0x', 0
msg_reading_disk db 'Reading disk...', 0
msg_read_ok db 'Read OK', 0
msg_in_read_disk db 'In read_disk', 0
; Helper functions for debug output
print_hex_word:
    ; Print AX as 4 hex digits
    push ax
    mov al, ah
    call print_hex_byte
    pop ax
    call print_hex_byte
    ret

print_hex_byte:
    ; Print AL as 2 hex digits
    push ax
    shr al, 4
    call print_hex_digit
    pop ax
    and al, 0x0F
    call print_hex_digit
    ret

print_hex_digit:
    ; Print AL (0-15) as single hex digit
    cmp al, 10
    jl .digit_0_9
    add al, 'A' - 10
    jmp .digit_print
.digit_0_9:
    add al, '0'
.digit_print:
    mov ah, 0x0E
    int 0x10
    ret

msg_timeout db 'DISK TIMEOUT', 0
msg_disk_init_timeout db 'DISK INIT TIMEOUT', 0
msg_fdc_timeout db 'FDC TIMEOUT', 0
msg_fdc_ready db 'FDC READY', 0
msg_fdc_init db 'FDC INIT', 0
msg_fdc_not_ready db 'FDC NOT READY', 0

kernel_entry dq 0       ; Kernel entry point address
segment_count dq 0
segments_loaded dq 0
last_segment_dest dq 0
last_segment_size dq 0
debug_before dq 0
debug_at dq 0
debug_after dq 0
debug_src dq 0
debug_dst dq 0
debug_size dq 0

align 16
; 32-bit GDT for initial protected mode
gdt_start:
    dq 0x0                  ; Null descriptor (required)

gdt_code_32:
    dw 0xFFFF               ; Limit
    dw 0x0                  ; Base low
    db 0x0                  ; Base mid
    db 10011010b            ; Access byte (code segment)
    db 11001111b            ; Granularity and flags
    db 0x0                  ; Base high

gdt_data_32:
    dw 0xFFFF               ; Limit
    dw 0x0
    db 0x0
    db 10010010b            ; Access byte (data segment)
    db 11001111b
    db 0x0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1 ; GDT size - 1
    dd gdt_start               ; GDT base address

CODE_SEG_32 equ gdt_code_32 - gdt_start
DATA_SEG_32 equ gdt_data_32 - gdt_start

align 16
; 64-bit GDT for long mode
gdt64_start:
    dq 0x0                  ; Null descriptor

gdt64_code:
    dq 0x00209A0000000000   ; 64-bit code segment (L=1, D=0)

gdt64_data:
    dq 0x0000920000000000   ; 64-bit data segment

gdt64_end:

gdt64_descriptor:
    dw gdt64_end - gdt64_start - 1
    dq gdt64_start

CODE_SEG_64 equ gdt64_code - gdt64_start
DATA_SEG_64 equ gdt64_data - gdt64_start

; Pad to 2048 bytes (4 sectors)
times 2048 - ($-$$) db 0