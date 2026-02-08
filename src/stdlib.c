#include "../include/stdlib.h"
#include "../include/ctype.h"

// Number conversion
int atoi(const char* str) {
    int result = 0;
    int sign = 1;
    
    // Skip whitespace
    while (isspace(*str)) {
        str++;
    }
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Convert digits
    while (isdigit(*str)) {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

long atol(const char* str) {
    long result = 0;
    int sign = 1;
    
    // Skip whitespace
    while (isspace(*str)) {
        str++;
    }
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Convert digits
    while (isdigit(*str)) {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

// Absolute value
int abs(int n) {
    return n < 0 ? -n : n;
}

long labs(long n) {
    return n < 0 ? -n : n;
}

// Simple Linear Congruential Generator for random numbers
static unsigned long rand_seed = 1;

void srand(unsigned int seed) {
    rand_seed = seed;
}

int rand(void) {
    // LCG parameters from Numerical Recipes
    rand_seed = rand_seed * 1103515245 + 12345;
    return (unsigned int)(rand_seed / 65536) % 32768;
}

// Busy-wait delay (very approximate, CPU-dependent)
void delay_ms(unsigned int ms) {
    // Rough calibration for ~1ms on a typical CPU
    // This is NOT accurate and will vary by CPU speed!
    for (unsigned int i = 0; i < ms; i++) {
        for (volatile unsigned int j = 0; j < 10000; j++) {
            // Busy wait
        }
    }
}
