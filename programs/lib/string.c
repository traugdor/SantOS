// Userspace string library
// Programs link with this to access kernel string functions via syscalls

#include "../../include/syscall.h"
#include "../../include/stdlib.h"
#include <stddef.h>

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
        : "rax", "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "memory"
    );
    return result;
}

// String functions - implemented locally since kernel can't access userspace memory
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return (unsigned char)s1[i] - (unsigned char)s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

char* strcpy(char* dest, const char* src) {
    char* original = dest;
    while ((*dest++ = *src++));
    return original;
}

char* strcat(char* dest, const char* src) {
    char* original = dest;
    while (*dest) dest++;
    while ((*dest++ = *src++));
    return original;
}

char* strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* copy = (char*)malloc(len + 1);
    if (!copy) return NULL;
    strcpy(copy, str);
    return copy;
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return s;
}

// Helper: reverse a string in place
static void reverse_string(char* str) {
    if (!str) return;
    size_t len = strlen(str);
    for (size_t i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = temp;
    }
}

// Substring function with 1-based indexing
// start: 1-based start index (negative for reverse)
// end: 1-based end index (0 or -1 means to end of string)
// Returns: newly allocated substring (caller must free)
char* substr(const char* input, int start, int end) {
    if (!input) return NULL;
    
    size_t input_len = strlen(input);
    if (input_len == 0) return NULL;
    
    // Handle negative start index (reverse substring)
    if (start < 0) {
        // Create reversed copy
        char* reversed = (char*)malloc(input_len + 1);
        if (!reversed) return NULL;
        strcpy(reversed, input);
        reverse_string(reversed);
        
        // Calculate positive index from reversed string
        int rev_start = -start;  // Convert negative to positive
        int count = (end == 0 || end == -1) ? (input_len - rev_start + 1) : (end - start + 1);
        
        if (rev_start < 1 || rev_start > (int)input_len) {
            free(reversed);
            return NULL;
        }
        
        // Extract substring from reversed string
        char* result = (char*)malloc(count + 1);
        if (!result) {
            free(reversed);
            return NULL;
        }
        
        int actual_count = 0;
        for (int i = 0; i < count && (rev_start - 1 + i) < (int)input_len; i++) {
            result[i] = reversed[rev_start - 1 + i];
            actual_count++;
        }
        result[actual_count] = '\0';
        
        // Reverse the result back
        reverse_string(result);
        free(reversed);
        return result;
    }
    
    // Handle positive start index
    if (start < 1 || start > (int)input_len) {
        return NULL;  // Invalid start index
    }
    
    // Convert 1-based to 0-based
    int start_idx = start - 1;
    int end_idx;
    
    if (end == 0 || end == -1) {
        // No end specified, go to end of string
        end_idx = input_len - 1;
    } else {
        // Convert 1-based to 0-based
        end_idx = end - 1;
        if (end_idx < start_idx || end_idx >= (int)input_len) {
            return NULL;  // Invalid end index
        }
    }
    
    // Calculate substring length
    size_t sub_len = end_idx - start_idx + 1;
    
    // Allocate result
    char* result = (char*)malloc(sub_len + 1);
    if (!result) return NULL;
    
    // Copy substring
    for (size_t i = 0; i < sub_len; i++) {
        result[i] = input[start_idx + i];
    }
    result[sub_len] = '\0';
    
    return result;
}

static char* strtok_state = NULL;

char* strtok(char* str, const char* delim) {
    if (str != NULL) {
        strtok_state = str;
    }
    if (strtok_state == NULL) {
        return NULL;
    }

    // Skip leading delimiters
    while (*strtok_state) {
        const char* d = delim;
        int is_delim = 0;
        while (*d) {
            if (*strtok_state == *d) {
                is_delim = 1;
                break;
            }
            d++;
        }
        if (!is_delim) break;
        strtok_state++;
    }

    if (*strtok_state == '\0') {
        strtok_state = NULL;
        return NULL;
    }

    // Mark start of token
    char* token_start = strtok_state;

    // Find end of token
    while (*strtok_state) {
        const char* d = delim;
        while (*d) {
            if (*strtok_state == *d) {
                *strtok_state = '\0';
                strtok_state++;
                return token_start;
            }
            d++;
        }
        strtok_state++;
    }

    // Reached end of string
    strtok_state = NULL;
    return token_start;
}
