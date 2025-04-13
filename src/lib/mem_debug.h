/*******************************************************************
 *
 * mem_debug.h - Memory debugging and tracking system
 *
 ******************************************************************/
#ifndef MEM_DEBUG_H
#define MEM_DEBUG_H

#include <stdlib.h>
#include "../main.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Initialize the memory debugging system
     */
    void mem_debug_init(void);

    /**
     * Clean up memory debugging system and report leaks
     */
    void mem_debug_deinit(void);

    /**
     * Print memory statistics
     */
    void mem_debug_print_stats(void);

    /**
     * Check for memory leaks and print detailed report
     * @return Number of leaks found
     */
    int mem_debug_check_leaks(void);

#ifdef LV_CAMPER_DEBUG
// In debug mode, override standard memory functions with tracking versions
#define mem_malloc(size) mem_debug_malloc(size, __FILE__, __LINE__)
#define mem_calloc(num, size) mem_debug_calloc(num, size, __FILE__, __LINE__)
#define mem_realloc(ptr, size) mem_debug_realloc(ptr, size, __FILE__, __LINE__)
#define mem_free(ptr) mem_debug_free(ptr, __FILE__, __LINE__)

    // Debug versions with tracking
    void* mem_debug_malloc(size_t size, const char* file, int line);
    void* mem_debug_calloc(size_t num, size_t size, const char* file, int line);
    void* mem_debug_realloc(void* ptr, size_t size, const char* file, int line);
    void  mem_debug_free(void* ptr, const char* file, int line);
#else
// In release mode, just use standard functions
#define mem_malloc(size) malloc(size)
#define mem_calloc(num, size) calloc(num, size)
#define mem_realloc(ptr, size) realloc(ptr, size)
#define mem_free(ptr) free(ptr)
#endif

#ifdef __cplusplus
}
#endif

#endif /* MEM_DEBUG_H */
