#ifndef NQ_COMMON_H
#define NQ_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>

// ─────────────────────────────────────────────
//  Nolqu Runtime Version
// ─────────────────────────────────────────────
#define NQ_VERSION_MAJOR 0
#define NQ_VERSION_MINOR 5
#define NQ_VERSION_PATCH 0
#define NQ_VERSION       "0.5.0"
#define NQ_LANG_NAME     "Nolqu"

// ─────────────────────────────────────────────
//  VM limits
// ─────────────────────────────────────────────
#define NQ_STACK_MAX    512
#define NQ_FRAMES_MAX   64
#define NQ_LOCALS_MAX   256
#define NQ_CONSTS_MAX   256

// ─────────────────────────────────────────────
//  Dynamic array growth macros
// ─────────────────────────────────────────────
#define GROW_CAPACITY(cap) ((cap) < 8 ? 8 : (cap) * 2)

#define GROW_ARRAY(type, ptr, old_cap, new_cap) \
    (type*)nq_realloc((ptr), sizeof(type) * (old_cap), sizeof(type) * (new_cap))

#define FREE_ARRAY(type, ptr, old_cap) \
    nq_realloc((ptr), sizeof(type) * (old_cap), 0)

// ─────────────────────────────────────────────
//  ANSI color codes
// ─────────────────────────────────────────────
#define NQ_COLOR_RED     "\033[1;31m"
#define NQ_COLOR_YELLOW  "\033[1;33m"
#define NQ_COLOR_CYAN    "\033[1;36m"
#define NQ_COLOR_BOLD    "\033[1m"
#define NQ_COLOR_RESET   "\033[0m"
#define NQ_COLOR_GREEN   "\033[1;32m"
#define NQ_COLOR_BLUE    "\033[1;34m"

// ─────────────────────────────────────────────
//  Suppress unused parameter warnings
// ─────────────────────────────────────────────
#define NQ_UNUSED(x) (void)(x)

// ─────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────
typedef struct VM VM;

#endif // NQ_COMMON_H
