/*
 * object.h — Nolqu heap-allocated objects
 *
 * All heap objects share the Obj base header (type tag + GC mark bit
 * + intrusive linked-list pointer).  Each concrete type embeds Obj as
 * its first member so the base pointer can be cast to/from any type.
 *
 * Object types (as of v1.0.0):
 *   OBJ_STRING   — immutable, interned UTF-8 string
 *   OBJ_FUNCTION — compiled Nolqu function (owns a Chunk)
 *   OBJ_NATIVE   — built-in C/C++ function
 *   OBJ_ARRAY    — dynamic heterogeneous array
 */

#ifndef NQ_OBJECT_H
#define NQ_OBJECT_H

#include "common.h"
#include "value.h"
#include "chunk.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Object type tags ────────────────────────────────────────────── */
typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_ARRAY,
} ObjType;

/* ── Base object header ──────────────────────────────────────────── */
struct Obj {
    ObjType     type;
    bool        marked;     /* GC reachability mark                  */
    struct Obj* next;       /* intrusive list — all live objects      */
};

/* ── ObjString — interned, immutable string ──────────────────────── */
struct ObjString {
    Obj      obj;
    int      length;
    char*    chars;
    uint32_t hash;          /* FNV-1a hash, cached at creation        */
};

/* ── ObjFunction — compiled Nolqu function ───────────────────────── */
typedef struct ObjFunction {
    Obj        obj;
    int        arity;
    Chunk*     chunk;       /* owned — freed when function is freed   */
    ObjString* name;        /* NULL for the top-level script          */
} ObjFunction;

/* ── ObjNative — built-in function pointer ───────────────────────── */
typedef Value (*NativeFn)(int argc, Value* args);

typedef struct {
    Obj         obj;
    NativeFn    fn;
    const char* name;
    int         arity;      /* -1 = variadic                          */
} ObjNative;

/* ── ObjArray — dynamic heterogeneous array ──────────────────────── */
typedef struct {
    Obj    obj;
    Value* items;
    int    count;
    int    capacity;
} ObjArray;

/* ── Type-check / cast macros ────────────────────────────────────── */
#define OBJ_TYPE(v)       (AS_OBJ(v)->type)
#define IS_STRING(v)      (IS_OBJ(v) && OBJ_TYPE(v) == OBJ_STRING)
#define IS_FUNCTION(v)    (IS_OBJ(v) && OBJ_TYPE(v) == OBJ_FUNCTION)
#define IS_NATIVE(v)      (IS_OBJ(v) && OBJ_TYPE(v) == OBJ_NATIVE)
#define IS_ARRAY(v)       (IS_OBJ(v) && OBJ_TYPE(v) == OBJ_ARRAY)

#define AS_STRING(v)      ((ObjString*)AS_OBJ(v))
#define AS_CSTRING(v)     (((ObjString*)AS_OBJ(v))->chars)
#define AS_FUNCTION(v)    ((ObjFunction*)AS_OBJ(v))
#define AS_NATIVE(v)      ((ObjNative*)AS_OBJ(v))
#define AS_ARRAY(v)       ((ObjArray*)AS_OBJ(v))

/* ── GC-visible globals ──────────────────────────────────────────── */
/* Head of the intrusive linked list of all live heap objects.        */
extern Obj*    nq_all_objects;
/* Weak pointer to the running VM, set by runVM, used by GC trigger.  */
extern VM*     nq_gc_vm;

/* ── String intern table ─────────────────────────────────────────── */
/*
 * Open-addressing hash table of interned ObjString pointers.
 * Strings are de-duplicated: copyString() returns an existing entry
 * if the same bytes are already interned.
 */
typedef struct {
    ObjString** entries;
    int         count;
    int         capacity;
} StringTable;

extern StringTable nq_string_table;

void       initStringTable (StringTable* t);
void       freeStringTable (StringTable* t);
ObjString* tableFindString (StringTable* t, const char* chars,
                             int len, uint32_t hash);
void       tableAddString  (StringTable* t, ObjString* s);

/* ── Object constructors ─────────────────────────────────────────── */
ObjString*   copyString (const char* chars, int length);
ObjString*   takeString (char* chars, int length);   /* takes ownership */
ObjFunction* newFunction(void);
ObjNative*   newNative  (NativeFn fn, const char* name, int arity);
ObjArray*    newArray   (void);

/* ── ObjArray helpers ────────────────────────────────────────────── */
void  arrayPush(ObjArray* arr, Value value);
Value arrayPop (ObjArray* arr);
Value arrayGet (ObjArray* arr, int index);   /* supports negative index */
void  arraySet (ObjArray* arr, int index, Value value);

/* ── Object utilities ────────────────────────────────────────────── */
void        printObject    (Value v);
void        freeObject     (Obj* obj);
void        freeAllObjects (void);
const char* objectTypeName (ObjType t);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NQ_OBJECT_H */
