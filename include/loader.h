#ifndef LOADER_H
#define LOADER_H

#include <stdint.h>

// Program loader - loads and executes programs from disk

// Load a program from disk into memory
// filename: name of the program file (e.g. "SHELL.ELF")
// load_addr: address to load the program at
// Returns: entry point address on success, 0 on error
uint64_t load_program(const char* filename, void* load_addr);

// Execute a loaded program
// entry_point: address of the program's entry point
// program_name: name of the program for logging (can be NULL)
// kernel_mode: if true, halt system on exit; if false, return to caller
// Returns when the program exits (only if kernel_mode is false)
void execute_program(uint64_t entry_point, const char* program_name, int kernel_mode);

#endif
