/*
 * vm.h — Nolqu virtual machine
 *
 * Stack-based bytecode interpreter.  One VM instance runs one program;
 * the REPL reuses the same VM across inputs so globals persist.
 *
 * Memory layout:
 *   vm.stack[]        — operand stack (Value[NQ_STACK_MAX])
 *   vm.frames[]       — call stack   (CallFrame[NQ_FRAMES_MAX])
 *   vm.try_stack[]    — error-handler stack (TryHandler[NQ_TRY_MAX])
 *   vm.globals        — hash table of global variables
 */

#ifndef NQ_VM_H
#define NQ_VM_H

#include "common.h"
#include "value.h"
#include "chunk.h"
#include "object.h"
#include "table.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Call frame ──────────────────────────────────────────────────── */
typedef struct {
    ObjFunction* function;  /* function being executed               */
    uint8_t*     ip;        /* instruction pointer (into chunk code) */
    Value*       slots;     /* base of this frame on the value stack */
} CallFrame;

/* ── Interpret result ────────────────────────────────────────────── */
typedef enum {
    INTERPRET_OK,
    INTERPRET_RUNTIME_ERROR,
    INTERPRET_COMPILE_ERROR,
} InterpretResult;

/* ── Try/catch handler ───────────────────────────────────────────── */
#define NQ_TRY_MAX 64

typedef struct {
    uint8_t* catch_ip;      /* ip to jump to on error               */
    Value*   stack_top;     /* stack top at OP_TRY (restored)       */
    int      frame_count;   /* frame depth at OP_TRY (restored)     */
} TryHandler;

/* ── The VM ──────────────────────────────────────────────────────── */
struct VM {
    /* Call stack */
    CallFrame frames[NQ_FRAMES_MAX];
    int       frame_count;

    /* Value (operand) stack */
    Value  stack[NQ_STACK_MAX];
    Value* stack_top;           /* points one past the top element   */

    /* Error-handler stack (pushed by OP_TRY, popped by OP_TRY_END) */
    TryHandler try_stack[NQ_TRY_MAX];
    int        try_depth;

    /*
     * Pending thrown value.  Set when an error is thrown; cleared
     * once a catch handler takes it (or the program terminates).
     */
    Value thrown;

    /* Global variable table */
    Table globals;

    /* Source file path — used in error messages */
    const char* source_path;
};

/* ── VM API ──────────────────────────────────────────────────────── */
void            initVM (VM* vm);
void            freeVM (VM* vm);
InterpretResult runVM  (VM* vm, ObjFunction* script,
                        const char* source_path);

/* Print a runtime error with source location and call-stack trace. */
void vmRuntimeError(VM* vm, const char* fmt, ...);

/* Inject ARGS global array — call after initVM(), before runFile(). */
void nq_set_args(VM* vm, int argc, char** argv);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NQ_VM_H */
