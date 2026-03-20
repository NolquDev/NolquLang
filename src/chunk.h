/*
 * chunk.h — Nolqu bytecode chunk
 *
 * A Chunk holds:
 *   - Raw bytecode (uint8_t array)
 *   - Per-instruction source-line table (for error messages)
 *   - Constant pool (Value array)
 *
 * The bytecode format is stable as of v1.0.0.
 * Each opcode is one byte; operands follow immediately.
 */

#ifndef NQ_CHUNK_H
#define NQ_CHUNK_H

#include "common.h"
#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Nolqu Instruction Set ───────────────────────────────────────── */
typedef enum {
    /* Literals */
    OP_CONST,           /* [idx]        push constants[idx]           */
    OP_NIL,             /*              push nil                      */
    OP_TRUE,            /*              push true                     */
    OP_FALSE,           /*              push false                    */

    /* Stack */
    OP_POP,             /*              discard top                   */
    OP_DUP,             /*              duplicate top                 */

    /* Globals */
    OP_DEFINE_GLOBAL,   /* [name_idx]   pop → globals[name]           */
    OP_GET_GLOBAL,      /* [name_idx]   push globals[name]            */
    OP_SET_GLOBAL,      /* [name_idx]   peek → globals[name]          */

    /* Locals (stack-relative, resolved at compile time) */
    OP_GET_LOCAL,       /* [slot]       push stack[base+slot]         */
    OP_SET_LOCAL,       /* [slot]       peek → stack[base+slot]       */

    /* Arithmetic */
    OP_ADD,             /* pop b,a → push a+b                        */
    OP_SUB,             /* pop b,a → push a-b                        */
    OP_MUL,             /* pop b,a → push a*b                        */
    OP_DIV,             /* pop b,a → push a/b                        */
    OP_MOD,             /* pop b,a → push a%b                        */
    OP_NEGATE,          /* pop a   → push -a                         */

    /* Strings */
    OP_CONCAT,          /* pop b,a → push a..b  (string concat)      */

    /* Comparison */
    OP_EQ,              /* pop b,a → push a==b                       */
    OP_NEQ,             /* pop b,a → push a!=b                       */
    OP_LT,              /* pop b,a → push a<b                        */
    OP_GT,              /* pop b,a → push a>b                        */
    OP_LTE,             /* pop b,a → push a<=b                       */
    OP_GTE,             /* pop b,a → push a>=b                       */

    /* Logical */
    OP_NOT,             /* pop a   → push !a                         */

    /* I/O */
    OP_PRINT,           /* pop a, print with newline                 */

    /* Control flow — jumps encode 16-bit offsets (hi, lo) */
    OP_JUMP,            /* [hi,lo]  ip += offset (unconditional)      */
    OP_JUMP_IF_FALSE,   /* [hi,lo]  if !peek: ip += offset; pop cond  */
    OP_LOOP,            /* [hi,lo]  ip -= offset (loop back)          */

    /* ── Superinstructions (fused opcodes for common patterns) ────── */
    OP_ADD_LOCAL_CONST, /* [slot][const_idx]  locals[slot] += const   */
                        /* replaces: GET_LOCAL, CONST, ADD, SET_LOCAL, POP */
    OP_LOOP_IF_LT,      /* [slot_a][slot_b][hi][lo]                   */
                        /* if locals[a] < locals[b]: ip -= offset      */
                        /* replaces: GET_LOCAL a, GET_LOCAL b, LT,     */
                        /*           JUMP_IF_FALSE, POP                */

    /* Arrays */
    OP_BUILD_ARRAY,     /* [count]  pop count values, push array      */
    OP_GET_INDEX,       /* pop idx,arr → push arr[idx]                */
    OP_SET_INDEX,       /* pop val,idx,arr → arr[idx]=val (in-place)  */
    OP_SLICE,           /* pop end,start,obj → push slice             */

    /* Error handling */
    OP_TRY,             /* [hi,lo]  push try-handler; offset=catch    */
    OP_TRY_END,         /* pop try-handler (normal exit)              */
    OP_THROW,           /* pop val, throw as error                    */

    /* Functions */
    OP_CALL,            /* [argc]   call stack[-argc-1] w/ argc args  */
    OP_RETURN,          /* return top of stack (or nil)               */

    /* Halt */
    OP_HALT,            /* stop execution                             */

    OP_COUNT            /* sentinel — total number of opcodes         */
} OpCode;

/* ── Chunk ───────────────────────────────────────────────────────── */
typedef struct Chunk Chunk;
struct Chunk {
    int        count;
    int        capacity;
    uint8_t*   code;
    int*       lines;       /* source line per byte (debug / errors) */
    ValueArray constants;
};

/* ── Chunk API ───────────────────────────────────────────────────── */
void initChunk (Chunk* chunk);
void freeChunk (Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int  addConstant(Chunk* chunk, Value value);

/* Patch a 16-bit forward-jump offset written as two placeholder bytes. */
void patchJump(Chunk* chunk, int offset);

/* ── Disassembler (debug output) ─────────────────────────────────── */
void disassembleChunk      (Chunk* chunk, const char* name);
int  disassembleInstruction(Chunk* chunk, int offset);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NQ_CHUNK_H */
