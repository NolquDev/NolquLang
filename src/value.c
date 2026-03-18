#include "value.h"
#include "memory.h"
#include "object.h"

void initValueArray(ValueArray* arr) {
    arr->values   = NULL;
    arr->capacity = 0;
    arr->count    = 0;
}

void writeValueArray(ValueArray* arr, Value v) {
    if (arr->count >= arr->capacity) {
        int old_cap   = arr->capacity;
        arr->capacity = GROW_CAPACITY(old_cap);
        arr->values   = GROW_ARRAY(Value, arr->values, old_cap, arr->capacity);
    }
    arr->values[arr->count++] = v;
}

void freeValueArray(ValueArray* arr) {
    FREE_ARRAY(Value, arr->values, arr->capacity);
    initValueArray(arr);
}

void printValue(Value v) {
    switch (v.type) {
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_BOOL:
            printf("%s", AS_BOOL(v) ? "true" : "false");
            break;
        case VAL_NUMBER: {
            double n = AS_NUMBER(v);
            if (n == (long long)n && !isinf(n)) {
                printf("%lld", (long long)n);
            } else {
                printf("%g", n);
            }
            break;
        }
        case VAL_OBJ:
            printObject(v);
            break;
    }
}

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_NIL:    return true;
        case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:    return AS_OBJ(a) == AS_OBJ(b);
        default:         return false;
    }
}

bool isTruthy(Value v) {
    if (IS_NIL(v))  return false;
    if (IS_BOOL(v)) return AS_BOOL(v);
    return true;
}

const char* valueTypeName(ValueType t) {
    switch (t) {
        case VAL_NIL:    return "nil";
        case VAL_BOOL:   return "bool";
        case VAL_NUMBER: return "number";
        case VAL_OBJ:    return "object";
        default:         return "unknown";
    }
}
