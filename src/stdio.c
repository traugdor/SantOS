#include "../include/stdio.h"
#include "../include/keyboard.h"
#include "../include/stdlib.h"
#include "../include/ctype.h"

// Get a single character from keyboard
int getchar(void) {
    return (int)keyboard_getchar();
}

// Write a single character to screen
int putchar(int c) {
    char str[2] = {(char)c, '\0'};
    printf("%s", str);
    return c;
}

// Write string with newline
int puts(const char* str) {
    printf("%s\n", str);
    return 0;
}

// Read a line (unsafe - no bounds checking, classic C)
char* gets(char* str) {
    int i = 0;
    while (1) {
        char c = keyboard_getchar();
        
        if (c == '\n') {
            str[i] = '\0';
            putchar('\n');
            return str;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                putchar('\b');
            }
        } else {
            str[i++] = c;
            putchar(c);
        }
    }
}

// Read a line (safe - with size limit)
char* fgets(char* str, int n) {
    if (n <= 0) return NULL;
    
    int i = 0;
    while (i < n - 1) {
        char c = keyboard_getchar();
        
        if (c == '\n') {
            str[i++] = '\n';
            str[i] = '\0';
            putchar('\n');
            return str;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                putchar('\b');
            }
        } else if (c >= 32 && c <= 126) {
            // Only accept printable characters
            str[i++] = c;
            putchar(c);
        }
        // Ignore all other non-printable characters
    }
    
    str[i] = '\0';
    return str;
}

// Simple scanf implementation
int scanf(const char* format, ...) {
    char buffer[256];
    fgets(buffer, sizeof(buffer));
    
    va_list args;
    va_start(args, format);
    
    int count = 0;
    const char* fmt = format;
    char* buf = buffer;
    
    while (*fmt) {
        // Skip whitespace in format
        while (*fmt == ' ' || *fmt == '\t' || *fmt == '\n') fmt++;
        
        if (*fmt == '%') {
            fmt++;
            
            // Skip whitespace in input
            while (*buf == ' ' || *buf == '\t') buf++;
            
            switch (*fmt) {
                case 'd': {  // Integer
                    int* ptr = va_arg(args, int*);
                    char num_buf[32];
                    int i = 0;
                    
                    // Handle negative
                    if (*buf == '-') {
                        num_buf[i++] = *buf++;
                    }
                    
                    // Read digits
                    while (isdigit(*buf) && i < 31) {
                        num_buf[i++] = *buf++;
                    }
                    num_buf[i] = '\0';
                    
                    if (i > 0) {
                        *ptr = atoi(num_buf);
                        count++;
                    }
                    break;
                }
                
                case 's': {  // String
                    char* ptr = va_arg(args, char*);
                    int i = 0;
                    
                    // Skip leading whitespace
                    while (*buf == ' ' || *buf == '\t') buf++;
                    
                    // Read until whitespace
                    while (*buf && *buf != ' ' && *buf != '\t' && *buf != '\n') {
                        ptr[i++] = *buf++;
                    }
                    ptr[i] = '\0';
                    
                    if (i > 0) count++;
                    break;
                }
                
                case 'c': {  // Character
                    char* ptr = va_arg(args, char*);
                    if (*buf) {
                        *ptr = *buf++;
                        count++;
                    }
                    break;
                }
            }
            fmt++;
        } else {
            // Match literal character
            if (*fmt == *buf) {
                fmt++;
                buf++;
            } else {
                break;
            }
        }
    }
    
    va_end(args);
    return count;
}

// Include atoi from stdlib (needed for scanf)
extern int atoi(const char* str);
