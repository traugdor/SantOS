#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// IDT entry structure (64-bit)
typedef struct {
    uint16_t offset_low;    // Lower 16 bits of handler address
    uint16_t selector;      // Code segment selector
    uint8_t  ist;           // Interrupt Stack Table (unused, set to 0)
    uint8_t  type_attr;     // Type and attributes
    uint16_t offset_mid;    // Middle 16 bits of handler address
    uint32_t offset_high;   // Upper 32 bits of handler address
    uint32_t zero;          // Reserved, must be 0
} __attribute__((packed)) idt_entry_t;

// IDT pointer structure
typedef struct {
    uint16_t limit;         // Size of IDT - 1
    uint64_t base;          // Base address of IDT
} __attribute__((packed)) idt_ptr_t;

// IDT functions
void idt_init(void);
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags);

#endif
