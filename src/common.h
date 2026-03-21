/*
 * common.h — Nolqu shared foundation
 *
 * Included by every translation unit (C and C++).
 * Must compile cleanly under:
 *   gcc   -std=c11   -Wall -Wextra -Wpedantic
 *   g++   -std=c++17 -Wall -Wextra
 *   clang / clang++ (same flags)
 */

#ifndef NQ_COMMON_H
#define NQ_COMMON_H

/* ── Standard headers ────────────────────────────────────────────── */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
/*
 * stdbool.h: gives bool/true/false in C.
 * In C++ bool is a keyword — this is a harmless no-op there.
 */
#include <stdbool.h>

/* ── Version ─────────────────────────────────────────────────────── */
#define NQ_VERSION_MAJOR 1
#define NQ_VERSION_MINOR 1
#define NQ_VERSION_PATCH 1

/* ── Compiler hints ─────────────────────────────────────────────────── */
#ifdef __GNUC__
#  define NQ_INLINE      __attribute__((always_inline)) static inline
#  define NQ_RESTRICT    __restrict__
#  define NQ_LIKELY(x)   __builtin_expect(!!(x), 1)
#  define NQ_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define NQ_INLINE      static inline
#  define NQ_RESTRICT
#  define NQ_LIKELY(x)   (x)
#  define NQ_UNLIKELY(x) (x)
#endif

#define NQ_VERSION       "1.2.2a5"
#define NQ_LANG_NAME     "Nolqu"

/* ── VM limits ───────────────────────────────────────────────────── */
#define NQ_STACK_MAX    512   /* maximum values on the operand stack  */
#define NQ_FRAMES_MAX   64    /* maximum call-frame depth             */
#define NQ_LOCALS_MAX   256   /* maximum locals per function scope    */
#define NQ_CONSTS_MAX   65535 /* maximum constants per chunk (16-bit index) */

/* ── Dynamic-array helpers ───────────────────────────────────────── */
/*
 * All allocations are routed through nq_realloc so the GC can track
 * every byte.  These macros are safe to use from both C and C++.
 */
#define GROW_CAPACITY(cap) ((cap) < 8 ? 8 : (cap) * 2)

#define GROW_ARRAY(type, ptr, old_cap, new_cap) \
    (type*)nq_realloc((ptr), sizeof(type) * (size_t)(old_cap), \
                             sizeof(type) * (size_t)(new_cap))

#define FREE_ARRAY(type, ptr, old_cap) \
    nq_realloc((ptr), sizeof(type) * (size_t)(old_cap), 0)

/* ── ANSI colour codes ───────────────────────────────────────────── */
#define NQ_COLOR_RED     "\033[1;31m"
#define NQ_COLOR_YELLOW  "\033[1;33m"
#define NQ_COLOR_CYAN    "\033[1;36m"
#define NQ_COLOR_BOLD    "\033[1m"
#define NQ_COLOR_RESET   "\033[0m"
#define NQ_COLOR_GREEN   "\033[1;32m"
#define NQ_COLOR_BLUE    "\033[1;34m"

/* ── Utility ─────────────────────────────────────────────────────── */
#define NQ_UNUSED(x) (void)(x)

/*
 * Forward-declare VM so headers that take VM* don't need to pull in
 * vm.h (which would create circular includes).
 * The extern "C" wrapper ensures C++ sees C linkage for this typedef.
 */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct VM VM;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NQ_COMMON_H */
