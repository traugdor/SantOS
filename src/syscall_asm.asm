; System call assembly handler
[BITS 64]

global syscall_handler_asm

extern syscall_handler

; INT 0x80 handler - saves context, calls kernel handler, restores context
; On entry from INT 0x80: rax=syscall_num, rdi=arg1, rsi=arg2, rdx=arg3
syscall_handler_asm:
    ; Save callee-saved registers
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    
    ; Rearrange registers for C calling convention:
    ; syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3)
    ; C expects: rdi=num, rsi=arg1, rdx=arg2, rcx=arg3
    ; We have:   rax=num, rdi=arg1, rsi=arg2, rdx=arg3
    mov rcx, rdx    ; rcx = arg3 (must be first to avoid overwrite)
    mov rdx, rsi    ; rdx = arg2
    mov rsi, rdi    ; rsi = arg1
    mov rdi, rax    ; rdi = syscall_num
    
    call syscall_handler
    
    ; Restore callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    
    ; RAX now contains the return value from syscall_handler
    iretq
