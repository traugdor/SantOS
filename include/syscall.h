#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

// System call numbers - I/O
#define SYSCALL_PUTCHAR   0
#define SYSCALL_GETCHAR   1
#define SYSCALL_PRINTF    2
#define SYSCALL_CLEAR     3
#define SYSCALL_SET_COLOR 4
#define SYSCALL_SET_CURSOR 5

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

// System call numbers - Filesystem
#define SYSCALL_LIST_DIR    30
#define SYSCALL_LIST_DIR_CLUSTER 31
#define SYSCALL_FIND_ENTRY  32

// System call numbers - File I/O
#define SYSCALL_READ_FILE   33
#define SYSCALL_CREATE_FILE 34
#define SYSCALL_WRITE_FILE  35

// System call numbers - Program execution
#define SYSCALL_EXEC_PROGRAM 40

// System call interface for userspace programs
int syscall(int num, ...);

// Kernel-side system call handler
uint64_t syscall_handler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3);

#endif
