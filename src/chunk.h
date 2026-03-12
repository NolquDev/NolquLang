#ifndef NQ_CHUNK_H
#define NQ_CHUNK_H

#include "common.h"
#include "value.h"

// ─────────────────────────────────────────────
//  Nolqu Bytecode Instruction Set
//  Each opcode is 1 byte. Operands follow immediately.
// ─────────────────────────────────────────────
typedef enum {
    // ── Literals ──────────────────────────────
    OP_CONST,           // [OP_CONST, const_idx]  push constants[idx]
    OP_NIL,             // push nil
    OP_TRUE,            // push true
    OP_FALSE,           // push false

    // ── Stack manipulation ────────────────────
    OP_POP,             // discard top of stack
    OP_DUP,             // duplicate top of stack

    // ── Global variables ──────────────────────
    OP_DEFINE_GLOBAL,   // [OP, name_idx]  pop → globals[name]
    OP_GET_GLOBAL,      // [OP, name_idx]  push globals[name]
    OP_SET_GLOBAL,      // [OP, name_idx]  peek → globals[name]

    // ── Local variables ───────────────────────
    OP_GET_LOCAL,       // [OP, slot]  push stack[base + slot]
    OP_SET_LOCAL,       // [OP, slot]  peek → stack[base + slot]

    // ── Arithmetic ────────────────────────────
    OP_ADD,             // pop b, pop a, push a + b
    OP_SUB,             // pop b, pop a, push a - b
    OP_MUL,             // pop b, pop a, push a * b
    OP_DIV,             // pop b, pop a, push a / b
    OP_MOD,             // pop b, pop a, push a % b
    OP_NEGATE,          // pop a, push -a

    // ── String ────────────────────────────────
    OP_CONCAT,          // pop b, pop a, push a..b (string concat)

    // ── Comparison ────────────────────────────
    OP_EQ,              // pop b, pop a, push a == b
    OP_NEQ,             // pop b, pop a, push a != b
    OP_LT,              // pop b, pop a, push a < b
    OP_GT,              // pop b, pop a, push a > b
    OP_LTE,             // pop b, pop a, push a <= b
    OP_GTE,             // pop b, pop a, push a >= b

    // ── Logical ───────────────────────────────
    OP_NOT,             // pop a, push !a (logical not)

    // ── I/O ───────────────────────────────────
    OP_PRINT,           // pop a, print a with newline

    // ── Control flow ──────────────────────────
    OP_JUMP,            // [OP, hi, lo]  ip += offset
    OP_JUMP_IF_FALSE,   // [OP, hi, lo]  if !peek: ip += offset (pops condition)
    OP_LOOP,            // [OP, hi, lo]  ip -= offset (jump back)

    // ── Arrays ────────────────────────────────
    OP_BUILD_ARRAY,     // [OP, count]  pop count values, push array
    OP_GET_INDEX,       // pop index, pop array/string, push value
    OP_SET_INDEX,       // pop value, pop index, pop array, set in-place

    // ── Error handling ────────────────────────
    OP_TRY,             // [OP, hi, lo]  push try handler; offset = catch block
    OP_TRY_END,         // pop try handler (no error occurred)
    OP_THROW,           // pop value, throw as error

    // ── Functions ─────────────────────────────
    OP_CALL,            // [OP, argc]  call stack[-argc-1] with argc args
    OP_RETURN,          // return top of stack (or nil)

    // ── Halt ──────────────────────────────────
    OP_HALT,            // stop execution

    OP_COUNT            // sentinel — number of opcodes
} OpCode;

// ─────────────────────────────────────────────
//  Chunk — holds bytecode + constants + debug info
// ─────────────────────────────────────────────
typedef struct Chunk Chunk;
struct Chunk {
    int      count;
    int      capacity;
    uint8_t* code;
    int*     lines;       // source line for each byte (for error messages)
    ValueArray constants;
};

// ─────────────────────────────────────────────
//  Chunk API
// ─────────────────────────────────────────────
void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int  addConstant(Chunk* chunk, Value value);

// Patch a 16-bit jump offset at position `offset` in the chunk
void patchJump(Chunk* chunk, int offset);

// ─────────────────────────────────────────────
//  Disassembler (debug output)
// ─────────────────────────────────────────────
void disassembleChunk(Chunk* chunk, const char* name);
int  disassembleInstruction(Chunk* chunk, int offset);

#endif // NQ_CHUNK_H
