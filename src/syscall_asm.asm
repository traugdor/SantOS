; System call assembly handler
[BITS 64]

global syscall_handler_asm

extern syscall_handler

; INT 0x80 handler - saves context, calls kernel handler, restores context
syscall_handler_asm:
    ; Save all registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Call kernel syscall handler
    call syscall_handler
    
    ; Restore all registers (except RAX which has return value)
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    add rsp, 8  ; Skip saved RAX, keep new RAX from syscall_handler
    
    iretq
