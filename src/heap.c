#include "../include/heap.h"
#include "../include/printf.h"
#include <stdint.h>
#include <stddef.h>

// Block header for heap allocations
typedef struct block_header {
    size_t size;                    // Size of block (excluding header)
    struct block_header* next;      // Next free block (NULL if allocated)
    uint32_t magic;                 // Magic number for validation
} block_header_t;

#define HEAP_MAGIC 0xDEADBEEF
#define BLOCK_HEADER_SIZE sizeof(block_header_t)
#define ALIGNMENT 16  // Align to 16 bytes

static void* heap_start = NULL;
static size_t heap_size = 0;
static block_header_t* free_list = NULL;

// Align size up to nearest multiple of ALIGNMENT
static inline size_t align_size(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

// Initialize the kernel heap
void heap_init(void* start, size_t size) {
    heap_start = start;
    heap_size = size;
    
    // Create initial free block spanning entire heap
    free_list = (block_header_t*)start;
    free_list->size = size - BLOCK_HEADER_SIZE;
    free_list->next = NULL;
    free_list->magic = HEAP_MAGIC;
    
    printf("Kernel heap initialized:\n");
    printf("  Start: %x\n", (uint32_t)(uint64_t)start);
    printf("  Size: %d bytes (%d KB)\n\n", size, size / 1024);
}

// Find a free block that fits the requested size (first-fit algorithm)
static block_header_t* find_free_block(size_t size) {
    block_header_t* current = free_list;
    block_header_t* prev = NULL;
    
    while (current != NULL) {
        if (current->magic != HEAP_MAGIC) {
            printf("HEAP CORRUPTION: Invalid magic at %x\n", (uint32_t)(uint64_t)current);
            return NULL;
        }
        
        if (current->size >= size) {
            // Found a suitable block
            return current;
        }
        
        prev = current;
        current = current->next;
    }
    
    return NULL;  // No suitable block found
}

// Split a block if it's large enough
static void split_block(block_header_t* block, size_t size) {
    // Only split if remainder is large enough for a new block
    if (block->size >= size + BLOCK_HEADER_SIZE + ALIGNMENT) {
        block_header_t* new_block = (block_header_t*)((uint8_t*)block + BLOCK_HEADER_SIZE + size);
        new_block->size = block->size - size - BLOCK_HEADER_SIZE;
        new_block->next = block->next;
        new_block->magic = HEAP_MAGIC;
        
        block->size = size;
        block->next = new_block;
    }
}

// Remove a block from the free list
static void remove_from_free_list(block_header_t* block) {
    if (free_list == block) {
        free_list = block->next;
        return;
    }
    
    block_header_t* current = free_list;
    while (current != NULL && current->next != block) {
        current = current->next;
    }
    
    if (current != NULL) {
        current->next = block->next;
    }
}

// Allocate memory from heap
void* malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    // Align size
    size = align_size(size);
    
    // Find a free block
    block_header_t* block = find_free_block(size);
    if (block == NULL) {
        printf("malloc: Out of memory (requested %d bytes)\n", size);
        return NULL;
    }
    
    // Split block if possible
    split_block(block, size);
    
    // Remove from free list
    remove_from_free_list(block);
    block->next = NULL;  // Mark as allocated
    
    // Return pointer to data (after header)
    return (void*)((uint8_t*)block + BLOCK_HEADER_SIZE);
}

// Free previously allocated memory
void free(void* ptr) {
    if (ptr == NULL) {
        return;
    }
    
    // Get block header
    block_header_t* block = (block_header_t*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    
    // Validate magic number
    if (block->magic != HEAP_MAGIC) {
        printf("free: Invalid pointer or corrupted heap at %x\n", (uint32_t)(uint64_t)ptr);
        return;
    }
    
    // Add block back to free list (at the beginning for simplicity)
    block->next = free_list;
    free_list = block;
    
    // TODO: Coalesce adjacent free blocks to reduce fragmentation
}

// Reallocate memory to a new size
void* realloc(void* ptr, size_t size) {
    if (ptr == NULL) {
        return malloc(size);
    }
    
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    
    // Get current block
    block_header_t* block = (block_header_t*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    
    // Validate magic number
    if (block->magic != HEAP_MAGIC) {
        printf("realloc: Invalid pointer or corrupted heap at %x\n", (uint32_t)(uint64_t)ptr);
        return NULL;
    }
    
    size_t aligned_size = align_size(size);
    
    // If new size fits in current block, just return same pointer
    if (aligned_size <= block->size) {
        return ptr;
    }
    
    // Allocate new block
    void* new_ptr = malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }
    
    // Copy old data to new block
    uint8_t* src = (uint8_t*)ptr;
    uint8_t* dst = (uint8_t*)new_ptr;
    for (size_t i = 0; i < block->size && i < size; i++) {
        dst[i] = src[i];
    }
    
    // Free old block
    free(ptr);
    
    return new_ptr;
}

// Allocate and zero-initialize memory
void* calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    
    void* ptr = malloc(total_size);
    if (ptr == NULL) {
        return NULL;
    }
    
    // Zero-initialize
    uint8_t* bytes = (uint8_t*)ptr;
    for (size_t i = 0; i < total_size; i++) {
        bytes[i] = 0;
    }
    
    return ptr;
}

// Get heap statistics
void heap_stats(uint32_t* total, uint32_t* used, uint32_t* free_bytes) {
    *total = heap_size;
    
    // Calculate free bytes by traversing free list
    uint32_t free_sum = 0;
    block_header_t* current = free_list;
    while (current != NULL) {
        free_sum += current->size + BLOCK_HEADER_SIZE;
        current = current->next;
    }
    
    *free_bytes = free_sum;
    *used = heap_size - free_sum;
}
