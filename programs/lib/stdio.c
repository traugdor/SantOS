// Userspace stdio library
// Programs link with this to access kernel I/O functions via syscalls

#include "../../include/syscall.h"

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
void putchar(char c) {
    do_syscall(SYSCALL_PUTCHAR, (uint64_t)c, 0, 0);
}

char getchar(void) {
    return (char)do_syscall(SYSCALL_GETCHAR, 0, 0, 0);
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
