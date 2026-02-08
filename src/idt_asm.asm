[BITS 64]

; Load IDT
global idt_load
idt_load:
    lidt [rdi]      ; RDI contains pointer to IDT pointer structure
    ret

; Keyboard interrupt handler (IRQ1)
global keyboard_handler_asm
extern keyboard_handler
keyboard_handler_asm:
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
    
    ; Call C handler
    call keyboard_handler
    
    ; Send EOI (End of Interrupt) to PIC
    mov al, 0x20
    out 0x20, al
    
    ; Restore all registers
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
    pop rax
    
    ; Return from interrupt
    iretq

; Timer interrupt handler (IRQ0)
global timer_handler_asm
extern timer_handler
timer_handler_asm:
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
    
    ; Call C handler
    call timer_handler
    
    ; Send EOI (End of Interrupt) to PIC
    mov al, 0x20
    out 0x20, al
    
    ; Restore all registers
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
    pop rax
    
    ; Return from interrupt
    iretq
