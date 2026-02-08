#ifndef VGA_H
#define VGA_H

#include <stdint.h>

// VGA dimensions
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// VGA colors
#define VGA_COLOR_BLACK 0
#define VGA_COLOR_BLUE 1
#define VGA_COLOR_GREEN 2
#define VGA_COLOR_CYAN 3
#define VGA_COLOR_RED 4
#define VGA_COLOR_MAGENTA 5
#define VGA_COLOR_BROWN 6
#define VGA_COLOR_LIGHT_GREY 7
#define VGA_COLOR_DARK_GREY 8
#define VGA_COLOR_LIGHT_BLUE 9
#define VGA_COLOR_LIGHT_GREEN 10
#define VGA_COLOR_LIGHT_CYAN 11
#define VGA_COLOR_LIGHT_RED 12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_YELLOW 14
#define VGA_COLOR_WHITE 15

// VGA functions
void vga_init(void);
void vga_clear(void);
void vga_putchar(char c);
void vga_write(const char* str);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_scroll(void);
void vga_update_cursor(void);
void vga_enable_cursor(uint8_t start, uint8_t end);
void vga_disable_cursor(void);

#endif
