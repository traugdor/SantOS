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
}

// Load IDT into CPU
extern void idt_load(uint64_t idt_ptr_addr);

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
    
    // Load IDT (will set up handlers later)
    idt_load((uint64_t)&idt_ptr);
    
    printf("IDT initialized\n");
}
