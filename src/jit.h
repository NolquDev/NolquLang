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

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of body variables in a JIT-able loop */
#define NQ_JIT_MAX_BODY_VARS 8

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
    double  deltas[NQ_JIT_MAX_BODY_VARS];    /* amounts to add        */
} JITLoopSpec;

/*
 * nq_jit_run_loop — compile and execute a JIT-able loop.
 *
 * On x86-64: generates 33–200 bytes of native code via mmap, executes it,
 * frees the mapping. The loop runs at native speed.
 *
 * On other architectures: falls back to a C tight-loop (still fast due
 * to the direct double* pointers — no bytecode dispatch overhead).
 *
 * Returns true if JIT executed successfully.
 * Returns false if JIT compilation failed (caller should interpret normally).
 */
bool nq_jit_run_loop(JITLoopSpec* spec);

#ifdef __cplusplus
}
#endif

#endif /* NQ_JIT_H */
