#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Keyboard functions
void keyboard_init(void);
char keyboard_getchar(void);  // Blocking - waits for key
int keyboard_available(void);  // Check if key is available

#endif
