#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>

// Output functions (already exist in printf.h, but included here for completeness)
int printf(const char* format, ...);
int sprintf(char* buffer, const char* format, ...);
int snprintf(char* buffer, size_t size, const char* format, ...);
int vsnprintf(char* buffer, size_t size, const char* format, va_list args);

// Input functions
int getchar(void);                          // Read single character
char* gets(char* str);                      // Read line (unsafe, but classic)
char* fgets(char* str, int n);              // Read line (safe, with limit)
int scanf(const char* format, ...);         // Formatted input

// Character output
int putchar(int c);                         // Write single character
int puts(const char* str);                  // Write string with newline

// Screen control
void clear_screen(void);                    // Clear the screen
void set_color(unsigned char fg, unsigned char bg);  // Set text color
void set_cursor_pos(unsigned char x, unsigned char y);  // Set cursor position

// Filesystem operations
int list_dir(void);                         // List root directory
int list_dir_cluster(unsigned short cluster);  // List specific directory by cluster
unsigned short find_entry(unsigned short dir_cluster, const char* name, int* is_directory);  // Find entry in directory

// File I/O
int read_file(const char* filename, char* buffer, int buffer_size);  // Read file contents
int create_file(const char* filename);  // Create an empty file
int write_file(const char* filename, const char* buffer, int size);  // Write buffer to file

// Special key codes (returned by getchar for non-ASCII keys)
#define KEY_LEFT  0x01
#define KEY_RIGHT 0x02
#define KEY_UP    0x03
#define KEY_DOWN  0x04

// Program execution
int exec_program(const char* filename, int argc, char** argv);  // Execute a program file with arguments

// VGA Color constants
#define COLOR_BLACK         0
#define COLOR_BLUE          1
#define COLOR_GREEN         2
#define COLOR_CYAN          3
#define COLOR_RED           4
#define COLOR_MAGENTA       5
#define COLOR_BROWN         6
#define COLOR_LIGHT_GRAY    7
#define COLOR_DARK_GRAY     8
#define COLOR_LIGHT_BLUE    9
#define COLOR_LIGHT_GREEN   10
#define COLOR_LIGHT_CYAN    11
#define COLOR_LIGHT_RED     12
#define COLOR_LIGHT_MAGENTA 13
#define COLOR_YELLOW        14
#define COLOR_WHITE         15

#endif
