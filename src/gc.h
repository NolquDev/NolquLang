/*
 * gc.h — Nolqu mark-and-sweep garbage collector
 *
 * The GC is triggered automatically from the allocator when
 * nq_bytes_allocated exceeds nq_gc_threshold.
 *
 * Collection order:
 *   1. markRoots     — walk VM stack, call frames, globals, thrown value
 *   2. removeWhiteStrings — prune dead interned strings before sweep
 *   3. sweep         — free all unmarked objects, reset mark bits
 *   4. update threshold — nq_gc_threshold = max(2 * surviving, 1 MB)
 */

#ifndef NQ_GC_H
#define NQ_GC_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Run a full mark-and-sweep collection. */
void nq_collect(VM* vm);

/*
 * Called from the allocator after every object allocation.
 * Triggers nq_collect() if nq_bytes_allocated > nq_gc_threshold,
 * or does nothing if vm == NULL (pre-VM bootstrap).
 */
void nq_gc_maybe(VM* vm);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NQ_GC_H */
