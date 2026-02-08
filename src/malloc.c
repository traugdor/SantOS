#include "../include/stdlib.h"
#include "../include/string.h"

// Heap configuration
#define HEAP_SIZE (64 * 1024)  // 64KB heap (reduced from 4MB to avoid huge BSS)
static uint8_t heap_memory[HEAP_SIZE] __attribute__((aligned(16)));

// Block header structure
typedef struct block_header {
    size_t size;                    // Size of the block (excluding header)
    int is_free;                    // 1 if free, 0 if allocated
    struct block_header* next;      // Next block in the list
} block_header_t;

// Head of the free list
static block_header_t* heap_start = NULL;

// Initialize the heap
static void heap_init(void) {
    if (heap_start != NULL) return;  // Already initialized
    
    heap_start = (block_header_t*)heap_memory;
    heap_start->size = HEAP_SIZE - sizeof(block_header_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;
}

// Find a free block that fits (first-fit strategy)
static block_header_t* find_free_block(size_t size) {
    block_header_t* current = heap_start;
    
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;  // No suitable block found
}

// Split a block if it's too large
static void split_block(block_header_t* block, size_t size) {
    // Only split if there's enough space for a new block + header
    if (block->size >= size + sizeof(block_header_t) + 16) {
        block_header_t* new_block = (block_header_t*)((uint8_t*)block + sizeof(block_header_t) + size);
        new_block->size = block->size - size - sizeof(block_header_t);
        new_block->is_free = 1;
        new_block->next = block->next;
        
        block->size = size;
        block->next = new_block;
    }
}

// Coalesce adjacent free blocks
static void coalesce_free_blocks(void) {
    block_header_t* current = heap_start;
    
    while (current != NULL && current->next != NULL) {
        if (current->is_free && current->next->is_free) {
            // Merge current with next
            current->size += sizeof(block_header_t) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

void* malloc(size_t size) {
    if (size == 0) return NULL;
    
    // Initialize heap on first call
    if (heap_start == NULL) {
        heap_init();
    }
    
    // Align size to 8 bytes
    size = (size + 7) & ~7;
    
    // Find a free block
    block_header_t* block = find_free_block(size);
    if (block == NULL) {
        return NULL;  // Out of memory
    }
    
    // Split the block if it's too large
    split_block(block, size);
    
    // Mark as allocated
    block->is_free = 0;
    
    // Return pointer to usable memory (after header)
    return (void*)((uint8_t*)block + sizeof(block_header_t));
}

void free(void* ptr) {
    if (ptr == NULL) return;
    
    // Get the block header
    block_header_t* block = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    
    // Mark as free
    block->is_free = 1;
    
    // Coalesce adjacent free blocks
    coalesce_free_blocks();
}

void* calloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void* ptr = malloc(total_size);
    
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    if (ptr == NULL) {
        return malloc(size);
    }
    
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    
    // Get the old block header
    block_header_t* old_block = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    
    // If new size fits in current block, just return it
    if (old_block->size >= size) {
        return ptr;
    }
    
    // Allocate new block
    void* new_ptr = malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }
    
    // Copy old data
    memcpy(new_ptr, ptr, old_block->size);
    
    // Free old block
    free(ptr);
    
    return new_ptr;
}
