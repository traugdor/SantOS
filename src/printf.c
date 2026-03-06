#include "../include/printf.h"
#include "../include/vga.h"
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

// Output context for different printf variants
typedef struct {
    char* buffer;
    size_t pos;
    size_t max_size;
    void (*putchar_fn)(char c, void* ctx);
} printf_ctx_t;

// Callback for printf (output to VGA)
static void vga_putchar_cb(char c, void* ctx) {
    (void)ctx;  // Unused
    vga_putchar(c);
}

// Callback for sprintf (output to buffer)
static void buf_putchar_cb(char c, void* ctx) {
    printf_ctx_t* pctx = (printf_ctx_t*)ctx;
    if (pctx->pos < pctx->max_size - 1) {
        pctx->buffer[pctx->pos++] = c;
    }
}

// Helper to print a signed number
static void print_signed(int num, void (*putc)(char, void*), void* ctx) {
    static char digits[] = "0123456789";
    char buffer[32];
    int i = 0;
    int is_negative = 0;
    
    if (num == 0) {
        putc('0', ctx);
        return;
    }
    
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    while (num > 0) {
        buffer[i++] = digits[num % 10];
        num /= 10;
    }
    
    if (is_negative) {
        putc('-', ctx);
    }
    
    // Print in reverse
    while (i > 0) {
        putc(buffer[--i], ctx);
    }
}

// Helper to print an unsigned number in a given base
static void print_unsigned(unsigned int num, int base, void (*putc)(char, void*), void* ctx) {
    static char digits[] = "0123456789ABCDEF";
    char buffer[32];
    int i = 0;
    
    if (num == 0) {
        putc('0', ctx);
        return;
    }
    
    while (num > 0) {
        buffer[i++] = digits[num % base];
        num /= base;
    }
    
    // Print in reverse
    while (i > 0) {
        putc(buffer[--i], ctx);
    }
}

// Helper to print a floating-point number with precision
static void print_float(long double num, int precision, void (*putc)(char, void*), void* ctx) {
    // Handle negative numbers
    if (num < 0) {
        putc('-', ctx);
        num = -num;
    }
    
    // Print integer part
    long long int_part = (long long)num;
    print_signed((int)int_part, putc, ctx);
    
    // Print decimal point
    putc('.', ctx);
    
    // Print fractional part
    long double frac_part = num - (long double)int_part;
    
    // Ensure we have at least some precision
    if (precision == 0) {
        precision = 1;
    }
    
    for (int i = 0; i < precision; i++) {
        frac_part *= 10.0L;
        long long digit = (long long)frac_part;
        putc('0' + (char)digit, ctx);
        frac_part -= (long double)digit;
    }
}

// Generic printf implementation
static int vprintf_internal(void (*putc)(char, void*), void* ctx, const char* format, va_list args) {
    int count = 0;
    
    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++;
            
            // Parse precision for floating-point (e.g., %.1f)
            int precision = 6;  // Default precision
            int has_precision = 0;
            
            if (format[i] == '.') {
                has_precision = 1;
                i++;
                precision = 0;
                while (format[i] >= '0' && format[i] <= '9') {
                    precision = precision * 10 + (format[i] - '0');
                    i++;
                }
            }
            
            // Now format[i] should be the format character (f, d, s, etc.)
            switch (format[i]) {
                case 's': {
                    const char* str = va_arg(args, const char*);
                    while (*str) {
                        putc(*str++, ctx);
                        count++;
                    }
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    putc(c, ctx);
                    count++;
                    break;
                }
                case 'd': {
                    int num = va_arg(args, int);
                    print_signed(num, putc, ctx);
                    count++;  // Approximate
                    break;
                }
                case 'f': {
                    long double num = va_arg(args, long double);
                    print_float(num, precision, putc, ctx);
                    count++;  // Approximate
                    break;
                }
                case 'x': {
                    unsigned int num = va_arg(args, unsigned int);
                    print_unsigned(num, 16, putc, ctx);
                    count++;  // Approximate
                    break;
                }
                case 'u': {
                    unsigned int num = va_arg(args, unsigned int);
                    print_unsigned(num, 10, putc, ctx);
                    count++;  // Approximate
                    break;
                }
                case '%': {
                    putc('%', ctx);
                    count++;
                    break;
                }
                default: {
                    putc('%', ctx);
                    putc(format[i], ctx);
                    count += 2;
                    break;
                }
            }
        } else {
            putc(format[i], ctx);
            count++;
        }
    }
    
    return count;
}

void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf_internal(vga_putchar_cb, NULL, format, args);
    va_end(args);
}

int sprintf(char* buf, const char* format, ...) {
    printf_ctx_t ctx = { buf, 0, (size_t)-1, buf_putchar_cb };
    va_list args;
    va_start(args, format);
    int count = vprintf_internal(buf_putchar_cb, &ctx, format, args);
    va_end(args);
    buf[ctx.pos] = '\0';  // Null terminate
    return count;
}

int snprintf(char* buf, size_t size, const char* format, ...) {
    if (size == 0) return 0;
    printf_ctx_t ctx = { buf, 0, size, buf_putchar_cb };
    va_list args;
    va_start(args, format);
    int count = vprintf_internal(buf_putchar_cb, &ctx, format, args);
    va_end(args);
    buf[ctx.pos] = '\0';  // Null terminate
    return count;
}
