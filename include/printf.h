#ifndef PRINTF_H
#define PRINTF_H

#include <stddef.h>

// Basic printf implementation
// Supports: %s (string), %c (char), %d (decimal), %x (hex), %u (unsigned), %% (literal %)
void printf(const char* format, ...);
int sprintf(char* buf, const char* format, ...);
int snprintf(char* buf, size_t size, const char* format, ...);

#endif
