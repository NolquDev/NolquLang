#include "object.h"
#include "memory.h"
#include "chunk.h"

Obj* nq_all_objects = NULL;
StringTable nq_string_table;

static uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

void initStringTable(StringTable* t) {
    t->entries  = NULL;
    t->count    = 0;
    t->capacity = 0;
}

void freeStringTable(StringTable* t) {
    FREE_ARRAY(ObjString*, t->entries, t->capacity);
    initStringTable(t);
}

ObjString* tableFindString(StringTable* t, const char* chars, int len, uint32_t hash) {
    if (t->capacity == 0) return NULL;
    uint32_t idx = hash % (uint32_t)t->capacity;
    for (;;) {
        ObjString* entry = t->entries[idx];
        if (entry == NULL) return NULL;
        if (entry->length == len && entry->hash == hash &&
            memcmp(entry->chars, chars, (size_t)len) == 0) {
            return entry;
        }
        idx = (idx + 1) % (uint32_t)t->capacity;
    }
}

void tableAddString(StringTable* t, ObjString* s) {
    if (t->count + 1 > t->capacity * 3 / 4) {
        int old_cap = t->capacity;
        t->capacity = GROW_CAPACITY(old_cap);
        ObjString** new_entries = NQ_ALLOC(ObjString*, t->capacity);
        memset(new_entries, 0, sizeof(ObjString*) * (size_t)t->capacity);
        for (int i = 0; i < old_cap; i++) {
            if (t->entries[i]) {
                uint32_t idx = t->entries[i]->hash % (uint32_t)t->capacity;
                while (new_entries[idx]) idx = (idx + 1) % (uint32_t)t->capacity;
                new_entries[idx] = t->entries[i];
            }
        }
        FREE_ARRAY(ObjString*, t->entries, old_cap);
        t->entries = new_entries;
    }
    uint32_t idx = s->hash % (uint32_t)t->capacity;
    while (t->entries[idx] && t->entries[idx] != s)
        idx = (idx + 1) % (uint32_t)t->capacity;
    if (!t->entries[idx]) { t->entries[idx] = s; t->count++; }
}

static Obj* allocObject(size_t size, ObjType type) {
    Obj* obj = (Obj*)nq_realloc(NULL, 0, size);
    obj->type   = type;
    obj->marked = false;
    obj->next   = nq_all_objects;
    nq_all_objects = obj;
    return obj;
}

#define ALLOC_OBJ(type, obj_type) \
    (type*)allocObject(sizeof(type), obj_type)

static ObjString* allocString(char* chars, int length, uint32_t hash) {
    ObjString* interned = tableFindString(&nq_string_table, chars, length, hash);
    if (interned) { free(chars); return interned; }
    ObjString* s = ALLOC_OBJ(ObjString, OBJ_STRING);
    s->chars  = chars;
    s->length = length;
    s->hash   = hash;
    tableAddString(&nq_string_table, s);
    return s;
}

ObjString* copyString(const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&nq_string_table, chars, length, hash);
    if (interned) return interned;
    char* buf = (char*)nq_realloc(NULL, 0, (size_t)(length + 1));
    memcpy(buf, chars, (size_t)length);
    buf[length] = '\0';
    return allocString(buf, length, hash);
}

ObjString* takeString(char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    return allocString(chars, length, hash);
}

ObjFunction* newFunction(void) {
    ObjFunction* fn = ALLOC_OBJ(ObjFunction, OBJ_FUNCTION);
    fn->arity = 0;
    fn->name  = NULL;
    fn->chunk = NQ_ALLOC(Chunk, 1);
    initChunk(fn->chunk);
    return fn;
}

ObjNative* newNative(NativeFn fn, const char* name, int arity) {
    ObjNative* native = ALLOC_OBJ(ObjNative, OBJ_NATIVE);
    native->fn    = fn;
    native->name  = name;
    native->arity = arity;
    return native;
}

ObjArray* newArray(void) {
    ObjArray* arr = ALLOC_OBJ(ObjArray, OBJ_ARRAY);
    arr->items    = NULL;
    arr->count    = 0;
    arr->capacity = 0;
    return arr;
}

void arrayPush(ObjArray* arr, Value value) {
    if (arr->count >= arr->capacity) {
        int old_cap   = arr->capacity;
        arr->capacity = GROW_CAPACITY(old_cap);
        arr->items    = GROW_ARRAY(Value, arr->items, old_cap, arr->capacity);
    }
    arr->items[arr->count++] = value;
}

Value arrayPop(ObjArray* arr) {
    if (arr->count == 0) return NIL_VAL;
    return arr->items[--arr->count];
}

Value arrayGet(ObjArray* arr, int index) {
    if (index < 0) index = arr->count + index; // negative indexing
    if (index < 0 || index >= arr->count) return NIL_VAL;
    return arr->items[index];
}

void arraySet(ObjArray* arr, int index, Value value) {
    if (index < 0) index = arr->count + index;
    if (index < 0 || index >= arr->count) return;
    arr->items[index] = value;
}

void printObject(Value v) {
    switch (OBJ_TYPE(v)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(v));
            break;
        case OBJ_FUNCTION: {
            ObjFunction* fn = AS_FUNCTION(v);
            if (fn->name) printf("<function %s>", fn->name->chars);
            else          printf("<script>");
            break;
        }
        case OBJ_NATIVE:
            printf("<native %s>", AS_NATIVE(v)->name);
            break;
        case OBJ_ARRAY: {
            ObjArray* arr = AS_ARRAY(v);
            printf("[");
            for (int i = 0; i < arr->count; i++) {
                if (IS_STRING(arr->items[i]))
                    printf("\"%s\"", AS_CSTRING(arr->items[i]));
                else
                    printValue(arr->items[i]);
                if (i < arr->count - 1) printf(", ");
            }
            printf("]");
            break;
        }
    }
}

void freeObject(Obj* obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString* s = (ObjString*)obj;
            nq_realloc(s->chars, (size_t)(s->length + 1), 0);
            nq_realloc(obj, sizeof(ObjString), 0);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* fn = (ObjFunction*)obj;
            freeChunk(fn->chunk);
            nq_realloc(fn->chunk, sizeof(Chunk), 0);
            nq_realloc(obj, sizeof(ObjFunction), 0);
            break;
        }
        case OBJ_NATIVE:
            nq_realloc(obj, sizeof(ObjNative), 0);
            break;
        case OBJ_ARRAY: {
            ObjArray* arr = (ObjArray*)obj;
            FREE_ARRAY(Value, arr->items, arr->capacity);
            nq_realloc(obj, sizeof(ObjArray), 0);
            break;
        }
    }
}

void freeAllObjects(void) {
    Obj* obj = nq_all_objects;
    while (obj) {
        Obj* next = obj->next;
        freeObject(obj);
        obj = next;
    }
    nq_all_objects = NULL;
}

const char* objectTypeName(ObjType t) {
    switch (t) {
        case OBJ_STRING:   return "string";
        case OBJ_FUNCTION: return "function";
        case OBJ_NATIVE:   return "native function";
        case OBJ_ARRAY:    return "array";
        default:           return "object";
    }
}
