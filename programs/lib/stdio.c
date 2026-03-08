// Userspace stdio library
// Programs link with this to access kernel I/O functions via syscalls

#include "../../include/syscall.h"
#include "../../include/stdio.h"

// Trigger system call via INT 0x80
static inline int64_t do_syscall(int num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    int64_t result;
    __asm__ volatile(
        "mov %1, %%rax\n"
        "mov %2, %%rdi\n"
        "mov %3, %%rsi\n"
        "mov %4, %%rdx\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(result)
        : "r"((uint64_t)num), "r"(arg1), "r"(arg2), "r"(arg3)
        : "rax", "rdi", "rsi", "rdx"
    );
    return result;
}

// I/O functions
int putchar(int c) {
    do_syscall(SYSCALL_PUTCHAR, (uint64_t)c, 0, 0);
    return c;
}

int getchar(void) {
    return (int)do_syscall(SYSCALL_GETCHAR, 0, 0, 0);
}

void clear_screen(void) {
    do_syscall(SYSCALL_CLEAR, 0, 0, 0);
}

void set_color(unsigned char fg, unsigned char bg) {
    do_syscall(SYSCALL_SET_COLOR, (uint64_t)fg, (uint64_t)bg, 0);
}

void set_cursor_pos(unsigned char x, unsigned char y) {
    do_syscall(SYSCALL_SET_CURSOR, (uint64_t)x, (uint64_t)y, 0);
}

int list_dir(void) {
    return (int)do_syscall(SYSCALL_LIST_DIR, 0, 0, 0);
}

int list_dir_cluster(unsigned short cluster) {
    return (int)do_syscall(SYSCALL_LIST_DIR_CLUSTER, (uint64_t)cluster, 0, 0);
}

unsigned short find_entry(unsigned short dir_cluster, const char* name, int* is_directory) {
    return (unsigned short)do_syscall(SYSCALL_FIND_ENTRY, (uint64_t)dir_cluster, (uint64_t)name, (uint64_t)is_directory);
}

int read_file(const char* filename, char* buffer, int buffer_size) {
    return (int)do_syscall(SYSCALL_READ_FILE, (uint64_t)filename, (uint64_t)buffer, (uint64_t)buffer_size);
}

int create_file(const char* filename) {
    return (int)do_syscall(SYSCALL_CREATE_FILE, (uint64_t)filename, 0, 0);
}

int write_file(const char* filename, const char* buffer, int size) {
    return (int)do_syscall(SYSCALL_WRITE_FILE, (uint64_t)filename, (uint64_t)buffer, (uint64_t)size);
}

// Call program with a dedicated stack at a fixed memory address
// Memory layout:
//   0x100000 (1MB)  - Shell code
//   0x200000 (2MB)  - Kernel code/data
//   0x500000 (5MB)  - Program code
//   0x700000 (7MB)  - Program stack top (grows down toward 6MB)
//   0x800000 (8MB)  - Physical memory manager
//   0x1000000 (16MB) - Kernel heap
static void call_with_new_stack(uint64_t entry_point, int argc, char** argv) {
    __asm__ volatile(
        "mov %%rsp, %%r15\n"            // Save current stack in r15
        "movabs $0x700000, %%rsp\n"     // Switch to program stack at 7MB
        "mov %1, %%rdi\n"              // argc in rdi (1st arg)
        "mov %2, %%rsi\n"              // argv in rsi (2nd arg)
        "call *%0\n"                    // Call program
        "mov %%r15, %%rsp\n"            // Restore original stack
        :
        : "r"(entry_point), "r"((uint64_t)argc), "r"((uint64_t)argv)
        : "r15", "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "memory"
    );
}

int exec_program(const char* filename, int argc, char** argv) {
    // Syscall loads the ELF and returns the entry point address (0 = error)
    uint64_t entry_point = (uint64_t)do_syscall(SYSCALL_EXEC_PROGRAM, (uint64_t)filename, 0, 0);
    if (entry_point == 0) {
        return -1;  // Failed to load
    }
    
    // Call program with dedicated stack, passing argc and argv
    call_with_new_stack(entry_point, argc, argv);
    
    // Program exited successfully
    printf("\nProgram exited.\n");
    
    return 0;
}

// Helper: print a string
static void print_str(const char* s) {
    while (*s) {
        putchar(*s++);
    }
}

// Helper: print unsigned integer
static void print_uint(uint64_t val, int base) {
    char buf[21];
    int i = 0;
    
    if (val == 0) {
        putchar('0');
        return;
    }
    
    while (val > 0) {
        int digit = val % base;
        buf[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        val /= base;
    }
    
    while (i > 0) {
        putchar(buf[--i]);
    }
}

// Helper: print signed integer
static void print_int(int64_t val) {
    if (val < 0) {
        putchar('-');
        print_uint((uint64_t)(-val), 10);
    } else {
        print_uint((uint64_t)val, 10);
    }
}

int printf(const char* fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    int count = 0;
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'c': {
                    char c = (char)__builtin_va_arg(args, int);
                    putchar(c);
                    count++;
                    break;
                }
                case 's': {
                    const char* s = __builtin_va_arg(args, const char*);
                    if (s) print_str(s);
                    else print_str("(null)");
                    count++;
                    break;
                }
                case 'd':
                case 'i': {
                    int64_t val = __builtin_va_arg(args, int);
                    print_int(val);
                    count++;
                    break;
                }
                case 'u': {
                    uint64_t val = __builtin_va_arg(args, unsigned int);
                    print_uint(val, 10);
                    count++;
                    break;
                }
                case 'x': {
                    uint64_t val = __builtin_va_arg(args, unsigned int);
                    print_uint(val, 16);
                    count++;
                    break;
                }
                case 'p': {
                    void* p = __builtin_va_arg(args, void*);
                    print_str("0x");
                    print_uint((uint64_t)p, 16);
                    count++;
                    break;
                }
                case '%':
                    putchar('%');
                    count++;
                    break;
                default:
                    putchar('%');
                    putchar(*fmt);
                    count++;
                    break;
            }
        } else {
            putchar(*fmt);
            count++;
        }
        fmt++;
    }
    
    __builtin_va_end(args);
    return count;
}

// scanf implementation - formatted input
int scanf(const char* format, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, format);
    
    int count = 0;
    const char* fmt = format;
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            
            // Skip whitespace in input
            char c = getchar();
            while (c == ' ' || c == '\t' || c == '\n') {
                c = getchar();
            }
            
            switch (*fmt) {
                case 'd': {  // Integer
                    int* ptr = __builtin_va_arg(args, int*);
                    char digit_buffer[32];
                    int digit_count = 0;
                    int negative = 0;
                    
                    if (c == '-') {
                        putchar(c);  // Echo minus sign
                        negative = 1;
                        c = getchar();
                    }
                    
                    // Read digits with backspace support
                    while (1) {
                        if (c == 8 || c == 127) {  // Backspace
                            if (digit_count > 0) {
                                digit_count--;
                                putchar(8);    // Move cursor back
                                putchar(' ');  // Clear character
                                putchar(8);    // Move cursor back again
                            }
                            c = getchar();
                        } else if (c >= '0' && c <= '9') {
                            if (digit_count < 31) {
                                digit_buffer[digit_count++] = c;
                                putchar(c);  // Echo digit
                            }
                            c = getchar();
                        } else {
                            break;  // Non-digit, stop reading
                        }
                    }
                    
                    if (digit_count == 0) {
                        // No valid digits found
                        __builtin_va_end(args);
                        return count;
                    }
                    
                    // Convert buffer to integer
                    int value = 0;
                    for (int i = 0; i < digit_count; i++) {
                        value = value * 10 + (digit_buffer[i] - '0');
                    }
                    // Note: c now contains the first non-digit character (like '\n')
                    // We've consumed it but that's okay for scanf behavior
                    
                    if (negative) value = -value;
                    *ptr = value;
                    count++;
                    break;
                }
                case 's': {  // String
                    char* ptr = __builtin_va_arg(args, char*);
                    int i = 0;
                    
                    // Read until whitespace with backspace support
                    while (1) {
                        if (c == 8 || c == 127) {  // Backspace
                            if (i > 0) {
                                i--;
                                putchar(8);    // Move cursor back
                                putchar(' ');  // Clear character
                                putchar(8);    // Move cursor back again
                            }
                            c = getchar();
                        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\0') {
                            break;  // Whitespace, stop reading
                        } else {
                            ptr[i++] = c;
                            putchar(c);  // Echo character
                            c = getchar();
                        }
                    }
                    ptr[i] = '\0';
                    count++;
                    break;
                }
                case 'c': {  // Character
                    char* ptr = __builtin_va_arg(args, char*);
                    putchar(c);  // Echo character
                    *ptr = c;
                    count++;
                    break;
                }
                default:
                    break;
            }
        } else if (*fmt == ' ' || *fmt == '\t' || *fmt == '\n') {
            // Skip whitespace in format string
        } else {
            // Match literal character
            char c = getchar();
            if (c != *fmt) {
                __builtin_va_end(args);
                return count;
            }
        }
        fmt++;
    }
    
    __builtin_va_end(args);
    
    // Print newline after scanf completes (quality of life improvement)
    putchar('\n');
    
    return count;
}
