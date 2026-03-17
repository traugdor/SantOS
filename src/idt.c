#include "../include/idt.h"
#include "../include/printf.h"

#define IDT_ENTRIES 256

// IDT table
static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

// Set an IDT gate
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].selector = selector;
    idt[num].ist = 0;
    idt[num].type_attr = flags;
    idt[num].offset_mid = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].zero = 0;
    
    printf("IDT[%d]: handler=0x%llx selector=0x%x flags=0x%x\n", 
           num, handler, selector, flags);
}

// Load IDT into CPU
extern void idt_load(uint64_t idt_ptr_addr);

// External handlers
extern void syscall_handler_asm(void);
extern void spurious_irq_handler_asm(void);

// Initialize IDT
void idt_init(void) {
    // Set up IDT pointer
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)&idt;
    
    // Clear IDT
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].type_attr = 0;
        idt[i].offset_mid = 0;
        idt[i].offset_high = 0;
        idt[i].zero = 0;
    }
    
    // Set up system call handler (INT 0x80)
    // 0x8E = Present, DPL=0, Type=Interrupt Gate
    idt_set_gate(0x80, (uint64_t)syscall_handler_asm, 0x08, 0x8E);
    
    // Spurious IRQ7 handler (vector 39) - the 8259A PIC can generate
    // spurious interrupts on IRQ7 when an IRQ is raised then de-asserted
    // before the CPU acknowledges it. Without a handler, this triple faults.
    idt_set_gate(39, (uint64_t)spurious_irq_handler_asm, 0x08, 0x8E);
    
    // Load IDT
    idt_load((uint64_t)&idt_ptr);
    
    printf("IDT initialized (syscall INT 0x80 registered)\n");
}
