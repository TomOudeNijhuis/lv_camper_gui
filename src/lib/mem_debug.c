/*******************************************************************
 *
 * mem_debug.c - Memory debugging and tracking implementation
 *
 ******************************************************************/
#include "mem_debug.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#ifdef LV_CAMPER_DEBUG

// Define memory block header structure for tracking
typedef struct mem_block
{
    size_t            size;  // Size of the allocated block
    const char*       file;  // Source file that allocated this block
    int               line;  // Line number in the source file
    unsigned int      magic; // Magic number to detect corruption
    struct mem_block* next;  // Pointer to next block in the list
    struct mem_block* prev;  // Pointer to previous block in the list
} mem_block_t;

// Define memory tracking structure
typedef struct
{
    mem_block_t*    blocks;           // Linked list of allocated blocks
    size_t          total_allocated;  // Total bytes currently allocated
    size_t          peak_allocated;   // Peak memory usage
    size_t          allocation_count; // Number of allocations performed
    size_t          free_count;       // Number of frees performed
    pthread_mutex_t mutex;            // Mutex for thread safety
} mem_debug_t;

// Magic number for block validation
#define MEM_BLOCK_MAGIC 0xDEADBEEF

// Static instance of memory tracking
static mem_debug_t mem_debug = {0};

/**
 * Initialize memory debugging system
 */
void mem_debug_init(void)
{
    memset(&mem_debug, 0, sizeof(mem_debug_t));
    pthread_mutex_init(&mem_debug.mutex, NULL);
    log_info("Memory debugging system initialized");
}

/**
 * Clean up memory debugging system and report leaks
 */
void mem_debug_deinit(void)
{
    mem_debug_print_stats();
    int leaks = mem_debug_check_leaks();

    if(leaks > 0)
    {
        log_warning("Memory leaks detected: %d blocks not freed", leaks);
    }
    else
    {
        log_info("No memory leaks detected");
    }

    pthread_mutex_destroy(&mem_debug.mutex);
}

/**
 * Print memory usage statistics
 */
void mem_debug_print_stats(void)
{
    pthread_mutex_lock(&mem_debug.mutex);

    log_info("Camper application usage statistics:");
    log_info("  Currently allocated: %zu bytes", mem_debug.total_allocated);
    log_info("  Peak allocated: %zu bytes", mem_debug.peak_allocated);
    log_info("  Allocation operations: %zu", mem_debug.allocation_count);
    log_info("  Free operations: %zu", mem_debug.free_count);

    pthread_mutex_unlock(&mem_debug.mutex);
}

/**
 * Check for memory leaks and print detailed report
 * @return Number of leaks found
 */
int mem_debug_check_leaks(void)
{
    pthread_mutex_lock(&mem_debug.mutex);

    int          leak_count = 0;
    mem_block_t* block      = mem_debug.blocks;

    while(block != NULL)
    {
        leak_count++;
        log_warning("Memory leak: %zu bytes at %p, allocated in %s:%d", block->size,
                    (void*)(block + 1), block->file, block->line);

        block = block->next;
    }

    pthread_mutex_unlock(&mem_debug.mutex);
    return leak_count;
}

/**
 * Add a block to the tracking list
 */
static void add_block(mem_block_t* block)
{
    pthread_mutex_lock(&mem_debug.mutex);

    // Update stats
    mem_debug.total_allocated += block->size;
    mem_debug.allocation_count++;

    if(mem_debug.total_allocated > mem_debug.peak_allocated)
    {
        mem_debug.peak_allocated = mem_debug.total_allocated;
    }

    // Add to linked list
    block->next = mem_debug.blocks;
    block->prev = NULL;

    if(mem_debug.blocks != NULL)
    {
        mem_debug.blocks->prev = block;
    }

    mem_debug.blocks = block;

    pthread_mutex_unlock(&mem_debug.mutex);
}

/**
 * Remove a block from the tracking list
 */
static void remove_block(mem_block_t* block)
{
    pthread_mutex_lock(&mem_debug.mutex);

    // Update stats
    mem_debug.total_allocated -= block->size;
    mem_debug.free_count++;

    // Remove from linked list
    if(block->prev != NULL)
    {
        block->prev->next = block->next;
    }
    else
    {
        mem_debug.blocks = block->next;
    }

    if(block->next != NULL)
    {
        block->next->prev = block->prev;
    }

    pthread_mutex_unlock(&mem_debug.mutex);
}

/**
 * Find a block in the tracking list
 */
static mem_block_t* find_block(void* ptr)
{
    mem_block_t* block = (mem_block_t*)ptr - 1;

    // Verify the magic number to check for corruption
    if(block->magic != MEM_BLOCK_MAGIC)
    {
        log_error("Memory corruption detected or invalid pointer %p", ptr);
        return NULL;
    }

    return block;
}

/**
 * Debug version of malloc with tracking
 */
void* mem_debug_malloc(size_t size, const char* file, int line)
{
    if(size == 0)
    {
        log_warning("Zero-size allocation requested at %s:%d", file, line);
        return NULL;
    }

    // Allocate memory with space for tracking header
    mem_block_t* block = (mem_block_t*)malloc(sizeof(mem_block_t) + size);
    if(block == NULL)
    {
        log_error("Memory allocation failed for %zu bytes at %s:%d", size, file, line);
        return NULL;
    }

    // Initialize block header
    block->size  = size;
    block->file  = file;
    block->line  = line;
    block->magic = MEM_BLOCK_MAGIC;

    // Add to tracking
    add_block(block);

    // Return pointer to the user's memory area (after the header)
    return (void*)(block + 1);
}

/**
 * Debug version of calloc with tracking
 */
void* mem_debug_calloc(size_t num, size_t size, const char* file, int line)
{
    size_t total_size = num * size;
    void*  ptr        = mem_debug_malloc(total_size, file, line);

    if(ptr != NULL)
    {
        memset(ptr, 0, total_size);
    }

    return ptr;
}

/**
 * Debug version of realloc with tracking
 */
void* mem_debug_realloc(void* ptr, size_t size, const char* file, int line)
{
    // Handle NULL pointer like malloc
    if(ptr == NULL)
    {
        return mem_debug_malloc(size, file, line);
    }

    // Handle zero size like free
    if(size == 0)
    {
        mem_debug_free(ptr, file, line);
        return NULL;
    }

    // Find the block header
    mem_block_t* block = find_block(ptr);
    if(block == NULL)
    {
        log_error("Invalid pointer passed to realloc at %s:%d", file, line);
        return NULL;
    }

    // Remove the old block from tracking
    remove_block(block);

    // Reallocate the memory
    mem_block_t* new_block = (mem_block_t*)realloc(block, sizeof(mem_block_t) + size);
    if(new_block == NULL)
    {
        log_error("Memory reallocation failed for %zu bytes at %s:%d", size, file, line);

        // Re-add the original block to tracking since realloc failed
        add_block(block);
        return NULL;
    }

    // Update block header
    new_block->size = size;
    new_block->file = file;
    new_block->line = line;

    // Add the new block to tracking
    add_block(new_block);

    // Return pointer to the user's memory area
    return (void*)(new_block + 1);
}

/**
 * Debug version of free with tracking
 */
void mem_debug_free(void* ptr, const char* file, int line)
{
    if(ptr == NULL)
    {
        log_warning("NULL pointer passed to free at %s:%d", file, line);
        return;
    }

    // Find the block header
    mem_block_t* block = find_block(ptr);
    if(block == NULL)
    {
        log_error("Invalid pointer passed to free at %s:%d", file, line);
        return;
    }

    // Remove from tracking
    remove_block(block);

    // Actually free the memory
    free(block);
}

#else /* LV_CAMPER_DEBUG */

// Non-debug implementations

void mem_debug_init(void)
{
    // Do nothing in release mode
}

void mem_debug_deinit(void)
{
    // Do nothing in release mode
}

void mem_debug_print_stats(void)
{
    // Do nothing in release mode
}

int mem_debug_check_leaks(void)
{
    // Always return 0 in release mode
    return 0;
}

#endif /* LV_CAMPER_DEBUG */
