#ifndef LOADER_H
#define LOADER_H

#include <stdint.h>

// Program loader - loads and executes programs from disk

// Load a program from disk into memory
// filename: name of the program file (e.g. "SHELL.BIN")
// load_addr: address to load the program at
// Returns: 0 on success, -1 on error
int load_program(const char* filename, void* load_addr);

// Execute a loaded program
// entry_point: address of the program's entry point
// This function does not return
void execute_program(void* entry_point) __attribute__((noreturn));

#endif
