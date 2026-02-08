[BITS 64]

global _start
extern kernel_main

_start:
    ; Write 'X' to show we entered
    mov rax, 0x0A58  ; 'X' green
    mov [0xB8000 + 160], ax
    
    ; Call kernel_main
    call kernel_main
    
    ; If it returns, halt
    cli
.hang:
    hlt
    jmp .hang
