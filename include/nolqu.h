/*
 * nolqu.h — Public C/C++ Embedding API
 * Nolqu v1.0.0-rc1
 *
 * Embed the Nolqu interpreter in your C or C++ application.
 *
 * USAGE (C):
 *   #include "nolqu.h"
 *
 *   NqState* nq = nq_open();
 *   nq_dostring(nq, "print \"hello from C!\"");
 *   nq_close(nq);
 *
 * USAGE (C++):
 *   #include "nolqu.h"   // extern C linkage declared automatically
 *
 * BUILD:
 *   Compile all src/ .c files together with your app:
 *   gcc -Isrc your_app.c src/memory.c src/value.c src/chunk.c
 *              src/object.c src/table.c src/lexer.c src/ast.c
 *              src/parser.c src/compiler.c src/gc.c src/vm.c
 *              -lm -o your_app
 */

#ifndef NOLQU_H
#define NOLQU_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────
 *  Opaque state handle
 * ───────────────────────────────────────────── */
typedef struct NqState NqState;

/* ─────────────────────────────────────────────
 *  Value types (mirrors internal tagged union)
 * ───────────────────────────────────────────── */
typedef enum {
    NQ_TYPE_NIL     = 0,
    NQ_TYPE_BOOL    = 1,
    NQ_TYPE_NUMBER  = 2,
    NQ_TYPE_STRING  = 3,
    NQ_TYPE_ARRAY   = 4,
    NQ_TYPE_FUNCTION= 5,
} NqType;

/* A Nolqu value visible to the host */
typedef struct {
    NqType type;
    union {
        bool        boolean;
        double      number;
        const char* string;   /* valid until next GC cycle */
    } as;
} NqValue;

/* Convenience constructors */
static inline NqValue nq_nil(void)            { NqValue v; v.type = NQ_TYPE_NIL; v.as.number = 0; return v; }
static inline NqValue nq_bool(bool b)         { NqValue v; v.type = NQ_TYPE_BOOL;   v.as.boolean = b; return v; }
static inline NqValue nq_number(double n)     { NqValue v; v.type = NQ_TYPE_NUMBER; v.as.number  = n; return v; }

/* ─────────────────────────────────────────────
 *  Native function callback
 * ───────────────────────────────────────────── */
typedef NqValue (*NqNativeFn)(NqState* nq, int argc, NqValue* argv);

/* ─────────────────────────────────────────────
 *  Lifecycle
 * ───────────────────────────────────────────── */

/* Create a new Nolqu interpreter state. Returns NULL on OOM. */
NqState* nq_open(void);

/* Destroy state and free all memory. */
void nq_close(NqState* nq);

/* ─────────────────────────────────────────────
 *  Execution
 * ───────────────────────────────────────────── */

/* Run Nolqu source code from a string. Returns 0 on success. */
int nq_dostring(NqState* nq, const char* source);

/* Run a Nolqu source file. Returns 0 on success. */
int nq_dofile(NqState* nq, const char* path);

/* Run source with a custom name (shown in error messages). */
int nq_dostring_named(NqState* nq, const char* source, const char* name);

/* ─────────────────────────────────────────────
 *  Global variable access
 * ───────────────────────────────────────────── */

/* Get a global variable value. Returns NQ_TYPE_NIL if not found. */
NqValue nq_getglobal(NqState* nq, const char* name);

/* Set a global variable. */
void nq_setglobal_number(NqState* nq, const char* name, double value);
void nq_setglobal_string(NqState* nq, const char* name, const char* value);
void nq_setglobal_bool  (NqState* nq, const char* name, bool value);
void nq_setglobal_nil   (NqState* nq, const char* name);

/* ─────────────────────────────────────────────
 *  Calling Nolqu functions from C/C++
 * ───────────────────────────────────────────── */

/* Call a global Nolqu function by name with arguments.
 * result is set to the return value. Returns 0 on success. */
int nq_call(NqState* nq, const char* fn_name,
            int argc, NqValue* argv, NqValue* result);

/* ─────────────────────────────────────────────
 *  Registering C/C++ functions in Nolqu
 * ───────────────────────────────────────────── */

/* Register a native C function as a Nolqu global.
 * arity: expected argument count (-1 = variadic) */
void nq_register(NqState* nq, const char* name, NqNativeFn fn, int arity);

/* ─────────────────────────────────────────────
 *  Error handling
 * ───────────────────────────────────────────── */

/* Get the last error message (empty string if none). */
const char* nq_lasterror(NqState* nq);

/* ─────────────────────────────────────────────
 *  GC control
 * ───────────────────────────────────────────── */

/* Run a GC cycle. Returns bytes freed. */
size_t nq_gc(NqState* nq);

/* Set GC threshold in bytes (default: 1 MB). */
void nq_set_gc_threshold(NqState* nq, size_t bytes);

/* ─────────────────────────────────────────────
 *  Version
 * ───────────────────────────────────────────── */
/* Version is defined in src/common.h — included transitively. */
#ifndef NQ_VERSION_MAJOR
#define NQ_VERSION_MAJOR 1
#define NQ_VERSION_MINOR 1
#define NQ_VERSION_PATCH 1
#endif
#define NQ_VERSION_STRING NQ_VERSION

#ifdef __cplusplus
}
#endif

#endif /* NOLQU_H */
