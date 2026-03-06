#include "../include/syscall.h"
#include "../include/stdio.h"
#include "../include/heap.h"
#include "../include/string.h"
#include <stdarg.h>

// Kernel-side system call handler
// Called when userspace triggers INT 0x80
void syscall_handler(void) {
    uint64_t syscall_num, arg1, arg2, arg3;
    
    // Get syscall number and arguments from registers
    __asm__ volatile(
        "mov %%rax, %0\n"
        "mov %%rdi, %1\n"
        "mov %%rsi, %2\n"
        "mov %%rdx, %3\n"
        : "=r"(syscall_num), "=r"(arg1), "=r"(arg2), "=r"(arg3)
    );
    
    uint64_t result = 0;
    
    switch (syscall_num) {
        // I/O syscalls
        case SYSCALL_PUTCHAR:
            putchar((char)arg1);
            result = 0;
            break;
            
        case SYSCALL_GETCHAR:
            // Re-enable interrupts so keyboard IRQ can fire
            __asm__ volatile("sti");
            result = (uint64_t)getchar();
            break;
            
        case SYSCALL_PRINTF:
            printf("%s", (const char*)arg1);
            result = 0;
            break;
        
        // Memory syscalls
        case SYSCALL_MALLOC:
            result = (uint64_t)malloc((size_t)arg1);
            break;
            
        case SYSCALL_FREE:
            free((void*)arg1);
            result = 0;
            break;
            
        case SYSCALL_REALLOC:
            result = (uint64_t)realloc((void*)arg1, (size_t)arg2);
            break;
            
        case SYSCALL_CALLOC:
            result = (uint64_t)calloc((size_t)arg1, (size_t)arg2);
            break;
        
        // String syscalls
        case SYSCALL_STRLEN:
            result = (uint64_t)strlen((const char*)arg1);
            break;
            
        case SYSCALL_STRCMP:
            result = (uint64_t)strcmp((const char*)arg1, (const char*)arg2);
            break;
            
        case SYSCALL_STRCPY:
            result = (uint64_t)strcpy((char*)arg1, (const char*)arg2);
            break;
            
        case SYSCALL_STRCAT:
            result = (uint64_t)strcat((char*)arg1, (const char*)arg2);
            break;
            
        case SYSCALL_MEMCPY:
            result = (uint64_t)memcpy((void*)arg1, (const void*)arg2, (size_t)arg3);
            break;
            
        case SYSCALL_MEMSET:
            result = (uint64_t)memset((void*)arg1, (int)arg2, (size_t)arg3);
            break;
            
        default:
            result = (uint64_t)-1;  // Invalid syscall
            break;
    }
    
    // Return result in RAX
    __asm__ volatile("mov %0, %%rax" : : "r"(result));
}
