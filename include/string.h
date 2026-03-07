#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdint.h>

// String functions
size_t strlen(const char* str);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t n);
char* strcpy(char* dest, const char* src);
char* strcat(char* dest, const char* src);
char* strchr(const char* str, int c);
char* strtok(char* str, const char* delim);
char* strdup(const char* str);
char* substr(const char* input, int start, int end);

// Memory functions
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* ptr, int value, size_t n);
int memcmp(const void* ptr1, const void* ptr2, size_t n);
void* memmove(void* dest, const void* src, size_t n);

#endif
