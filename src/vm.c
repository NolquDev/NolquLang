#include "vm.h"
#include "memory.h"
#include "object.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// ─────────────────────────────────────────────
//  Native function implementations
// ─────────────────────────────────────────────

// input([prompt]) — read a line from stdin, return as string
static Value nativeInput(int argc, Value* args) {
    if (argc == 1 && IS_STRING(args[0])) {
        printf("%s", AS_CSTRING(args[0]));
        fflush(stdout);
    }
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) {
        return OBJ_VAL(copyString("", 0));
    }
    int len = (int)strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';
    return OBJ_VAL(copyString(buf, len));
}

// str(value) — convert any value to string
static Value nativeStr(int argc, Value* args) {
    if (argc != 1) return OBJ_VAL(copyString("", 0));
    Value v = args[0];
    char buf[64];
    if (IS_STRING(v))  return v;
    if (IS_NIL(v))     return OBJ_VAL(copyString("nil", 3));
    if (IS_BOOL(v))    return OBJ_VAL(copyString(AS_BOOL(v) ? "true" : "false",
                                                  AS_BOOL(v) ? 4 : 5));
    if (IS_NUMBER(v)) {
        double n = AS_NUMBER(v);
        if (n == (long long)n) snprintf(buf, sizeof(buf), "%lld", (long long)n);
        else                   snprintf(buf, sizeof(buf), "%g", n);
        return OBJ_VAL(copyString(buf, (int)strlen(buf)));
    }
    return OBJ_VAL(copyString("<object>", 8));
}

// num(value) — convert string to number, returns nil if invalid
static Value nativeNum(int argc, Value* args) {
    if (argc != 1) return NIL_VAL;
    if (IS_NUMBER(args[0])) return args[0];
    if (IS_STRING(args[0])) {
        char* end;
        double n = strtod(AS_CSTRING(args[0]), &end);
        if (end != AS_CSTRING(args[0]) && *end == '\0') return NUMBER_VAL(n);
        return NIL_VAL;
    }
    return NIL_VAL;
}

// type(value) — return the type name as a string
static Value nativeType(int argc, Value* args) {
    if (argc != 1) return NIL_VAL;
    Value v = args[0];
    const char* name;
    if (IS_NIL(v))      name = "nil";
    else if (IS_BOOL(v))   name = "bool";
    else if (IS_NUMBER(v)) name = "number";
    else if (IS_STRING(v)) name = "string";
    else if (IS_ARRAY(v))  name = "array";
    else if (IS_FUNCTION(v) || IS_NATIVE(v)) name = "function";
    else name = "object";
    return OBJ_VAL(copyString(name, (int)strlen(name)));
}

// len(arr_or_string) — return length
static Value nativeLen(int argc, Value* args) {
    if (argc != 1) return NIL_VAL;
    if (IS_ARRAY(args[0]))  return NUMBER_VAL(AS_ARRAY(args[0])->count);
    if (IS_STRING(args[0])) return NUMBER_VAL(AS_STRING(args[0])->length);
    return NIL_VAL;
}

// push(arr, value) — append to array, return array
static Value nativePush(int argc, Value* args) {
    if (argc != 2 || !IS_ARRAY(args[0])) return NIL_VAL;
    arrayPush(AS_ARRAY(args[0]), args[1]);
    return args[0];
}

// pop(arr) — remove and return last element
static Value nativePop(int argc, Value* args) {
    if (argc != 1 || !IS_ARRAY(args[0])) return NIL_VAL;
    return arrayPop(AS_ARRAY(args[0]));
}

// remove(arr, index) — remove element at index, return removed value
static Value nativeRemove(int argc, Value* args) {
    if (argc != 2 || !IS_ARRAY(args[0]) || !IS_NUMBER(args[1])) return NIL_VAL;
    ObjArray* arr = AS_ARRAY(args[0]);
    int idx = (int)AS_NUMBER(args[1]);
    if (idx < 0) idx = arr->count + idx;
    if (idx < 0 || idx >= arr->count) return NIL_VAL;
    Value removed = arr->items[idx];
    for (int i = idx; i < arr->count - 1; i++)
        arr->items[i] = arr->items[i + 1];
    arr->count--;
    return removed;
}

// contains(arr, value) — return true if value is in array
static Value nativeContains(int argc, Value* args) {
    if (argc != 2 || !IS_ARRAY(args[0])) return BOOL_VAL(false);
    ObjArray* arr = AS_ARRAY(args[0]);
    for (int i = 0; i < arr->count; i++)
        if (valuesEqual(arr->items[i], args[1])) return BOOL_VAL(true);
    return BOOL_VAL(false);
}

// ─────────────────────────────────────────────
//  VM init / free
// ─────────────────────────────────────────────
static void registerNative(VM* vm, const char* name, NativeFn fn, int arity) {
    ObjNative*  native   = newNative(fn, name, arity);
    ObjString*  key      = copyString(name, (int)strlen(name));
    tableSet(&vm->globals, key, OBJ_VAL(native));
}

void initVM(VM* vm) {
    vm->stack_top   = vm->stack;
    vm->frame_count = 0;
    vm->source_path = NULL;
    initTable(&vm->globals);
    initStringTable(&nq_string_table);

    // Register built-in functions
    registerNative(vm, "input",    nativeInput,    -1);
    registerNative(vm, "str",      nativeStr,       1);
    registerNative(vm, "num",      nativeNum,       1);
    registerNative(vm, "type",     nativeType,      1);
    registerNative(vm, "len",      nativeLen,       1);
    registerNative(vm, "push",     nativePush,      2);
    registerNative(vm, "pop",      nativePop,       1);
    registerNative(vm, "remove",   nativeRemove,    2);
    registerNative(vm, "contains", nativeContains,  2);
}

void freeVM(VM* vm) {
    freeTable(&vm->globals);
    freeStringTable(&nq_string_table);
    freeAllObjects();
}

// ─────────────────────────────────────────────
//  Stack
// ─────────────────────────────────────────────
static void push(VM* vm, Value v) {
    if (vm->stack_top >= vm->stack + NQ_STACK_MAX) {
        fprintf(stderr,
            NQ_COLOR_RED "[ Runtime Error ]" NQ_COLOR_RESET
            " Stack overflow! Possible infinite recursion.\n");
        exit(1);
    }
    *vm->stack_top++ = v;
}

static Value pop(VM* vm)              { return *--vm->stack_top; }
static Value peek(VM* vm, int dist)   { return vm->stack_top[-1 - dist]; }

// ─────────────────────────────────────────────
//  Runtime error
// ─────────────────────────────────────────────
void vmRuntimeError(VM* vm, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, NQ_COLOR_RED "\n[ Runtime Error ]" NQ_COLOR_RESET);
    if (vm->source_path) fprintf(stderr, " %s", vm->source_path);

    for (int i = vm->frame_count - 1; i >= 0; i--) {
        CallFrame*   frame = &vm->frames[i];
        ObjFunction* fn    = frame->function;
        int offset = (int)(frame->ip - fn->chunk->code - 1);
        int line   = fn->chunk->lines[offset];
        fprintf(stderr, ":%d\n", line);
        if (fn->name) fprintf(stderr, "  in function '%s'\n", fn->name->chars);
    }

    fprintf(stderr, "  ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n\n");
    va_end(args);
}

// ─────────────────────────────────────────────
//  Type helpers
// ─────────────────────────────────────────────
static bool checkNumber(VM* vm, Value v, const char* hint) {
    if (IS_NUMBER(v)) return true;
    vmRuntimeError(vm,
        "Expected a number, got %s.\n  Hint: %s",
        IS_STRING(v) ? "string" : IS_BOOL(v) ? "bool" : IS_NIL(v) ? "nil" : "object",
        hint);
    return false;
}

static ObjString* valueToString(Value v) {
    if (IS_STRING(v)) return AS_STRING(v);
    char buf[64];
    if (IS_NIL(v))       snprintf(buf, sizeof(buf), "nil");
    else if (IS_BOOL(v)) snprintf(buf, sizeof(buf), "%s", AS_BOOL(v) ? "true" : "false");
    else if (IS_NUMBER(v)) {
        double n = AS_NUMBER(v);
        if (n == (long long)n) snprintf(buf, sizeof(buf), "%lld", (long long)n);
        else                   snprintf(buf, sizeof(buf), "%g", n);
    } else snprintf(buf, sizeof(buf), "<object>");
    return copyString(buf, (int)strlen(buf));
}

static Value concatStrings(ObjString* a, ObjString* b) {
    int len  = a->length + b->length;
    char* buf = (char*)malloc((size_t)(len + 1));
    memcpy(buf,             a->chars, (size_t)a->length);
    memcpy(buf + a->length, b->chars, (size_t)b->length);
    buf[len] = '\0';
    return OBJ_VAL(takeString(buf, len));
}

// ─────────────────────────────────────────────
//  Function call
// ─────────────────────────────────────────────
static bool callFunction(VM* vm, ObjFunction* fn, int argc) {
    if (argc != fn->arity) {
        vmRuntimeError(vm,
            "Function '%s' expects %d argument(s), but got %d.",
            fn->name ? fn->name->chars : "<script>",
            fn->arity, argc);
        return false;
    }
    if (vm->frame_count >= NQ_FRAMES_MAX) {
        vmRuntimeError(vm, "Stack overflow — too many nested function calls.");
        return false;
    }
    CallFrame* frame = &vm->frames[vm->frame_count++];
    frame->function  = fn;
    frame->ip        = fn->chunk->code;
    frame->slots     = vm->stack_top - argc - 1;
    return true;
}

// ─────────────────────────────────────────────
//  Main execution loop
// ─────────────────────────────────────────────
InterpretResult runVM(VM* vm, ObjFunction* script, const char* source_path) {
    vm->source_path = source_path;
    push(vm, OBJ_VAL(script));
    callFunction(vm, script, 0);
    CallFrame* frame = &vm->frames[vm->frame_count - 1];

#define READ_BYTE()    (*frame->ip++)
#define READ_UINT16()  (frame->ip += 2, \
                        (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONST()   (frame->function->chunk->constants.values[READ_BYTE()])

#define BINARY_NUM(op) do { \
    if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
        vmRuntimeError(vm, \
            "This operation only works on numbers. " \
            "Use '..' to concatenate strings."); \
        return INTERPRET_RUNTIME_ERROR; \
    } \
    double b = AS_NUMBER(pop(vm)); \
    double a = AS_NUMBER(pop(vm)); \
    push(vm, NUMBER_VAL(a op b)); \
} while (0)

    for (;;) {
        uint8_t instruction = READ_BYTE();
        switch (instruction) {

            case OP_CONST:  push(vm, READ_CONST()); break;
            case OP_NIL:    push(vm, NIL_VAL);       break;
            case OP_TRUE:   push(vm, BOOL_VAL(true));  break;
            case OP_FALSE:  push(vm, BOOL_VAL(false)); break;
            case OP_POP:    pop(vm); break;
            case OP_DUP:    push(vm, peek(vm, 0)); break;

            case OP_DEFINE_GLOBAL: {
                ObjString* name = AS_STRING(READ_CONST());
                tableSet(&vm->globals, name, peek(vm, 0));
                pop(vm);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = AS_STRING(READ_CONST());
                Value val;
                if (!tableGet(&vm->globals, name, &val)) {
                    vmRuntimeError(vm,
                        "Undefined variable '%s'.\n"
                        "  Hint: Did you declare it with 'let %s = ...'?",
                        name->chars, name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, val);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = AS_STRING(READ_CONST());
                if (tableSet(&vm->globals, name, peek(vm, 0))) {
                    tableDelete(&vm->globals, name);
                    vmRuntimeError(vm,
                        "Variable '%s' is not declared.\n"
                        "  Hint: Use 'let %s = ...' to declare it first.",
                        name->chars, name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_LOCAL: { uint8_t slot = READ_BYTE(); push(vm, frame->slots[slot]); break; }
            case OP_SET_LOCAL: { uint8_t slot = READ_BYTE(); frame->slots[slot] = peek(vm, 0); break; }

            case OP_ADD: BINARY_NUM(+); break;
            case OP_SUB: BINARY_NUM(-); break;
            case OP_MUL: BINARY_NUM(*); break;
            case OP_DIV: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1))) {
                    vmRuntimeError(vm, "Division only works on numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm));
                if (b == 0.0) {
                    vmRuntimeError(vm,
                        "Division by zero is not allowed.\n"
                        "  Hint: Check the divisor value before dividing.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, NUMBER_VAL(a / b));
                break;
            }
            case OP_MOD: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1))) {
                    vmRuntimeError(vm, "Modulo only works on numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm));
                if (b == 0.0) { vmRuntimeError(vm, "Modulo by zero is not allowed."); return INTERPRET_RUNTIME_ERROR; }
                push(vm, NUMBER_VAL(fmod(a, b)));
                break;
            }
            case OP_NEGATE: {
                Value v = peek(vm, 0);
                if (!checkNumber(vm, v, "Use the minus sign only on numbers. Example: -5"))
                    return INTERPRET_RUNTIME_ERROR;
                pop(vm); push(vm, NUMBER_VAL(-AS_NUMBER(v)));
                break;
            }
            case OP_CONCAT: {
                Value b_val = pop(vm), a_val = pop(vm);
                push(vm, concatStrings(valueToString(a_val), valueToString(b_val)));
                break;
            }
            case OP_EQ:  { Value b = pop(vm), a = pop(vm); push(vm, BOOL_VAL( valuesEqual(a, b))); break; }
            case OP_NEQ: { Value b = pop(vm), a = pop(vm); push(vm, BOOL_VAL(!valuesEqual(a, b))); break; }
            case OP_LT: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1))) { vmRuntimeError(vm, "Operator '<' only works on numbers."); return INTERPRET_RUNTIME_ERROR; }
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm)); push(vm, BOOL_VAL(a < b)); break;
            }
            case OP_GT: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1))) { vmRuntimeError(vm, "Operator '>' only works on numbers."); return INTERPRET_RUNTIME_ERROR; }
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm)); push(vm, BOOL_VAL(a > b)); break;
            }
            case OP_LTE: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1))) { vmRuntimeError(vm, "Operator '<=' only works on numbers."); return INTERPRET_RUNTIME_ERROR; }
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm)); push(vm, BOOL_VAL(a <= b)); break;
            }
            case OP_GTE: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1))) { vmRuntimeError(vm, "Operator '>=' only works on numbers."); return INTERPRET_RUNTIME_ERROR; }
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm)); push(vm, BOOL_VAL(a >= b)); break;
            }
            case OP_NOT: { Value v = pop(vm); push(vm, BOOL_VAL(!isTruthy(v))); break; }

            case OP_PRINT: { printValue(pop(vm)); printf("\n"); break; }

            case OP_BUILD_ARRAY: {
                uint8_t count = READ_BYTE();
                ObjArray* arr = newArray();
                // items are on stack in order, bottom to top
                Value* base = vm->stack_top - count;
                for (int i = 0; i < count; i++) arrayPush(arr, base[i]);
                vm->stack_top -= count;
                push(vm, OBJ_VAL(arr));
                break;
            }
            case OP_GET_INDEX: {
                Value idx_val = pop(vm);
                Value obj_val = pop(vm);
                if (IS_ARRAY(obj_val)) {
                    if (!IS_NUMBER(idx_val)) {
                        vmRuntimeError(vm, "Array index must be a number.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    int idx = (int)AS_NUMBER(idx_val);
                    push(vm, arrayGet(AS_ARRAY(obj_val), idx));
                } else if (IS_STRING(obj_val)) {
                    if (!IS_NUMBER(idx_val)) {
                        vmRuntimeError(vm, "String index must be a number.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    ObjString* s = AS_STRING(obj_val);
                    int idx = (int)AS_NUMBER(idx_val);
                    if (idx < 0) idx = s->length + idx;
                    if (idx < 0 || idx >= s->length) { push(vm, NIL_VAL); break; }
                    push(vm, OBJ_VAL(copyString(&s->chars[idx], 1)));
                } else {
                    vmRuntimeError(vm, "Only arrays and strings can be indexed.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_INDEX: {
                Value val     = pop(vm);
                Value idx_val = pop(vm);
                Value obj_val = pop(vm);
                if (!IS_ARRAY(obj_val)) {
                    vmRuntimeError(vm, "Only arrays support index assignment.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (!IS_NUMBER(idx_val)) {
                    vmRuntimeError(vm, "Array index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                arraySet(AS_ARRAY(obj_val), (int)AS_NUMBER(idx_val), val);
                break;
            }

            case OP_JUMP:          { uint16_t off = READ_UINT16(); frame->ip += off; break; }
            case OP_JUMP_IF_FALSE: { uint16_t off = READ_UINT16(); if (!isTruthy(peek(vm, 0))) frame->ip += off; break; }
            case OP_LOOP:          { uint16_t off = READ_UINT16(); frame->ip -= off; break; }

            case OP_CALL: {
                int argc = READ_BYTE();
                Value callee = peek(vm, argc);

                // Handle native functions
                if (IS_NATIVE(callee)) {
                    ObjNative* native = AS_NATIVE(callee);
                    // Validate arity (-1 = variadic)
                    if (native->arity != -1 && argc != native->arity) {
                        vmRuntimeError(vm,
                            "Built-in function '%s' expects %d argument(s), but got %d.",
                            native->name, native->arity, argc);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    Value result = native->fn(argc, vm->stack_top - argc);
                    vm->stack_top -= argc + 1;
                    push(vm, result);
                    break;
                }

                if (!IS_FUNCTION(callee)) {
                    vmRuntimeError(vm,
                        "Only functions can be called, not %s.\n"
                        "  Hint: Make sure the name you are calling is a function.",
                        IS_STRING(callee) ? "string" :
                        IS_NUMBER(callee) ? "number" :
                        IS_BOOL(callee)   ? "bool"   :
                        IS_NIL(callee)    ? "nil"    : "object");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (!callFunction(vm, AS_FUNCTION(callee), argc))
                    return INTERPRET_RUNTIME_ERROR;
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_RETURN: {
                Value result = pop(vm);
                vm->frame_count--;
                if (vm->frame_count == 0) { pop(vm); return INTERPRET_OK; }
                vm->stack_top = frame->slots;
                push(vm, result);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_HALT:
                return INTERPRET_OK;

            default:
                vmRuntimeError(vm,
                    "Unknown bytecode instruction: %d. This is an internal bug.", instruction);
                return INTERPRET_RUNTIME_ERROR;
        }
    }

#undef READ_BYTE
#undef READ_UINT16
#undef READ_CONST
#undef BINARY_NUM
}
