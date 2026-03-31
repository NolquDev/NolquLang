/*
 * jit.h — Nolqu Template JIT Compiler
 *
 * Scope:
 *   Compiles a very specific pattern — a numeric for-range loop whose
 *   body consists only of "local += constant" operations — to native
 *   x86-64 machine code and executes it via mmap'd executable memory.
 *
 *   Pattern:
 *     for i = start to stop [step s]
 *       n  += delta1
 *       m  += delta2
 *       ...
 *     end
 *
 *   This covers the canonical benchmark (Python range() equivalent) and
 *   most simple counter/accumulator loops.
 *
 * Non-JIT-able loops (any other body) execute via the normal bytecode VM.
 *
 * Platform:
 *   x86-64 Linux/macOS (mmap + PROT_EXEC).
 *   All other platforms → fallback (same bytecode VM, no JIT).
 *
 * The JIT is activated at compile time when the compiler detects the
 * pattern. It emits OP_JIT_FOR_RANGE instead of the normal loop opcodes.
 * The VM executes it by generating native code once and running it.
 *
 * Safety:
 *   - No GC interaction: only reads/writes double fields of Value slots
 *     that are already live on the stack. No allocations inside JIT code.
 *   - No pointer arithmetic: slots are accessed via precomputed addresses.
 *   - On error (mmap fail): falls back to interpreted execution silently.
 */

#ifndef NQ_JIT_H
#define NQ_JIT_H

#include "common.h"
#include "value.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of body variables in a JIT-able loop */
#define NQ_JIT_MAX_BODY_VARS 8

/* Operations supported in JIT loop body */
typedef enum {
    JIT_OP_ADD = 0,   /* var += delta */
    JIT_OP_MUL = 1,   /* var *= delta */
} JITOp;

/*
 * JITLoopSpec — everything the JIT needs to compile a loop.
 *
 * All values are doubles (Nolqu numbers), accessed via Value* slots.
 * The Value struct is 16 bytes; .as.number is at byte offset 8.
 */
typedef struct {
    double* i_ptr;          /* pointer to the counter variable    */
    double  stop;           /* loop bound (not a pointer — const) */
    double  step;           /* step (positive or negative)        */
    int     n_vars;         /* number of body accumulator vars    */
    double* var_ptrs[NQ_JIT_MAX_BODY_VARS];  /* pointers to body vars */
    double  deltas[NQ_JIT_MAX_BODY_VARS];    /* amounts to add/mul    */
    JITOp   ops[NQ_JIT_MAX_BODY_VARS];       /* operation per var     */
} JITLoopSpec;

/*
 * nq_jit_run_loop — compile and execute a JIT-able loop.
 *
 * On x86-64: generates native code via mmap, caches it by loop shape,
 * and executes it at native speed.
 *
 * On other architectures: falls back to a C tight-loop (still fast due
 * to the direct double* pointers — no bytecode dispatch overhead).
 *
 * Returns true if JIT executed successfully.
 * Returns false if JIT compilation failed (caller should interpret normally).
 */
bool nq_jit_run_loop(JITLoopSpec* spec);

typedef struct {
    uint64_t attempts;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t compiled;
    uint64_t fallbacks;
    uint64_t cache_evictions;
    uint64_t cache_flushes;
    uint64_t unsupported_specs;
    uint64_t cache_entries;
} NQJitStats;

void nq_jit_get_stats(NQJitStats* out_stats);
void nq_jit_reset_stats(void);
void nq_jit_flush_cache(void);
uint64_t nq_jit_cache_capacity(void);
bool nq_jit_set_enabled(bool enabled);
bool nq_jit_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* NQ_JIT_H */
