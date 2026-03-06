#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

// Initialize the kernel heap
// start: starting address for heap
// size: size of heap in bytes
void heap_init(void* start, size_t size);

// Allocate memory from heap
// size: number of bytes to allocate
// Returns: pointer to allocated memory, or NULL on failure
void* malloc(size_t size);

// Free previously allocated memory
// ptr: pointer to memory to free
void free(void* ptr);

// Reallocate memory to a new size
// ptr: pointer to previously allocated memory (or NULL)
// size: new size in bytes
// Returns: pointer to reallocated memory, or NULL on failure
void* realloc(void* ptr, size_t size);

// Allocate and zero-initialize memory
// nmemb: number of elements
// size: size of each element
// Returns: pointer to allocated memory, or NULL on failure
void* calloc(size_t nmemb, size_t size);

// Get heap statistics
void heap_stats(uint32_t* total, uint32_t* used, uint32_t* free);

#endif
