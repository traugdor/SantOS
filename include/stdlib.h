#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

// Number conversion
int atoi(const char* str);
long atol(const char* str);

// Absolute value
int abs(int n);
long labs(long n);

// Random numbers (simple LCG)
int rand(void);
void srand(unsigned int seed);

// Timing (busy-wait, not accurate)
void delay_ms(unsigned int ms);

// Memory allocation
void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t num, size_t size);
void* realloc(void* ptr, size_t size);

#endif
