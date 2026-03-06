#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

// System call numbers - I/O
#define SYSCALL_PUTCHAR  0
#define SYSCALL_GETCHAR  1
#define SYSCALL_PRINTF   2

// System call numbers - Memory
#define SYSCALL_MALLOC   10
#define SYSCALL_FREE     11
#define SYSCALL_REALLOC  12
#define SYSCALL_CALLOC   13

// System call numbers - String
#define SYSCALL_STRLEN   20
#define SYSCALL_STRCMP   21
#define SYSCALL_STRCPY   22
#define SYSCALL_STRCAT   23
#define SYSCALL_MEMCPY   24
#define SYSCALL_MEMSET   25

// System call interface for userspace programs
int syscall(int num, ...);

// Kernel-side system call handler
void syscall_handler(void);

#endif
