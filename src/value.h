/*
 * value.h — Nolqu runtime value system
 *
 * Values are represented as a tagged union.  The "box" macros differ
 * between C and C++ because C99 compound literals are not valid C++:
 *
 *   C   uses: ((Value){VAL_NIL, {.number = 0}})   — compound literal
 *   C++ uses: inline helper functions               — portable
 *
 * All other declarations are identical for both languages.
 */

#ifndef NQ_VALUE_H
#define NQ_VALUE_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Forward declarations for heap-allocated types ───────────────── */
typedef struct Obj        Obj;
typedef struct ObjString  ObjString;
typedef struct ObjFunction ObjFunction;

/* ── Value type tags ─────────────────────────────────────────────── */
typedef enum {
    VAL_NIL,      /* nil                              */
    VAL_BOOL,     /* true / false                     */
    VAL_NUMBER,   /* 64-bit float (all Nolqu numbers) */
    VAL_OBJ,      /* heap-allocated object            */
} ValueType;

/* ── The Value — a tagged union ──────────────────────────────────── */
typedef struct {
    ValueType type;
    union {
        bool   boolean;
        double number;
        Obj*   obj;
    } as;
} Value;

/* ── Type-check predicates ───────────────────────────────────────── */
#define IS_NIL(v)       ((v).type == VAL_NIL)
#define IS_BOOL(v)      ((v).type == VAL_BOOL)
#define IS_NUMBER(v)    ((v).type == VAL_NUMBER)
#define IS_OBJ(v)       ((v).type == VAL_OBJ)

/* ── Unbox macros ────────────────────────────────────────────────── */
#define AS_BOOL(v)      ((v).as.boolean)
#define AS_NUMBER(v)    ((v).as.number)
#define AS_OBJ(v)       ((v).as.obj)

/* ── Box macros — create a Value from a primitive ────────────────── */
/*
 * C99 compound literals are not valid in C++.
 * Solution: C++ gets inline helper functions; C gets compound literals.
 * The macro names (NIL_VAL, BOOL_VAL, …) remain the same in both.
 */
#ifdef __cplusplus
} /* close extern "C" — inline functions must be outside it */

static inline Value NQ_NIL_VAL(void) {
    Value v; v.type = VAL_NIL; v.as.number = 0; return v;
}
static inline Value NQ_BOOL_VAL(bool b) {
    Value v; v.type = VAL_BOOL; v.as.boolean = b; return v;
}
static inline Value NQ_NUMBER_VAL(double n) {
    Value v; v.type = VAL_NUMBER; v.as.number = n; return v;
}
static inline Value NQ_OBJ_VAL(Obj* o) {
    Value v; v.type = VAL_OBJ; v.as.obj = o; return v;
}

#define NIL_VAL         NQ_NIL_VAL()
#define BOOL_VAL(b)     NQ_BOOL_VAL(b)
#define NUMBER_VAL(n)   NQ_NUMBER_VAL(n)
#define OBJ_VAL(o)      NQ_OBJ_VAL((Obj*)(o))

extern "C" {
#else   /* plain C — use C99 compound literals */

#define NIL_VAL         ((Value){VAL_NIL,    {.number  = 0}})
#define BOOL_VAL(b)     ((Value){VAL_BOOL,   {.boolean = (b)}})
#define NUMBER_VAL(n)   ((Value){VAL_NUMBER, {.number  = (n)}})
#define OBJ_VAL(o)      ((Value){VAL_OBJ,    {.obj = (Obj*)(o)}})

#endif /* __cplusplus */

/* ── Resizable Value array (used for chunk constants) ────────────── */
typedef struct {
    int    capacity;
    int    count;
    Value* values;
} ValueArray;

void initValueArray(ValueArray* arr);
void writeValueArray(ValueArray* arr, Value v);
void freeValueArray(ValueArray* arr);

/* ── Value utilities ─────────────────────────────────────────────── */
void        printValue(Value v);
bool        valuesEqual(Value a, Value b);
bool        isTruthy(Value v);
const char* valueTypeName(ValueType t);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NQ_VALUE_H */
