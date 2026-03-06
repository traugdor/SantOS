// Userspace stdlib library
// Programs link with this to access kernel memory functions via syscalls

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

// Memory allocation functions
void* malloc(size_t size) {
    return (void*)do_syscall(SYSCALL_MALLOC, (uint64_t)size, 0, 0);
}

void free(void* ptr) {
    do_syscall(SYSCALL_FREE, (uint64_t)ptr, 0, 0);
}

void* realloc(void* ptr, size_t size) {
    return (void*)do_syscall(SYSCALL_REALLOC, (uint64_t)ptr, (uint64_t)size, 0);
}

void* calloc(size_t nmemb, size_t size) {
    return (void*)do_syscall(SYSCALL_CALLOC, (uint64_t)nmemb, (uint64_t)size, 0);
}
