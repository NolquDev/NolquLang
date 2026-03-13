/*
 * memory.h — Nolqu tracked allocator
 *
 * All heap allocations in the runtime flow through nq_realloc so the
 * GC can track live bytes and trigger collection automatically.
 */

#ifndef NQ_MEMORY_H
#define NQ_MEMORY_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * nq_realloc — unified allocate / reallocate / free.
 *
 *   ptr == NULL, old_size == 0, new_size >  0  →  fresh allocation
 *   ptr != NULL, old_size >  0, new_size >  0  →  resize
 *   ptr != NULL, old_size >  0, new_size == 0  →  free
 *
 * On allocation failure the runtime prints an error and calls exit(1).
 * (Nolqu does not attempt to recover from OOM.)
 */
void* nq_realloc(void* ptr, size_t old_size, size_t new_size);

/* Live byte counter — updated by nq_realloc, read by the GC. */
extern size_t nq_bytes_allocated;
/* Trigger threshold — GC fires when nq_bytes_allocated exceeds this. */
extern size_t nq_gc_threshold;

/* ── Convenience wrappers ─────────────────────────────────────────── */
#define NQ_ALLOC(type, count) \
    (type*)nq_realloc(NULL, 0, sizeof(type) * (size_t)(count))

#define NQ_FREE(type, ptr) \
    nq_realloc((ptr), sizeof(type), 0)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NQ_MEMORY_H */
