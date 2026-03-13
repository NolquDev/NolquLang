/*
 * memory.c — Nolqu tracked allocator
 *
 * Every heap allocation in the runtime flows through nq_realloc so the
 * GC can maintain an accurate byte count and trigger collection.
 *
 * IMPORTANT: do not call malloc/realloc/free directly anywhere in the
 * runtime.  Always use nq_realloc, NQ_ALLOC, NQ_FREE, GROW_ARRAY or
 * FREE_ARRAY so the counters stay correct.
 */

#include "memory.h"
#include <stdio.h>
#include <stdlib.h>

size_t nq_bytes_allocated = 0;
size_t nq_gc_threshold    = 1024 * 1024;   /* 1 MB initial threshold */

void* nq_realloc(void* ptr, size_t old_size, size_t new_size) {
    /*
     * Update the live-byte counter.
     * Guard against underflow: if old_size > nq_bytes_allocated the
     * counter is already at zero (this can happen if an allocation
     * was made before the counter was initialised).
     */
    if (old_size <= nq_bytes_allocated)
        nq_bytes_allocated -= old_size;
    else
        nq_bytes_allocated = 0;

    nq_bytes_allocated += new_size;

    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void* result = realloc(ptr, new_size);
    if (result == NULL) {
        fprintf(stderr,
            "\033[1;31m[nolqu]\033[0m"
            " Out of memory: could not allocate %zu bytes.\n",
            new_size);
        exit(1);
    }
    return result;
}
