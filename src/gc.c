/*
 * gc.c — Nolqu mark-and-sweep garbage collector
 *
 * Collection phases:
 *   1. markRoots()         — mark everything reachable from the VM
 *   2. removeWhiteStrings()— prune dead interned strings (must happen
 *                            BEFORE sweep so sweep can safely free them)
 *   3. sweep()             — free all unmarked objects; reset mark bits
 *   4. Adjust threshold    — nq_gc_threshold = max(2 × surviving, 1 MB)
 *
 * BUG FIX:
 *   Previous removeWhiteStrings() used calloc/free directly, bypassing
 *   the nq_realloc tracker.  This caused nq_bytes_allocated to drift,
 *   which made the GC trigger too early or too late.
 *   Fixed: all allocations now go through NQ_ALLOC / FREE_ARRAY.
 */

#include "gc.h"
#include "vm.h"
#include "object.h"
#include "memory.h"
#include "table.h"
#include <stdio.h>
#include <string.h>

/* ── Mark phase ──────────────────────────────────────────────────── */

static void markObject(Obj* obj) {
    if (obj == NULL || obj->marked) return;
    obj->marked = true;

    switch (obj->type) {
        case OBJ_STRING:
        case OBJ_NATIVE:
            /* Leaf objects — no heap children to recurse into. */
            break;

        case OBJ_FUNCTION: {
            ObjFunction* fn = (ObjFunction*)obj;
            if (fn->name) markObject((Obj*)fn->name);
            /* Mark all values in the constant pool. */
            for (int i = 0; i < fn->chunk->constants.count; i++) {
                Value v = fn->chunk->constants.values[i];
                if (IS_OBJ(v)) markObject(AS_OBJ(v));
            }
            break;
        }

        case OBJ_ARRAY: {
            ObjArray* arr = (ObjArray*)obj;
            for (int i = 0; i < arr->count; i++)
                if (IS_OBJ(arr->items[i])) markObject(AS_OBJ(arr->items[i]));
            break;
        }
    }
}

static void markValue(Value v) {
    if (IS_OBJ(v)) markObject(AS_OBJ(v));
}

static void markTable(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* e = &table->entries[i];
        if (e->key == NULL) continue;
        markObject((Obj*)e->key);
        markValue(e->value);
    }
}

static void markRoots(VM* vm) {
    /* 1. Every value currently on the operand stack. */
    for (Value* slot = vm->stack; slot < vm->stack_top; slot++)
        markValue(*slot);

    /* 2. The function object for each active call frame. */
    for (int i = 0; i < vm->frame_count; i++)
        markObject((Obj*)vm->frames[i].function);

    /*
     * 3. try_stack saved stack pointers point into vm->stack[], which
     *    is already fully scanned above — no extra marking needed.
     */

    /* 4. All global variables. */
    markTable(&vm->globals);

    /* 5. Any pending thrown error value. */
    markValue(vm->thrown);
}

/* ── Prune dead interned strings ─────────────────────────────────── */
/*
 * The string intern table is an open-addressing array.  We cannot
 * simply NULL out dead entries because that would break existing probe
 * chains.  Safe strategy: rebuild the table from scratch, re-inserting
 * only the marked (surviving) strings.
 *
 * FIX: use NQ_ALLOC / FREE_ARRAY (via nq_realloc) so the byte counter
 * stays accurate.  The old code used calloc + free directly.
 */
static void removeWhiteStrings(void) {
    StringTable* t = &nq_string_table;
    if (t->capacity == 0) return;

    int old_cap         = t->capacity;
    ObjString** old_buf = t->entries;

    /* Allocate a zeroed replacement buffer through the tracked allocator. */
    ObjString** new_buf = NQ_ALLOC(ObjString*, old_cap);
    memset(new_buf, 0, sizeof(ObjString*) * (size_t)old_cap);

    int new_count = 0;
    for (int i = 0; i < old_cap; i++) {
        ObjString* s = old_buf[i];
        if (s == NULL || !s->obj.marked) continue;
        /* Re-insert into the new buffer using linear probing. */
        uint32_t idx = s->hash % (uint32_t)old_cap;
        while (new_buf[idx] != NULL)
            idx = (idx + 1) % (uint32_t)old_cap;
        new_buf[idx] = s;
        new_count++;
    }

    /* Release the old buffer through the tracked allocator. */
    FREE_ARRAY(ObjString*, old_buf, old_cap);

    t->entries  = new_buf;
    t->count    = new_count;
    /* capacity stays the same — we rebuilt into a same-sized table. */
}

/* ── Sweep phase ─────────────────────────────────────────────────── */

static void sweep(void) {
    Obj** obj = &nq_all_objects;
    while (*obj) {
        if ((*obj)->marked) {
            /* Survived — clear mark for the next collection cycle. */
            (*obj)->marked = false;
            obj = &(*obj)->next;
        } else {
            /* Unreachable — unlink and free. */
            Obj* garbage = *obj;
            *obj = garbage->next;
            freeObject(garbage);
        }
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

void nq_collect(VM* vm) {
    markRoots(vm);
    /* Prune dead interned strings BEFORE sweep so sweep can free them. */
    removeWhiteStrings();
    sweep();

    /* Grow the threshold: trigger next GC at 2× surviving heap size,
     * with a minimum of 1 MB to avoid thrashing on small programs. */
    nq_gc_threshold = nq_bytes_allocated * 2;
    if (nq_gc_threshold < 1024 * 1024)
        nq_gc_threshold = 1024 * 1024;
}

void nq_gc_maybe(VM* vm) {
    if (vm == NULL) return;
    if (nq_bytes_allocated > nq_gc_threshold)
        nq_collect(vm);
}
