#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>

// Output functions (already exist in printf.h, but included here for completeness)
int printf(const char* format, ...);
int sprintf(char* buffer, const char* format, ...);
int snprintf(char* buffer, size_t size, const char* format, ...);
int vsnprintf(char* buffer, size_t size, const char* format, va_list args);

// Input functions
int getchar(void);                          // Read single character
char* gets(char* str);                      // Read line (unsafe, but classic)
char* fgets(char* str, int n);              // Read line (safe, with limit)
int scanf(const char* format, ...);         // Formatted input

// Character output
int putchar(int c);                         // Write single character
int puts(const char* str);                  // Write string with newline

#endif
