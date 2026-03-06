// Userspace string library
// Programs link with this to access kernel string functions via syscalls

#include "../../include/syscall.h"
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
        : "rax", "rdi", "rsi", "rdx"
    );
    return result;
}

// String functions
size_t strlen(const char* str) {
    return (size_t)do_syscall(SYSCALL_STRLEN, (uint64_t)str, 0, 0);
}

int strcmp(const char* s1, const char* s2) {
    return (int)do_syscall(SYSCALL_STRCMP, (uint64_t)s1, (uint64_t)s2, 0);
}

char* strcpy(char* dest, const char* src) {
    return (char*)do_syscall(SYSCALL_STRCPY, (uint64_t)dest, (uint64_t)src, 0);
}

char* strcat(char* dest, const char* src) {
    return (char*)do_syscall(SYSCALL_STRCAT, (uint64_t)dest, (uint64_t)src, 0);
}

void* memcpy(void* dest, const void* src, size_t n) {
    return (void*)do_syscall(SYSCALL_MEMCPY, (uint64_t)dest, (uint64_t)src, (uint64_t)n);
}

void* memset(void* s, int c, size_t n) {
    return (void*)do_syscall(SYSCALL_MEMSET, (uint64_t)s, (uint64_t)c, (uint64_t)n);
}
