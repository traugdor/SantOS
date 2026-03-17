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

; FDC interrupt handler (IRQ6)
global fdc_handler_asm
extern fdc_irq_handler
fdc_handler_asm:
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
    
    call fdc_irq_handler
    
    mov al, 0x20
    out 0x20, al
    
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
    
    iretq

; Spurious IRQ handler (IRQ7 = vector 39)
; The 8259A PIC generates spurious IRQ7 when an IRQ is raised then
; de-asserted before the CPU acknowledges it. Do NOT send EOI for
; spurious interrupts - just return immediately.
global spurious_irq_handler_asm
spurious_irq_handler_asm:
    iretq
