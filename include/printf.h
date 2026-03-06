#ifndef PRINTF_H
#define PRINTF_H

#include <stddef.h>

// Standard printf implementation
// Format specifiers: %s (string), %c (char), %d (decimal), %x (hex), %u (unsigned), %f (float with precision), %% (literal %)
// Note: Escape sequences (\n, \t, etc.) are handled by the C compiler in string literals, not by printf itself
void printf(const char* format, ...);
int sprintf(char* buf, const char* format, ...);
int snprintf(char* buf, size_t size, const char* format, ...);

#endif
