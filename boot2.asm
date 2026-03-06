%ifidn __OUTPUT_FORMAT__, elf64
    ; ELF-specific code (no ORG needed)
%else
    org 0x9000  ; Binary output format - loaded at 0x9000?
%endif
[bits 16]

start:
    cli                     ; Disable interrupts
    
    ; Check for CPUID support
    call check_cpuid
    
    ; Check for long mode support
    call check_long_mode
    
    ; Print success message
    mov si, msg_checks_ok
    call print_string_16
    call delay_16
    
    ; Detect memory using E820
    call detect_memory_e820
    
    ; Setup page tables for identity mapping
    call setup_page_tables
    
    mov si, msg_page_tables_ok
    call print_string_16
    call delay_16
    
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

; Detect memory using BIOS INT 0x15, EAX=0xE820
; Memory map stored at 0x500:
;   Offset 0x00: Entry count (4 bytes)
;   Offset 0x04: First entry (24 bytes)
;   Each entry: base_addr(8), length(8), type(4), extended(4)
; Highest usable address stored at 0x8000 (4 bytes)
detect_memory_e820:
    push es
    push di
    push bp
    push si
    
    xor ax, ax
    mov es, ax
    mov di, 0x504       ; ES:DI = 0x0000:0x0504 (first entry, after count)
    xor bp, bp          ; BP = entry counter
    xor ebx, ebx        ; EBX must be 0 for first call
    
    ; Initialize highest address to 128MB (safe default)
    mov dword [0x8000], 0x8000000
    
.loop:
    mov eax, 0xE820     ; E820 function
    mov ecx, 24         ; Request 24 bytes
    mov edx, 0x534D4150 ; 'SMAP' signature
    
    ; Set ACPI 3.0 Extended Attributes to 1 (some BIOSes require this)
    mov dword [es:di + 20], 1
    
    int 0x15
    
    jc .done            ; CF set = error or done
    
    cmp eax, 0x534D4150 ; Verify 'SMAP' returned
    jne .done
    
    cmp ecx, 20         ; Entry must be at least 20 bytes
    jb .skip_entry
    
    ; Valid entry, increment counter
    inc bp
    
    ; Check if this is usable RAM (type == 1) and calculate highest address
    cmp dword [es:di + 16], 1   ; Check type field
    jne .next_entry
    
    ; Only process if upper 32 bits of base and length are 0 (below 4GB)
    cmp dword [es:di + 4], 0
    jne .next_entry
    cmp dword [es:di + 12], 0
    jne .next_entry
    
    ; Calculate end address: base + length
    mov eax, [es:di]        ; Base (lower 32 bits)
    mov esi, [es:di + 8]    ; Length (lower 32 bits)
    add eax, esi            ; End address
    jc .next_entry          ; Skip if overflow
    
    ; Update highest if this is higher
    cmp eax, [0x8000]
    jbe .next_entry
    mov [0x8000], eax
    
.next_entry:
    add di, 24          ; Move to next entry (24 bytes)
    
.skip_entry:
    test ebx, ebx       ; EBX = 0 means last entry
    jz .done
    jmp .loop
    
.done:
    ; Store entry count at 0x500 (as dword to match kernel)
    xor eax, eax
    mov ax, bp
    mov dword [0x500], eax
    
    pop si
    pop bp
    pop di
    pop es
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
    
    ; Calculate number of 2MB pages needed based on highest address from E820
    ; Highest address is stored at 0x8000 (4 bytes)
    mov eax, [0x8000]       ; Get highest usable address
    
    ; Round up to next 2MB boundary
    add eax, 0x001FFFFF     ; Add 2MB - 1
    shr eax, 21             ; Divide by 2MB (shift right 21 bits)
    mov ecx, eax            ; ECX = number of 2MB pages to map
    
    ; Cap at 512 entries (1GB) to fit in one page directory
    cmp ecx, 512
    jbe .map_pages
    mov ecx, 512
    
.map_pages:
    ; Setup PD entries - map memory with 2MB pages
    ; Each entry is 8 bytes (64-bit), maps 2MB
    mov edi, 0x3000         ; Start of PD
    mov eax, 0x00000083     ; First page: 0MB, Present + Writable + Huge
    mov ebx, 0x00000000     ; Upper 32 bits
    
.map_loop:
    mov [edi], eax          ; Lower 32 bits
    mov [edi + 4], ebx      ; Upper 32 bits
    add eax, 0x00200000     ; Next 2MB page
    add edi, 8              ; Next PD entry
    loop .map_loop
    
    ; Set CR3 to PML4 address
    mov eax, 0x1000
    mov cr3, eax
    
    ret

; Error messages
error_no_cpuid db 'ERROR: CPUID not supported', 0
error_no_long_mode db 0x0D, 0x0A, 'ERROR: Long mode not supported', 0
msg_checks_ok db 0x0D, 0x0A, 'CPU checks passed, entering long mode...', 0x0D, 0x0A, 0
msg_page_tables_ok db 'Page tables setup complete', 0x0D, 0x0A, 0
msg_entering_pm db 'Entering protected mode...', 0x0D, 0x0A, 0

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
    
    ; Load CR3 with page table address (must be done after PAE is enabled)
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
    
    ; Display initial message
    call clear_screen
    
    ; Display welcome message
    mov rsi, msg
    mov rdi, 0xB8000        ; VGA text mode buffer
    mov ah, 0x0F            ; Text attribute: white on black
    call print_string_pm
    
    ; Display success message on second line
    mov rdi, 0xB8000 + 160  ; Move to second line
    mov rsi, msg_success
    mov ah, 0x0A            ; Light green on black
    call print_string_pm
    
    ; Display loading kernel message
    mov rdi, 0xB8000 + 320  ; Third line
    mov rsi, msg_loading_kernel
    mov ah, 0x0F
    call print_string_pm
    
    ; Simple kernel copy - just copy everything from ELF to 0x200000
    ; ELF is at 0x20000 (loaded by FAT12.asm to 0x2000:0x0000)
    ; Kernel code starts at offset 0x1000 in ELF
    ; Copy 128KB to handle larger kernel with syscall support
    mov rsi, 0x21000        ; Source: ELF base (0x20000) + 0x1000 offset
    mov rdi, 0x200000       ; Destination: kernel load address
    mov rcx, 0x20000        ; Copy 128KB
    rep movsb
    
    ; Set entry point
    mov qword [kernel_entry], 0x200000
    
    ; Debug: Show we got past load_kernel
    mov rdi, 0xB8000 + 480
    mov rsi, msg_kernel_loaded
    mov ah, 0x0A
    call print_string_pm
    
    ; Display jumping to kernel message
    mov rdi, 0xB8000 + 480  ; Fourth line
    mov rsi, msg_jumping_kernel
    mov ah, 0x0E            ; Yellow on black
    call print_string_pm
    
    ; Debug: Show entry point
    mov rax, [kernel_entry]
    mov rdi, 0xB8000 + 800  ; Line 5
    call print_hex
    
    ; Small delay so we can see the entry point
    mov rcx, 0x10000000
.delay:
    dec rcx
    jnz .delay
    
    ; Set up stack for kernel (16KB stack at 0x80000)
    mov rsp, 0x80000
    
    ; Debug: Show we set up stack
    mov rdi, 0xB8000 + 960  ; Line 6
    mov al, 'S'
    mov ah, 0x0C            ; Red
    stosw
    
    ; Debug: Show first 4 bytes at kernel entry
    mov rax, [kernel_entry]
    mov rbx, [rax]          ; Read first 4 bytes of kernel
    mov rax, rbx
    mov rdi, 0xB8000 + 1280  ; Line 8
    call print_hex
    
    ; Jump to kernel entry point
    mov rax, [kernel_entry]
    call rax
    
    ; If kernel returns, halt
    cli
.halt_loop:
    hlt
    jmp .halt_loop

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

; Load kernel from disk and parse ELF
load_kernel:
    ; Debug: Show we entered load_kernel
    push rdi
    push rsi
    push rax
    mov rdi, 0xB8000 + 640
    mov rsi, msg_in_load_kernel
    mov ah, 0x0C
    call print_string_pm
    pop rax
    pop rsi
    pop rdi
    
    ; Kernel was already loaded by fat12.asm to 0x20000
    
    ; Parse ELF header
    mov r15, 0x20000        ; R15 = ELF base address (preserve this!)
    
    ; Check ELF magic number (0x7F 'E' 'L' 'F')
    mov eax, [r15]
    cmp eax, 0x464C457F
    jne .elf_error
    
    ; Get entry point from ELF header (offset 0x18 for 64-bit ELF)
    mov rax, [r15 + 0x18]
    mov [kernel_entry], rax
    
    ; Get program header offset (offset 0x20)
    mov rax, [r15 + 0x20]
    add rax, r15            ; RAX = program header table address
    
    ; Get program header entry count (offset 0x38, 2 bytes)
    movzx rcx, word [r15 + 0x38]
    
    ; Get program header entry size (offset 0x36, 2 bytes)
    movzx rdx, word [r15 + 0x36]
    
    ; Save segment count for debugging
    mov [segment_count], rcx
    
    ; Debug: Show segment count
    push rax
    push rcx
    push rdx
    mov rdi, 0xB8000 + 640
    mov al, '0'
    add al, cl          ; Convert count to ASCII (assumes < 10)
    mov ah, 0x0B        ; Cyan
    stosw
    pop rdx
    pop rcx
    pop rax
    
    ; Initialize loaded segment counter
    mov qword [segments_loaded], 0
    
.load_segments:
    test rcx, rcx
    jz .done
    
    ; Check if this is a LOAD segment (p_type == 1)
    cmp dword [rax], 1
    jne .next_segment
    
    ; Increment loaded counter
    inc qword [segments_loaded]
    
    ; Debug: Show we're loading a segment (append to screen)
    push rax
    push rcx
    push rdx
    push rdi
    mov rdi, 0xB8000 + 640
    mov rax, [segments_loaded]
    shl rax, 1              ; Multiply by 2 (each char is 2 bytes)
    add rdi, rax            ; Offset by segment count
    mov al, '*'
    mov ah, 0x0E
    stosw
    pop rdi
    pop rdx
    pop rcx
    pop rax
    
    ; Get segment info
    mov rbx, [rax + 0x08]   ; p_offset - offset in file
    add rbx, r15            ; RBX = source address (ELF base + offset)
    mov rdi, [rax + 0x10]   ; p_vaddr - virtual address (destination)
    mov r8, [rax + 0x20]    ; p_filesz - size in file
    mov r9, [rax + 0x28]    ; p_memsz - size in memory
    
    ; Debug: Show destination and source of first segment only
    cmp qword [segments_loaded], 1
    jne .skip_debug
    push rax
    push rdi
    push rbx
    
    ; Show destination
    mov rax, rdi
    mov rdi, 0xB8000 + 1120  ; Line 7
    call print_hex
    
    ; Show source
    pop rbx
    push rbx
    mov rax, rbx
    mov rdi, 0xB8000 + 1280  ; Line 8
    call print_hex
    
    pop rbx
    pop rdi
    pop rax
.skip_debug:
    
    ; Save destination for debugging (last segment)
    mov [last_segment_dest], rdi
    mov [last_segment_size], r8
    
    ; Verify source and destination are valid
    test r8, r8             ; Check if size is non-zero
    jz .next_segment
    
    ; Copy segment to destination
    ; RDI already contains destination address from line 353
    ; RBX contains source address
    ; R8 contains size
    
    ; Save copy parameters for verification
    mov [debug_src], rbx
    mov [debug_dst], rdi  
    mov [debug_size], r8
    
    ; Save registers that will be modified
    push rax
    push rcx
    push rdx
    push rsi
    
    mov rcx, r8             ; Count
    mov rsi, rbx            ; Source
    ; RDI already has destination!
    
    rep movsb               ; Copy bytes (modifies RSI, RDI, RCX)
    
    ; TODO: Zero out BSS - skipping for now as 4MB takes too long
    ; Zero out BSS (MemSiz - FileSiz)
    ; RDI now points right after copied data
    ; mov rcx, r9             ; MemSiz
    ; sub rcx, r8             ; MemSiz - FileSiz = BSS size
    ; jz .no_bss              ; Skip if no BSS
    ; xor al, al              ; Zero byte
    ; rep stosb               ; Zero out BSS
    
.no_bss:
    ; Restore registers
    pop rsi
    pop rdx
    pop rcx
    pop rax
    
.next_segment:
    add rax, rdx            ; Move to next program header
    dec rcx
    jmp .load_segments
    
.done:
    ret

.elf_error:
    mov rdi, 0xB8000 + 320
    mov rsi, msg_elf_error
    mov ah, 0x0C            ; Light red on black
    call print_string_pm
    cli
    hlt

; Read sectors from disk using LBA
; RAX = starting LBA sector
; RBX = destination address
; RCX = number of sectors
read_disk_lba:
    push rax
    push rbx
    push rcx
    push rdx
    
    ; We need to use BIOS interrupts, but we're in long mode
    ; For simplicity, we'll assume the kernel was already loaded by boot.asm
    ; In a real implementation, you'd need to either:
    ; 1. Use ATA PIO mode directly (no BIOS)
    ; 2. Load everything in boot.asm before switching modes
    
    ; For now, just return (kernel should be loaded by updated boot.asm)
    
    pop rdx
    pop rcx
    pop rbx
    pop rax
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
    dd gdt_start              ; GDT base address

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