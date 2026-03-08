#include "../include/keyboard.h"
#include "../include/idt.h"

// I/O port functions
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// Keyboard ports
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// Circular buffer for keyboard input
#define BUFFER_SIZE 256
static char keyboard_buffer[BUFFER_SIZE];
static volatile int buffer_read = 0;
static volatile int buffer_write = 0;

// US QWERTY scan code to ASCII table (set 1)
static const char scancode_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, // Ctrl
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, // Left shift
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0, // Right shift
    '*',
    0, // Alt
    ' ', // Space
    0, // Caps lock
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // F1-F10
    0, // Num lock
    0, // Scroll lock
    0, // Home
    0, // Up arrow
    0, // Page up
    '-',
    0, // Left arrow
    0,
    0, // Right arrow
    '+',
    0, // End
    0, // Down arrow
    0, // Page down
    0, // Insert
    0, // Delete
    0, 0, 0,
    0, 0, // F11, F12
};

// Shift versions of keys
static const char scancode_to_ascii_shift[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, // Ctrl
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, // Left shift
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
};

static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int e0_prefix = 0;  // Track extended scancode prefix (0xE0)

// Keyboard interrupt handler (called from assembly)
void keyboard_handler(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    // Handle extended scancode prefix
    if (scancode == 0xE0) {
        e0_prefix = 1;
        return;
    }
    
    // Check for shift press/release
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        e0_prefix = 0;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        e0_prefix = 0;
        return;
    }
    
    // Check for ctrl press/release
    if (scancode == 0x1D && !e0_prefix) {
        ctrl_pressed = 1;
        e0_prefix = 0;
        return;
    }
    if (scancode == 0x9D) {
        ctrl_pressed = 0;
        e0_prefix = 0;
        return;
    }
    
    // Ignore key releases (high bit set)
    if (scancode & 0x80) {
        e0_prefix = 0;
        return;
    }
    
    // Convert scan code to ASCII
    char ascii = 0;
    
    // Handle extended keys (arrow keys use E0 prefix)
    if (e0_prefix) {
        e0_prefix = 0;
        if (scancode == 0x4B) { ascii = 0x01; }       // Left arrow
        else if (scancode == 0x4D) { ascii = 0x02; }   // Right arrow
        else if (scancode == 0x48) { ascii = 0x03; }   // Up arrow
        else if (scancode == 0x50) { ascii = 0x04; }   // Down arrow
    } else if (scancode < sizeof(scancode_to_ascii)) {
        if (ctrl_pressed) {
            // Ctrl+letter generates ASCII 1-26 (control characters)
            char base = scancode_to_ascii[scancode];
            if (base >= 'a' && base <= 'z') {
                ascii = base - 'a' + 1;  // Ctrl+A=1, Ctrl+Q=17, Ctrl+S=19
            }
        } else if (shift_pressed && scancode < sizeof(scancode_to_ascii_shift)) {
            ascii = scancode_to_ascii_shift[scancode];
        } else {
            ascii = scancode_to_ascii[scancode];
        }
    }
    
    // Add to buffer if valid
    if (ascii != 0) {
        int next_write = (buffer_write + 1) % BUFFER_SIZE;
        if (next_write != buffer_read) {
            keyboard_buffer[buffer_write] = ascii;
            buffer_write = next_write;
        }
    }
}

// Initialize keyboard
void keyboard_init(void) {
    // Enable keyboard interrupt in PIC (IRQ1)
    uint8_t mask = inb(0x21);
    mask &= ~0x02;  // Clear bit 1 (IRQ1)
    outb(0x21, mask);
    
    // Set up keyboard interrupt handler (IRQ1 = INT 33)
    extern void keyboard_handler_asm(void);
    idt_set_gate(33, (uint64_t)keyboard_handler_asm, 0x08, 0x8E);
}

// Check if a key is available
int keyboard_available(void) {
    return buffer_read != buffer_write;
}

// Get a character (blocking)
char keyboard_getchar(void) {
    // Wait for a key
    while (!keyboard_available()) {
        __asm__ volatile("hlt");
    }
    
    // Read from buffer
    char c = keyboard_buffer[buffer_read];
    buffer_read = (buffer_read + 1) % BUFFER_SIZE;
    return c;
}
