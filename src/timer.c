#include "../include/timer.h"
#include "../include/idt.h"
#include "../include/printf.h"

// I/O port functions
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

// PIT constants
#define PIT_FREQUENCY 1193182  // Base frequency of PIT
#define PIT_CHANNEL0 0x40      // Channel 0 data port
#define PIT_COMMAND 0x43       // Command register

// PIC constants (Programmable Interrupt Controller)
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

// Timer tick counter
static volatile uint64_t timer_ticks = 0;
static uint32_t timer_frequency = 0;

// Timer interrupt handler (called from assembly)
void timer_handler(void) {
    timer_ticks++;
}

// Initialize PIC (Programmable Interrupt Controller)
static void pic_init(void) {
    // ICW1: Initialize PIC
    outb(PIC1_COMMAND, 0x11);  // Start initialization
    outb(PIC2_COMMAND, 0x11);
    
    // ICW2: Set interrupt vector offsets
    outb(PIC1_DATA, 0x20);     // Master PIC: IRQ 0-7 → INT 32-39
    outb(PIC2_DATA, 0x28);     // Slave PIC: IRQ 8-15 → INT 40-47
    
    // ICW3: Set up cascade
    outb(PIC1_DATA, 0x04);     // Tell master PIC slave is at IRQ2
    outb(PIC2_DATA, 0x02);     // Tell slave PIC its cascade identity
    
    // ICW4: Set mode
    outb(PIC1_DATA, 0x01);     // 8086 mode
    outb(PIC2_DATA, 0x01);
    
    // Mask all interrupts except timer (IRQ0)
    outb(PIC1_DATA, 0xFE);     // 11111110 - only IRQ0 enabled
    outb(PIC2_DATA, 0xFF);     // All disabled on slave
}

// Initialize timer with given frequency (Hz)
void timer_init(uint32_t frequency) {
    timer_frequency = frequency;
    
    // Calculate divisor
    uint32_t divisor = PIT_FREQUENCY / frequency;
    
    // Send command byte: Channel 0, lo/hi byte, rate generator
    outb(PIT_COMMAND, 0x36);
    
    // Send divisor (low byte then high byte)
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
    
    // Initialize PIC
    pic_init();
    
    // Set up timer interrupt handler (IRQ0 = INT 32)
    extern void timer_handler_asm(void);
    idt_set_gate(32, (uint64_t)timer_handler_asm, 0x08, 0x8E);
    
    // Enable interrupts
    __asm__ volatile("sti");
    
    printf("Timer initialized at %d Hz\n", frequency);
}

// Get current tick count
uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

// Wait for specified number of ticks
void timer_wait(uint32_t ticks) {
    uint64_t end_tick = timer_ticks + ticks;
    while (timer_ticks < end_tick) {
        __asm__ volatile("hlt");  // Halt until next interrupt
    }
}

// Sleep for specified milliseconds
void sleep_ms(uint32_t ms) {
    if (timer_frequency == 0) return;  // Timer not initialized
    
    // Calculate ticks needed
    uint32_t ticks = (ms * timer_frequency) / 1000;
    timer_wait(ticks);
}
